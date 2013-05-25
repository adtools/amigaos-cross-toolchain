from collections import namedtuple
import struct


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
    s = str_table[self.symbolnum]
    return '{0:08x} {1:>6} {2}'.format(self.address, t, s)


class SymbolInfo(namedtuple('SymbolInfo', ('strx', 'type', 'other', 'desc',
                                           'value'))):
  type_map = [
    ('UNDF', 0x00), ('EXT', 0x01), ('ABS', 0x02), ('TEXT', 0x04),
    ('DATA', 0x06), ('BSS', 0x08), ('INDR', 0x0a), ('SIZE', 0x0c),
    ('COMM', 0x12), ('FN', 0x1e), ('WARN', 0x1e), ('TYPE', 0x1e),
    ('SLINE', 0x44), ('SO', 0x64), ('SOL', 0x84), ('STAB', 0xe0)]

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

  def as_string(self, str_map):
    visibility = 'g' if self.external else 'l'
    return '{3:08x} {5} {0:>4} {2:04x} {1:02x} {6:02x} {4}'.format(
      self.type_str, self.other, self.desc, self.value, str_map[self.strx],
      visibility, self.type)
