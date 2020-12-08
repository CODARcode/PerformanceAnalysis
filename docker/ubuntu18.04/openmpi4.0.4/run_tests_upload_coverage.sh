#!/bin/bash
#Fail if anything fails
set -e
set -o pipefail

cd /opt/chimbuko/ad/test
echo "Executing tests"
./run_all.sh DOCKER_SETUP_MOCHI
cd /build
echo "Gathering coverage data"
#gcovr -j 8 -v -r /Downloads/PerformanceAnalysis --object-directory=. -e '.*3rdparty/' -e '.*app/' --xml /Downloads/PerformanceAnalysis/coverage.xml

#lcov --capture --directory . --rc lcov_branch_coverage=1 --output-file coverage.info
#lcov --rc lcov_branch_coverage=1 --remove coverage.info '/usr/*' '/spack/*' '*/3rdparty/*' '*/app/*' '/opt/*' --output-file coverage2.info

lcov --capture --directory . --output-file coverage.info
lcov --remove coverage.info '/usr/*' '/spack/*' '*/3rdparty/*' '*/app/*' '/opt/*' --output-file coverage2.info
mv coverage2.info /Downloads/PerformanceAnalysis/

echo "Uploading coverage data"
cd /Downloads/PerformanceAnalysis
#bash <(curl -s https://codecov.io/bash) -t d416f950-cacb-4716-a3a8-e608fd3bf84a -f coverage.xml -v -K

#Add -d to the line below to dump the upload to stdout rather than uploading it
bash <(curl -s https://codecov.io/bash) -t d416f950-cacb-4716-a3a8-e608fd3bf84a -f coverage2.info -X gcov -K -v
echo "Done"
