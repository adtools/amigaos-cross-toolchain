import logging
import os
import struct
import textwrap

from collections import defaultdict, namedtuple
from contextlib import contextmanager

import util
from aout import StringTable, SymbolInfo


log = logging.getLogger(__name__)


HunkMap = {
  'HUNK_UNIT': 999,
  'HUNK_NAME': 1000,
  'HUNK_CODE': 1001,
  'HUNK_DATA': 1002,
  'HUNK_BSS': 1003,
  'HUNK_RELOC32': 1004,
  'HUNK_RELOC16': 1005,
  'HUNK_RELOC8': 1006,
  'HUNK_EXT': 1007,
  'HUNK_SYMBOL': 1008,
  'HUNK_DEBUG': 1009,
  'HUNK_END': 1010,
  'HUNK_HEADER': 1011,
  'HUNK_OVERLAY': 1013,
  'HUNK_BREAK': 1014,
  'HUNK_DREL32': 1015,
  'HUNK_DREL16': 1016,
  'HUNK_DREL8': 1017,
  'HUNK_LIB': 1018,
  'HUNK_INDEX': 1019,
  'HUNK_RELOC32SHORT': 1020,
  'HUNK_RELRELOC32': 1021,
  'HUNK_ABSRELOC16': 1022
}

HunkMapRev = dict((v, k) for k, v in HunkMap.items())

HunkExtMap = {
  'EXT_SYMB': 0,          # symbol table
  'EXT_DEF': 1,           # relocatable definition
  'EXT_ABS': 2,           # Absolute definition
  'EXT_RES': 3,           # no longer supported
  'EXT_REF32': 129,       # 32 bit absolute reference to symbol
  'EXT_COMMON': 130,      # 32 bit absolute reference to COMMON block
  'EXT_REF16': 131,       # 16 bit PC-relative reference to symbol
  'EXT_REF8': 132,        # 8  bit PC-relative reference to symbol
  'EXT_DEXT32': 133,      # 32 bit data relative reference
  'EXT_DEXT16': 134,      # 16 bit data relative reference
  'EXT_DEXT8': 135,       # 8  bit data relative reference
  'EXT_RELREF32': 136,    # 32 bit PC-relative reference to symbol
  'EXT_RELCOMMON': 137,   # 32 bit PC-relative reference to COMMON block
  'EXT_ABSREF16': 138,    # 16 bit absolute reference to symbol
  'EXT_ABSREF8': 139      # 8 bit absolute reference to symbol
}

HunkExtMapRev = dict((v, k) for k, v in HunkExtMap.items())

# Any hunks that have the HUNKB_ADVISORY bit set will be ignored if they
# aren't understood.  When ignored, they're treated like HUNK_DEBUG hunks.
# NOTE: this handling of HUNKB_ADVISORY started as of V39 dos.library!  If
# lading such executables is attempted under <V39 dos, it will fail with a
# bad hunk type.
HUNKB_ADVISORY = 29
HUNKB_CHIP = 30
HUNKB_FAST = 31

HunkFlagsMap = {
  'HUNKF_ADVISORY': 1 << HUNKB_ADVISORY,
  'HUNKF_CHIP': 1 << HUNKB_CHIP,
  'HUNKF_FAST': 1 << HUNKB_FAST
}


Symbol = namedtuple('Symbol', 'name size refs')


class Hunk(object):
  def __init__(self, type_):
    self.type = type_

  @staticmethod
  def getType(number):
    number &= 0x1fffffff

    try:
      return HunkMapRev[number]
    except KeyError:
      raise ValueError('Unknown Hunk: %d' % number)

  @staticmethod
  def getFlags(number):
    return [name for name, value in HunkFlagsMap.items() if value & number]


class HunkSep(Hunk):
  @classmethod
  def parse(cls, hf):
    type_, _ = hf.readHunk('HUNK_END', 'HUNK_BREAK')
    return cls(type_)

  def dump(self):
    print self.type


class HunkStr(Hunk):
  def __init__(self, type_, name=''):
    Hunk.__init__(self, type_)
    self.name = name

  @classmethod
  def parse(cls, hf):
    type_, _ = hf.readHunk('HUNK_NAME', 'HUNK_UNIT')
    return cls(type_, hf.readString())

  def dump(self):
    if self.type == 'HUNK_UNIT':
      print '-' * 80
      print ''

    print self.type
    print '  ' + repr(self.name)


