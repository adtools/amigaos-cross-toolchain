#!/usr/bin/env python2.7 -B

from collections import defaultdict
import struct
import sys
import textwrap
from objtools import util
from objtools.hunk import (GetHunkName, GetHunkExtName, GetHunkFlags, Hunk,
                           Header)


class HunkParser(object):
  def __init__(self, hunkfile):
    self.data = hunkfile.read()
    self.index = 0

  def ReadLong(self):
    data = struct.unpack_from('>I', self.data, self.index)[0]
    self.index += 4
    return data

  def ReadWord(self):
    data = struct.unpack_from('>H', self.data, self.index)[0]
    self.index += 2
    return data

  def ReadInt(self):
    data = struct.unpack_from('>i', self.data, self.index)[0]
    self.index += 4
    return data

  def ReadBytes(self, n=None):
    if n is None:
      n = self.ReadLong() * 4
    data = self.data[self.index:self.index + n]
    self.index += n
    return data

  def ReadString(self, n=None):
    return self.ReadBytes(n).strip('\0')

  def ReadSymbol(self, length):
    symbol = self.ReadString(length)
    value = self.ReadInt()
    return (symbol, value)

  def ReadSymbols(self):
    symbols = []

    while True:
      length = self.ReadLong() * 4

      if not length:
        break

      symbols.append(self.ReadSymbol(length))

    return symbols

  def ReadSymbolRefs(self, length):
    symbol = self.ReadString(length)
    count = self.ReadLong()
    refs = [self.ReadLong() for i in range(count)]
    return (symbol, refs)

  def ReadHunkExt(self):
    hunks = defaultdict(list)

    while True:
      longs = self.ReadLong()

      if not longs:
        break

      length = (longs & 0xffffff) * 4
      extName = GetHunkExtName(longs >> 24)

      if extName in ['EXT_DEF', 'EXT_ABS', 'EXT_REL']:
        data = self.ReadSymbol(length)
      elif extName in ['EXT_REF32', 'EXT_REF16', 'EXT_REF8',
                       'EXT_DEXT32', 'EXT_DEXT16', 'EXT_DEXT8']:
        data = self.ReadSymbolRefs(length)
      else:
        raise NotImplementedError('%s not handled.' % extName)

      hunks[extName].append(data)

    return hunks

  def ReadRelocs(self):
    relocs = {}

    while True:
      longs = self.ReadLong()

      if not longs:
        break

      hunkRef = self.ReadLong()
      offsets = [self.ReadLong() for i in range(longs)]
      relocs[hunkRef] = offsets

    return relocs

  def ReadShortRelocs(self):
    start = self.index
    relocs = {}

    while True:
      words = self.ReadWord()

      if not words:
        break

      hunkRef = self.ReadWord()
      offsets = [self.ReadWord() for i in range(words)]
      relocs[hunkRef] = offsets

    if (self.index - start) & 3:
      self.index += 2

    return relocs

  def ReadHeader(self):
    residents = []

    while True:
      longs = self.ReadLong()

      if not longs:
        break

      residents.append(self.ReadString(longs * 4))

    hunks = self.ReadLong()
    first = self.ReadLong()
    last = self.ReadLong()
    specifiers = [self.ReadLong() for i in range(last - first + 1)]

    return Header(residents, hunks, first, last, specifiers)

  def ReadOverlay(self):
    length = self.ReadLong() * 4
    self.index += length + 4
    return ''

  def Parse(self):
    executable = None
    hunks = []

    while self.index < len(self.data):
      hunkId = self.ReadLong()

      try:
        flags = GetHunkFlags(hunkId)
        name = GetHunkName(hunkId)
      except ValueError:
        print 'Parse error at position %d.' % self.index
        raise

      if executable is None:
        executable = (name == 'HUNK_HEADER')

      if name in ['HUNK_END', 'HUNK_BREAK']:
        data = None
      elif name == 'HUNK_EXT':
        data = self.ReadHunkExt()
      elif name == 'HUNK_SYMBOL':
        data = self.ReadSymbols()
      elif name == 'HUNK_HEADER':
        data = self.ReadHeader()
      elif name in ['HUNK_NAME', 'HUNK_UNIT']:
        data = self.ReadString()
      elif name in ['HUNK_CODE', 'HUNK_PPC_CODE', 'HUNK_DATA', 'HUNK_DEBUG',
                    'HUNK_INDEX']:
        data = self.ReadBytes()
      elif name == 'HUNK_OVERLAY':
        data = self.ReadOverlay()
      elif name in ['HUNK_BSS', 'HUNK_LIB']:
        data = self.ReadLong() * 4
      elif name in ['HUNK_RELOC32', 'HUNK_RELOC16', 'HUNK_RELOC8']:
        data = self.ReadRelocs()
      elif name in ['HUNK_DREL32', 'HUNK_DREL16', 'HUNK_DREL8']:
        if executable:
          data = self.ReadShortRelocs()
        else:
          data = self.ReadRelocs()
      else:
        raise NotImplementedError('%s not handled.' % name)

      hunks.append(Hunk(name, data, flags))

    return hunks

if __name__ == '__main__':
  for path in sys.argv[1:]:
    print 'Parsing "%s".' % path
    with open(path) as hunkfile:
      parser = HunkParser(hunkfile)
      for hunk in parser.Parse():
        print hunk.name
        if hunk.name == 'HUNK_EXT':
          for name, symbols in hunk.data.items():
            print ' ', name
            sl = max(len(s) for s, _ in symbols)
            for symbol, value in symbols:
              print '   ', symbol.ljust(sl, ' '), '=', value
        elif hunk.name == 'HUNK_END':
          print ''
        elif hunk.data:
          if isinstance(hunk.data, dict):
            for k, nums in hunk.data.items():
              prefix = '  %d: ' % k
              print textwrap.fill(', '.join(str(n) for n in nums),
                                  width=80,
                                  initial_indent=prefix,
                                  subsequent_indent=' ' * len(prefix))
          elif isinstance(hunk.data, str):
            util.hexdump(hunk.data)
          elif isinstance(hunk.data, list):
            namelen = max(len(name) for name, _ in hunk.data) + 1
            for name, offset in hunk.data:
              print '  {0}: {1}'.format(name.ljust(namelen, ' '), offset)
          else:
            print ' ', repr(hunk.data)
