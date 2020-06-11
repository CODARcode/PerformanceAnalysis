#!/bin/bash

#In the docker images we must load the MOCHI libraries using spack
if [[ $# == 1 && $1 == "DOCKER_SETUP_MOCHI" ]]; then
    source /spack/spack/share/spack/setup-env.sh
    spack load mochi-sonata
fi

echo "Unit tests"
cd unit_tests
./run_all.sh
cd -

echo "Simple test"
./mainSimple

echo "Stat sender test"
echo "Wait for next tests for 5 seconds ..."
sleep 5
./run_stat_sender.sh


echo "Parameter server test"
echo "Wait for next tests for 5 seconds ..."
sleep 5
./run_net.sh

if [ -f "../bin/provdb_admin" ]; then
    echo "Provenance DB test"
    echo "Wait for next tests for 5 seconds ..."
    sleep 5
    ./run_provdb_client_test.sh
fi

echo "AD module test"
echo "Wait for next tests for 5 seconds ..."
sleep 5
./run_ad.sh


if [ -f "../bin/provdb_admin" ]; then
    echo "AD module with provDB test"
    echo "Wait for next tests for 5 seconds ..."
    sleep 5
    ./run_ad_with_provdb.sh
fi