class HunkBinary(Hunk):
  def __init__(self, type_, flags=None, data=''):
    Hunk.__init__(self, type_)
    self.flags = flags or []
    self.data = data

  @classmethod
  def parse(cls, hf):
    type_, flags = hf.readHunk('HUNK_DATA', 'HUNK_CODE')
    return cls(type_, flags, hf.readBytes())

  def dump(self):
    print '{0} {1}'.format(self.type, ', '.join(self.flags))
    if self.data:
      util.hexdump(self.data)
    else:
      print '  [empty]'


class HunkDebug(Hunk):
  def __init__(self, fmt='?', data=''):
    Hunk.__init__(self, 'HUNK_DEBUG')
    self.fmt = fmt
    self.data = data

  @classmethod
  def parse(cls, hf):
    hf.readHunk('HUNK_DEBUG')
    length = hf.readLong() * 4

    with hf.rollback():
      fmt1 = hf.readLong()
      fmt2 = hf.readString(4)

    if fmt1 == 0x10b:
      # magic-number: 0x10b
      # symtabsize strtabsize
      # symtabdata [length=symtabsize]
      # strtabdata [length=strtabsize]
      # [pad bytes]
      hf.skip(4)
      symtabsize = hf.readLong()
      strtabsize = hf.readLong()
      symtab = hf.read(symtabsize)
      hf.skip(4)
      strtab = hf.read(strtabsize)

      symbols = []
      for i in range(0, symtabsize, 12):
        symbols.append(SymbolInfo.decode(symtab[i:i + 12]))

      strings = StringTable.decode(strtab)

      if strtabsize & 3:
        hf.skip(4 - strtabsize & 3)

      return cls('GNU', (symbols, strings))
    elif fmt2 == 'OPTS':
      hf.skip(8)
      return cls('SAS/C opts', hf.read(length - 8))

    return cls('?', hf.read(length))

  def dump(self):
    print '{0} (format: {1!r})'.format(self.type, self.fmt)

    if self.fmt is 'GNU':
      for symbol in self.data[0]:
        print ' ', symbol.as_string(self.data[1])
    else:
      util.hexdump(self.data)


class HunkOverlay(Hunk):
  def __init__(self):
    Hunk.__init__(self, 'HUNK_OVERLAY')

  @classmethod
  def parse(cls, hf):
    hf.readHunk('HUNK_OVERLAY')
    hf.skip(hf.readLong() * 4 + 4)
    return cls()


class HunkBss(Hunk):
  def __init__(self, flags=None, size=0):
    Hunk.__init__(self, 'HUNK_BSS')
    self.flags = flags or []
    self.size = size

  @classmethod
  def parse(cls, hf):
    _, flags = hf.readHunk('HUNK_BSS')
    return cls(flags, hf.readLong() * 4)

  def dump(self):
    print self.type
    print '  {0} bytes'.format(self.size)


class HunkLib(Hunk):
  def __init__(self, size=0):
    Hunk.__init__(self, 'HUNK_LIB')
    self.size = size

  @classmethod
  def parse(cls, hf):
    _, flags = hf.readHunk('HUNK_LIB')
    return cls(hf.readLong() * 4)

  def dump(self):
    print self.type


class HunkReloc(Hunk):
  def __init__(self, type_, relocs=None):
    Hunk.__init__(self, type_)
    self.relocs = relocs or {}

  @classmethod
  def parse(cls, hf):
    type_, _ = hf.readHunk('HUNK_RELOC32', 'HUNK_RELOC16', 'HUNK_RELOC8',
                           'HUNK_DREL32', 'HUNK_DREL16', 'HUNK_DREL8')

    if hf.type is 'executable' and type_ in ['HUNK_DREL32', 'HUNK_DREL16',
                                             'HUNK_DREL8']:
      relocs = hf.readShortRelocs()
    else:
      relocs = hf.readRelocs()

    return cls(type_, relocs)

  def dump(self):
    print self.type
    for k, nums in self.relocs.items():
      prefix = '  %d: ' % k
      print textwrap.fill('[' + ', '.join(str(n) for n in sorted(nums)) + ']',
                          width=68, initial_indent=prefix,
                          subsequent_indent=' ' * (len(prefix) + 1))


