#/bin/sh

h="$(dirname $0)"
force=false
: ${bd:=build}
: ${PREFIX:=$HOME/.local}
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

cd "$h"
h=$(pwd)
mkdir -p "$bd" || exit
cd "$bd" || exit

$force && { rm -r * 2>/dev/null || rm CMakeCache.txt 2>/dev/null; }
test -f CMakeCache.txt -a -f Makefile || \
cmake \
	-DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX:=$PREFIX} \
	-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE:=Debug} \
	-DWITH_MONITORING=${WITH_MONITORING:=ON} \
	-DWITH_EXTENSION=${WITH_EXTENSION:=ON} \
	"$h"

make -j "$@"

