#!/usr/bin/env python2.7 -B

import sys
from objtools import aout

if __name__ == '__main__':
  for path in sys.argv[1:]:
    obj = aout.Aout()
    obj.read(path)
    obj.dump()
