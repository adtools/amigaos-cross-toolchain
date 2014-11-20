#!/bin/bash

readonly TOP_DIR="$(pwd)"

function download {
  local -r url="$1"
  local -r file="${2:-$(basename "${url}")}"

  if [ ! -f "${file}" ]; then
    wget -O "${file}" "${url}" || exit 1
  fi
}

function list {
  echo "Available SDKs:"

  for sdk in ${TOP_DIR}/sdk/*.sdk; do
    name=$(basename ${sdk})
    short=`sed -ne "s/Short: //p" ${sdk}`
    ver=`sed -ne "s/Version: //p" ${sdk}`
    echo " - ${name%%.sdk} : $short $ver"
  done
}

function add_stubs {
  local src="$1"
  local libdir="$2"
  local obj="${src%.c}.o"

  echo "${obj} -> ${PREFIX}/lib/${libdir}/libstubs.a"
  m68k-amigaos-gcc "${CFLAGS}" -noixemul -c -o "${obj}" "${src}" && \
    m68k-amigaos-ar rs "${PREFIX}/lib/${libdir}/libstubs.a" "${obj}"
  rm -f "${obj}"
}

function add_lib {
  local src="$1"
  local lib="$2"
  local obj="${src%.a}.o"

  echo "${src} -> ${PREFIX}/lib/${lib}"
  m68k-amigaos-gcc "${CFLAGS}" -noixemul -c -o "${obj}" "${src}" && \
    m68k-amigaos-ar rcs "${PREFIX}/lib/${lib}" "${obj}"
  rm -f "${obj}"
}

function install_sdk {
  local name="$1"
  local sdk="$2"

  local url=`sed -ne "s/Url: //p" ${sdk}`
  local tmp=`mktemp -d -t "${name}"`

  pushd ${tmp}

  download "${url}"

  lha -xgq `basename ${url}`

  sed -ne '5,$p' ${sdk} | while read -a line; do
    if [[ ${#line[@]} > 1 ]]; then
      case ${line[1]} in
        "=")
          file=${line[2]}
          path=${line[0]}
          new_path=$(dirname ${path})/${file}
          mv -v ${path} ${new_path}
          path=${new_path}
          ;;
        ":")
          case ${line[0]} in
            "fd2sfd")
              sfd="$(basename ${line[2]%%.fd}.sfd)"
              echo "${line[2]} ${line[3]} -> ${sfd}"
              fd2sfd -o ${sfd} ${line[2]} ${line[3]}
              mkdir -p "${PREFIX}/os-lib/sfd"
              cp -v ${sfd} "${PREFIX}/os-lib/sfd/${sfd}"
              ;;
            "sfdc")
              path=${line[2]}
              file=$(basename ${path})
              base=${file%_lib.sfd}

              mkdir -p "${PREFIX}/os-include/proto"
              mkdir -p "${PREFIX}/os-include/inline"
              mkdir -p "${PREFIX}/os-include/lvo"

              echo "${path} -> ${PREFIX}/os-include/proto/${base}.h"
              sfdc --quiet --target=m68k-amigaos --mode=proto \
                --output="${PREFIX}/os-include/proto/${base}.h" ${path}

              echo "${path} -> ${PREFIX}/os-include/inline/${base}.h"
              sfdc --quiet --target=m68k-amigaos --mode=macros \
                --output="${PREFIX}/os-include/inline/${base}.h" ${path}

              echo "${path} -> ${PREFIX}/os-include/lvo/${base}.i"
              sfdc --quiet --target=m68k-amigaos --mode=lvo \
                --output="${PREFIX}/os-include/lvo/${base}.i" ${path}
              ;;
            "stubs")
              path=${line[2]}
              filepart=$(basename ${path})
              file="${filepart%_lib.sfd}.c"

              echo "${path} -> ${file}"
              sfdc --quiet --target=m68k-amigaos --mode=autoopen \
                --output="${file}" ${path}

              CFLAGS="-Wall -O3 -fomit-frame-pointer"
              add_stubs "${file}" "libnix"
              CFLAGS="-Wall -O3 -fomit-frame-pointer -fbaserel -DSMALL_DATA"
              add_stubs "${file}" "libb/libnix"
              CFLAGS="-Wall -O3 -fomit-frame-pointer -m68020"
              add_stubs "${file}" "libm020/libnix"
              CFLAGS="-Wall -O3 -fomit-frame-pointer -fbaserel -DSMALL_DATA -m68020"
              add_stubs "${file}" "libb/libm020/libnix"
              CFLAGS="-Wall -O3 -fomit-frame-pointer -m68020 -m68881"
              add_stubs "${file}" "libm020/libm881/libnix"
              CFLAGS="-Wall -O3 -fomit-frame-pointer -fbaserel -DSMALL_DATA -m68020 -m68881"
              add_stubs "${file}" "libb/libm020/libm881/libnix"
              CFLAGS="-Wall -O3 -fomit-frame-pointer -fbaserel32 -DSMALL_DATA -m68020"
              add_stubs "${file}" "libb32/libm020/libnix"
              ;;
            "lib")
              path=${line[2]}
              filepart=$(basename ${path})
              file="${filepart%_lib.sfd}.c"
              lib="lib${name}.a"

              echo "${path} -> ${file}"
              sfdc --quiet --target=m68k-amigaos --mode=stubs \
                --output="${file}" ${path}

              CFLAGS="-Wall -O3 -fomit-frame-pointer"
              add_lib "${file}" "${lib}"
              CFLAGS="-Wall -O3 -fomit-frame-pointer -fbaserel -DSMALL_DATA"
              add_lib "${file}" "libb/${lib}"
              CFLAGS="-Wall -O3 -fomit-frame-pointer -m68020"
              add_lib "${file}" "libm020/${lib}"
              CFLAGS="-Wall -O3 -fomit-frame-pointer -fbaserel -DSMALL_DATA -m68020"
              add_lib "${file}" "libb/libm020/${lib}"
              CFLAGS="-Wall -O3 -fomit-frame-pointer -m68020 -m68881"
              add_lib "${file}" "libm020/libm881/${lib}"
              CFLAGS="-Wall -O3 -fomit-frame-pointer -fbaserel -DSMALL_DATA -m68020 -m68881"
              add_lib "${file}" "libb/libm020/libm881/${lib}"
              CFLAGS="-Wall -O3 -fomit-frame-pointer -fbaserel32 -DSMALL_DATA -m68020"
              add_lib "${file}" "libb32/libm020/${lib}"
              ;;
            *)
              echo "Unknown preprocessor: '${line}'"
              exit 1
              ;;
          esac
          continue
          ;;
        *)
          echo "Syntax error: '${line}'"
          exit 1
          ;;
      esac
    else
      file=$(basename ${line})
      path=${line}
    fi

    lastdir=$(basename $(dirname ${path}))

    case ${file} in
      *.doc|*.html|*.pdf|*.ps) 
        mkdir -p "${PREFIX}/doc"
        cp -v ${path} ${PREFIX}/doc/${file}
        ;;
      *.guide)
        mkdir -p "${PREFIX}/guide"
        cp -v ${path} ${PREFIX}/guide/${file}
        ;;
      *.i|*.h)
        mkdir -p "${PREFIX}/os-include/${lastdir}"
        cp -v ${path} ${PREFIX}/os-include/${lastdir}/${file}
        ;;
      *.fd)
        mkdir -p "${PREFIX}/os-lib/fd"
        cp -v ${path} ${PREFIX}/os-lib/fd/${file}
        ;;
      *.sfd)
        mkdir -p "${PREFIX}/os-lib/sfd"
        cp -v ${path} ${PREFIX}/os-lib/sfd/${file}
        ;;
      *)
        echo "${path} ???"
        exit 1
        ;;
    esac
  done

  popd

  rm -rf "${tmp}"
}

function main {
  PREFIX="${TOP_DIR}/target"

  local action

  while [ -n "${1:-}" ]; do
    case "$1" in
      --prefix=*)
        PREFIX=${1#*=}
        ;;
      --*)
        echo "Unknown option: $1"
        exit 1
        ;;
      *)
        break
        ;;
    esac
    shift
  done

  if [[ "$@" == "" ]]; then
    echo "Usage: $0 sdk1 sdk2 ..."
    echo ""
    list
    exit 1
  fi

  for name in $@; do
    sdk="${TOP_DIR}/sdk/${name}.sdk"

    if [ ! -f "${sdk}" ]; then
      echo "Unknown SDK - '${name}' !"
      exit 1
    fi

    echo "Installing '${name}'..."
    install_sdk "${name}" "${sdk}"
  done
}

main "$@"
