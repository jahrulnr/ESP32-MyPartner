#!/usr/bin/env python3
"""Pack the AFE VAD model for the model partition."""
import argparse
from pathlib import Path
import struct


def pack(models):
    header_size = 4 + sum(36 + 40 * len(files) for files in models.values())
    header = bytearray(struct.pack('<I', len(models)))
    payload = bytearray()
    for model, files in models.items():
        header += model.encode().ljust(32, b'\0') + struct.pack('<I', len(files))
        for name, content in files.items():
            assert len(name.encode()) <= 31
            header += name.encode().ljust(32, b'\0')
            header += struct.pack('<II', header_size + len(payload), len(content))
            payload += content
    assert len(header) == header_size
    return header + payload


def build_models(source):
    directory = source / 'vadnet_model/vadnet1_medium'
    files = {p.name: p.read_bytes() for p in sorted(directory.iterdir()) if p.is_file()}
    if not files:
        raise ValueError('vadnet1_medium contains no model assets')
    return {'vadnet1_medium': files}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=Path('model/partner-vad.bin'))
    args = parser.parse_args()
    root = Path(__file__).resolve().parent.parent
    source = root / 'model/esp_sr/model'
    models = build_models(source)
    output = args.output if args.output.is_absolute() else root / args.output
    output.write_bytes(pack(models))
    print(f'{output}: {output.stat().st_size} bytes; AFE VAD only')


if __name__ == '__main__':
    main()
