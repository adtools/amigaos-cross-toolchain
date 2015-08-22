#!/usr/bin/env python -B

from logging import debug, info, error
from glob import glob
from os import path
from fnmatch import fnmatch
import contextlib
import distutils.spawn
import shutil
import os
import subprocess
import sys
import tarfile
import urllib2
import zipfile


VARS = {}
setvar = VARS.update


def fill_in(value):
  if type(value) == str:
    return value.format(**VARS)
  return value


def fill_in_args(fn):
  def wrapper(*args, **kwargs):
    args = list(fill_in(arg) for arg in args)
    kwargs = dict((key, fill_in(value)) for key, value in kwargs.items())
    return fn(*args, **kwargs)
  return wrapper


def flatten(*args):
  queue = list(args)

  while queue:
    item = queue.pop(0)
    if type(item) == list:
      queue = item + queue
    elif type(item) == tuple:
      queue = list(item) + queue
    else:
      yield item


chdir = fill_in_args(os.chdir)
path.exists = fill_in_args(path.exists)
path.join = fill_in_args(path.join)


@fill_in_args
def panic(*args):
  error(*args)
  sys.exit(1)


@fill_in_args
def cmpver(op, v1, v2):
  assert op in ['eq', 'lt', 'gt']

  v1 = [int(x) for x in v1.split('.')]
  v2 = [int(x) for x in v2.split('.')]

  def _cmp(l1, l2):
    if not len(l1) and not len(l2):
      return 0
    if not len(l1):
      return -1
    if not len(l2):
      return 1

    if l1[0] < l2[0]:
      return -1
    if l1[0] > l2[0]:
      return 1
    if l1[0] == l2[0]:
      return _cmp(l1[1:], l2[1:])

  res = _cmp(v1, v2)

  return ((op == 'eq' and res == 0) or
          (op == 'lt' and res < 0) or
          (op == 'gt' and res > 0))


@fill_in_args
def relpath(name):
  if not path.isabs(name):
    name = path.abspath(name)
  return path.relpath(name, fill_in('{top}'))


@fill_in_args
def find_executable(name):
  return (distutils.spawn.find_executable(name) or
          panic('Executable "%s" not found!', name))


@fill_in_args
def find_files(pattern):
  found = []
  for root, dirs, files in os.walk('.', topdown=True):
    for name in files:
      if fnmatch(name, pattern):
        found.append(path.join(root, name))
  return found


@fill_in_args
def touch(name):
  try:
    os.utime(name, None)
  except:
    open(name, 'a').close()


@fill_in_args
def rmtree(*names):
  for name in flatten(names):
    if path.isdir(name):
      debug('rmtree "%s"', relpath(name))
      shutil.rmtree(name)


@fill_in_args
def remove(*names):
  for name in flatten(names):
    if path.isfile(name):
      debug('remove "%s"', relpath(name))
      os.remove(name)


@fill_in_args
def mkdir(*names):
  for name in flatten(names):
    debug('makedir "%s"', relpath(name))
    os.makedirs(name)


@fill_in_args
def copytree(src, dst, **kwargs):
  debug('copytree "%s" to "%s"', relpath(src), relpath(dst))
  shutil.copytree(src, dst, **kwargs)


@fill_in_args
def rename(src, dst):
  debug('rename "%s" to "%s"', relpath(src), relpath(dst))
  os.rename(src, dst)


@fill_in_args
def symlink(src, name):
  debug('symlink "%s" from "%s"', src, relpath(name))
  os.symlink(src, name)


@fill_in_args
def execute(*cmd):
  debug('execute "%s"', " ".join(cmd))
  try:
    subprocess.check_call(cmd)
  except subprocess.CalledProcessError as ex:
    panic('command "%s" failed with %d', " ".join(list(ex.cmd)), ex.returncode)


