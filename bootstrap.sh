#!/bin/bash -Eeux

readonly TOP_DIR="$(pwd)"
readonly SRC_DIR="${TOP_DIR}/src"
readonly BUILD_DIR="${TOP_DIR}/build"
readonly HOST_DIR="${TOP_DIR}/host"
readonly TARGET_DIR="${TOP_DIR}/target"
readonly STAMP="${TOP_DIR}/stamps"

source "${TOP_DIR}/bootstrap.conf"

function prepare_target {
  mkdir -p "${STAMP}" "${BUILD_DIR}" "${TARGET_DIR}"

  [ -f "${STAMP}/prepare-target" ] && return 0

  pushd "${TARGET_DIR}"
  mkdir -p "m68k-amigaos" "lib" "os-include" "os-lib"
  ln -sf "../os-include" "m68k-amigaos/include"
  ln -sf "../lib" "m68k-amigaos/lib"
  tar -xjf "${TOP_DIR}/${LIBAMIGA_SRC}"
  popd

  touch "${STAMP}/prepare-target"
}

function unpack_sources {
  [ -f "${STAMP}/unpack-sources" ] && return 0

  rm -rf "${SRC_DIR}"
  mkdir -p "${SRC_DIR}"
  pushd "${SRC_DIR}"

  rm -rf "${BISON}"
  tar -xjf "${TOP_DIR}/${BISON_SRC}"

  rm -rf "${BINUTILS}"
  tar -xjf "${TOP_DIR}/${BINUTILS_SRC}"
  pushd "${BINUTILS}"
  zcat "${TOP_DIR}/${BINUTILS_PATCH}" | patch -p1
  popd

  rm -rf "${GCC}"
  tar -xjf "${TOP_DIR}/${GCC_CORE_SRC}"
  tar -xjf "${TOP_DIR}/${GCC_CPP_SRC}"
  pushd "${GCC}"
  zcat "${TOP_DIR}/${GCC_CORE_PATCH}" | patch -p1
  zcat "${TOP_DIR}/${GCC_CPP_PATCH}" | patch -p1
  popd

  rm -rf "${FD2INLINE}"
  tar -xjf "${TOP_DIR}/${FD2INLINE_SRC}"

  rm -rf "${SFDC}"
  tar -xjf "${TOP_DIR}/${SFDC_SRC}"

  rm -rf "${NDK}"
  lha x "${TOP_DIR}/${NDK_SRC}"
  rm -rf ndk_* *.info

  rm -rf "${IXEMUL}"
  tar -xjf "${TOP_DIR}/${IXEMUL_SRC}"
  chmod a+x "${IXEMUL}/configure"

  rm -rf "${LIBNIX}"
  tar -xjf "${TOP_DIR}/${LIBNIX_SRC}"
  chmod a+x "${LIBNIX}/mkinstalldirs"

  popd

  touch "${STAMP}/unpack-sources"
}

function build_tools {
  [ -f "${STAMP}/build-tools" ] && return 0

  rm -rf "${HOST_DIR}"
  mkdir -p "${HOST_DIR}"

  pushd "${BUILD_DIR}"
	rm -rf "${BISON}"
	mkdir -p "${BISON}"
  cd "${BISON}"
  "${SRC_DIR}/${BISON}/configure" \
    --prefix="${HOST_DIR}"
	make
	make install
  popd

	touch "${STAMP}/build-tools"
}

function build_binutils {
  [ -f "${STAMP}/build-binutils" ] && return 0

  pushd "${BUILD_DIR}"
	rm -rf "${BINUTILS}"
	mkdir -p "${BINUTILS}"
  cd "${BINUTILS}"
  "${SRC_DIR}/${BINUTILS}/configure" \
    --prefix="${TARGET_DIR}" \
    --target=m68k-amigaos
  make all
  make install
  popd

	touch "${STAMP}/build-binutils"
}

readonly FLAGS_FOR_TARGET=( \
    "MAKEINFO=makeinfo" \
    "CFLAGS_FOR_TARGET=-noixemul" \
    "AR_FOR_TARGET=${TARGET_DIR}/bin/m68k-amigaos-ar" \
    "AS_FOR_TARGET=${TARGET_DIR}/bin/m68k-amigaos-as" \
    "LD_FOR_TARGET=${TARGET_DIR}/bin/m68k-amigaos-ld" \
    "NM_FOR_TARGET=${TARGET_DIR}/bin/m68k-amigaos-nm" \
    "OBJCOPY_FOR_TARGET=${TARGET_DIR}/bin/m68k-amigaos-objcopy" \
    "OBJDUMP_FOR_TARGET=${TARGET_DIR}/bin/m68k-amigaos-objdump" \
    "RANLIB_FOR_TARGET=${TARGET_DIR}/bin/m68k-amigaos-ranlib")

