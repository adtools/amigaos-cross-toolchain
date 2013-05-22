#!/bin/bash -Eeux

readonly TOP_DIR="$(pwd)"
readonly ARCHIVES="${TOP_DIR}/archives"
readonly PATCHES="${TOP_DIR}/patches"
readonly SOURCES="${TOP_DIR}/sources"
readonly BUILD_DIR="${TOP_DIR}/build"
readonly HOST_DIR="${TOP_DIR}/host"
readonly STAMP="${TOP_DIR}/stamps"
readonly MAKE="make -j$(getconf NPROCESSORS_CONF)"

function prepare_target {
  mkdir -p "${STAMP}" "${BUILD_DIR}" "${PREFIX}"

  [ -f "${STAMP}/prepare-target" ] && return 0

  pushd "${PREFIX}"
  mkdir -p "bin" "etc" "lib" "m68k-amigaos"
  mkdir -p "os-include/lvo" "os-lib" "vbcc-include" "vbcc-lib"
  ln -sf "../os-include" "m68k-amigaos/include"
  ln -sf "../lib" "m68k-amigaos/lib"
  popd

  touch "${STAMP}/prepare-target"
}

function copy_non_diff {
  find "${PATCHES}/$1" -type f \! -name '*.diff' | while read IN ; do
    OUT=$(echo "$IN" | sed -e "s,$PATCHES/,$SOURCES/,g")
    cp "$IN" "$OUT"
  done
}

function apply_patches {
  for file in $(find "${PATCHES}/$1" -type f -iname '*.diff' | sort); do
    patch -i $file -t -p1
  done
}

function mkdir_empty {
  rm -rf "$1"
  mkdir -p "$1"
}

function unpack_clean {
  local cmd

  rm -rf "$1"

  while (( "$#" > 1 )); do
    shift

    if grep "\.tar\.gz$" <<<"$1"; then
      cmd="tar -xzf"
    elif grep "\.tar.bz2$" <<<"$1"; then
      cmd="tar -xjf"
    elif grep "\.lha$" <<<"$1"; then
      cmd="lha -xgq"
    else
      cmd="false"
    fi

    ${cmd} "${ARCHIVES}/$1"
  done
}

function compare_version {
  python - "$1" "$2" "$3" <<EOF
from distutils.version import StrictVersion
import sys

cmp = lambda x, y: StrictVersion(x).__cmp__(y)

op = {"lt": [-1],    "gt": [1],
      "le": [-1, 0], "ge": [1, 0],
      "eq": [0],     "ne": [-1, 1]}

res = cmp(sys.argv[1], sys.argv[3]) in op[sys.argv[2]]

sys.exit(int(not res))
EOF
}