@fill_in_args
def download(url, name):
  u = urllib2.urlopen(url)
  meta = u.info()
  size = int(meta.getheaders('Content-Length')[0])
  info('download: %s (size: %d)' % (name, size))

  with open(name, 'wb') as f:
    done = 0
    block = 8192
    while True:
      buf = u.read(block)
      if not buf:
        break
      done += len(buf)
      f.write(buf)
      status = r"%10d  [%3.2f%%]" % (done, done * 100. / size)
      status = status + chr(8) * (len(status) + 1)
      print status,

  print ""


@fill_in_args
def unarc(name):
  info('extract files from "%s"' % relpath(name))

  if name.endswith('.lha'):
    execute('lha', '-xq', name)
  else:
    if name.endswith('.tar.gz') or name.endswith('.tar.bz2'):
      module = tarfile
    elif name.endswith('.zip'):
      module = zipfile
    else:
      raise RuntimeError('Unrecognized archive: "%s"', name)

    arc = module.open(name, 'r')
    for item in arc:
      debug('extract "%s"' % item.name)
      arc.extract(item)
    arc.close()


@contextlib.contextmanager
def cwd(name):
  old = os.getcwd()
  if not path.exists(name):
    mkdir(name)
  try:
    debug('enter directory "%s"', relpath(name))
    chdir(name)
    yield
  finally:
    chdir(old)


@contextlib.contextmanager
def env(**kwargs):
  backup = {}
  try:
    for key, value in kwargs.items():
      debug('changing environment variable "%s" to "%s"', key, value)
      old = os.environ.get(key, None)
      os.environ[key] = fill_in(value)
      backup[key] = old
    yield
  finally:
    for key, value in backup.items():
      debug('restoring old value of environment variable "%s"', key)
      if value is None:
        del os.environ[key]
      else:
        os.environ[key] = value


def check_stamp(fn):
  @fill_in_args
  def wrapper(*args, **kwargs):
    name = fn.func_name.replace('_', '-')
    if len(args) > 0:
      name = name + "-" + str(fill_in(args[0]))
    stamp = path.join('{stamps}', name)
    if not path.exists('{stamps}'):
      mkdir('{stamps}')
    if not path.exists(stamp):
      fn(*args, **kwargs)
      touch(stamp)
    else:
      info('already done "%s"', name)

  return wrapper


@check_stamp
def fetch(name, url):
  if url.startswith('http') or url.startswith('ftp'):
    if not path.exists(name):
      download(url, name)
    else:
      info('File "%s" already downloaded.', name)
  elif url.startswith('svn'):
    if not path.exists(name):
      execute('svn', 'checkout', url, name)
    else:
      execute('svn', 'update', name)


@check_stamp
def source(name, copy=None):
  try:
    src = glob(path.join('{archives}', name) + '*')[0]
  except IndexError:
    panic('Missing source for "%s".', src)

  dst = path.join('{sources}', name)
  rmtree(dst)

  info('preparing source "%s"', name)

  with cwd('{sources}'):
    if path.isdir(src):
      copytree(src, dst, ignore=shutil.ignore_patterns('.svn'))
    else:
      unarc(src)

    if copy is not None:
      rmtree(copy)
      copytree(dst, copy)


@check_stamp
def configure(name, *confopts):
  info('configuring "%s"', name)

  with cwd(path.join('{build}', name)):
    remove(find_files('config.cache'))
    execute(path.join('{sources}', name, 'configure'), *confopts)


@check_stamp
def build(name, *confopts):
  info('building "%s"', name)

  with cwd(path.join('{build}', name)):
    execute('make')


@check_stamp
def install(name, *confopts):
  info('installing "%s"', name)

  with cwd(path.join('{build}', name)):
    execute('make', 'install')


__all__ = ['setvar', 'panic', 'cmpver', 'find_executable', 'execute',
           'rmtree', 'mkdir', 'copytree', 'unarc', 'fetch', 'cwd',
           'remove', 'rename', 'find_files', 'env', 'path', 'check_stamp',
           'source', 'configure', 'build', 'install']
