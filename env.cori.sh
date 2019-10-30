#module load gcc/7.3.0
module load bzip2
module load adios2
#module load openmpi

export CRAYPE_LINK_TYPE=dynamic
export ZEROMQ_ROOT=/global/homes/a/amalik/CODAR/libzmq/install
#export CHIMBUKO=/global/homes/a/amalik/CODAR/PerformanceAnalysis/cpp
export LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:${ZEROMQ_ROOT}/lib

