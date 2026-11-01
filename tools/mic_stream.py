#!/usr/bin/env python3
"""Capture ESP32 PCM over WebSocket and optionally transcribe locally with whisper.cpp."""
import argparse
from contextlib import closing
import json
import math
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import time
import wave


class PcmFrames:
    """Reconstruct a sample-indexed PCM stream; replace dropped samples with silence."""
    def __init__(self):
        self.expected = None
        self.missing_samples = 0
        self.samples = 0
        self.peak = 0
        self.squares = 0
        self.clipped = 0

    def add(self, frame):
        if len(frame) < 6 or (len(frame) - 4) % 2:
            raise ValueError('Invalid PCM frame')
        index = struct.unpack_from('<I', frame)[0]
        data = frame[4:]
        gap = 0 if self.expected is None else (index - self.expected) & 0xffffffff
        if gap > 160000:
            raise ValueError('Out-of-order audio or a gap longer than 10 seconds')
        self.missing_samples += gap
        self.samples += gap + len(data) // 2
        for (sample,) in struct.iter_unpack('<h', data):
            magnitude = abs(sample)
            self.peak = max(self.peak, magnitude)
            self.squares += sample * sample
            if sample == -32768 or sample == 32767:
                self.clipped += 1
        self.expected = (index + len(data) // 2) & 0xffffffff
        return bytes(gap * 2) + data


def connect(host, port):
    from websocket import create_connection
    ws = create_connection(f'ws://{host}:{port}/', timeout=10, http_no_proxy=[host])
    ready = json.loads(ws.recv())
    if ready.get('type') != 'ready' or ready.get('rate') != 16000:
        ws.close()
        raise RuntimeError(f'Unexpected server greeting: {ready}')
    return ws


def control(args):
    with closing(connect(args.host, args.port)) as ws:
        ws.send('STATUS' if args.action == 'status' else 'STOP')
        result = json.loads(ws.recv())
        print(json.dumps(result), flush=True)
        if result.get('type') == 'error':
            raise RuntimeError(result.get('message', 'WebSocket error'))


def capture(args):
    from websocket import WebSocketTimeoutException
    from websocket._abnf import ABNF
    temporary = None
    if args.output:
        path = Path(args.output).expanduser()
        path.parent.mkdir(parents=True, exist_ok=True)
    else:
        handle = tempfile.NamedTemporaryFile(prefix='partner-mic-', suffix='.wav', delete=False)
        temporary = Path(handle.name)
        handle.close()
        path = temporary
    frames = PcmFrames()
    dropped = None
    try:
        with closing(connect(args.host, args.port)) as ws:
            ws.send('START')
            event = json.loads(ws.recv())
            if event.get('type') != 'started':
                raise RuntimeError(f'Stream did not start: {event}')
            print('Mic ON; STOP via partner-mic off, Ctrl-C, or --seconds.', flush=True)
            ws.settimeout(.25)
            deadline = time.monotonic() + args.seconds if args.seconds else None
            stopped = False
            stop_deadline = None
            with wave.open(str(path), 'wb') as output:
                output.setnchannels(1)
                output.setsampwidth(2)
                output.setframerate(16000)
                while True:
                    try:
                        if deadline is not None and time.monotonic() >= deadline and not stopped:
                            ws.send('STOP')
                            stopped = True
                            stop_deadline = time.monotonic() + 10
                        if stop_deadline is not None and time.monotonic() >= stop_deadline:
                            raise TimeoutError('Device did not acknowledge STOP')
                        try:
                            opcode, packet = ws.recv_data()
                        except WebSocketTimeoutException:
                            continue
                        if opcode == ABNF.OPCODE_BINARY:
                            output.writeframesraw(frames.add(packet))
                        elif opcode == ABNF.OPCODE_TEXT:
                            event = json.loads(packet)
                            if event.get('type') == 'stopped':
                                dropped = event.get('dropped_frames')
                                break
                            if event.get('type') == 'error':
                                raise RuntimeError(event.get('message', 'WebSocket error'))
                        elif opcode == ABNF.OPCODE_CLOSE:
                            raise ConnectionError('Device closed the stream')
                    except KeyboardInterrupt:
                        if not stopped:
                            ws.send('STOP')
                            stopped = True
                            stop_deadline = time.monotonic() + 10
                            print('\nStopping stream...', flush=True)
        print(f'Mic OFF: {frames.samples / 16000:.2f} s, missing samples={frames.missing_samples}, device dropped frames={dropped}', flush=True)
        if not frames.samples:
            raise RuntimeError('No microphone samples received')
        print(f'PCM16 peak={frames.peak}, RMS={math.sqrt(frames.squares / frames.samples):.1f}, clipped={frames.clipped}', flush=True)
        if args.output:
            print(f'WAV: {path.resolve()}', flush=True)
        if args.transcribe and frames.samples:
            model = Path(args.model).expanduser()
            command = ['whisper-cli', '-m', str(model), '-f', str(path), '-l', args.language,
                       '-t', str(args.threads), '-nt', '-np']
            print('Whisper transcription (local):', flush=True)
            result = subprocess.run(command, check=True, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            print(result.stdout.strip() or '[no speech recognized]', flush=True)
    finally:
        if temporary:
            temporary.unlink(missing_ok=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=('on', 'off', 'status'))
    parser.add_argument('--host', required=True, help='ESP32 address, e.g. 192.168.18.136')
    parser.add_argument('--port', type=int, default=81)
    parser.add_argument('--seconds', type=float, help='Automatically stop after this many seconds')
    parser.add_argument('--output', help='Optionally retain captured WAV on this computer')
    parser.add_argument('--no-transcribe', dest='transcribe', action='store_false')
    parser.add_argument('--language', default='auto', help='Whisper language or auto')
    parser.add_argument('--threads', type=int, default=4)
    parser.add_argument('--model', default=os.environ.get('PARTNER_WHISPER_MODEL',
        '~/.local/share/partner-whisper/whisper.cpp/models/ggml-base.bin'))
    args = parser.parse_args()
    if args.seconds is not None and args.seconds <= 0:
        parser.error('--seconds must be positive')
    if args.action == 'on':
        capture(args)
    else:
        control(args)


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as error:
        print(f'partner-mic: {error}', file=sys.stderr)
        sys.exit(1)
