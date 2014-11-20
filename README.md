AmigaOS/m68k targeted cross compilers and assembler
===

**Author:** [Krystian Bacławski](mailto:krystian.baclawski@gmail.com)

**Short description:** m68k-amigaos gcc / binutils / vbcc / vasm toolchain build script.

### Overview

**m68k-amigaos-toolchain** project provides an easy way to build m68k AmigaOS 3.x target toolchain in a Unix-like environment. Build process should produce following set of tools:

 * gcc 2.95.3
 * g++ 2.95.3
 * libstdc++ 2.10
 * binutils 2.9.1 (assembler, linker, etc.)
 * libnix 2.1 (standard ANSI/C library replacement for AmigaOS)
 * libm 5.4 (provides math library implementation for non-FPU Amigas)
 * AmigaOS headers & libraries & autodocs (for AmigaOS 3.9)
 * ixemul.library 48.2
 * vbcc 0.9b + vclib
 * vasm 1.6b
 * vlink 0.14c

**Note:** *Patches are welcome!*

### Downloads

There are no downloads provided for the time being. I do as much as possible to make the toolchain portable among Unix-like environments. Following platforms were tested and the toolchain is known to work for them:

 * Cygwin 1.7.18 (gcc 4.5.3)
 * Ubuntu 14.04 LTS 32-bit (gcc 4.8.2)
 * Ubuntu 14.04 LTS 64-bit (gcc 4.8.2) *Requires gcc-multilib package, and i386 libraries!*
 * MacOS X 10.9.3 (MacPorts - Apple's clang-503.0.40)
 
### Documentation

Documentation from Free Software Fundation:

 * [gcc 2.95.3](http://gcc.gnu.org/onlinedocs/gcc-2.95.3/gcc.html)
 * [gcc 3.4.6](http://gcc.gnu.org/onlinedocs/gcc-3.4.6/gcc/)
 * [binutils](http://sourceware.org/binutils/docs/)

Texinfo documents from GeekGadgets converted into HTML:

 * [libnix - a static library for GCC on the amiga](http://cahirwpz.users.sourceforge.net/libnix/index.html)
 * [AmigaOS-only features of GCC](http://cahirwpz.users.sourceforge.net/gcc-amigaos/index.html)

AmigaOS specific documents:

 * [Amiga Developer Docs](http://amigadev.elowar.com)

### Compiling

*Firstly… you should have basic understanding of Unix console environment, really* ;-)

#### Prerequisites

You have to have following packages installed in your system:

 * GNU autoconf
 * GNU gcc 4.x **32-bit version!**
 * GNU make 3.x
 * lha
 * perl 5.10
 * libncurses5-dev **32-bit version!**

*For MacOSX users*: you'll likely need to have [MacPorts](http://www.macports.org) or [Homebrew](http://brew.sh) installed in order to build the toolchain.

#### How to build?

**Warning:** *Building with `sudo` is not recommended. I'm not responsible for any damage to your system.*

Follow steps listed below:

1. Fetch *m68k-amigaos-toolchain* project to your local drive:  

    ```
# git clone git://github.com/cahirwpz/m68k-amigaos-toolchain.git
# cd m68k-amigaos-toolchain
```

2. Download sources (use `fetch.sh` script in `archives` directory):   

    ```
# cd archives   
# ./fetch.sh
```
   
3. Run `bootstrap.sh` script (with `--prefix` option to specify where to install the toolchain). Note, that the destination directory must be writable by the user. 

    ```
# cd ..
# ./bootstrap.sh --prefix=/opt/m68k-amigaos build
```

4. Wait for the result :-)

5. *(optional)* Install additional SDKs (e.g. AHI, CyberGraphX, Magic User Interface, etc.):

    ```
# export PATH=/opt/m68k-amigaos/bin:$PATH
# ./install-sdk.sh --prefix=/opt/m68k-amigaos ahi cgx mui
```

**Note:** *If the build process fails, please write me an e-mail.  I'll try to help out. Don't forget to put into e-mail as much data about your environment as possible!*

