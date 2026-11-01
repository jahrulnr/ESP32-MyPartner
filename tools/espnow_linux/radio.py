"""Linux monitor-mode packet transport. No interface configuration is changed."""
import socket
import shutil
import subprocess
import time
from pathlib import Path

from . import protocol


class MonitorRadio:
    def __init__(self, interface):
        iw = shutil.which('iw') or '/usr/sbin/iw'
        info = subprocess.run([iw, 'dev', interface, 'info'], text=True, capture_output=True, check=True)
        if 'type monitor' not in info.stdout:
            raise ValueError(f'{interface} is not a monitor interface')
        self.mac = bytes.fromhex(Path(f'/sys/class/net/{interface}/address').read_text().strip().replace(':', ''))
        self.socket = socket.socket(socket.AF_PACKET, socket.SOCK_RAW, socket.htons(3))
        self.socket.bind((interface, 0))
        self.socket.settimeout(0.1)
        self.sequence = 0

    def send_heartbeat(self):
        packet = protocol.heartbeat(self.sequence, int(time.monotonic() * 1000))
        self.socket.send(protocol.action_frame(self.mac, packet, self.sequence))
        self.sequence = (self.sequence + 1) & 0xffff

    def receive(self):
        try:
            packet = self.socket.recv(4096)
        except socket.timeout:
            return None
        result = protocol.read_action_frame(packet)
        if not result:
            return None
        source, destination, payload = result
        if source == self.mac or destination not in (self.mac, protocol.BROADCAST):
            return None
        event = protocol.decode_node(payload)
        if event is None:
            return None
        return ':'.join(f'{byte:02x}' for byte in source), event

    def close(self):
        self.socket.close()
