#!/bin/bash


rm -f mp* wp*
echo "==========================================="
echo "Launch master server"
echo "==========================================="
jsrun -n1 -a1 -c1 -g 0  ./hpserver 0  &

#./run_pserver.sh &


sleep 10

echo "==========================================="
echo "Launch parameter client"
echo "==========================================="
jsrun -n1  -a1 -c1 -g 0  ./hpserver 1  &
#./hpclient $PS_TCP 


sleep 10

echo "==========================================="
echo "Launch client"
echo "==========================================="
#jsrun -n2  -a6 -c4 -g 0  ./hpclient 
#./hpclient $PS_TCP

sleep 10

echo "bye!"


