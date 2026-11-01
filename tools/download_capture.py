#!/usr/bin/env python3
"""Download boot WAV files over FTP, validating duration and repeated-transfer hashes."""
import argparse
from ftplib import FTP
import hashlib
import json
from pathlib import Path
import time
import wave


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('host')
    parser.add_argument('--user', default='pio')
    parser.add_argument('--password', default='tes12345')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    with FTP(args.host, timeout=90) as ftp:
        ftp.login(args.user, args.password)
        deadline = time.monotonic() + 120
        while True:
            names = {name.rsplit('/', 1)[-1] for name in ftp.nlst()}
            if {'mic-boot.wav', 'afe-boot.wav'} <= names:
                break
            if time.monotonic() >= deadline:
                raise TimeoutError('Boot WAV files not available')
            print('Waiting for capture files...', flush=True)
            time.sleep(3)
        for name in ('mic-boot.wav', 'afe-boot.wav'):
            path = args.output / name
            with path.open('wb') as output:
                ftp.retrbinary('RETR /' + name, output.write, blocksize=8192)
            checksum = hashlib.sha256(path.read_bytes()).hexdigest()
            repeated = hashlib.sha256()
            ftp.retrbinary('RETR /' + name, repeated.update, blocksize=8192)
            if checksum != repeated.hexdigest():
                raise IOError(f'FTP checksum mismatch: {name}')
            with wave.open(str(path), 'rb') as wav:
                shape = wav.getframerate(), wav.getnchannels(), wav.getsampwidth(), wav.getnframes()
                if shape != (16000, 1, 2, 480000):
                    raise ValueError(f'Unexpected WAV format/duration: {shape}')
            print(json.dumps({'file': str(path.resolve()), 'bytes': path.stat().st_size,
                              'seconds': 30, 'sha256': checksum}), flush=True)


if __name__ == '__main__':
    main()
