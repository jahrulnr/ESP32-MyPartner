import importlib.util
from pathlib import Path
import struct
import unittest

spec = importlib.util.spec_from_file_location('mic_stream', Path(__file__).resolve().parents[1] / 'tools/mic_stream.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


def frame(index, samples):
    return struct.pack('<I', index) + struct.pack('<' + 'h' * len(samples), *samples)


class PcmFramesTest(unittest.TestCase):
    def test_continuity_gap_and_sample_index_rollover(self):
        parser = module.PcmFrames()
        self.assertEqual(parser.add(frame(0xfffffffe, [10, 20])), struct.pack('<hh', 10, 20))
        self.assertEqual(parser.add(frame(0, [30])), struct.pack('<h', 30))
        self.assertEqual(parser.add(frame(3, [40])), bytes(4) + struct.pack('<h', 40))
        self.assertEqual(parser.missing_samples, 2)
        self.assertEqual(parser.samples, 6)
        self.assertEqual(parser.peak, 40)
        self.assertEqual(parser.squares, 3000)

    def test_rejects_malformed_and_reordered_frames(self):
        parser = module.PcmFrames()
        with self.assertRaises(ValueError): parser.add(b'bad')
        parser.add(frame(100, [1]))
        with self.assertRaises(ValueError): parser.add(frame(99, [2]))


if __name__ == '__main__': unittest.main()
