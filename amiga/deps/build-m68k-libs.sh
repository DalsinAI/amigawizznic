#!/bin/sh
# Cross-builds the static libraries the Team's ports share (zlib, libpng,
# IJG jpeg, sqlite3, libxml2) for AmigaOS 3.x, 68040/68060 + FPU, libnix,
# into $PREFIX (include/ and lib/). Run one library at a time:
#   sh build-m68k-libs.sh zlib|png|jpeg|sqlite|xml2
# -j2 at most: the build machine is short of memory.
set -e
STOVE=${STOVE:-$HOME/CradleLab/toolchain/openamigagcc-16.2.0-open4-x86_64-linux}
DEPS=${DEPS:-/mnt/amigachrome/team2/deps}
PREFIX=${PREFIX:-$DEPS/m68k-amigaos}
CC=$STOVE/bin/m68k-amigaos-gcc
AR=$STOVE/bin/m68k-amigaos-ar
RANLIB=$STOVE/bin/m68k-amigaos-ranlib
CFLAGS="-noixemul -m68040 -m68881 -O2 -fomit-frame-pointer -fno-tree-loop-distribute-patterns"
mkdir -p $PREFIX/include $PREFIX/lib

objs() { # dir lib files...
  d=$1; lib=$2; shift 2
  rm -rf $d/.m68k; mkdir -p $d/.m68k
  for f in "$@"; do
    $CC $CFLAGS $EXTRA -I$d -I$PREFIX/include -c $d/$f -o $d/.m68k/$(basename $f .c).o
  done
  rm -f $PREFIX/lib/$lib
  $AR rcs $PREFIX/lib/$lib $d/.m68k/*.o
  $RANLIB $PREFIX/lib/$lib
}

case "$1" in
zlib)
  d=$DEPS/zlib-1.3.1
  EXTRA="-DHAVE_UNISTD_H -DHAVE_STDARG_H"
  objs $d libz.a adler32.c compress.c crc32.c deflate.c gzclose.c gzlib.c \
       gzread.c gzwrite.c infback.c inffast.c inflate.c inftrees.c trees.c \
       uncompr.c zutil.c
  cp $d/zlib.h $d/zconf.h $PREFIX/include/
  ;;
png)
  d=$DEPS/libpng-1.6.43
  cp $d/scripts/pnglibconf.h.prebuilt $d/pnglibconf.h
  EXTRA="-DPNG_ARM_NEON_OPT=0"
  objs $d libpng.a png.c pngerror.c pngget.c pngmem.c pngpread.c pngread.c \
       pngrio.c pngrtran.c pngrutil.c pngset.c pngtrans.c pngwio.c pngwrite.c \
       pngwtran.c pngwutil.c
  cp $d/png.h $d/pngconf.h $d/pnglibconf.h $PREFIX/include/
  ;;
jpeg)
  d=$DEPS/jpeg-9f
  cp $d/jconfig.txt $d/jconfig.h
  EXTRA=""
  objs $d libjpeg.a jaricom.c jcapimin.c jcapistd.c jcarith.c jccoefct.c \
       jccolor.c jcdctmgr.c jchuff.c jcinit.c jcmainct.c jcmarker.c jcmaster.c \
       jcomapi.c jcparam.c jcprepct.c jcsample.c jctrans.c jdapimin.c \
       jdapistd.c jdarith.c jdatadst.c jdatasrc.c jdcoefct.c jdcolor.c \
       jddctmgr.c jdhuff.c jdinput.c jdmainct.c jdmarker.c jdmaster.c \
       jdmerge.c jdpostct.c jdsample.c jdtrans.c jerror.c jfdctflt.c \
       jfdctfst.c jfdctint.c jidctflt.c jidctfst.c jidctint.c jquant1.c \
       jquant2.c jutils.c jmemmgr.c jmemnobs.c
  cp $d/jpeglib.h $d/jconfig.h $d/jmorecfg.h $d/jerror.h $PREFIX/include/
  ;;
sqlite)
  d=$DEPS/sqlite-amalgamation-3460100
  # sqlite3_amiga.c: the amalgamation with its mutexes on Exec semaphores
  # and its own VFS (amiga_vfs.c: dos.library's file calls, SQLite's locks
  # kept in memory for all of the program's connections). sqlite3_patched.c lets SQLITE_MUTEX_APPDEF keep SQLite from
  # picking its no-op mutexes for an OS it does not know.
  python3 - "$d/sqlite3.c" "$d/sqlite3_patched.c" <<'PY'
import sys
s = open(sys.argv[1]).read()
old = "#  else\n#    define SQLITE_MUTEX_NOOP\n#  endif\n"
new = "#  elif !defined(SQLITE_MUTEX_APPDEF)\n#    define SQLITE_MUTEX_NOOP\n#  endif\n"
assert s.count(old) == 1
open(sys.argv[2], "w").write(s.replace(old, new))
PY
  # Thread-safe; no shared memory, no mmap, no loadable extensions; temp
  # tables in memory.
  EXTRA="-DSQLITE_THREADSAFE=1 -DSQLITE_MUTEX_APPDEF=1 -DSQLITE_OMIT_LOAD_EXTENSION -DSQLITE_OMIT_WAL \
 -DSQLITE_MAX_MMAP_SIZE=0 -DSQLITE_OMIT_SHARED_CACHE -DSQLITE_ENABLE_LOCKING_STYLE=0 \
 -DSQLITE_DEFAULT_MEMSTATUS=0 -DSQLITE_OMIT_PROGRESS_CALLBACK \
 -DSQLITE_TEMP_STORE=3 -DSQLITE_OS_OTHER=1"
  objs $d libsqlite3.a sqlite3_amiga.c
  cp $d/sqlite3.h $PREFIX/include/
  ;;
xml2)
  # libxml2's own configure, cut down to the parser and the tree (no
  # threads, HTTP/FTP, iconv, ICU, zlib, lzma, catalogs, modules ...).
  d=$DEPS/libxml2-2.12.9
  ( cd $d && PATH=$STOVE/bin:$PATH CC=m68k-amigaos-gcc AR=m68k-amigaos-ar \
    RANLIB=m68k-amigaos-ranlib CFLAGS="$CFLAGS" LDFLAGS="-noixemul" \
    ./configure --host=m68k-amigaos --prefix=$PREFIX --disable-shared \
      --enable-static --without-python --without-threads \
      --without-thread-alloc --without-http --without-ftp --without-iconv \
      --without-icu --without-zlib --without-lzma --without-catalog \
      --without-modules --without-debug --without-readline \
      --without-history --without-schematron --without-xinclude \
      --without-xptr --without-c14n --without-legacy && \
    PATH=$STOVE/bin:$PATH make -j2 libxml2.la )
  cp $d/.libs/libxml2.a $PREFIX/lib/
  mkdir -p $PREFIX/include/libxml2
  rm -rf $PREFIX/include/libxml2/libxml
  cp -r $d/include/libxml $PREFIX/include/libxml2/
  rm -f $PREFIX/include/libxml2/libxml/Makefile* $PREFIX/include/libxml2/libxml/*.in
  ;;
*)
  echo "usage: $0 zlib|png|jpeg|sqlite|xml2" >&2; exit 1 ;;
esac
echo "built $1 into $PREFIX"
