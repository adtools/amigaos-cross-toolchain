#!/usr/bin/env python2.7 -B

import logging
import json
import os
import sha
import sys
from UserDict import UserDict

from objtools import ar, aout, hunk


def ShaSum(data):
  return sha.new(data).digest().encode('hex')


class Database(UserDict):
  def __init__(self, topdir):
    UserDict.__init__(self)
    self.topdir = topdir

  def name(self, filepath):
    return filepath[len(self.topdir):]

  def addFile(self, filepath, filetype):
    with open(filepath) as f:
      cksum = ShaSum(f.read())
    size = os.path.getsize(filepath)
    name = self.name(filepath)
    self.data[name] = {'size': size, 'sha': cksum, 'type': filetype}

  def addArchive(self, filepath, archive):
    objects = [(obj.name, {'size': len(obj.data), 'sha': ShaSum(obj.data)})
               for obj in archive]
    name = self.name(filepath)
    self.data[name] = {'objects': dict(objects), 'type': 'archive'}

  def dumps(self):
    return json.dumps(self.data, indent=2, sort_keys=True)

  def readAout(self, filepath):
    try:
      aout.ReadFile(filepath)
    except ValueError:
      return False

    self.addFile(filepath, 'a.out')
    return True

  def readHunk(self, filepath):
    try:
      hs = hunk.ReadFile(filepath)
    except ValueError:
      return False

    if hs[0].type == 'HUNK_UNIT':
      units = sum(1 for h in hs if h.type == 'HUNK_UNIT')
      if units > 1:
        filetype = 'ALink'
      else:
        filetype = 'Amiga Hunk object'
    elif hs[0].type == 'HUNK_LIB':
      filetype = 'BLink'
    elif hs[0].type == 'HUNK_HEADER':
      filetype = 'Amiga Hunk executable'

    self.addFile(filepath, filetype)
    return True

  def readAr(self, filepath):
    try:
      archive = ar.ReadFile(filepath)
    except ValueError:
      return False

    self.addArchive(filepath, archive)
    return True

  def build(self):
    for path, _, filenames in os.walk(self.topdir):
      for filename in filenames:
        filepath = os.path.join(path, filename)

        if os.path.islink(filepath):
          continue

        if self.readAout(filepath):
          continue

        if self.readHunk(filepath):
          continue

        if self.readAr(filepath):
          continue


if __name__ == '__main__':
  logging.basicConfig()

  db = Database(sys.argv[1])
  db.build()
  print db.dumps()
