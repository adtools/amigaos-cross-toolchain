#!/usr/bin/env python2.7 -B

import logging
import sys

from objtools import ar


if __name__ == '__main__':
  logging.basicConfig()

  for path in sys.argv[1:]:
    print '%s:' % path
    for num, entry in enumerate(ar.ReadFile(path), start=1):
      print '%5d:' % num, entry.name, '(length: %d)' % len(entry.data)
    print ''
