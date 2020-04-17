#/bin/sh

h="$(dirname $0)"
force=false
bd=
: ${PREFIX:=}
eval set -- $(getopt -o b:fp: -l buildir:,force,prefix: -- "$@") || exit
while :; do
	case "$1" in
	-b|--buildir) bd="$2"; shift;;
	-f|--force) force=true;;
	-p|--prefix) PREFIX="$2"; shift;;
	--) shift; break;;
	esac
	shift
done

: ${bd:=build}
mkdir -p "$h/$bd" || exit
cd "$h/$bd" || exit

$force && { rm -r * 2>/dev/null || rm CMakeCache.txt 2>/dev/null; }
test -f CMakeCache.txt -a -f Makefile || \
cmake \
	-DPROJECT_VERSION=9.0.0 \
	-DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX:=${PREFIX:=$HOME/.local}} \
	-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE:=Debug} \
	-DAFB_DEVEL=${AGL_DEVEL:=ON} \
	-DWITH_MONITORING=${WITH_MONITORING:=ON} \
	..

make -j "$@"

