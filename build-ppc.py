#!/usr/bin/python -B

# Build cross toolchain for AmigaOS 4.x / PowerPC target.

from glob import glob
from logging import info
from os import environ
import argparse
import logging
import platform

URLS = \
  ['ftp://ftp.gnu.org/gnu/gmp/gmp-5.1.3.tar.bz2',
   'ftp://ftp.gnu.org/gnu/mpc/mpc-1.0.3.tar.gz',
   'ftp://ftp.gnu.org/gnu/mpfr/mpfr-3.1.3.tar.bz2',
   'ftp://ftp.gnu.org/gnu/texinfo/texinfo-4.12.tar.gz',
   'http://isl.gforge.inria.fr/isl-0.12.2.tar.bz2',
   'http://www.bastoul.net/cloog/pages/download/cloog-0.18.4.tar.gz',
   'http://soulsphere.org/projects/lhasa/lhasa-0.3.0.tar.gz',
   ('http://hyperion-entertainment.biz/index.php/downloads' +
    '?view=download&amp;format=raw&amp;file=69', 'SDK_53.24.lha'),
   ('svn://svn.code.sf.net/p/adtools/code/trunk/binutils', 'binutils-2.18'),
   ('svn://svn.code.sf.net/p/adtools/code/trunk/gcc', 'gcc-4.2.4'),
   ('svn://svn.code.sf.net/p/adtools/code/branches/binutils/2.23.2',
    'binutils-2.23.2'),
   ('svn://svn.code.sf.net/p/adtools/code/branches/gcc/4.9.x', 'gcc-4.9.1')]


from common import * # NOQA


@check_stamp
def prepare_sdk():
  info('preparing SDK')

  base = ''
  clib2 = ''
  newlib = ''

  with cwd('{sources}'):
    for arc in ['base.lha', 'clib2-*.lha', 'newlib-*.lha']:
      info('extracting "%s"' % arc)
      execute('lha', '-xifq', path.join('{archives}', 'SDK_53.24.lha'),
              path.join('SDK_Install', arc))
    base = path.join('{sources}', glob('base*.lha')[0])
    clib2 = path.join('{sources}', glob('clib2*.lha')[0])
    newlib = path.join('{sources}', glob('newlib*.lha')[0])

  with cwd(path.join('{target}', 'ppc-amigaos/SDK')):
    execute('lha', '-xf', clib2, 'clib2/*')
    execute('lha', '-xf', newlib, 'newlib/*')
    execute('lha', '-xf', base, 'Include/*')
    rename('Include', 'include')


