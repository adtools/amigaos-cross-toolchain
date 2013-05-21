GCC compiler for AmigaOS 3.x
===

**Author:** [Krystian Bacławski](mailto:krystian.baclawski@gmail.com)

**Short description:** m68k-amigaos gcc / binutils toolchain build script.

### Overview:

**m68k-amigaos-toolchain** provides you a shell script that builds M68k AmigaOS 3.x) toolchain in your un*x-like environment. Build process should produce following set of tools that targets m68k-amigaos platform:

 * gcc 2.95.3
 * g++ 2.95.3
 * libstdc++ 2.10
 * binutils 2.9.1 (assembler, linker, etc.)
 * libnix 2.1 (standard ANSI/C library replacement for AmigaOS)
 * libm 5.4 (provides math library implementation for non-FPU Amigas)
 * AmigaOS headers & libraries (for AmigaOS 3.9)
 * ixemul.library 48.2
 * vbcc 0.9b + vclib
 * vasm 1.5c
 * vlink 0.14a

**Note:** *Patches are welcome!*

### Prerequisites

#### Installed in your system

 * GNU gcc 4.x
 * GNU flex 2.5.x
 * GNU make 3.x
 * lha
 * perl 5.10

#### Fetched by the script

As listed in `bootstrap.conf` file.

 * sources from GNU project:
   - gcc 2.95.3
   - binutils 2.9.1
   - bison 1.35
   - gawk 1.3.8
 * Amiga specific sources & binaries:
   - libnix 2.1
   - libm 5.4
   - AmigaOS NDK 3.9
   - sfdc 1.4
   - libamiga-bin
 * VBCC related sources & binaries:
   - vbcc 0.9b
   - vasm 1.5c
   - vlink 0.14a
   - vbcc m68k-amigaos target files

### How to build?

**Note:** *Well… you should have basic understanding of unix console environment, really.*

1. Download sources (use `fetch.sh` script in `archives` directory).
2. Run `bootstrap.sh` script (with `--prefix` option to specify where to install the toolchain).
3. Wait for the result :-)

**Note:** *If the build process fails, please write me an e-mail.  I'll try to help out. Don't forget to put into e-mail as much data about your environment as possible!*

### Tested on

Following platforms were tested:

 * Ubuntu 10.04 32-bit (gcc 4.4.3)
 * MacOS X 10.7.5 (MacPorts gcc47 4.7.3_0)
 
But I do as much as possible to make the toolchain portable among Un*x-like environments.