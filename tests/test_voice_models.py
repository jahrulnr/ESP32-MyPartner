import importlib.util
from pathlib import Path
import struct
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('pack_voice_models', Path(__file__).parents[1] / 'tools/pack_voice_models.py')
packer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(packer)


class VoiceModelPackageTest(unittest.TestCase):
    def test_all_models_fit_and_have_required_assets_and_commands(self):
        source = Path(__file__).resolve().parents[1] / 'model/esp_sr/model'
        for version, name in packer.MODEL_NAMES.items():
            with self.subTest(version=version):
                models = packer.build_models(source, version)
                self.assertEqual(set(models), {'vadnet1_medium', name, 'fst'})
                self.assertIn(b'HI PARTNER', models['fst']['commands_en.txt'])
                self.assertIn(b'HEY PARTNER', models['fst']['commands_en.txt'])
                self.assertLessEqual(len(packer.pack(models)), 0x500000)

    def test_config_rejects_unknown_or_noninteger_model(self):
        with tempfile.TemporaryDirectory() as directory:
            config = Path(directory) / 'model.json'
            for value in ('5', '6', '7'):
                config.write_text('{"multinet": ' + value + '}')
                self.assertEqual(packer.selected_version(config), int(value))
            for value in ('4', '8', 'true', '"6"'):
                config.write_text('{"multinet": ' + value + '}')
                with self.assertRaises(ValueError):
                    packer.selected_version(config)

    def test_packed_offsets_preserve_vocab_and_commands(self):
        models = {'mn6_en': {'vocab': b'S\t0\n', 'mn6_data': b'\0\xffweights'},
                  'fst': {'commands_en.txt': b'1,HI PARTNER\n'}}
        packed = packer.pack(models)
        offset = 4
        recovered = {}
        for _ in range(struct.unpack_from('<I', packed)[0]):
            model = packed[offset:offset + 32].split(b'\0')[0].decode()
            count = struct.unpack_from('<I', packed, offset + 32)[0]
            offset += 36
            recovered[model] = {}
            for _ in range(count):
                name = packed[offset:offset + 32].split(b'\0')[0].decode()
                start, size = struct.unpack_from('<II', packed, offset + 32)
                recovered[model][name] = packed[start:start + size]
                offset += 40
        self.assertEqual(recovered, models)


if __name__ == '__main__':
    unittest.main()
