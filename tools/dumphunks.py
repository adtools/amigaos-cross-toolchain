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

  def ReadIndex(self):
    length = self.ReadLong() * 4
    last = self.index + length

    strsize = self.ReadWord()
    strdata = self.ReadBytes(strsize)
    names = {}

    s = 0

    while True:
      e = strdata.find('\0', s, strsize)

      if e == -1:
        break

      if e > s:
        names[s] = strdata[s:e]

      s = e + 1

    units = []

    while self.index < last:
      unit_name = names[self.ReadWord()]
      first_hunk = self.ReadWord() * 4

      hunks_count = self.ReadWord()
      hunks = []

      for i in range(hunks_count):
        h_name = names[self.ReadWord()]
        h_size = self.ReadWord() * 4
        h_type = self.ReadWord()

        refs_count = self.ReadWord()
        refs = []

        for i in range(refs_count):
          n = self.ReadWord()
          try:
            refs.append(names[n])
          except KeyError:
            refs.append(names[n + 1])

        symbols_count = self.ReadWord()
        symbols = []

        for i in range(symbols_count):
          s_name = names[self.ReadWord()]
          s_value = self.ReadWord()
          s_type = self.ReadWord()
          symbols.append((s_name, s_value, s_type))

        hunks.append((h_name, h_size, h_type, refs, symbols))

      units.append((unit_name, first_hunk, hunks))

    return units

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

      kind = 'binary'

      if name in ['HUNK_END', 'HUNK_BREAK']:
        data = None
        kind = 'separator'
      elif name == 'HUNK_EXT':
        data = self.ReadHunkExt()
      elif name == 'HUNK_SYMBOL':
        data = self.ReadSymbols()
        kind = 'symbols'
      elif name == 'HUNK_HEADER':
        data = self.ReadHeader()
        kind = 'header'
      elif name == 'HUNK_INDEX':
        data = self.ReadIndex()
        kind = 'index'
      elif name in ['HUNK_NAME', 'HUNK_UNIT']:
        data = self.ReadString()
        kind = 'string'
      elif name in ['HUNK_CODE', 'HUNK_PPC_CODE', 'HUNK_DATA', 'HUNK_DEBUG']:
        data = self.ReadBytes()
      elif name == 'HUNK_OVERLAY':
        data = self.ReadOverlay()
      elif name in ['HUNK_BSS', 'HUNK_LIB']:
        data = self.ReadLong() * 4
        kind = 'container'
      elif name in ['HUNK_RELOC32', 'HUNK_RELOC16', 'HUNK_RELOC8']:
        data = self.ReadRelocs()
        kind = 'relocs'
      elif name in ['HUNK_DREL32', 'HUNK_DREL16', 'HUNK_DREL8']:
        if executable:
          data = self.ReadShortRelocs()
        else:
          data = self.ReadRelocs()
        kind = 'relocs'
      else:
        raise NotImplementedError('%s not handled.' % name)

      hunks.append(Hunk(name, kind, data, flags))

    return hunks

if __name__ == '__main__':
  for path in sys.argv[1:]:
    print 'Parsing "%s".' % path
    with open(path) as hunkfile:
      parser = HunkParser(hunkfile)
      for hunk in parser.Parse():
        print hunk.name

        if hunk.kind == 'separator':
          print ''
        elif hunk.kind == 'header':
          print hunk.data
        elif hunk.name == 'HUNK_EXT':
          for name, symbols in hunk.data.items():
            print ' ', name
            sl = max(len(s) for s, _ in symbols)
            for symbol, value in symbols:
              print '   ', symbol.ljust(sl, ' '), '=', value
        elif hunk.kind == 'relocs':
          for k, nums in hunk.data.items():
            prefix = '  %d: ' % k
            print textwrap.fill('[' + ', '.join(str(n) for n in nums) + ']',
                                width=68,
                                initial_indent=prefix,
                                subsequent_indent=' ' * (len(prefix) + 1))
        elif hunk.kind == 'binary':
          util.hexdump(hunk.data)
        elif hunk.kind == 'symbols':
          namelen = max(len(name) for name, _ in hunk.data) + 1
          for name, offset in hunk.data:
            print '  {0}: {1}'.format(name.ljust(namelen, ' '), offset)
        elif hunk.kind == 'string':
          print ' ', hunk.data
        elif hunk.kind == 'index':
          for u in hunk.data:
            print ' ', 'UNIT', repr(u[0]), u[1]
            for h in u[2]:
              print '   ', GetHunkName(h[2]), repr(h[0]), h[1]
              if h[3]:
                print '     ', 'REFS'
                for s in sorted(h[3]):
                  print '       ', s
              if h[4]:
                print '     ', 'DEFS'
                l = max(len(s[0]) for s in h[4])
                for s in sorted(h[4], key=lambda x: x[1]):
                  print '       ', s[0].ljust(l), '=', s[1]
            print ''
        else:
          print ' ', repr(hunk.data)
