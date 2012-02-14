#!/bin/bash

function download {
  local -r url="$1"
  local -r file="${2:-$(basename "${url}")}"

  if [ ! -f "${file}" ]; then
    wget -O "${file}" "${url}"
  fi
}

download "ftp://ftp.gnu.org/gnu/bison/bison-1.28.tar.gz"
download "ftp://ftp.gnu.org/gnu/gcc/gcc-2.95.3/gcc-core-2.95.3.tar.gz"
download "ftp://ftp.gnu.org/gnu/gcc/gcc-2.95.3/gcc-g++-2.95.3.tar.gz"
download "ftp://ftp.gnu.org/gnu/binutils/binutils-2.9.1.tar.gz"

download \
  "http://sourceforge.net/projects/libnix/files/latest/download" \
  "libnix-2.1.tar.gz"
download "http://www.haage-partner.de/download/AmigaOS/NDK39.lha"
download \
  "http://ftp.back2roots.org/geekgadgets/amiga/m68k/alpha/fd2inline/fd2inline-1.21-src.tgz" \
  "fd2inline-1.21.tar.gz"
download \
  "http://aminet.net/dev/gcc/sfdc.lha" \
  "sfdc-1.4.lha"
download \
  "http://ftp.back2roots.org/geekgadgets/amiga/m68k/snapshots/990529/bin/libamiga-bin.tgz" \
  "libamiga-bin.tar.gz"
download \
  "http://sourceforge.net/projects/amiga/files/ixemul.library/48.2/ixemul-src.lha/download" \
  "ixemul-48.2.lha"
