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


echo "run test ZMQNet"
echo "First run web-server for NetStatSenderTest"
python3 ${appdir}/ws_flask_stat.py &
sleep 1

mpirun --allow-run-as-root --oversubscribe -n 1 mainStatSender -n 1 &
test_pid=$!

wait $test_pid
curl -X POST "http://0.0.0.0:5000/shutdown"