function unpack_sources {
  [ -f "${STAMP}/unpack-sources" ] && return 0

  rm -rf "${SOURCES}"
  mkdir -p "${SOURCES}"
  pushd "${SOURCES}"

  unpack_clean "${BINUTILS}" "${BINUTILS_SRC}"
  pushd "${BINUTILS}"
  copy_non_diff "${BINUTILS}"
  apply_patches "${BINUTILS}"
  popd

  unpack_clean "${GCC}" "${GCC_CORE_SRC}" "${GCC_CPP_SRC}"
  pushd "${GCC}"
  copy_non_diff "${GCC}"
  apply_patches "${GCC}"
  popd

  if compare_version "${GCC_VER}" "ge" "4.0.0"; then
    unpack_clean "${GMP}" "${GMP_SRC}"
    unpack_clean "${MPC}" "${MPC_SRC}"
    unpack_clean "${MPFR}" "${MPFR_SRC}"
  fi

  unpack_clean "${SFDC}" "${SFDC_SRC}"
  tar -xzf "${SFDC}.tar.gz"
  for file in $(ls -1d sfdc*); do
    [ -f "${file}" ] && rm "${file}"
  done

  unpack_clean "${NDK}" "${NDK_SRC}"
  rm -rf ndk_* *.info
  pushd "${NDK}"
  mkdir Include/include_h/inline
  copy_non_diff "${NDK}"
  apply_patches "${NDK}"
  popd

  unpack_clean "${IXEMUL}" "${IXEMUL_SRC}"
  mv "ixemul" "${IXEMUL}"
  pushd "${IXEMUL}"
  chmod a+x "configure" "mkinstalldirs"
  apply_patches "${IXEMUL}"
  popd

  unpack_clean "${LIBNIX}" "${LIBNIX_SRC}"
  mv "libnix" "${LIBNIX}"
  chmod a+x "${LIBNIX}/mkinstalldirs"

  unpack_clean "${LIBM}" "${LIBM_SRC}"
  mv "contrib/libm" "${LIBM}"
  rmdir "contrib"

  mkdir_empty "${LIBAMIGA}"
  pushd "${LIBAMIGA}"
  tar -xzf "${ARCHIVES}/${LIBAMIGA_SRC}"
  popd

  unpack_clean "${BISON}" "${BISON_SRC}"
  unpack_clean "${GAWK}" "${GAWK_SRC}"

  unpack_clean "${VASM}" "${VASM_SRC}"
  mv "vasm" "${VASM}"

  unpack_clean "${VLINK}" "${VLINK_SRC}"
  mv "vlink" "${VLINK}"

  unpack_clean "${VBCC}" "${VBCC_SRC}"
  mv "vbcc" "${VBCC}"
  pushd "${VBCC}"
  apply_patches "${VBCC}"
  popd

  unpack_clean "${VCLIB}" "${VCLIB_SRC}"
  mv "vbcc_target_m68k-amigaos" "${VCLIB}"
  rm "vbcc_target_m68k-amigaos.info"

  popd

  touch "${STAMP}/unpack-sources"
}

function build_tools {
  [ -f "${STAMP}/build-tools" ] && return 0

  mkdir_empty "${HOST_DIR}"

  pushd "${BUILD_DIR}"

  mkdir_empty "${GAWK}"
  pushd "${GAWK}"
  "${SOURCES}/${GAWK}/configure" \
    --prefix="${HOST_DIR}"
  ${MAKE} && make install
  popd

  if compare_version "${GCC_VER}" "le" "3.4.6"; then
    mkdir_empty "${BISON}"
    pushd "${BISON}"
    "${SOURCES}/${BISON}/configure" \
      --prefix="${HOST_DIR}"
    ${MAKE} && make install
    popd
  fi

  if compare_version "${GCC_VER}" "ge" "4.0.0"; then
    mkdir_empty "${GMP}"
    pushd "${GMP}"
    "${SOURCES}/${GMP}/configure" \
      --prefix="${HOST_DIR}" \
      --disable-shared
    ${MAKE} && make install
    popd

    mkdir_empty "${MPFR}"
    pushd "${MPFR}"
    "${SOURCES}/${MPFR}/configure" \
      --prefix="${HOST_DIR}" \
      --disable-shared \
      --with-gmp="${HOST_DIR}"
    ${MAKE} && make install
    popd

    mkdir_empty "${MPC}"
    pushd "${MPC}"
    "${SOURCES}/${MPC}/configure" \
      --prefix="${HOST_DIR}" \
      --disable-shared \
      --with-gmp="${HOST_DIR}" \
      --with-mpfr="${HOST_DIR}"
    ${MAKE} && make install
    popd
  fi

  popd

  touch "${STAMP}/build-tools"
}

function build_vasm {
  [ -f "${STAMP}/build-vasm" ] && return 0

  pushd "${BUILD_DIR}"
  rm -rf "${VASM}"
  cp -a "${SOURCES}/${VASM}" .
  cd "${VASM}"
  make CPU="m68k" SYNTAX="mot"
  cp "vasmm68k_mot" "vobjdump" "${PREFIX}/bin/"

  cat >"${PREFIX}/bin/vasm" <<EOF
#!/bin/sh

vasmm68k_mot -I${PREFIX}/os-include "\$@"
EOF
  popd

  touch "${STAMP}/build-vasm"
}

