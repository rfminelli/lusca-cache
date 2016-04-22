# Basics #

  1. Setup compilers and AutoTools
    * gcc 3.x and 4.x are ok.
    * If your system lacks uudecode, install sharutils.
  1. DownloadSource
  1. ./bootstrap.sh
  1. ./configure
  1. make install

## Configure options ##

TODO: Some docs better than --help are preferable.

# Linux #

TODO

# FreeBSD #

TODO

# Cygwin #

## Prepare source tree ##

Most of the source codes in svn are set "svn:eol-tyle=native" property. On the other hand, bash (/bin/sh) requires [LF-ended lines by default](http://cygwin.com/ml/cygwin-announce/2007-01/msg00015.html). So you need to:
  * Checkout with Cygwin's svn command.
  * Mount your working directory with the [text option](http://cygwin.com/cygwin-ug-net/using.html#mount-table) if you stick with Windows-based svn clients (e.g. TortoiseSVN)
  * Use bash with igncr option (not tested)

Note that the text option is not perfect. ``echo 1 >n.txt; echo 2 >>n.txt; echo `cat n.txt``` and see the result.

cf. http://cygwin.com/cygwin-ug-net/using-textbinary.html

## Required packages ##

  * automake
  * gcc4
  * make
  * sharutils

## Build ##

Nothing special. Just note that your filesystem is c:\cygwin rooted.

# MinGW #

We assume cross-compiling on Cygwin.

## Required packages ##

Cygwin's requirements plus:

  * gcc-mingw

## Build ##

  1. ./bootstrap.sh
  1. CC="gcc-3 -mno-cygwin" ./configure --host=i686-pc-mingw32 --enable-win32-service
  1. make install