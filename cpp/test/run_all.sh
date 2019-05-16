#!/bin/bash


# base (simple) tests
echo "Simple test"
./mainSimple

echo "MPINET test"
echo "Wait for next tests for 5 seconds ..."
sleep 5
# now, we need to run ompi-server required for the following tests
MPIURI=mpiuri
MPIRUN=/opt/openmpi-4.0.1/bin/mpirun
#ompi-server --no-daemonize -r ${MPIURI} &
${MPIRUN} --allow-run-as-root -n 1 --ompi-server file:${MPIURI} mainMpinet -n 1 &
sleep 1
${MPIRUN} --allow-run-as-root -n 1 --ompi-server file:${MPIURI} mpiClient
sleep 1
${MPIRUN} --allow-run-as-root -n 10 --ompi-server file:${MPIURI} mpiClient 
sleep 1
${MPIRUN} --allow-run-as-root -n 10 --ompi-server file:${MPIURI} mpiClient 

echo "AD module test"
echo "Wait for next tests for 5 seconds ..."
sleep 5
mkdir -p temp
${MPIRUN} --allow-run-as-root -n 1 --ompi-server file:${MPIURI} mpiServer &
${MPIRUN} --allow-run-as-root -n 4 --ompi-server file:${MPIURI} mainAd -n 4
rm -r temp