function build_vlink {
  [ -f "${STAMP}/build-vlink" ] && return 0

  pushd "${BUILD_DIR}"
  rm -rf "${VLINK}"
  cp -a "${SOURCES}/${VLINK}" .
  cd "${VLINK}"
  make
  cp "vlink" "${PREFIX}/bin/"
  popd

  touch "${STAMP}/build-vlink"
}

function build_vbcc {
  [ -f "${STAMP}/build-vbcc" ] && return 0

  pushd "${BUILD_DIR}"
  rm -rf "${VBCC}"
  cp -a "${SOURCES}/${VBCC}" .
  cd "${VBCC}"
  mkdir "bin"
  make TARGET="m68k" CC+="-DETCDIR=\\\"${PREFIX}/etc/\\\"" <<EOF
y
y
signed char
y
unsigned char
n
y
signed short
n
y
unsigned short
n
y
signed int
n
y
unsigned int
n
y
signed long
n
y
unsigned long
n
y
float
n
y
double
EOF
  cp "bin/vbccm68k" "bin/vc" "bin/vprof" "${PREFIX}/bin/"
  popd

  touch "${STAMP}/build-vbcc"
}

function install_vclib {
  [ -f "${STAMP}/install-vclib" ] && return 0

  pushd "${PREFIX}/vbcc-include"
  cp -av "${SOURCES}/${VCLIB}/targets/m68k-amigaos/include/"* .
  popd

  pushd "${PREFIX}/vbcc-lib"
  cp -av "${SOURCES}/${VCLIB}/targets/m68k-amigaos/lib/"* .
  popd

  sed -e "s,-Ivincludeos3:,-I${PREFIX}/vbcc-include -I${PREFIX}/os-include,g" \
      -e "s,-Lvlibos3:,-L${PREFIX}/vbcc-lib -L${PREFIX}/vbcc-include,g" \
      -e "s,vlibos3:,${PREFIX}/vbcc-lib/,g" \
      -e "s,-lvc,-lvc -lamiga,g" \
      -e "/^-as/s,\$, -I${PREFIX}/os-include,g" \
      -e "s,delete quiet,rm,g" \
      -e "s,delete,rm -v,g" \
      "${SOURCES}/${VCLIB}/config/aos68k" > "${PREFIX}/etc/vc.config"

  touch "${STAMP}/install-vclib"
}

function build_binutils {
  [ -f "${STAMP}/build-binutils" ] && return 0

  pushd "${BUILD_DIR}"
  mkdir_empty "${BINUTILS}"
  cd "${BINUTILS}"
  CC="${CC} ${ARCH:-}" "${SOURCES}/${BINUTILS}/configure" \
    --prefix="${PREFIX}" \
    --host="i686-linux-gnu" \
    --target="m68k-amigaos"
  make all
  make install install-info
  popd

  touch "${STAMP}/build-binutils"
}

function build_gcc {
  [ -f "${STAMP}/build-gcc" ] && return 0

  pushd "${BUILD_DIR}"
  mkdir_empty "${GCC}"
  cd "${GCC}"
  "${SOURCES}/${GCC}/configure" \
    --prefix="${PREFIX}" \
    --target="m68k-amigaos" \
    --enable-languages=c \
    --with-headers="${SOURCES}/${IXEMUL}/include" \
    ${GCC_CONFIGURE_OPTS[*]}
  make all ${FLAGS_FOR_TARGET[*]}
  make install ${FLAGS_FOR_TARGET[*]}
  popd

  touch "${STAMP}/build-gcc"
}

