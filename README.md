GCC compiler for AmigaOS/m68k
===

**Author:** [Krystian Bacławski](mailto:krystian.baclawski@gmail.com)

**Short description:** m68k-amigaos gcc / binutils toolchain build script.

### Overview

**m68k-amigaos-toolchain** provides you a shell script that builds M68k AmigaOS 3.x) toolchain in your Un\*x-like environment. Build process should produce following set of tools that targets m68k-amigaos platform:

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

### Downloads

There are no downloads provided for the time being. I do as much as possible to make the toolchain portable among Un\*x-like environments. Following platforms were tested and the toolchain is known to work for them:

 * Cygwin 1.7.18 (gcc 4.5.3)
 * Ubuntu 12.04 LTS 32-bit (gcc 4.6.3)
 * Ubuntu 12.04 LTS 64-bit (gcc 4.6.3) *requires gcc-multilib*
 * MacOS X 10.7.5 (MacPorts - gcc 4.7.3)
 

### Compiling

*Firstly… you should have basic understanding of Un\*x console environment, really* ;-)

#### Prerequisites

You have to have following packages installed in your system:

 * GNU autoconf
 * GNU flex 2.5.x
 * GNU gcc 4.x
 * GNU make 3.x
 * GNU texinfo 4.x
 * lha
 * perl 5.10

#### How to build?

Follow steps listed below:

1. Fetch *m68k-amigaos-toolchain* project to your local drive:  
      
       git clone git://github.com/cahirwpz/m68k-amigaos-toolchain.git
       cd m68k-amigaos-toolchain
      
2. Download sources (use `fetch.sh` script in `archives` directory):   
   
       cd archives
       ./fetch.sh
   
3. Run `bootstrap.sh` script (with `--prefix` option to specify where to install the toolchain).

       cd ..
       ./bootstrap.sh --prefix=/opt/m68k-amigaos build


4. Wait for the result :-)

**Note:** *If the build process fails, please write me an e-mail.  I'll try to help out. Don't forget to put into e-mail as much data about your environment as possible!*

