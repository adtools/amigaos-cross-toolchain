from collections import namedtuple
import os
import struct
from util import hexdump


class Header(namedtuple('Header', ('mid', 'magic', 'text', 'data', 'bss',
                                   'syms', 'entry', 'trsize', 'drsize'))):
  magic_map = {'OMAGIC': 0407, 'NMAGIC': 0410, 'ZMAGIC': 0413}
  mid_map = {'ZERO': 0, 'SUN010': 1, 'SUN020': 2, 'HP200': 200,
             'HP300': 300, 'HPUX': 0x20C, 'HPUX800': 0x20B}

  @classmethod
  def decode(cls, data):
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

  def as_string(self, str_table):
    t = '{0}{1}'.format('BASE' if self.baserel else '', 8 * (1 << self.length))
    try:
      s = str_table[self.symbolnum]
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
  type_map = [
    ('UNDF', 0x00), ('EXT', 0x01), ('ABS', 0x02), ('TEXT', 0x04),
    ('DATA', 0x06), ('BSS', 0x08), ('INDR', 0x0a), ('SIZE', 0x0c),
    ('COMM', 0x12), ('FN', 0x1e), ('WARN', 0x1e), ('TYPE', 0x1e), ('FN', 0x1f),
    ('GSYM', 0x20), ('FNAME', 0x22), ('FUN', 0x24), ('STSYM', 0x26),
    ('LCSYM', 0x28), ('RSYM', 0x40), ('SLINE', 0x44), ('SSYM', 0x60),
    ('SO', 0x64), ('LSYM', 0x80), ('SOL', 0x84), ('PSYM', 0xa0),
    ('ENTRY', 0xa4), ('LBRAC', 0xc0), ('STAB', 0xe0), ('RBRAC', 0xe0),
    ('BCOMM', 0xe2), ('ECOMM', 0xe4), ('ECOML', 0xe8), ('LENG', 0xfe)]

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

  def as_string(self, str_map):
    visibility = 'g' if self.external else 'l'
    symbol = str_map.get(self.strx, '')
    return '{3:08x} {5} {0:<5} {2:04x} {1:02x} {6:02x} {4}'.format(
      self.type_str, self.other, self.desc, self.value, symbol,
      visibility, self.type)


class Aout(object):
  def __init__(self):
    self._path = None
    self._header = None
    self._text = ''
    self._data = ''
    self._str_map = {}
    self._str_table = []
    self._symbols = []
    self._text_relocs = []
    self._data_relocs = []

  def addString(self, offset, text):
    self._str_map[offset] = text
    self._str_table.append(text)

  def read(self, path):
    self._path = path

    with open(path) as data:
      print 'Reading {0} of size {1} bytes.'.format(
        repr(path), os.stat(path)[6])

      self._header = Header.decode(data.read(32))

      self._text = data.read(self._header.text)
      self._data = data.read(self._header.data)
      text_reloc = data.read(self._header.trsize)
      data_reloc = data.read(self._header.drsize)
      symbols = data.read(self._header.syms)
      str_size = struct.unpack('>I', data.read(4))[0]
      strings = data.read()

      if str_size != len(strings):
        print 'Warning: wrong size of string table!'

      s = 0
      while True:
        e = strings.find('\0', s)
        if e == -1:
          self.addString(s + 4, strings[s:])
          break
        else:
          self.addString(s + 4, strings[s:e])
        s = e + 1

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
      print ' ', symbol.as_string(self._str_map)
    print ''

    if self._text_relocs:
      print 'Text relocations:'
      for reloc in self._text_relocs:
        print ' ', reloc.as_string(self._str_table)
      print ''

    if self._data_relocs:
      print 'Data relocations:'
      for reloc in self._text_relocs:
        print ' ', reloc.as_string(self._str_table)
      print ''