class HunkSymbol(Hunk):
  def __init__(self, symbols=None):
    Hunk.__init__(self, 'HUNK_SYMBOL')
    self.symbols = symbols or []

  @classmethod
  def parse(cls, hf):
    hf.readHunk('HUNK_SYMBOL')
    return cls(hf.readSymbols())

  def dump(self):
    print self.type

    l = max(len(s.name) for s in self.symbols) + 1

    for s in sorted(self.symbols, key=lambda s: s.name):
      print '  {0}: {1}'.format(s.name.ljust(l, ' '), s.refs)


class HunkHeader(Hunk):
  def __init__(self, residents=None, hunks=0, first=0, last=0,
               specifiers=None):
    Hunk.__init__(self, 'HUNK_HEADER')
    self.residents = residents or []
    self.hunks = hunks
    self.first = first
    self.last = last
    self.specifiers = specifiers or []

  @classmethod
  def parse(cls, hf):
    hf.readHunk('HUNK_HEADER')

    residents = []

    while True:
      longs = hf.readLong()

      if not longs:
        break

      residents.append(hf.readString(longs * 4))

    hunks, first, last = hf.readLong(), hf.readLong(), hf.readLong()
    specifiers = [hf.readLong() for i in range(last - first + 1)]

    return cls(residents, hunks, first, last, specifiers)

  def dump(self):
    print self.type
    print '  hunks={0}, first={1}, last={2}'.format(self.hunks, self.first,
                                                    self.last)
    print '  residents  : ' + repr(self.residents)
    print '  specifiers : ' + repr(self.specifiers)


class HunkExt(Hunk):
  def __init__(self, hunks=None):
    Hunk.__init__(self, 'HUNK_EXT')
    self.hunks = hunks or defaultdict(list)

  @staticmethod
  def getType(number):
    try:
      return HunkExtMapRev[number]
    except KeyError:
      raise ValueError('Unknown HunkExt: %d' % number)

  @classmethod
  def parse(cls, hf):
    hf.readHunk('HUNK_EXT')

    hunks = defaultdict(list)

    while True:
      longs = hf.readLong()

      if not longs:
        break

      length = (longs & 0xffffff) * 4
      extName = HunkExt.getType(longs >> 24)

      if extName in ['EXT_DEF', 'EXT_ABS', 'EXT_REL']:
        symbol = hf.readSymbol(length)
      elif extName in ['EXT_REF32', 'EXT_REF16', 'EXT_REF8',
                       'EXT_DEXT32', 'EXT_DEXT16', 'EXT_DEXT8']:
        symbol = hf.readString(length)
        count = hf.readLong()
        refs = [hf.readLong() for i in range(count)]
        symbol = Symbol(symbol, None, refs)
      elif extName in ['EXT_COMMON']:
        name = hf.readString(length)
        size = hf.readLong()
        refs_num = hf.readLong()
        refs = [hf.readLong() for i in range(refs_num)]
        symbol = Symbol(name, size, refs)
      else:
        raise NotImplementedError('%s not handled.' % extName)

      hunks[extName].append(symbol)

    return cls(hunks)

  def dump(self):
    print self.type

    for name, symbols in self.hunks.items():
      print ' ', name
      sl = max(len(s.name) for s in symbols)
      for symbol, size, value in symbols:
        print '   ', symbol.ljust(sl, ' '),
        if value is not None:
          print '=', sorted(value) if isinstance(value, list) else value
        else:
          print ':', repr(size)


class HunkIndex(Hunk):
  def __init__(self, units=None):
    Hunk.__init__(self, 'HUNK_INDEX')
    self.units = units or []

  @classmethod
  def parse(cls, hf):
    hf.readHunk('HUNK_INDEX')

    length = hf.readLong() * 4
    last = hf.tell() + length

    strsize = hf.readWord()
    strdata = hf.read(strsize)
    names = {}

    s = 0

    while True:
      e = strdata.find('\0', s, strsize)

      if e == -1:
        names[s] = strdata[s:]
        break

      if e > s:
        names[s] = strdata[s:e]

      s = e + 1

    units = []

    while hf.tell() < last:
      unit_name = names[hf.readWord()]
      first_hunk = hf.readWord() * 4

      hunks_count = hf.readWord()
      hunks = []

      for i in range(hunks_count):
        h_name = names[hf.readWord()]
        h_size = hf.readWord() * 4
        h_type = hf.readWord()

        refs_count = hf.readWord()
        refs = []

        for i in range(refs_count):
          n = hf.readWord()
          try:
            refs.append(names[n])
          except KeyError:
            refs.append(names[n + 1])

        symbols_count = hf.readWord()
        symbols = []

        for i in range(symbols_count):
          s_name = names[hf.readWord()]
          s_value = hf.readWord()
          s_type = hf.readWord()
          symbols.append((s_name, s_value, s_type))

        hunks.append((h_name, h_size, h_type, refs, symbols))

      units.append((unit_name, first_hunk, hunks))

    return cls(units)

  def dump(self):
    print self.type

    for u in self.units:
      print ' ', 'UNIT', repr(u[0]), u[1]
      for h in u[2]:
        print '   ', Hunk.getType(h[2]), repr(h[0]), h[1]
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


