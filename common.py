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


def setvar(**kwargs):
  for key, item in kwargs.items():
    VARS[key] = item.format(**VARS)


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
path.relpath = fill_in_args(path.relpath)


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
def topdir(name):
  if not path.isabs(name):
    name = path.abspath(name)
  return path.relpath(name, '{top}')


@fill_in_args
def find_executable(name):
  return (distutils.spawn.find_executable(name) or
          panic('Executable "%s" not found!', name))


@fill_in_args
def find(root, include=None, exclude=None):
  lst = []
  for name in sorted(os.listdir(root)):
    if exclude and any(fnmatch(name, pat) for pat in exclude):
      continue
    if include and not any(fnmatch(name, pat) for pat in include):
      continue
    fullname = path.join(root, name)
    lst.append(fullname)
    if path.isdir(fullname):
      lst.extend(find(fullname, include, exclude))
  return lst


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
      debug('rmtree "%s"', topdir(name))
      shutil.rmtree(name)


@fill_in_args
def remove(*names):
  for name in flatten(names):
    if path.isfile(name):
      debug('remove "%s"', topdir(name))
      os.remove(name)


@fill_in_args
def mkdir(*names):
  for name in flatten(names):
    if not path.isdir(name):
      debug('makedir "%s"', topdir(name))
      os.makedirs(name)


@fill_in_args
def copy(src, dst):
  debug('copy "%s" to "%s"', topdir(src), topdir(dst))
  shutil.copy(src, dst)


@fill_in_args
def copytree(src, dst, **kwargs):
  debug('copytree "%s" to "%s"', topdir(src), topdir(dst))

  mkdir(dst)

  for name in find(src, **kwargs):
    if path.isdir(name):
      mkdir(path.join(dst, path.relpath(name, src)))
    else:
      copy(name, path.join(dst, path.relpath(name, src)))


@fill_in_args
def move(src, dst):
  debug('move "%s" to "%s"', topdir(src), topdir(dst))
  shutil.move(src, dst)


@fill_in_args
def symlink(src, name):
  if not path.islink(name):
    debug('symlink "%s" points at "%s"', topdir(name), src)
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
  info('extract files from "%s"' % topdir(name))

  if name.endswith('.lha'):
    execute('lha', '-x', name)
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
    debug('enter directory "%s"', topdir(name))
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
      with cwd(name):
        execute('svn', 'update')
  elif url.startswith('git'):
    if not path.exists(name):
      execute('git', 'clone', url, name)
    else:
      with cwd(name):
        execute('git', 'pull')
  else:
    panic('URL "%s" not recognized!', url)


@check_stamp
def unpack(name, top_dir=None, in_dir=None):
  try:
    src = glob(path.join('{archives}', name) + '*')[0]
  except IndexError:
    panic('Missing files for "%s".', src)

  dst = path.join('{sources}', name)
  rmtree(dst)

  info('preparing files for "%s"', name)

  if in_dir is not None:
    workdir = path.join('{sources}', in_dir)
  else:
    workdir = '{sources}'

  with cwd(workdir):
    if path.isdir(src):
      copytree(src, dst, exclude=['*.svn'])
    else:
      unarc(src)
      if top_dir is not None:
        move(top_dir, name)
        if path.dirname(top_dir):
          rmtree(path.split(top_dir))


@check_stamp
def patch(name):
  with cwd('{sources}'):
    found = []

    for root, _, files in os.walk(path.join('{patches}', name), topdown=True):
      for name in files:
        if not fnmatch(name, '*~'):
          found.append(path.join(root, name))

    for name in sorted(found):
      if fnmatch(name, '*.diff'):
        execute('patch', '-t', '-p0', '-i', name)
      else:
        dst = path.relpath(name, '{patches}')
        mkdir(path.dirname(dst))
        copy(path.join(root, name), dst)


@check_stamp
def configure(name, *confopts, **kwargs):
  info('configuring "%s"', name)

  if kwargs.get('copy_source', False):
    rmtree(path.join('{build}', name))
    copytree(path.join('{sources}', name), path.join('{build}', name))
    from_dir = '.'
  else:
    from_dir = path.join('{sources}', name)

  with cwd(path.join('{build}', name)):
    remove(find('.', include=['config.cache']))
    execute(path.join(from_dir, 'configure'), *confopts)


@check_stamp
def build(name, *targets, **makevars):
  info('building "%s"', name)

  with cwd(path.join('{build}', name)):
    args = list(targets) + ['%s=%s' % item for item in makevars.items()]
    execute('make', *args)


@check_stamp
def install(name, *targets, **makevars):
  info('installing "%s"', name)

  if not targets:
    targets = ['install']

  with cwd(path.join('{build}', name)):
    args = list(targets) + ['%s=%s' % item for item in makevars.items()]
    execute('make', *args)


__all__ = ['setvar', 'panic', 'cmpver', 'find_executable', 'execute',
           'rmtree', 'mkdir', 'copy', 'copytree', 'unarc', 'fetch', 'cwd',
           'symlink', 'remove', 'move', 'find', 'env', 'path', 'check_stamp',
           'unpack', 'patch', 'configure', 'build', 'install']