def doit():
  for var in environ.keys():
    if var not in ['_', 'LOGNAME', 'HOME', 'SHELL', 'TMPDIR', 'PWD']:
      del environ[var]

  environ['PATH'] = '/usr/bin:/bin'
  environ['LANG'] = 'C'
  environ['TERM'] = 'xterm'

  if platform.system() == 'Darwin':
    cc, cxx = 'clang', 'clang++'
  else:
    cc, cxx = 'gcc', 'g++'

  environ['CC'] = find_executable(cc)
  environ['CXX'] = find_executable(cxx)

  find_executable('bison')
  find_executable('flex')
  find_executable('make')
  find_executable('svn')

  environ['PATH'] = ":".join([path.join('{target}', 'bin'),
                              path.join('{host}', 'bin'),
                              environ['PATH']])

  with cwd('{archives}'):
    for url in URLS:
      if type(url) == tuple:
        url, name = url[0], url[1]
      else:
        name = path.basename(url)
      fetch(name, url)

  source('{lha}', copy=path.join('{build}', '{lha}'))
  configure('{lha}',
            '--disable-shared',
            '--prefix={host}')
  build('{lha}')
  install('{lha}')

  source('{texinfo}')
  configure('{texinfo}',
            '--prefix={host}')
  build('{texinfo}')
  install('{texinfo}')

  source('{gmp}')
  configure('{gmp}',
            '--disable-shared',
            '--prefix={host}')
  build('{gmp}')
  install('{gmp}')

  source('{mpfr}')
  configure('{mpfr}',
            '--disable-shared',
            '--prefix={host}',
            '--with-gmp={host}')
  build('{mpfr}')
  install('{mpfr}')

  source('{mpc}')
  configure('{mpc}',
            '--disable-shared',
            '--prefix={host}',
            '--with-gmp={host}',
            '--with-mpfr={host}')
  build('{mpc}')
  install('{mpc}')

  source('{isl}')
  configure('{isl}',
            '--disable-shared',
            '--prefix={host}',
            '--with-gmp-prefix={host}')
  build('{isl}')
  install('{isl}')

  source('{cloog}')
  configure('{cloog}',
            '--disable-shared',
            '--prefix={host}',
            '--with-isl=system',
            '--with-gmp-prefix={host}',
            '--with-isl-prefix={host}')
  build('{cloog}')
  install('{cloog}')

  binutils_env = {}
  if cmpver('eq', '{binutils_ver}', '2.18'):
    binutils_env.update(CFLAGS='-Wno-error')
  elif cmpver('eq', '{binutils_ver}', '2.23.2'):
    binutils_env.update(CFLAGS='-Wno-error')

  source('{binutils}')
  with env(**binutils_env):
    configure('{binutils}',
              '--prefix={target}',
              '--target=ppc-amigaos')
    build('{binutils}')
    install('{binutils}')

  prepare_sdk()

  gcc_env = {}
  if cmpver('eq', '{gcc_ver}', '4.2.4'):
    cflags = ['-std=gnu89']
    if platform.machine() == 'x86_64':
      cflags.append('-m32')
    gcc_env.update(CFLAGS=' '.join(cflags))

  source('{gcc}')
  with env(**gcc_env):
    configure('{gcc}',
              '--with-bugurl="http://sf.net/p/adtools"',
              '--target=ppc-amigaos',
              '--with-gmp={host}',
              '--with-mpfr={host}',
              '--with-cloog={host}',
              '--prefix={target}',
              '--enable-languages=c,c++',
              '--enable-haifa',
              '--enable-sjlj-exceptions'
              '--disable-libstdcxx-pch'
              '--disable-tls')
    build('{gcc}')
    install('{gcc}')


def clean():
  rmtree('{stamps}')
  rmtree('{sources}')
  rmtree('{host}')
  rmtree('{build}')


if __name__ == "__main__":
  logging.basicConfig(level=logging.DEBUG, format='%(levelname)s: %(message)s')

  parser = argparse.ArgumentParser(description='Build cross toolchain.')
  parser.add_argument('action', choices=['doit', 'clean'], default='doit',
                      help='perform action')
  parser.add_argument('--binutils', choices=['2.18', '2.23.2'], default='2.18',
                      help='desired binutils version')
  parser.add_argument('--gcc', choices=['4.2.4', '4.9.1'], default='4.2.4',
                      help='desired gcc version')
  parser.add_argument('--prefix', type=str, default=None,
                      help='installation directory')
  args = parser.parse_args()

  if not (platform.system() in ['Darwin', 'Linux'] or
          fnmatch(platform.system(), 'CYGWIN*')):
    panic('Build on %s not supported!', platform.system())

  if platform.machine() not in ['i686', 'x86_64']:
    panic('Build on %s architecture not supported!', platform.machine())

  setvar(top=path.abspath('.'),
         binutils_ver=args.binutils,
         gcc_ver=args.gcc)

  setvar(lha='lhasa-0.3.0',
         gmp='gmp-5.1.3',
         mpfr='mpfr-3.1.3',
         mpc='mpc-1.0.3',
         isl='isl-0.12.2',
         cloog='cloog-0.18.4',
         texinfo='texinfo-4.12',
         binutils='binutils-{binutils_ver}',
         gcc='gcc-{gcc_ver}',
         patches=path.join('{top}', 'patches'),
         stamps=path.join('{top}', 'stamps'),
         build=path.join('{top}', 'build'),
         sources=path.join('{top}', 'sources'),
         host=path.join('{top}', 'host'),
         target=path.join('{top}', 'target'),
         archives=path.join('{top}', 'archives', 'ppc'))

  if args.prefix is not None:
    setvar(target=args.prefix)

  if not path.exists('{target}'):
    mkdir('{target}')

  eval(args.action + "()")
