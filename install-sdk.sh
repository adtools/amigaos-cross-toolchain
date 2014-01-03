#!/bin/bash

readonly TOP_DIR="$(pwd)"

function download {
  local -r url="$1"
  local -r file="${2:-$(basename "${url}")}"

  if [ ! -f "${file}" ]; then
    wget -O "${file}" "${url}"
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

function install_sdk {
  local name=$1
  local sdk="${TOP_DIR}/sdk/${name}.sdk"

  if [ ! -f "${sdk}" ]; then
    echo "Unknown SDK - '${name}' !"
    echo ""
    list
    exit 1
  fi

  echo "Installing '${name}'..."

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

              sfdc --target=m68k-amigaos --mode=proto \
                --output="${PREFIX}/os-include/proto/${base}.h" ${path}
              sfdc --target=m68k-amigaos --mode=macros \
                --output="${PREFIX}/os-include/inline/${base}.h" ${path}
              sfdc --target=m68k-amigaos --mode=lvo \
                --output="${PREFIX}/os-include/lvo/${base}.i" ${path}
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
        ;;
    esac
  done

  popd

  echo "rm -rf ${tmp}"
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

  if [ -n "${1:-}" ]; then
    action="$1"
  fi

  case "${action}" in
    "list")
      list
      ;;
    "install")
      install_sdk "$2"
      ;;
    *)
      echo "Please specify valid action: 'list' or 'install'!"
      exit 1
      ;;
  esac
}

main "$@"
