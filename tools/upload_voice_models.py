#!/usr/bin/env python3
"""Flash local ESP-SR and picoTTS data using offsets from the board's CSV."""
import argparse
import csv
import os
from pathlib import Path
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', default='/dev/ttyACM0')
    parser.add_argument('--baud', default='921600')
    parser.add_argument('--check', action='store_true', help='Validate files/partitions without flashing')
    parser.add_argument('--with-tts', action='store_true', help='Also flash both picoTTS voice partitions')
    args = parser.parse_args()
    root = Path(__file__).resolve().parent.parent
    subprocess.run([sys.executable, str(root / 'tools/pack_voice_models.py')], check=True)
    partitions = {}
    for line in (root / 'boards/esp32-s3-devkitc1-n16r8.csv').read_text().splitlines():
        if not line.strip() or line.lstrip().startswith('#'):
            continue
        row = [cell.strip() for cell in next(csv.reader([line]))]
        partitions[row[0]] = (int(row[3], 0), int(row[4], 0))
    data = []
    files = [('model', 'partner-vad.bin')]
    if args.with_tts:
        files = [('picotts_ta', 'en-US_ta.bin'), ('picotts_sg', 'en-US_lh0_sg.bin')] + files
    for name, filename in files:
        path = root / 'model' / filename
        offset, capacity = partitions[name]
        if not path.is_file() or not 0 < path.stat().st_size <= capacity:
            raise SystemExit(f'{path} missing, empty, or larger than partition {name}')
        print(f'{name}: {path.stat().st_size} bytes at {offset:#x}', flush=True)
        data.extend([hex(offset), str(path)])
    if args.check:
        return
    core = Path(os.environ.get('PLATFORMIO_CORE_DIR', Path.home() / '.platformio'))
    python = core / 'penv/bin/python'
    esptool = core / 'packages/tool-esptoolpy/esptool.py'
    subprocess.run([str(python), str(esptool), '--chip', 'esp32s3', '--port', args.port,
                    '--baud', args.baud, '--before', 'default-reset', '--after', 'hard-reset',
                    'write-flash', *data], check=True, cwd=root)


if __name__ == '__main__':
    main()
