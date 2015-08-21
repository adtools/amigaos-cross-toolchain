#!/usr/bin/env python

# Build cross toolchain for AmigaOS 4.x / PowerPC target.

from subprocess import check_call, check_output, CalledProcessError
from os import getcwd, chdir, environ, path
from contextlib import contextmanager
from argparse import ArgumentParser
import shutil
import os
import logging
from sys import exit
from logging import debug, info, error
from glob import glob

URLS = \
  ["ftp://ftp.gnu.org/gnu/gmp/gmp-5.1.3.tar.bz2",
   "ftp://ftp.gnu.org/gnu/mpc/mpc-1.0.3.tar.gz",
   "ftp://ftp.gnu.org/gnu/mpfr/mpfr-3.1.3.tar.bz2",
   "http://isl.gforge.inria.fr/isl-0.12.2.tar.bz2",
   "http://www.bastoul.net/cloog/pages/download/cloog-0.18.4.tar.gz",
   "https://soulsphere.org/projects/lhasa/lhasa-0.3.0.tar.gz",
   ("http://hyperion-entertainment.biz/index.php/downloads" +
    "?view=download&amp;format=raw&amp;file=69", "SDK_53.24.lha"),
   ("svn://svn.code.sf.net/p/adtools/code/trunk/binutils", "binutils-2.18"),
   ("svn://svn.code.sf.net/p/adtools/code/trunk/gcc", "gcc-4.2.4"),
   ("svn://svn.code.sf.net/p/adtools/code/branches/binutils/2.23.2",
    "binutils-2.23.2"),
   ("svn://svn.code.sf.net/p/adtools/code/branches/gcc/4.9.x", "gcc-4.9.1"),
   ("https://github.com/adtools/sfdc/archive/master.zip", "sfdc-master.zip"),
   ("http://clib2.cvs.sourceforge.net/viewvc/clib2/?view=tar", "clib2.tar.gz")]

VARS = {}
TARGET_DIR = ""
MAKE_OPTS = []
TOP_DIR = ""
STAMP_DIR = ""
BUILD_DIR = ""
ARCHIVES = ""
PATCHES = ""
SOURCE_DIR = ""
BUILD_DIR = ""
HOST_DIR = ""


def mkver(strver):
  return tuple(int(x) for x in strver.split('.'))


def relpath(name):
  if not path.isabs(name):
    name = path.abspath(name)
  return path.relpath(name, TOP_DIR)


def rmtree(*names):
  for name in names:
    if path.exists(name):
      debug('rmtree "%s"', relpath(name))
      shutil.rmtree(name)


def makedirs(*names):
  for name in names:
    debug('makedir "%s"', relpath(name))
    os.makedirs(name)


def copytree(src, dst, **kwargs):
  debug('copytree "%s" to "%s"', relpath(src), relpath(dst))
  shutil.copytree(src, dst, **kwargs)


def rename(src, dst):
  debug('rename "%s" to "%s"', relpath(src), relpath(dst))
  os.rename(src, dst)


def symlink(src, name):
  debug('symlink "%s" from "%s"', src, relpath(name))
  os.symlink(src, name)


def execute(*cmd):
  debug('execute "%s"', " ".join(cmd))
  try:
    check_call(cmd)
  except CalledProcessError as ex:
    error('command "%s" failed with %d', " ".join(list(ex.cmd)), ex.returncode)
    exit()


@contextmanager
def cwd(name):
  old = getcwd()
  if not path.exists(name):
    makedirs(name)
  try:
    debug('enter directory "%s"', relpath(name))
    chdir(name)
    yield
  finally:
    chdir(old)


@contextmanager
def env(**kwargs):
  backup = {}
  try:
    for key, value in kwargs.items():
      debug('changing environment variable "%s" to "%s"', key, value)
      old = environ.get(key, None)
      environ[key] = value
      backup[key] = old
    yield
  finally:
    for key, value in backup.items():
      debug('restoring old value of environment variable "%s"', key)
      if value is None:
        del environ[key]
      else:
        environ[key] = value


def check_stamp(fn):
  def wrapper(*args, **kwargs):
    name = fn.func_name.replace('_', '-')
    if len(args) > 0:
      name = name + "-" + str(args[0])
    stamp = path.join(STAMP_DIR, name)
    if not path.exists(STAMP_DIR):
      makedirs(STAMP_DIR)
    if not path.exists(stamp):
      fn(*args, **kwargs)
      check_call(['touch', stamp])
    else:
      info('already done "%s"', name)

  return wrapper


