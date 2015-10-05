import unittest

from sysconfig import get_config_vars
BITPACKED = get_config_vars().get('BITPACKED')

import _testbitpacked
mode = _testbitpacked.get_mode()

class BitPackedTestCase(unittest.TestCase):
    def test_macro_sideeffect(self):
        _testbitpacked.test_macro_sideeffect()

    def test_internal(self):
        self.assertIsInstance(BITPACKED, int)
        self.assertIn(BITPACKED, [0, 1])
        _testbitpacked.print_bitpacked_configuration()
        print('This interpreter is {} mode'.format('BitPacked' if BITPACKED else 'Normal'))
        print('mode: {:032b}'.format(mode))
        tid_prefix = 'BITPACKED_TYPEID_'
        tid_dict = {k[len(tid_prefix):]: getattr(_testbitpacked, k) for k in dir(_testbitpacked) if k.startswith(tid_prefix)}
        typetbl = _testbitpacked.get_typetable()
        if BITPACKED:
            self.assertEqual(len(tid_dict), 12)
            self.assertEqual(len(typetbl), 16)
            self.assertEqual(set(tid_dict.values()), {n for n in range(0, 32, 2) if n % 8 != 0})
            self.assertEqual(tid_dict['LONG'] ^ tid_dict['BOOL'], 0b00010000)
            self.assertEqual(tid_dict['FLOAT'] ^ tid_dict['FLOAT_RSV'], 0b00010000)
            self.assertLess(tid_dict['FLOAT'], tid_dict['FLOAT_RSV'])
            self.assertEqual(len({tid_dict[k] & 0b00000110 for k
                                  in ['LONG', 'BOOL', 'FLOAT', 'FLOAT_RSV']}), 1)
            self.assertTrue(all(typetbl[n] is None for n in range(0, 16, 4)))
            self.assertIs(typetbl[tid_dict['LONG']//2], int)
            self.assertIs(typetbl[tid_dict['BOOL']//2], bool)
            self.assertIs(typetbl[tid_dict['FLOAT']//2], float)
            self.assertIs(typetbl[tid_dict['FLOAT_RSV']//2], float)
            self.assertIs(typetbl[tid_dict['NONE']//2], type(None))
            self.assertIs(typetbl[tid_dict['NOTIMPL']//2], type(NotImplemented))
            self.assertIs(typetbl[tid_dict['RANGE']//2], range)
        else:
            self.assertEqual(len(tid_dict), 0)
            self.assertIs(typetbl, None)

    def test_longobject(self):
        n = 20

        if BITPACKED:
            self.assertEqual(id(n) % 32, _testbitpacked.BITPACKED_TYPEID_LONG)
            self.assertEqual(id(n), n << 32 | _testbitpacked.BITPACKED_TYPEID_LONG)
            self.assertIs(40 * 300, -600 * -20)
        else:
            self.assertEqual(id(n) % 8, 0)
            self.assertIsNot(40 * 300, -600 * -20)

    def test_boolobject(self):
        if BITPACKED:
            self.assertEqual(id(1) ^ id(True), 0x0010)
            self.assertEqual(id(0) ^ id(False), 0x0010)
            self.assertEqual(id(0) ^ id(0.0), 0x0008)
            self.assertEqual(id(True), 1 << 32 | _testbitpacked.BITPACKED_TYPEID_BOOL)
            self.assertEqual(id(False), _testbitpacked.BITPACKED_TYPEID_BOOL)
        else:
            self.assertEqual(id(True) % 8, 0)
            self.assertEqual(id(False) % 8, 0)
        self.assertIs(40 < 300, -600 < -20)
        self.assertIs(40 > 300, -600 > -20)

    def test_floatobject(self):
        n = 3.1415

        if BITPACKED:
            self.assertEqual(id(n) % 16, _testbitpacked.BITPACKED_TYPEID_FLOAT)
            self.assertEqual(id(n) ^ id(-n), 1 << 63)
            self.assertIs(4.0 * 3.0, -6.0 * -2.0)
            def check_bitpackedobj(x):
                self.assertEqual(id(x) % 16, _testbitpacked.BITPACKED_TYPEID_FLOAT)
            def check_conventional(x):
                self.assertEqual(id(x) % 8, 0)
            check_bitpackedobj(float('-inf'))
            check_bitpackedobj(-8.371198e+298)
            check_conventional(-8.371142e+298)
            check_conventional(-8.589973e+9)
            check_bitpackedobj(-8.589915e+9)
            check_bitpackedobj(-4.656634e-10)
            check_conventional(-4.656602e-10)
            check_conventional(-4.778331e-299)
            check_bitpackedobj(-4.778299e-299)
            check_bitpackedobj(0.0)
            check_bitpackedobj(+4.778299e-299)
            check_conventional(+4.778331e-299)
            check_conventional(+4.656602e-10)
            check_bitpackedobj(+4.656634e-10)
            check_bitpackedobj(+8.589915e+9)
            check_conventional(+8.589973e+9)
            check_conventional(+8.371142e+298)
            check_bitpackedobj(+8.371198e+298)
            check_bitpackedobj(float('+inf'))
            check_bitpackedobj(float('nan'))
        else:
            self.assertEqual(id(n) % 8, 0)
            self.assertIsNot(4.0 * 3.0, -6.0 * -2.0)

    def test_refcnt(self):
        from sys import getrefcount
        a = [1234, 567.8, None, True, False, range(20), NotImplemented]
        b = []
        refcnt1 = refcnt2 = 0
        for x in a:
            refcnt1 = getrefcount(x)
            b.append(x)
            refcnt2 = getrefcount(x)
            if mode & _testbitpacked.MODE_BITPACKED:
                self.assertNotEqual(id(x) % 8, 0)
                if mode & _testbitpacked.MODE_NOREFCNT:
                    self.assertEqual(refcnt1, _testbitpacked.BITPACKED_DUMMY_REFCNT)
                else:
                    refcnt1 += 1
                self.assertEqual(refcnt1, refcnt2)
            else:
                self.assertEqual(id(x) % 8, 0)
                self.assertEqual(refcnt1+1, refcnt2)

    def test_rangeobject(self):
        r1 = range(0, 300, 4)[::3]
        r2 = range(0, 600, 2)[:150:6]
        if BITPACKED:
            self.assertIs(r1, r2)
            self.assertNotEqual(id(r1) % 8, 0)
            self.assertNotEqual(id(r2) % 8, 0)
        else:
            self.assertIsNot(r1, r2)
            self.assertEqual(id(r1) % 8, 0)
            self.assertEqual(id(r2) % 8, 0)


if __name__ == "__main__":
    unittest.main()
