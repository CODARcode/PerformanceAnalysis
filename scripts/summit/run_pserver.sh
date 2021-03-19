#!/bin/bash

NTHREADS=$1
PARAM_OUT=$2
APP_NMPI=$3
WS_ADDR=$4

echo `pwd`
ls -l
echo "PS parameters:"
echo "POOL SIZE: ${NTHREADS}"
echo "OUTPUT: ${PARAM_OUT}"
echo "APP NMPI: ${APP_NMPI}"
echo "WS ADDR: ${WS_ADDR}"

bin/pserver ${NTHREADS} ${PARAM_OUT} ${APP_NMPI} ${WS_ADDR} &
ps_pid=$!
sleep 10

echo `hostname` >> ps.host
echo "5559" >> ps.port
echo $ps_pid >> ps.pid

ls -l

