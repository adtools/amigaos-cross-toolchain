DejaGNU board for testing m68k-amigaos GCC
---

* Download
[gcc-testsuite](https://ftp.gnu.org/pub/gnu/gcc/gcc-3.0/gcc-testsuite-3.0.tar.gz)
and unpack into `submodules/gcc-2.95.3`
* Install [amitools](https://github.com/cnvogelg/amitools/) and configure `vamos`
* Configure DejaGNU board search path with `$HOME/.dejagnurc` containing:
```tcl
if ![info exists boards_dir] {
    set boards_dir {}
}
lappend boards_dir "$HOME/workspace/amigaos-cross-toolchain/dejagnu/boards"
```
* Run tests:
```
$ cd .build-m68k/gcc-2.95.3/gcc
$ make check-gcc \
    RUNTESTFLAGS='--target_board=amigaos execute.exp=20000113-1* -v SIM=vamos'
```
