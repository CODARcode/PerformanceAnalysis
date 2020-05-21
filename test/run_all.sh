#!/bin/bash

echo "Unit tests"
cd unit_tests
./run_all.sh
cd -

echo "Simple test"
./mainSimple

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

