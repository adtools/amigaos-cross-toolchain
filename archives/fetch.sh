#!/bin/bash

function download {
  local -r url="$1"
  local -r file="${2:-$(basename "${url}")}"

  if [ ! -f "${file}" ]; then
    wget -O "${file}" "${url}"
  fi
}

download "ftp://ftp.gnu.org/gnu/gawk/gawk-3.1.8.tar.gz"
download "ftp://ftp.gnu.org/gnu/bison/bison-1.35.tar.gz"
download "ftp://ftp.gnu.org/gnu/gcc/gcc-2.95.3/gcc-core-2.95.3.tar.gz"
download "ftp://ftp.gnu.org/gnu/gcc/gcc-2.95.3/gcc-g++-2.95.3.tar.gz"
download "ftp://ftp.gnu.org/gnu/binutils/binutils-2.9.1.tar.gz"

download \
  "http://sourceforge.net/projects/libnix/files/latest/download" \
  "libnix-2.1.tar.gz"
download "http://www.haage-partner.de/download/AmigaOS/NDK39.lha"
download \
  "http://aminet.net/dev/gcc/sfdc.lha" \
  "sfdc-1.4.lha"
download \
  "http://ftp.back2roots.org/geekgadgets/amiga/m68k/snapshots/990529/bin/libamiga-bin.tgz" \
  "libamiga-bin.tar.gz"
download \
  "http://sourceforge.net/projects/amiga/files/ixemul.library/48.2/ixemul-src.lha/download" \
  "ixemul-48.2.lha"

download "ftp://gcc.gnu.org/pub/gcc/infrastructure/gmp-4.3.2.tar.bz2"
download "ftp://gcc.gnu.org/pub/gcc/infrastructure/mpc-0.8.1.tar.gz"
download "ftp://gcc.gnu.org/pub/gcc/infrastructure/mpfr-2.4.2.tar.bz2"
download "ftp://gcc.gnu.org/pub/gcc/infrastructure/isl-0.10.tar.bz2"
download "ftp://gcc.gnu.org/pub/gcc/infrastructure/cloog-0.17.0.tar.gz"
