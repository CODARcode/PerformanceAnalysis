#!/bin/bash
#Fail if anything fails
set -e
set -o pipefail

cd /opt/chimbuko/ad/test
echo "Executing tests"
./run_all.sh DOCKER_SETUP_MOCHI
cd /build
echo "Gathering coverage data"
gcovr -j 8 -v -r /Downloads/PerformanceAnalysis --object-directory=. -e '.*3rdparty/' -e '.*app/' --xml /Downloads/PerformanceAnalysis/coverage.xml
echo "Uploading coverage data"
cd /Downloads/PerformanceAnalysis
bash <(curl -s https://codecov.io/bash) -t d416f950-cacb-4716-a3a8-e608fd3bf84a -f coverage.xml -v -K
echo "Done"
