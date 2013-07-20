import logging
import os
import struct

from cStringIO import StringIO
from collections import namedtuple, Sequence

from util import hexdump


log = logging.getLogger(__name__)


class Header(namedtuple('Header', ('mid', 'magic', 'text', 'data', 'bss',
                                   'syms', 'entry', 'trsize', 'drsize'))):
  magic_map = {'OMAGIC': 0407, 'NMAGIC': 0410, 'ZMAGIC': 0413}
  mid_map = {'ZERO': 0, 'SUN010': 1, 'SUN020': 2, 'HP200': 200,
             'HP300': 300, 'HPUX': 0x20C, 'HPUX800': 0x20B}

  @classmethod
  def decode(cls, fh):
    data = fh.read(32)

    if len(data) != 32:
      raise ValueError('Not a valid a.out header!')

    mid, magic, text, data, bss, syms, entry, trsize, drsize = \
        struct.unpack('>HHIIIIIII', data)

    for name, value in cls.magic_map.items():
      if magic == value:
        magic = name
        break

    for name, value in cls.mid_map.items():
      if mid == value:
        mid = name
        break

    if magic not in cls.magic_map or mid not in cls.mid_map:
      raise ValueError('Not a valid a.out header!')

    return cls(mid, magic, text, data, bss, syms, entry, trsize, drsize)


class RelocInfo(namedtuple('RelocInfo', ('address', 'symbolnum', 'pcrel',
                                         'length', 'extern', 'baserel',
                                         'jmptable', 'relative', 'copy'))):
  @classmethod
  def decode(cls, data):
    r_address, info = struct.unpack('>II', data)
    r_symbolnum = (info >> 8) & 0xffffff
    r_pcrel = bool(info & 128)
    r_length = (info >> 5) & 3
    r_extern = bool(info & 16)
    r_baserel = bool(info & 8)
    r_jmptable = bool(info & 4)
    r_relative = bool(info & 2)
    r_copy = bool(info & 1)

    return cls(r_address, r_symbolnum, r_pcrel, r_length, r_extern, r_baserel,
               r_jmptable, r_relative, r_copy)

  def as_string(self, strings):
    t = '{0}{1}'.format('BASE' if self.baserel else '', 8 * (1 << self.length))
    try:
      s = strings[self.symbolnum]
    except IndexError:
      if self.symbolnum == 4:
        s = '.text'
      elif self.symbolnum == 6:
        s = '.data'
      elif self.symbolnum == 8:
        s = '.bss'
      else:
        s = str(self.symbolnum)
    return '{0:08x} {1:>6} {2}'.format(self.address, t, s)


class SymbolInfo(namedtuple('SymbolInfo', ('strx', 'type', 'other', 'desc',
                                           'value'))):
  # http://sourceware.org/gdb/current/onlinedocs/stabs/Stab-Types.html
  type_map = [
    ('UNDF', 0x00), ('EXT', 0x01), ('ABS', 0x02), ('TEXT', 0x04),
    ('DATA', 0x06), ('BSS', 0x08), ('INDR', 0x0a), ('SIZE', 0x0c),
    ('COMM', 0x12), ('SETA', 0x14), ('SETT', 0x16), ('SETD', 0x18),
    ('SETB', 0x1a), ('SETV', 0x1c), ('WARNING', 0x1e), ('FN', 0x1f),
    ('GSYM', 0x20), ('FNAME', 0x22), ('FUN', 0x24), ('STSYM', 0x26),
    ('LCSYM', 0x28), ('MAIN', 0x2a), ('ROSYM', 0x2c), ('PC', 0x30),
    ('NSYMS', 0x32), ('NOMAP', 0x34), ('MAC_DEFINE', 0x36), ('OBJ', 0x38),
    ('MAC_UNDEF', 0x3a), ('OPT', 0x3c), ('RSYM', 0x40), ('SLINE', 0x44),
    ('DSLINE', 0x46), ('BSLINE', 0x48), ('FLINE', 0x4c), ('EHDECL', 0x50),
    ('CATCH', 0x54), ('SSYM', 0x60), ('ENDM', 0x62), ('SO', 0x64),
    ('LSYM', 0x80), ('BINCL', 0x82), ('SOL', 0x84), ('PSYM', 0xa0),
    ('EINCL', 0xa2), ('ENTRY', 0xa4), ('LBRAC', 0xc0), ('EXCL', 0xc2),
    ('SCOPE', 0xc4), ('RBRAC', 0xe0), ('BCOMM', 0xe2), ('ECOMM', 0xe4),
    ('ECOML', 0xe8), ('WITH', 0xea), ('NBTEXT', 0xf0), ('NBDATA', 0xf2),
    ('NBBSS', 0xf4), ('NBSTS', 0xf6), ('NBLCS', 0xf8)]

  @classmethod
  def decode(cls, data):
    n_strx, n_type, n_other, n_desc, n_value = \
        struct.unpack('>iBbhI', data)

    return cls(n_strx, n_type, n_other, n_desc, n_value)

  @property
  def external(self):
    return bool(self.type & 1)

  @property
  def type_str(self):
    for t, v in self.type_map:
      if (self.type & ~1) == v:
        return t
    return 'DEBUG'

  def as_string(self, strings):
    visibility = 'g' if self.external else 'l'
    symbolnum = strings.offsetToIndex(self.strx)
    if symbolnum == -1:
      symbol = ''
    else:
      symbol = strings[symbolnum]
    return '{3:08x} {5} {0:<5} {2:04x} {1:02x} {6:02x} {4}'.format(
      self.type_str, self.other, self.desc, self.value, symbol,
      visibility, self.type)


