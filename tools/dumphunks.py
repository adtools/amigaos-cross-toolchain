#!/usr/bin/env python2.7 -B

import sys
from objtools.hunk import HunkParser


if __name__ == '__main__':
  for path in sys.argv[1:]:
    print 'Parsing "%s".' % path
    print ''

    for hunk in HunkParser(path):
      hunk.dump()
      print ''
