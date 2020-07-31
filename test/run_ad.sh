#!/bin/bash
#Fail if any test fails
set -e
set -o pipefail

appdir=../app
if [ ! -f "${appdir}/pserver" ]; then
   appdir="../bin"
   if [ ! -f "${appdir}/pserver" ]; then
       echo "Could not find application directory"
       exit
   fi
fi

srcdir=@srcdir@
if [ ! -d "data" ]; then
  ln -s $srcdir/data data
fi

mkdir -p temp perf

export CHIMBUKO_DISABLE_CUDA_JIT_WORKAROUND=1

mpirun --allow-run-as-root --oversubscribe -n 1 ${appdir}/pserver 4 "./perf/" &
ps_wid=$!

sleep 5
mpirun --allow-run-as-root --oversubscribe  -n 4 ./mainAd -n 4

wait $ps_wid
rm -rf temp perf