function build_gpp {
  [ -f "${STAMP}/build-gpp" ] && return 0

  local GPP="${GCC/gcc/g++}"

  pushd "${BUILD_DIR}"
  mkdir_empty "${GPP}"
  cd "${GPP}"
  "${SOURCES}/${GCC}/configure" \
    --prefix="${PREFIX}" \
    --target="m68k-amigaos" \
    --enable-languages=c++ \
    --with-headers="${SOURCES}/${IXEMUL}/include" \
    ${GCC_CONFIGURE_OPTS[*]}
  make all ${FLAGS_FOR_TARGET[*]}
  make install ${FLAGS_FOR_TARGET[*]}
  popd

  touch "${STAMP}/build-gpp"
}

function process_ndk {
  [ -f "${STAMP}/process-ndk" ] && return 0

  pushd "${BUILD_DIR}"
	rm -rf "${SFDC}"
	cp -a "${SOURCES}/${SFDC}" "${SFDC}"
  cd "${SFDC}"
  ./configure \
    --prefix="${PREFIX}"
  make && make install
  popd

  pushd "${PREFIX}/os-include"
  cp -av "${SOURCES}/${NDK}/Include/include_h/"* .
  cp -av "${SOURCES}/${NDK}/Include/include_i/"* .
  for file in ${SOURCES}/${NDK}/Include/sfd/*.sfd; do
    base=$(basename ${file%_lib.sfd})

    sfdc --target=m68k-amigaos --mode=proto \
      --output="proto/${base}.h" $file
    sfdc --target=m68k-amigaos --mode=macros \
      --output="inline/${base}.h" $file
    sfdc --target=m68k-amigaos --mode=lvo \
      --output="lvo/${base}.i" $file
  done
  popd

  pushd "${PREFIX}/os-lib"
  cp -av "${SOURCES}/${NDK}/Include/linker_libs/"* .
  popd

  touch "${STAMP}/process-ndk"
}

function install_libamiga {
  [ -f "${STAMP}/install-libamiga" ] && return 0

  pushd "${PREFIX}"
  cp -av "${SOURCES}/${LIBAMIGA}/lib" .
  popd

  touch "${STAMP}/install-libamiga"
}

function build_libnix {
  [ -f "${STAMP}/build-libnix" ] && return 0

  pushd "${BUILD_DIR}"
  mkdir_empty "${LIBNIX}"
  cd "${LIBNIX}"
  "${SOURCES}/${LIBNIX}/configure" \
    --prefix="${PREFIX}" \
    --host="i686-linux-gnu" \
    --target="m68k-amigaos"
  make all \
    CC=m68k-amigaos-gcc \
    CPP="m68k-amigaos-gcc -E" \
    AR=m68k-amigaos-ar \
    AS=m68k-amigaos-as \
    RANLIB=m68k-amigaos-ranlib \
    LD=m68k-amigaos-ld \
    MAKEINFO=:
  [ -f "${PREFIX}/guide" ] && rm -f "${PREFIX}/guide"
  make install MAKEINFO=:
  popd

  touch "${STAMP}/build-libnix"
}

function build_libm {
  [ -f "${STAMP}/build-libm" ] && return 0

  pushd "${BUILD_DIR}"
  mkdir_empty "${LIBM}"
  cd "${LIBM}"
  CC=m68k-amigaos-gcc \
  AR=m68k-amigaos-ar \
  CFLAGS="-noixemul" \
  RANLIB=m68k-amigaos-ranlib \
	"${SOURCES}/${LIBM}/configure" \
    --prefix="${PREFIX}" \
    --host="i686-linux-gnu" \
    --build="m68k-amigaos"
  make && make install
  popd

  touch "${STAMP}/build-libm"
}

function build_ixemul {
  [ -f "${STAMP}/build-ixemul" ] && return 0

  pushd "${BUILD_DIR}"
  mkdir_empty "${IXEMUL}"
  cd "${IXEMUL}"
  CC=m68k-amigaos-gcc \
  CFLAGS="-noixemul -I${PREFIX}/os-include" \
  AR=m68k-amigaos-ar \
  RANLIB=m68k-amigaos-ranlib \
	"${SOURCES}/${IXEMUL}/configure" \
    --prefix="${PREFIX}" \
    --host="i686-linux-gnu" \
    --target="m68k-amigaos"
  make MAKEINFO=: all
  make MAKEINFO=: install
  popd

  touch "${STAMP}/build-ixemul"
}

function build {
  source "${TOP_DIR}/bootstrap.conf"

  # On 64-bit architecture GNU Assembler crashes writing out an object, due to
  # (probably) miscalculated structure sizes.  There could be some other bugs
  # lurking there in 64-bit mode, but I have little incentive chasing them.
  # Just compile everything in 32-bit mode and forget about the issues.
  if [ "$(uname -m)" == "x86_64" ]; then
    ARCH="-m32"
  fi

  # Make sure we always choose known compiler (from the distro) and not one
  # in user's path that could shadow the original one.
  CC="$(which gcc)"
  CXX="$(which g++)"

  # Define extra options for gcc's configure script.
  if [ "${VERSION}" != "4" ]; then
    # Older gcc compilers (i.e. 2.95.3 and 3.4.6) have to be tricked into
    # thinking that they're being compiled on IA-32 architecture.
    CC="${CC} ${ARCH:-}"
    CXX="${CXX} ${ARCH:-}"
    GCC_CONFIGURE_OPTS+=("--host=i686-linux-gnu")
  else
    # GCC 4.x requires some extra dependencies to be supplied.
    GCC_CONFIGURE_OPTS+=("--with-gmp=${HOST_DIR}" \
                         "--with-mpfr=${HOST_DIR}" \
                         "--with-mpc=${HOST_DIR}" \
                         "--disable-shared")
  fi

  # Take over the path -- to preserve hermetic build. 
  export PATH="/opt/local/bin:/usr/local/bin:/usr/bin:/bin"
  export CC CXX

  readonly FLAGS_FOR_TARGET=( \
      "MAKEINFO=makeinfo" \
      "CFLAGS_FOR_TARGET=-noixemul" \
      "AR_FOR_TARGET=${PREFIX}/bin/m68k-amigaos-ar" \
      "AS_FOR_TARGET=${PREFIX}/bin/m68k-amigaos-as" \
      "LD_FOR_TARGET=${PREFIX}/bin/m68k-amigaos-ld" \
      "NM_FOR_TARGET=${PREFIX}/bin/m68k-amigaos-nm" \
      "OBJCOPY_FOR_TARGET=${PREFIX}/bin/m68k-amigaos-objcopy" \
      "OBJDUMP_FOR_TARGET=${PREFIX}/bin/m68k-amigaos-objdump" \
      "RANLIB_FOR_TARGET=${PREFIX}/bin/m68k-amigaos-ranlib")

  prepare_target
  unpack_sources
  build_tools

  export PATH="${HOST_DIR}/bin:${PATH}"

  build_vasm
  build_vlink
  build_vbcc
  install_vclib

  build_binutils
  build_gcc

  export PATH="${PREFIX}/bin:${PATH}"

  process_ndk
  install_libamiga
  build_libnix
  build_libm
  build_gpp

  # TODO: Ixemul is not suited for cross compilation very well.  The build
  # process compiles some tools with cross compiler and tries to run them
  # locally.

  #build_ixemul
}

function main {
  PREFIX="${TOP_DIR}/target"
  VERSION="2"

  local action="build"

  while [ -n "${1:-}" ]; do
    case "$1" in
      --prefix=*)
        PREFIX=${1#*=}
        ;;
      --version=*)
        VERSION=${1#*=}
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
    "build")
      build
      ;;
    "clean")
      for dir in ${SOURCES} ${BUILD_DIR} ${HOST_DIR} ${STAMP}; do
        rm -rf "${dir}"
      done
      ;;
    *)
      echo "Unknown action '${action}'!"
      exit 1
      ;;
  esac
}

main "$@"