def fetch():
  with cwd(ARCHIVES):
    for url in URLS:
      if type(url) == tuple:
        url, name = url[0], url[1]
      else:
        name = path.basename(url)

      if url.startswith("http") or url.startswith("ftp"):
        if not path.exists(name):
          execute("wget", "--no-check-certificate", "-O", name, url)
        else:
          info("File '%s' already downloaded.", name)
      elif url.startswith("svn"):
        if not path.exists(name):
          execute("svn", "checkout", url, name)
        else:
          execute("svn", "update", name)


@check_stamp
def prepare_target():
  info('preparing "%s"', relpath(TARGET_DIR))

  rmtree(TARGET_DIR)
  with cwd(TARGET_DIR):
    makedirs('bin', 'doc', 'etc', 'lib', 'ppc-amigaos')
    symlink('../os-include', 'ppc-amigaos/include')
    symlink('../lib', 'ppc-amigaos/lib')


@check_stamp
def prepare_sdk():
  info('preparing SDK')

  base = ''
  clib2 = ''
  newlib = ''

  with cwd(SOURCE_DIR):
    for arc in ['base.lha', 'clib2-*.lha', 'newlib-*.lha']:
      info('extracting "%s"' % arc)
      execute('lha', '-xifq', path.join(ARCHIVES, 'SDK_53.24.lha'),
              path.join('SDK_Install', arc))
    base = path.join(SOURCE_DIR, glob('base*.lha')[0])
    clib2 = path.join(SOURCE_DIR, glob('clib2*.lha')[0])
    newlib = path.join(SOURCE_DIR, glob('newlib*.lha')[0])

  with cwd(path.join(TARGET_DIR, 'ppc-amigaos/SDK')):
    execute('lha', '-xf', clib2, 'clib2/*')
    execute('lha', '-xf', newlib, 'newlib/*')
    execute('lha', '-xf', base, 'Include/*')
    rename('Include', 'include')


@check_stamp
def prepare_source(name, copy=None):
  src = path.join(ARCHIVES, name)
  dst = name

  if src.endswith('.tar.gz'):
    fmt = 'tar.gz'
  elif src.endswith('.tar.bz2'):
    fmt = 'tar.bz2'
  elif src.endswith('.lha'):
    fmt = 'lha'
  elif src.endswith('.zip'):
    fmt = 'zip'
  elif path.isdir(src):
    fmt = 'dir'
  else:
    raise RuntimeError('Unrecognized source: %s', src)

  if fmt != 'dir':
    name = name.rstrip('.' + fmt)
  dst = path.join(SOURCE_DIR, name)
  rmtree(dst)

  info('preparing source "%s"', name)

  with cwd(SOURCE_DIR):
    if fmt == 'tar.gz':
      execute('tar', '-xzf', src)
    elif fmt == 'tar.bz2':
      execute('tar', '-xjf', src)
    elif fmt == 'lha':
      execute('lha', '-xq', src)
    elif fmt == 'zip':
      execute('unzip', src)
    elif fmt == 'dir':
      copytree(src, dst, ignore=shutil.ignore_patterns('.svn'))

    if copy is not None:
      rmtree(copy)
      copytree(dst, copy)


@check_stamp
def configure(name, *confopts):
  info('configuring "%s"', name)

  with cwd(path.join(BUILD_DIR, name)):
    execute('find', '.', '-name', 'config.cache', '-delete', '-print')
    execute(path.join(SOURCE_DIR, name, 'configure'), *confopts)


@check_stamp
def build(name, *confopts):
  info('building "%s"', name)

  with cwd(path.join(BUILD_DIR, name)):
    execute("make", *MAKE_OPTS)


@check_stamp
def install(name, *confopts):
  info('installing "%s"', name)

  with cwd(path.join(BUILD_DIR, name)):
    execute("make", "install")


