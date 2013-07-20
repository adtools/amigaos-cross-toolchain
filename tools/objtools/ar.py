import logging
import os
import struct

from cStringIO import StringIO
from collections import namedtuple


log = logging.getLogger(__name__)


class ArEntry(namedtuple('ArEntry',
                         'name modtime owner group mode data')):
  @classmethod
  def decode(cls, fh):
    data = fh.read(60)

    if len(data) != 60:
      raise ValueError('Not a valid ar archive header!')

    name, modtime, owner, group, mode, length, magic = \
        struct.unpack('16s12s6s6s8s10s2s', data)

    if magic != '`\n':
      raise ValueError('Not a valid ar archive header!')

    length = int(length.strip())
    modtime = int(modtime.strip() or '0')
    owner = int(owner.strip() or '0')
    group = int(group.strip() or '0')
    mode = mode.strip() or '100644'

    if name.startswith('#1/'):
      name_length = int(name[3:])
      name = fh.read(name_length).strip('\0')
    else:
      name_length = 0
      name = name.strip()

    data = fh.read(length - name_length)

    log.debug('entry: file %r, size %d,', name, len(data))

    # next block starts at even boundary
    if length & 1:
      fh.seek(1, os.SEEK_CUR)

    return cls(name, modtime, owner, group, mode, data)


def ReadFile(path):
  entries = []

  with open(path) as fh:
    data = StringIO(fh.read())

    if data.read(8) != '!<arch>\n':
      raise ValueError('%s is not an ar archive' % path)

    size = os.path.getsize(path)

    log.debug('Reading ar archive %r of size %d bytes.', path, size)

    while data.tell() < size:
      # Some archives have version information attached at the end of file,
      # that confuses ArEntry parser, so just skip it.
      try:
        entries.append(ArEntry.decode(data))
      except struct.error:
        break

  return entries