class StringTable(Sequence):
  def __init__(self):
    self._map = {}
    self._table = []

  def __getitem__(self, index):
    return self._table[index]

  def __len__(self):
    return len(self._table)

  def __iter__(self):
    return iter(self._table)

  def __contains__(self, item):
    return item in self._table

  def addString(self, offset, text):
    self._map[offset] = len(self._table)
    self._table.append(text)

  @classmethod
  def decode(cls, data):
    strings = cls()
    s = 0
    while True:
      e = data.find('\0', s)
      if e == -1:
        strings.addString(s + 4, data[s:])
        break
      else:
        strings.addString(s + 4, data[s:e])
      s = e + 1
    return strings

  def offsetToIndex(self, offset):
    return self._map.get(offset, -1)


class Aout(object):
  def __init__(self):
    self._path = None
    self._header = None
    self._text = ''
    self._data = ''
    self._symbols = []
    self._text_relocs = []
    self._data_relocs = []
    self._strings = None

  def read(self, path):
    self._path = path

    with open(path) as fh:
      log.debug('Reading %r of size %d bytes.', path, os.path.getsize(path))

      data = StringIO(fh.read())

      self._header = Header.decode(data)

      self._text = data.read(self._header.text)
      self._data = data.read(self._header.data)
      text_reloc = data.read(self._header.trsize)
      data_reloc = data.read(self._header.drsize)
      symbols = data.read(self._header.syms)
      str_size = struct.unpack('>I', data.read(4))[0]
      strings = data.read()

      if str_size != len(strings):
        log.warn('Wrong size of string table!')

      self._strings = StringTable.decode(strings)

      for i in range(0, len(symbols), 12):
        self._symbols.append(SymbolInfo.decode(symbols[i:i + 12]))

      for i in range(0, len(text_reloc), 8):
        self._text_relocs.append(RelocInfo.decode(text_reloc[i:i + 8]))

      for i in range(0, len(data_reloc), 8):
        self._data_relocs.append(RelocInfo.decode(data_reloc[i:i + 8]))

  def dump(self):
    print self._header
    print ''

    if self._text:
      print 'Text:'
      hexdump(self._text)
      print ''

    if self._data:
      print 'Data:'
      hexdump(self._data)
      print ''

    print 'Symbols:'
    for symbol in self._symbols:
      print ' ', symbol.as_string(self._strings)
    print ''

    if self._text_relocs:
      print 'Text relocations:'
      for reloc in self._text_relocs:
        print ' ', reloc.as_string(self._strings)
      print ''

    if self._data_relocs:
      print 'Data relocations:'
      for reloc in self._text_relocs:
        print ' ', reloc.as_string(self._strings)
      print ''


def ReadFile(path):
  aout = Aout()
  aout.read(path)
  return aout
