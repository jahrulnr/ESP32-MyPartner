import importlib.util
from pathlib import Path
import struct
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('pack_voice_models', Path(__file__).parents[1] / 'tools/pack_voice_models.py')
packer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(packer)


class VoiceModelPackageTest(unittest.TestCase):
    def test_vad_package_contains_only_vad_assets(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory)
            vad = source / 'vadnet_model/vadnet1_medium'
            vad.mkdir(parents=True)
            (vad / 'weights').write_bytes(b'vad-weights')
            models = packer.build_models(source)
            self.assertEqual(models, {'vadnet1_medium': {'weights': b'vad-weights'}})
            self.assertLessEqual(len(packer.pack(models)), 0x500000)

    def test_packed_offsets_preserve_model_files(self):
        models = {'vadnet1_medium': {'index': b'index', 'data': b'\0\xffweights'}}
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
