#!/usr/bin/env python2.7 -B

import logging
import sys

from objtools import hunk


if __name__ == '__main__':
  logging.basicConfig()

  for path in sys.argv[1:]:
    print 'Parsing "%s".' % path
    print ''

    for h in hunk.ReadFile(path):
      h.dump()
      print ''