def doit():
  for var in environ.keys():
    if var not in ['_', 'LOGNAME', 'HOME', 'SHELL', 'TMPDIR', 'PWD']:
      del environ[var]

  environ['PATH'] = '/usr/bin:/bin'
  environ['CC'] = check_output(['which', 'gcc']).strip()
  environ['CXX'] = check_output(['which', 'g++']).strip()
  environ['LANG'] = 'C'
  environ['TERM'] = 'xterm'
  environ['PATH'] = ":".join([path.join(TARGET_DIR, 'bin'),
                              path.join(HOST_DIR, 'bin'),
                              environ['PATH']])

  prepare_source('lhasa-0.3.0.tar.gz',
                 copy=path.join(BUILD_DIR, 'lhasa-0.3.0'))
  configure("lhasa-0.3.0",
            "--disable-shared",
            "--prefix=" + HOST_DIR)
  build("lhasa-0.3.0")
  install("lhasa-0.3.0")

  gmp = VARS['gmp']
  prepare_source(gmp + '.tar.bz2')
  configure(gmp,
            '--disable-shared',
            '--prefix=' + HOST_DIR)
  build(gmp)
  install(gmp)

  mpfr = VARS['mpfr']
  prepare_source(mpfr + '.tar.bz2')
  configure(mpfr,
            '--disable-shared',
            '--prefix=' + HOST_DIR,
            '--with-gmp=' + HOST_DIR)
  build(mpfr)
  install(mpfr)

  mpc = VARS['mpc']
  prepare_source(mpc + '.tar.gz')
  configure(mpc,
            '--disable-shared',
            '--prefix=' + HOST_DIR,
            '--with-gmp=' + HOST_DIR,
            '--with-mpfr=' + HOST_DIR)
  build(mpc)
  install(mpc)

  isl = VARS['isl']
  prepare_source(isl + '.tar.bz2')
  configure(isl,
            '--disable-shared',
            '--prefix=' + HOST_DIR,
            '--with-gmp-prefix=' + HOST_DIR)
  build(isl)
  install(isl)

  cloog = VARS['cloog']
  prepare_source(cloog + '.tar.gz')
  configure(cloog,
            '--disable-shared',
            '--prefix=' + HOST_DIR,
            '--with-isl=system',
            '--with-gmp-prefix=' + HOST_DIR,
            '--with-isl-prefix=' + HOST_DIR)
  build(cloog)
  install(cloog)

  binutils = VARS['binutils']
  binutils_env = {}
  if VARS['binutils-ver'] == (2, 18):
    binutils_env.update(CFLAGS='-Wno-error')
  elif VARS['binutils-ver'] == (2, 23, 2):
    binutils_env.update(CFLAGS='-Wno-error')

  prepare_source(binutils)
  with env(**binutils_env):
    configure(binutils,
              '--prefix=' + TARGET_DIR,
              '--target=ppc-amigaos')
    build(binutils)
    install(binutils)

  prepare_sdk()

  gcc = VARS['gcc']
  gcc_env = {}
  if VARS['gcc-ver'] == (4, 2, 4):
    gcc_env.update(CFLAGS='-std=gnu89 -m32')

  prepare_source(gcc)
  with env(**gcc_env):
    configure(gcc,
              '--with-bugurl="http://sf.net/p/adtools"',
              '--target=ppc-amigaos',
              '--with-gmp=' + HOST_DIR,
              '--with-mpfr=' + HOST_DIR,
              '--with-mpc=' + HOST_DIR,
              '--with-isl=' + HOST_DIR,
              '--with-cloog=' + HOST_DIR,
              '--prefix=' + TARGET_DIR,
              '--enable-languages=c,c++',
              '--enable-haifa',
              '--enable-sjlj-exceptions'
              '--disable-libstdcxx-pch'
              '--disable-tls')
    build(gcc)
    install(gcc)


def clean():
  rmtree(STAMP_DIR)
  rmtree(SOURCE_DIR)
  rmtree(HOST_DIR)


if __name__ == "__main__":
  logging.basicConfig(level=logging.DEBUG, format='%(levelname)s: %(message)s')

  parser = ArgumentParser(description='Build cross toolchain.')
  parser.add_argument('action', choices=['fetch', 'doit', 'clean'],
                      help='perform action')
  parser.add_argument('--binutils', choices=['2.18', '2.23.2'], default='2.18',
                      help='desired binutils version')
  parser.add_argument('--gcc', choices=['4.2.4', '4.9.1'], default='4.2.4',
                      help='desired gcc version')
  parser.add_argument('--prefix', type=str, default=None,
                      help='installation directory')
  args = parser.parse_args()

  VARS = {
    'gmp': 'gmp-5.1.3',
    'mpfr': 'mpfr-3.1.3',
    'mpc': 'mpc-1.0.3',
    'isl': 'isl-0.12.2',
    'cloog': 'cloog-0.18.4',
    'binutils': 'binutils-' + args.binutils,
    'gcc': 'gcc-' + args.gcc,
    'binutils-ver': mkver(args.binutils),
    'gcc-ver': mkver(args.gcc)
  }

  TOP_DIR = path.abspath('.')

  if args.prefix is not None:
    TARGET_DIR = args.prefix
  else:
    TARGET_DIR = path.join(TOP_DIR, 'target')

  if not path.exists(TARGET_DIR):
    makedirs(TARGET_DIR)

    # MAKE_OPTS = "-j" + check_output(['getconf', '_NPROCESSORS_CONF']).strip()

  STAMP_DIR = path.join(TOP_DIR, 'stamp')
  BUILD_DIR = path.join(TOP_DIR, 'build')
  SOURCE_DIR = path.join(TOP_DIR, 'source')
  HOST_DIR = path.join(TOP_DIR, 'host')
  ARCHIVES = path.join(TOP_DIR, 'archives/ppc')
  PATCHES = path.join(TOP_DIR, 'patches')

  eval(args.action + "()")
