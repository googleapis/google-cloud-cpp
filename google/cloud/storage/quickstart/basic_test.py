import unittest

import basic


class TestBasic(unittest.TestCase):

  def test_add(self):
    self.assertEqual(basic.add(1, 2), 3)
    self.assertEqual(basic.add(2, 2), 4)


if __name__ == "__main__":
  unittest.main()
