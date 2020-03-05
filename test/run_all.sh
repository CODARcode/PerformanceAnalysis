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

echo "AD module test"
echo "Wait for next tests for 5 seconds ..."
sleep 5
./run_ad.sh


