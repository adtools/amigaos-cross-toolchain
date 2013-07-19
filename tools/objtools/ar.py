import os
import struct

from collections import namedtuple


class ArEntry(namedtuple('ArEntry',
                         'name modtime owner group mode data')):
  @classmethod
  def decode(cls, ar):
    name, modtime, owner, group, mode, length, magic = \
        struct.unpack('16s12s6s6s8s10s2s', ar.read(60))

    length = int(length.strip())
    modtime = int(modtime.strip())
    owner = int(owner.strip() or '0')
    group = int(group.strip() or '0')
    mode = mode.strip() or '100644'

    if name.startswith('#1/'):
      name_length = int(name[3:])
      name = ar.read(name_length).strip('\0')
    else:
      name_length = 0
      name = name.strip()

    data = ar.read(length - name_length)

    # next block starts at even boundary
    if length & 1:
      ar.seek(1, os.SEEK_CUR)

    return cls(name, modtime, owner, group, mode, data)


def ReadFile(path):
  entries = []

  with open(path) as ar:
    if ar.read(8) != '!<arch>\n':
      raise RuntimeError('%s is not an ar archive', path)

    ar_size = os.stat(path).st_size

    while ar.tell() < ar_size:
      entries.append(ArEntry.decode(ar))

  return entries
