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
import tempfile


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
def find(root, **kwargs):
  only_files = kwargs.get('only_files', False)
  include = kwargs.get('include', None)
  exclude = kwargs.get('exclude', None)
  lst = []
  for name in sorted(os.listdir(root)):
    if exclude and any(fnmatch(name, pat) for pat in exclude):
      continue
    if include and not any(fnmatch(name, pat) for pat in include):
      continue
    fullname = path.join(root, name)
    if not (path.isdir(fullname) and only_files):
      lst.append(fullname)
    if path.isdir(fullname):
      lst.extend(find(fullname, **kwargs))
  return lst


@fill_in_args
def touch(name):
  try:
    os.utime(name, None)
  except:
    open(name, 'a').close()


@fill_in_args
def mkdtemp(**kwargs):
  if 'dir' in kwargs and not path.isdir(kwargs['dir']):
    mkdir(kwargs['dir'])
  return tempfile.mkdtemp(**kwargs)


@fill_in_args
def mkstemp(**kwargs):
  if 'dir' in kwargs and not path.isdir(kwargs['dir']):
    mkdir(kwargs['dir'])
  return tempfile.mkstemp(**kwargs)


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
  shutil.copy2(src, dst)


@fill_in_args
def copytree(src, dst, **kwargs):
  debug('copytree "%s" to "%s"', topdir(src), topdir(dst))

  mkdir(dst)

  for name in find(src, **kwargs):
    target = path.join(dst, path.relpath(name, src))
    if path.isdir(name):
      mkdir(target)
    else:
      copy(name, target)


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
def chmod(name, mode):
  debug('change permissions on "%s" to "%o"', topdir(name), mode)
  os.chmod(name, mode)


@fill_in_args
def execute(*cmd):
  debug('execute "%s"', " ".join(cmd))
  try:
    subprocess.check_call(cmd)
  except subprocess.CalledProcessError as ex:
    panic('command "%s" failed with %d', " ".join(list(ex.cmd)), ex.returncode)


@fill_in_args
def textfile(*lines):
  f, name = mkstemp(dir='{tmpdir}')
  debug('creating text file script "%s"', topdir(name))
  os.write(f, '\n'.join(lines) + '\n')
  os.close(f)
  return name


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
def unpack(name, work_dir='{sources}', top_dir=None, dst_dir=None):
  try:
    src = glob(path.join('{archives}', name) + '*')[0]
  except IndexError:
    panic('Missing files for "%s".', name)

  dst = path.join(work_dir, dst_dir or name)

  info('preparing files for "%s"', name)

  if path.isdir(src):
    if top_dir is not None:
      src = path.join(src, top_dir)
    copytree(src, dst, exclude=['.svn', '.git'])
  else:
    tmpdir = mkdtemp(dir='{tmpdir}')
    with cwd(tmpdir):
      unarc(src)
    copytree(path.join(tmpdir, top_dir or name), dst)
    rmtree(tmpdir)


@check_stamp
def patch(name, work_dir='{sources}'):
  with cwd(work_dir):
    for name in find(path.join('{patches}', name),
                     only_files=True, exclude=['*~']):
      if fnmatch(name, '*.diff'):
        execute('patch', '-t', '-p0', '-i', name)
      else:
        dst = path.relpath(name, '{patches}')
        mkdir(path.dirname(dst))
        copy(name, dst)


@check_stamp
def configure(name, *confopts, **kwargs):
  info('configuring "%s"', name)

  if 'from_dir' in kwargs:
    from_dir = kwargs['from_dir']
  else:
    from_dir = path.join('{sources}', name)

  if kwargs.get('copy_source', False):
    rmtree(path.join('{build}', name))
    copytree(path.join('{sources}', name), path.join('{build}', name))
    from_dir = '.'

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


__all__ = ['setvar', 'panic', 'cmpver', 'find_executable', 'chmod', 'execute',
           'rmtree', 'mkdir', 'copy', 'copytree', 'unarc', 'fetch', 'cwd',
           'symlink', 'remove', 'move', 'find', 'textfile', 'env', 'path',
           'check_stamp', 'unpack', 'patch', 'configure', 'build', 'install']
