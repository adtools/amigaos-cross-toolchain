from collections import namedtuple
import os
import struct


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


def GetHunkName(number):
  number &= 0x1fffffff

  for name, value in HunkMap.items():
    if value == number:
      return name

  raise ValueError('Unknown Hunk: %d' % number)


def GetHunkExtName(number):
  for name, value in HunkExtMap.items():
    if value == number:
      return name

  raise ValueError('Unknown HunkExt: %d' % number)


def GetHunkFlags(number):
  flags = []

  for name, value in HunkFlagsMap.items():
    if value & number:
      flags.append(name)

  return flags


class Hunk(namedtuple('Hunk', 'name kind data flags')):
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


class HunkFile(object):
  def __init__(self):
    self._file = None

  def open(self, path, mode='r'):
    self._file = open(path, mode)

  def close(self):
    self._file.close()

  def readLong(self):
    return struct.unpack_from('>I', self._file.read(4))[0]

  def readWord(self):
    return struct.unpack_from('>H', self._file.read(2))[0]

  def readInt(self):
    return struct.unpack_from('>i', self._file.read(4))[0]

  def readBytes(self, n=None):
    if n is None:
      n = self.readLong() * 4
    return self._file.read(n)

  def readString(self, n=None):
    return self.readBytes(n).strip('\0')

  def skip(self, n):
    self._file.seek(n, os.SEEK_CUR)
