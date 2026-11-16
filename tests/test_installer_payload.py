import importlib.util
from pathlib import Path
import struct
import tempfile
import unittest

REPO_ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('payload', REPO_ROOT / 'Installer/generate_payload.py')
payload = importlib.util.module_from_spec(spec)
spec.loader.exec_module(payload)


class PayloadTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.bundle = self.root / 'D.A AURA.vst3'
        self.binary = self.bundle / 'Contents/x86_64-win/D.A AURA.vst3'
        self.binary.parent.mkdir(parents=True)
        self.output = self.root / 'output'
        self.art = self.root / 'welcome.png'
        self.manifest = self.root / 'installer.manifest'
        self.art.write_bytes(b'image')
        self.manifest.write_text('manifest')

    def generate(self):
        payload.generate(self.bundle, self.output, self.art, self.manifest)

    def test_complete_bundle_is_embedded_with_relative_paths(self):
        self.binary.write_bytes(b'plugin')
        extra = self.bundle / 'Contents/Resources/moduleinfo.json'
        extra.parent.mkdir(parents=True)
        extra.write_text('{}')
        self.generate()
        rc = (self.output / 'payload.rc').read_text()
        header = (self.output / 'payload.h').read_text()
        self.assertIn('1 24 ', rc)
        self.assertIn('1 VERSIONINFO', rc)
        self.assertIn('D.A AURA Installer', rc)
        self.assertIn('100 RCDATA ', rc)
        self.assertEqual(rc.count('RCDATA'), 3)
        self.assertIn('moduleinfo.json', header)
        self.assertIn('D.A AURA.vst3', header)
        self.assertNotIn(str(self.root), header)

    def test_empty_or_wrong_bundle_fails(self):
        with self.assertRaises(ValueError):
            self.generate()
        self.binary.write_bytes(b'')
        with self.assertRaises(ValueError):
            self.generate()
        self.binary.write_bytes(b'plugin')
        with self.assertRaises(ValueError):
            payload.generate(self.root, self.output, self.art, self.manifest)
        self.art.unlink()
        with self.assertRaises(ValueError):
            self.generate()

    def test_repository_welcome_artwork_matches_exact_installer_dimensions(self):
        welcome = REPO_ROOT / 'Installer/welcome.png'
        data = welcome.read_bytes()
        self.assertEqual(data[:8], b'\x89PNG\r\n\x1a\n')
        width, height, bit_depth, color_type = struct.unpack('>IIBB', data[16:26])
        self.assertEqual((width, height), (1737, 664))
        self.assertEqual(bit_depth, 8)
        self.assertEqual(color_type, 6)  # RGBA with transparent octagonal corners


if __name__ == '__main__':
    unittest.main()
