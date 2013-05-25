#!/usr/bin/env python2.7 -B

import os
import struct
import sys
from objtools import aout, util


if __name__ == '__main__':
  for path in sys.argv[1:]:
    with open(path) as data:
      print 'Parsing {0} of size {1} bytes.'.format(
        repr(path), os.stat(path)[6])

      header = aout.Header.decode(data.read(32))

      text_seg = data.read(header.text)
      data_seg = data.read(header.data)
      text_reloc = data.read(header.trsize)
      data_reloc = data.read(header.drsize)
      symbols = data.read(header.syms)
      str_size = struct.unpack('>I', data.read(4))[0]
      strings = data.read()

      if str_size != len(strings):
        print 'Warning: wrong size of string table!'

      str_map = {}
      str_table = []
      s = 0

      while True:
        e = strings.find('\0', s)
        if e == -1:
          str_map[s + 4] = strings[s:]
          str_table.append(strings[s:])
          break
        str_map[s + 4] = strings[s:e]
        str_table.append(strings[s:e])
        s = e + 1

      print header

      print "Text:"
      util.hexdump(text_seg)

      print "Data:"
      util.hexdump(data_seg)

      print "Symbols:"
      for i in range(0, len(symbols), 12):
        symbol = aout.SymbolInfo.decode(symbols[i:i + 12])
        print ' ', symbol.as_string(str_map)

      print "Text relocations:"
      for i in range(0, len(text_reloc), 8):
        reloc = aout.RelocInfo.decode(text_reloc[i:i + 8])
        print ' ', reloc.as_string(str_table)

      print "Data relocations:"
      for i in range(0, len(data_reloc), 8):
        reloc = aout.RelocInfo.decode(data_reloc[i:i + 8])
        print ' ', reloc.as_string(str_table)
