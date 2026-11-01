"""ESP-NOW v1 action frames and espnow-sensors-node v1 application packets."""
import os
import struct

OUI = bytes.fromhex('18fe34')
BROADCAST = b'\xff' * 6
RADIOTAP_TX = b'\x00\x00\x08\x00\x00\x00\x00\x00'
FRAME_HEADER = struct.Struct('<BBHIB')


def heartbeat(sequence, milliseconds):
    payload = b'PIO_MASTER_V1'
    return FRAME_HEADER.pack(1, 2, sequence & 0xffff, milliseconds & 0xffffffff, len(payload)) + payload


def action_frame(source, payload, sequence=0):
    if len(source) != 6 or len(payload) > 250:
        raise ValueError('Invalid MAC or ESP-NOW v1 payload length')
    dot11 = struct.pack('<HH6s6s6sH', 0x00d0, 0, BROADCAST, source, BROADCAST, (sequence & 0xfff) << 4)
    element = b'\xdd' + bytes([len(payload) + 5]) + OUI + b'\x04\x01' + payload
    return RADIOTAP_TX + dot11 + b'\x7f' + OUI + os.urandom(4) + element


def read_action_frame(packet):
    """Return (source, destination, payload), or None for non-ESP-NOW frames."""
    if len(packet) < 8 or packet[0] != 0:
        return None
    rt_len = int.from_bytes(packet[2:4], 'little')
    if rt_len < 8 or len(packet) < rt_len + 39:
        return None
    frame = packet[rt_len:]
    control = int.from_bytes(frame[:2], 'little')
    if control & 0x00fc != 0x00d0 or control & 0x4000:
        return None
    if frame[24:28] != b'\x7f' + OUI:
        return None
    # A v1 application packet fits in one vendor-specific information element.
    if frame[32] != 0xdd or frame[34:38] != OUI + b'\x04':
        return None
    element_size = frame[33]
    if element_size < 5 or 34 + element_size > len(frame):
        return None
    version = frame[38] & 0x0f
    if version not in (1, 2):
        return None
    return frame[10:16], frame[4:10], frame[39:34 + element_size]


def decode_node(payload):
    if len(payload) < FRAME_HEADER.size:
        return None
    version, kind, sequence, timestamp, length = FRAME_HEADER.unpack_from(payload)
    if version != 1 or length > 200 or len(payload) < FRAME_HEADER.size + length:
        return None
    data = payload[FRAME_HEADER.size:FRAME_HEADER.size + length]
    if kind == 1:
        return {'kind': 'hello'}
    if kind != 4 or len(data) < 4 or data[:2] != b'\xb1\x01':
        return None
    subtype = data[2]
    if subtype == 1 and len(data) == 28:
        return {'kind': 'identity', 'name': data[4:28].split(b'\0', 1)[0].decode('utf-8', 'replace')}
    if subtype == 2 and len(data) == 8:
        temperature, humidity = struct.unpack_from('<hH', data, 4)
        return {'kind': 'climate', 'temperature_c': temperature / 10, 'humidity_percent': humidity / 10}
    if subtype == 6 and len(data) == 4:
        return {'kind': 'alive'}
    if subtype == 11 and len(data) in (12, 14):
        detected, has_distance, distance, frames, count = struct.unpack_from('<BBHHH', data, 4)
        if detected > 1 or has_distance > 1:
            return None
        return {'kind': 'presence', 'detected': bool(detected) if frames else None,
                'distance_cm': distance if frames and has_distance else None,
                'radar_frames': frames, 'radar_bytes': count}
    return None