class HunkFile(file):
  def __init__(self, *args, **kwargs):
    file.__init__(self, *args, **kwargs)

    self.size = os.path.getsize(self.name)
    self.type = 'object'

  @contextmanager
  def rollback(self):
    pos = self.tell()
    yield self
    self.seek(pos, os.SEEK_SET)

  def readWord(self):
    return struct.unpack_from('>H', self.read(2))[0]

  def readLong(self):
    return struct.unpack_from('>I', self.read(4))[0]

  def readInt(self):
    return struct.unpack_from('>i', self.read(4))[0]

  def readBytes(self):
    return self.read(self.readLong() * 4)

  def readString(self, n=None):
    if n:
      s = self.read(n)
    else:
      s = self.readBytes()
    return s.strip('\0')

  def readSymbol(self, length):
    symbol = self.readString(length)
    value = self.readInt()
    return Symbol(symbol, None, value)

  def readSymbols(self):
    symbols = []

    while True:
      length = self.readLong() * 4

      if not length:
        break

      symbols.append(self.readSymbol(length))

    return symbols

  def readHunk(self, *types):
    hunkId = self.readLong()
    hunkType = Hunk.getType(hunkId)
    hunkFlags = Hunk.getFlags(hunkId)

    if types:
      if hunkType not in types:
        raise ValueError('Unexpected hunk type: %s', hunkType)

    return hunkType, hunkFlags

  def readRelocs(self):
    relocs = {}

    while True:
      longs = self.readLong()

      if not longs:
        break

      hunkRef = self.readLong()
      offsets = [self.readLong() for i in range(longs)]
      relocs[hunkRef] = offsets

    return relocs

  def readShortRelocs(self):
    start = self.tell()
    relocs = {}

    while True:
      words = self.readWord()

      if not words:
        break

      hunkRef = self.readWord()
      offsets = [self.readWord() for i in range(words)]
      relocs[hunkRef] = offsets

    if (self.tell() - start) & 3:
      self.skip(2)

    return relocs

  def skip(self, n):
    self.seek(n, os.SEEK_CUR)

  def eof(self):
    return self.tell() == self.size


HunkClassMap = {
  'HUNK_END': HunkSep,
  'HUNK_BREAK': HunkSep,
  'HUNK_EXT': HunkExt,
  'HUNK_SYMBOL': HunkSymbol,
  'HUNK_HEADER': HunkHeader,
  'HUNK_INDEX': HunkIndex,
  'HUNK_NAME': HunkStr,
  'HUNK_UNIT': HunkStr,
  'HUNK_CODE': HunkBinary,
  'HUNK_DATA': HunkBinary,
  'HUNK_OVERLAY': HunkOverlay,
  'HUNK_BSS': HunkBss,
  'HUNK_LIB': HunkLib,
  'HUNK_RELOC32': HunkReloc,
  'HUNK_RELOC16': HunkReloc,
  'HUNK_RELOC8': HunkReloc,
  'HUNK_DREL32': HunkReloc,
  'HUNK_DREL16': HunkReloc,
  'HUNK_DREL8': HunkReloc,
  'HUNK_DEBUG': HunkDebug
}


def ReadFile(path):
  with HunkFile(path) as hf:
    hunks = []
    units = 0

    while not hf.eof():
      with hf.rollback():
        hunkId = hf.readLong()

      type_ = Hunk.getType(hunkId)

      if type_ is 'HUNK_HEADER':
        hf.type = 'executable'

      if type_ is 'HUNK_UNIT':
        units += 1
        if units > 1:
          hf.type = 'library'

      hunk = HunkClassMap.get(type_, None)

      if not hunk:
        raise NotImplementedError('%s not handled.' % type_)

      try:
        hunks.append(hunk.parse(hf))
      except ValueError:
        log.error('Parse error at position 0x%x.', hf.tell())
        util.hexdump(hf.read())

    return hunks