function build_gcc {
  [ -f "${STAMP}/build-gcc" ] && return 0

  pushd "${BUILD_DIR}"
	rm -rf "${GCC}"
	mkdir -p "${GCC}"
  cd "${GCC}"
  "${SRC_DIR}/${GCC}/configure" \
    --prefix="${TARGET_DIR}" \
    --target=m68k-amigaos \
    --enable-languages=c \
    --with-headers="${SRC_DIR}/${IXEMUL}/include"
  make all ${FLAGS_FOR_TARGET[*]}
  make install ${FLAGS_FOR_TARGET[*]}
  popd

	touch "${STAMP}/build-gcc"
}

function build_gpp {
  [ -f "${STAMP}/build-gpp" ] && return 0

  pushd "${BUILD_DIR}"
	rm -rf "${GCC}"
	mkdir -p "${GCC}"
  cd "${GCC}"
  "${SRC_DIR}/${GCC}/configure" \
    --prefix="${TARGET_DIR}" \
    --target=m68k-amigaos \
    --enable-languages=c++ \
    --with-headers="${SRC_DIR}/${IXEMUL}/include"
  make all ${FLAGS_FOR_TARGET[*]}
  make install ${FLAGS_FOR_TARGET[*]}
  popd

	touch "${STAMP}/build-gpp"
}

function process_headers {
  [ -f "${STAMP}/process-headers" ] && return 0

  pushd "${BUILD_DIR}"
	rm -rf "${SFDC}"
	cp -a "${SRC_DIR}/${SFDC}" "${SFDC}"
  cd "${SFDC}"
  ./configure \
    --prefix="${TARGET_DIR}"
  make
	make install
  popd

  pushd "${TARGET_DIR}/include"
  cp -av "${SRC_DIR}/${NDK}/Include/include_h/"* .
  mkdir -p clib proto inline
  patch -d devices -p0 < ${SRC_DIR}/${FD2INLINE}/patches/timer.h.diff
  cp -v "${SRC_DIR}/${FD2INLINE}/include-src/inline/alib.h" inline/
  cp -v "${SRC_DIR}/${FD2INLINE}/include-src/inline/macros.h" inline/
  cp -v "${SRC_DIR}/${FD2INLINE}/include-src/inline/stubs.h" inline/
  cp -v "${SRC_DIR}/${FD2INLINE}/include-src/proto/alib.h" proto/
  for file in ${SRC_DIR}/${NDK}/Include/sfd/*.sfd; do
    base=$(basename ${file%_lib.sfd})

    sfdc --target=m68k-amigaos --mode=clib \
      --output="clib/${base}_protos.h" $file
    sfdc --target=m68k-amigaos --mode=proto \
      --output="proto/${base}.h" $file
    sfdc --target=m68k-amigaos --mode=macros \
      --output="inline/${base}.h" $file
  done
  popd

	touch "${STAMP}/process-headers"
}

function build_libnix {
  [ -f "${STAMP}/build-libnix" ] && return 0

  pushd "${BUILD_DIR}"
	rm -rf "${LIBNIX}"
	mkdir -p "${LIBNIX}"
  cd "${LIBNIX}"
  "${SRC_DIR}/${LIBNIX}/configure" \
    --prefix="${TARGET_DIR}" \
    --target=m68k-amigaos
  make all \
    CC=m68k-amigaos-gcc \
    CPP="m68k-amigaos-gcc -E" \
    AR=m68k-amigaos-ar \
    AS=m68k-amigaos-as \
    RANLIB=m68k-amigaos-ranlib \
    LD=m68k-amigaos-ld \
    MAKEINFO=:
  [ -f "${TARGET_DIR}/guide" ] && rm -f "${TARGET_DIR}/guide"
  make install MAKEINFO=:
  popd

	touch "${STAMP}/build-libnix"
}

function build_ixemul {
  [ -f "${STAMP}/build-ixemul" ] && return 0

  pushd "${BUILD_DIR}"
	rm -rf "${IXEMUL}"
	mkdir -p "${IXEMUL}"
  cd "${IXEMUL}"
  CC=m68k-amigaos-gcc \
  CFLAGS=-noixemul \
  AR=m68k-amigaos-ar \
  RANLIB=m68k-amigaos-ranlib \
	"${SRC_DIR}/${IXEMUL}/configure" \
	      --prefix=${TARGET_DIR} \
	      --target=m68k-amigaos
  make MAKEINFO=: all
  make MAKEINFO=: install
  popd

	touch "${STAMP}/build-ixemul"
}

function main {
  local -r action="${1:-build}"

  case "${action}" in
    "build")
      prepare_target
      unpack_sources
      build_tools

      export PATH="${HOST_DIR}/bin:${PATH}"

      build_binutils
      build_gcc

      export PATH="${TARGET_DIR}/bin:${PATH}"

      process_headers
      build_libnix
      #build_ixemul
      build_gpp
      ;;
    "clean")
      for dir in ${SRC_DIR} ${BUILD_DIR} ${HOST_DIR} ${TARGET_DIR} ${STAMP}; do
        echo rm -rf "${dir}"
      done
      ;;
    *)
      echo "action '${action}' not supported!"
      exit 1
      ;;
  esac
}

main "$@"
