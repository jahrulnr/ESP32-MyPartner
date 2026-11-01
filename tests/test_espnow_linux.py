import json
import struct
import threading
import unittest
import urllib.request
from http.server import ThreadingHTTPServer

from tools.espnow_linux import protocol
from tools.espnow_linux.__main__ import handler_for
from tools.espnow_linux.state import SensorState


def state_packet(data):
    return struct.pack('<BBHIB', 1, 4, 1, 0, len(data)) + data


class ProtocolTest(unittest.TestCase):
    def test_heartbeat_and_wire_frame(self):
        mac = bytes.fromhex('7008941c7d5f')
        beat = protocol.heartbeat(0x1234, 0x12345678)
        self.assertEqual(beat, bytes.fromhex('01023412785634120d') + b'PIO_MASTER_V1')
        frame = protocol.action_frame(mac, beat)
        self.assertEqual(protocol.read_action_frame(frame), (mac, protocol.BROADCAST, beat))
        self.assertEqual(frame[32:36], bytes.fromhex('7f18fe34'))
        self.assertEqual(frame[40:47], bytes.fromhex('dd1b18fe340401'))
        for cut in (0, 8, 39, len(frame) - 1):
            self.assertIsNone(protocol.read_action_frame(frame[:cut]))
        encrypted = bytearray(frame)
        encrypted[9] |= 0x40
        self.assertIsNone(protocol.read_action_frame(encrypted))

    def test_sensor_frames(self):
        climate = state_packet(bytes.fromhex('b1010200f4ff2b02'))
        self.assertEqual(protocol.decode_node(climate), {'kind': 'climate', 'temperature_c': -1.2, 'humidity_percent': 55.5})
        radar = state_packet(bytes.fromhex('b1010b0001012c01030014000203'))
        self.assertEqual(protocol.decode_node(radar), {'kind': 'presence', 'detected': True, 'distance_cm': 300, 'radar_frames': 3, 'radar_bytes': 20})
        no_frames = state_packet(bytes.fromhex('b1010b0001012c0100001400'))
        self.assertIsNone(protocol.decode_node(no_frames)['detected'])
        self.assertIsNone(protocol.decode_node(no_frames)['distance_cm'])
        self.assertIsNone(protocol.decode_node(radar[:-1]))

    def test_http_and_staleness(self):
        now = [100.0]
        state = SensorState(clock=lambda: now[0])
        state.update('01:02:03:04:05:06', {'kind': 'climate', 'temperature_c': 31.0, 'humidity_percent': 58.0})
        state.update('01:02:03:04:05:06', {'kind': 'presence', 'detected': True, 'distance_cm': 93})
        server = ThreadingHTTPServer(('127.0.0.1', 0), handler_for(state))
        worker = threading.Thread(target=server.serve_forever)
        worker.start()
        try:
            with urllib.request.urlopen(f'http://127.0.0.1:{server.server_port}/sensors') as response:
                node = json.load(response)['nodes'][0]
            self.assertTrue(node['online'])
            self.assertEqual(node['climate']['humidity_percent'], 58.0)
            self.assertEqual(node['presence']['distance_cm'], 93)
            now[0] += 46
            self.assertFalse(state.snapshot()['nodes'][0]['online'])
            self.assertTrue(state.snapshot()['nodes'][0]['presence']['fresh'])
            self.assertTrue(state.snapshot()['nodes'][0]['climate']['fresh'])
            now[0] += 254
            self.assertFalse(state.snapshot()['nodes'][0]['presence']['fresh'])
            self.assertFalse(state.snapshot()['nodes'][0]['climate']['fresh'])
        finally:
            server.shutdown()
            server.server_close()
            worker.join()


if __name__ == '__main__':
    unittest.main()
