#!/bin/bash

ROOT=$(dirname $0)
cd ${ROOT:-.}
ROOT=$(pwd)
echo ROOT=$ROOT

AFB=$(type -p afb-binder)
HELLO=~/redpesk/redpesk-core/afb-binding/tutorials/v4/hello4.so
PORT=12345
TEST=test

OUT=stress-out-server
rm $OUT*

tool=
ws=false
rpc=false
blind=false
traps=false
eval set -- $(getopt -o wrgsvbmct -l ws,rpc,gdb,strace,valgrind,blind,memcheck,callgrind,traps -- "$@") || exit
while true
do
	case "$1" in
	-w|--ws) ws=true; rpc=false; shift;;
	-r|--rpc) ws=false; rpc=true; shift;;
	-g|--gdb) tool=gdb; shift;;
	-s|--strace) tool=strace; shift;;
	-v|--valgrind) tool=valgrind; shift;;
	-m|--memcheck) tool=memcheck; shift;;
	-c|--callgrind) tool=callgrind; shift;;
	-b|--blind) blind=true; shift;;
	-t|--traps) traps=true; shift;;
	--) shift; break;;
	esac
done

case $tool in
 gdb) cmd="$(type -p gdb) -ex run --args";;
 valgrind) cmd="$(type -p valgrind) --leak-check=full";;
 memckeck) cmd="$(type -p valgrind) --tool=memcheck --leak-check=full";;
 callgrind) cmd="$(type -p valgrind) --tool=callgrind";;
 strace) cmd="$(type -p strace) -tt -f -o $OUT.strace";;
 *) cmd=;;
esac

OPTGLOB="--session-max=100 --workdir=$ROOT"
if $traps; then
  OPTGLOB="$OPTGLOB --trap-faults=no"
fi
OPTHTTP="--port=$PORT --roothttp=$TEST"
OPTHELLO="--binding=$HELLO"
OPTFRONT="$OPTGLOB --port=$PORT --roothttp=$TEST"
OPTBACK="$OPTGLOB -q --no-httpd $OPTHELLO"



if $rpc; then
  CMD="$AFB $OPTBACK --rpc-server=unix:@afw/hello --exec $cmd $AFB $OPTFRONT --rpc-client=unix:@afw/hello"
elif $ws; then
  CMD="$AFB $OPTBACK --ws-server=unix:@afw/hello --exec $cmd $AFB $OPTFRONT --ws-client=unix:@afw/hello"
else
  CMD="$cmd $AFB $OPTFRONT $OPTHELLO"
fi


echo "launch: $CMD $@"
case $tool in
 gdb) $CMD "$@";;
 *) if $blind; then exec $CMD "$@" > $OUT 2>&1; else exec $CMD "$@" 2>&1 | tee $OUT; fi
esac

