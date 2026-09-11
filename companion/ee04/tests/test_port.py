"""Verify automatic USB discovery without opening a device."""
import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location('select_port', Path(__file__).resolve().parents[1] / 'scripts' / 'select_port.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

def entry(address, vid='0x303a'):
    return {'port': {'address': address, 'properties': {'vid': vid}}}

class PortTests(unittest.TestCase):
    def test_posix_discovery(self):
        for address in ['/dev/cu.usbmodem1234', '/dev/ttyACM0', '/dev/ttyUSB0']:
            self.assertEqual(module.choose_port([entry(address)], preferred=None), address)
    def test_ambiguity_and_explicit_preference(self):
        ports = [entry('/dev/ttyACM0'), entry('/dev/ttyACM1')]
        with self.assertRaises(ValueError): module.choose_port(ports, preferred=None)
        self.assertEqual(module.choose_port(ports, preferred='/dev/ttyACM1'), '/dev/ttyACM1')
    def test_no_match_and_duplicates(self):
        with self.assertRaises(ValueError): module.choose_port([entry('/dev/ttyACM0', '0x1234')], preferred=None)
        self.assertEqual(module.choose_port([entry('/dev/ttyACM0')] * 2, preferred=None), '/dev/ttyACM0')

if __name__ == '__main__': unittest.main()
