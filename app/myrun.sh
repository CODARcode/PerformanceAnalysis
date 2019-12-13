#!/bin/bash


rm -f ps.host ps.port ps.pid
echo "==========================================="
echo "Launch server"
echo "==========================================="
jsrun -n2 -a2 -c2 -g 0  ./run_pserver.sh  &

#./run_pserver.sh &

while [ ! -f ps.host ];
 do
    sleep 1
 done
PS_HOST=$(<ps.host)
while [ ! -f ps.port ];
 do
   sleep 1
 done
PS_PORT=$(<ps.port)
echo "PS_HOST: ${PS_HOST}"
echo "PS_PORT: ${PS_PORT}"
PS_TCP="tcp://${PS_HOST}:${PS_PORT}"
echo "PS_TCP: ${PS_TCP}"
while [ ! -f ps.pid ];
 do
    sleep 1
 done
ps_pid=$(<ps.pid)
echo "PS_PID: ${ps_pid}"

ps

sleep 1

echo "==========================================="
echo "Launch client"
echo "==========================================="
jsrun -n2  -a6 -c6 -g 0  ./hpclient $PS_TCP 
#./hpclient $PS_TCP 

wait $ps_pid 

echo "bye!"


