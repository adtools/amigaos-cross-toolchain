#!/usr/bin/env python

from collections import namedtuple, defaultdict
import inspect
import struct
import sys
import textwrap


class HunkMap(object):
  # Any hunks that have the HUNKB_ADVISORY bit set will be ignored if they
  # aren't understood.  When ignored, they're treated like HUNK_DEBUG hunks.
  # NOTE: this handling of HUNKB_ADVISORY started as of V39 dos.library!  If
  # lading such executables is attempted under <V39 dos, it will fail with a
  # bad hunk type.
  HUNKB_ADVISORY = 29
  HUNKB_CHIP = 30
  HUNKB_FAST = 31
  HUNKF_ADVISORY = 1 << HUNKB_ADVISORY
  HUNKF_CHIP = 1 << HUNKB_CHIP
  HUNKF_FAST = 1 << HUNKB_FAST

  HUNK_UNIT = 999
  HUNK_NAME = 1000
  HUNK_CODE = 1001
  HUNK_DATA = 1002
  HUNK_BSS = 1003
  HUNK_RELOC32 = 1004
  HUNK_RELOC16 = 1005
  HUNK_RELOC8 = 1006
  HUNK_EXT = 1007
  HUNK_SYMBOL = 1008
  HUNK_DEBUG = 1009
  HUNK_END = 1010
  HUNK_HEADER = 1011
  HUNK_OVERLAY = 1013
  HUNK_BREAK = 1014
  HUNK_DREL32 = 1015
  HUNK_DREL16 = 1016
  HUNK_DREL8 = 1017
  HUNK_LIB = 1018
  HUNK_INDEX = 1019
  HUNK_RELOC32SHORT = 1020
  HUNK_RELRELOC32 = 1021
  HUNK_ABSRELOC16 = 1022
  HUNK_PPC_CODE = 1257
  HUNK_RELRELOC26 = 1260

  @classmethod
  def GetName(cls, number):
    number &= 0x1fffffff

    for name, value in inspect.getmembers(cls):
      if name.startswith('HUNK_'):
        if value == number:
          return name

    raise ValueError('Unknown Hunk: %d' % number)

  @classmethod
  def GetFlags(cls, number):
    flags = []

    for name, value in inspect.getmembers(cls):
      if name.startswith('HUNKF_'):
        if value & number:
          flags.append(name)

    return flags


class HunkExtMap(object):
  EXT_SYMB = 0           # symbol table
  EXT_DEF = 1            # relocatable definition
  EXT_ABS = 2            # Absolute definition
  EXT_RES = 3            # no longer supported
  EXT_REF32 = 129        # 32 bit absolute reference to symbol
  EXT_COMMON = 130       # 32 bit absolute reference to COMMON block
  EXT_REF16 = 131        # 16 bit PC-relative reference to symbol
  EXT_REF8 = 132         # 8  bit PC-relative reference to symbol
  EXT_DEXT32 = 133       # 32 bit data relative reference
  EXT_DEXT16 = 134       # 16 bit data relative reference
  EXT_DEXT8 = 135        # 8  bit data relative reference
  EXT_RELREF32 = 136     # 32 bit PC-relative reference to symbol
  EXT_RELCOMMON = 137	   # 32 bit PC-relative reference to COMMON block
  EXT_ABSREF16 = 138     # 16 bit absolute reference to symbol
  EXT_ABSREF8 = 139      # 8 bit absolute reference to symbol
  EXT_RELREF26 = 229

  @classmethod
  def GetName(cls, number):
    for name, value in inspect.getmembers(cls):
      if name.startswith('EXT_'):
        if value == number:
          return name
    raise ValueError('Unknown HunkExt: %d' % number)


class Hunk(namedtuple('Hunk', 'name data flags')):
  def __repr__(self):
    if self.data:
      if self.flags:
        flags = ', flags=%s' % '|'.join(self.flags)
      else:
        flags = ''
      return '%s(%r%s)' % (self.name, self.data, flags)
    else:
      return self.name


Header = namedtuple('Header', 'residents hunks first last specifiers')


class HunkParser(object):
  def __init__(self, hunkfile):
    self.data = hunkfile.read()
    self.index = 0

  def ReadLong(self):
    data = struct.unpack('>I', self.data[self.index:self.index + 4])[0]
    self.index += 4
    return data

  def ReadWord(self):
    data = struct.unpack('>H', self.data[self.index:self.index + 2])[0]
    self.index += 2
    return data

  def ReadInt(self):
    data = struct.unpack('>i', self.data[self.index:self.index + 4])[0]
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
      extName = HunkExtMap.GetName(longs >> 24)

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
      relocs.update({hunkRef: offsets})

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
      relocs.update({hunkRef: offsets})

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
        flags = HunkMap.GetFlags(hunkId)
        name = HunkMap.GetName(hunkId)
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
            hexch = ['%.2x' % ord(c) for c in hunk.data]
            ascii = []
            for c in hunk.data:
              if ord(c) >= 32 and ord(c) < 127:
                ascii.append(c)
              else:
                ascii.append('.')
            for i in range(0, len(hexch), 16):
              print '  {0} |{1}|'.format(
                ' '.join(hexch[i:i + 16]).ljust(47, ' '),
                ''.join(ascii[i:i + 16]))
          elif isinstance(hunk.data, list):
            namelen = max(len(name) for name, _ in hunk.data) + 1
            for name, offset in hunk.data:
              print '  {0}: {1}'.format(name.ljust(namelen, ' '), offset)
          else:
            print ' ', repr(hunk.data)
