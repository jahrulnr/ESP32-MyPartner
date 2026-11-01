#!/usr/bin/env python3
"""Pack the configured AFE and English MultiNet model for the model partition."""
import argparse
import json
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


MODEL_NAMES = {5: 'mn5q8_en', 6: 'mn6_en', 7: 'mn7_en'}
COMMANDS = {
    5: b'1,HI PARTNER,hi PnRTNk\n1,HEY PARTNER,hd PnRTNk\n',
    6: b'1,HI PARTNER\n1,HEY PARTNER\n',
    7: b'1,HI PARTNER,hi PnRTNk\n1,HEY PARTNER,hd PnRTNk\n',
}


def selected_version(config):
    value = json.loads(config.read_text())['multinet']
    if type(value) is not int or value not in MODEL_NAMES:
        raise ValueError('multinet must be 5, 6, or 7')
    return value


def build_models(source, version):
    name = MODEL_NAMES[version]
    models = {}
    for model, relative in [('vadnet1_medium', 'vadnet_model/vadnet1_medium'),
                            (name, f'multinet_model/{name}')]:
        models[model] = {p.name: p.read_bytes() for p in sorted((source / relative).iterdir()) if p.is_file()}
    required = {f'mn{version if version != 5 else "5q8"}_data',
                f'mn{version if version != 5 else "5q8"}_index', '_MODEL_INFO_'}
    if version in (6, 7):
        required.add('vocab')
    missing = required - models[name].keys()
    if missing:
        raise ValueError(f'{name} missing assets: {sorted(missing)}')
    models['fst'] = {'commands_en.txt': COMMANDS[version]}
    return models


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=Path('model/partner-mn.bin'))
    parser.add_argument('--config', type=Path, default=Path('model/voice_model.json'))
    args = parser.parse_args()
    root = Path(__file__).resolve().parent.parent
    source = root / 'model/esp_sr/model'
    config = args.config if args.config.is_absolute() else root / args.config
    version = selected_version(config)
    models = build_models(source, version)
    output = args.output if args.output.is_absolute() else root / args.output
    output.write_bytes(pack(models))
    print(f'{output}: {output.stat().st_size} bytes; AFE + {MODEL_NAMES[version]}, no WakeNet')


if __name__ == '__main__':
    main()
