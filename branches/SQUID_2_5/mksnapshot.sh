#!/bin/sh -e
if [ $# -gt 1 ]; then
	echo "Usage: $0 [branch]"
	exit 1
fi
package=squid
tag=${1:-HEAD}
startdir=$PWD
date=`env TZ=GMT date +%Y%m%d`

tmpdir=${TMPDIR:-${PWD}}/${package}-${tag}-mksnapshot

CVSROOT=${CVSROOT:-/server/cvs-server/squid}
export CVSROOT

rm -rf $tmpdir
trap "rm -rf $tmpdir" 0

rm -f ${tag}.out
cvs -Q export -d $tmpdir -r $tag $package
if [ ! -f $tmpdir/configure ]; then
	echo "ERROR! Tag $tag not found in $package"
fi

cd $tmpdir
eval CVS`grep ^VERSION= configure`
VERSION=`echo $CVSVERSION | sed -e 's/-CVS//'`
eval `grep ^PACKAGE= configure`
ed -s configure.in <<EOS
g/${CVSVERSION}/ s//${VERSION}-${date}/
w
EOS
ed -s configure <<EOS
g/${CVSVERSION}/ s//${VERSION}-${date}/
w
EOS

./configure --silent
make -s dist-all

cd $startdir
cp -p $tmpdir/${PACKAGE}-${VERSION}-${date}.tar.gz .
echo ${PACKAGE}-${VERSION}-${date}.tar.gz >>${tag}.out
cp -p $tmpdir/${PACKAGE}-${VERSION}-${date}.tar.bz2 .
echo ${PACKAGE}-${VERSION}-${date}.tar.bz2 >>${tag}.out

relnotes=$tmpdir/doc/release-notes/release-`echo $VERSION | cut -d. -f1,2 | cut -d- -f1`.html
if [ -f $relnotes ]; then
	cp -p $relnotes ${PACKAGE}-${VERSION}-${date}-RELEASENOTES.html
	echo ${PACKAGE}-${VERSION}-${date}-RELEASENOTES.html >>${tag}.out
fi
cp -p ChangeLog
echo ChangeLog >>${tag}.out

if (echo $VERSION | grep PRE) || (echo $VERSION | grep STABLE); then
  echo "Differences from ${PACKAGE}-${VERSION} to ${PACKAGE}-${VERSION}-${date}" >${PACKAGE}-${VERSION}-${date}.diff
  cvs -q rdiff -u -r SQUID_`echo $VERSION | tr .- __` -r $tag $package >>${PACKAGE}-${VERSION}-${date}.diff
  echo ${PACKAGE}-${VERSION}-${date}.diff >>${tag}.out
fi
