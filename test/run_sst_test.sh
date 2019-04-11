#!/bin/bash

export PYTHONPATH=${PYTHONPATH}:/opt/adios2/lib/python3.5/site-packages
export PYTHONPATH=${PYTHONPATH}:../lib

# test with pseudo test data
echo "****** TEST WITH PSEUDO TEST DATA (MPI) ******"
rm -f *.bp*
mpirun -n 4 python3 sst_writer.py &
mpirun -n 4 python3 sst_reader.py 2>&1 | tee sst_test_pseudo.log

# test with real bp file
echo "****** TEST WITH REAL TEST DATA ******"
rm -f *.bp*
python3 sst_writer_simple.py ../data/shortdemo/tau-metrics.bp 2>&1 | tee sst_writer_simple.log &
python3 sst_reader_simple.py 2>&1 | tee sst_reader_simple.log

# test with real bp file (mpi)
echo "****** TEST WITH REAL TEST DATA (MPI) ******"
mpirun -n 4 python3 sst_writer_simple_mpi.py ../data/shortdemo/tau-metrics.bp 2>&1 | tee sst_writer_simple_mpi.log &
mpirun -n 4 python3 sst_reader_simple_mpi.py 2>&1 | tee sst_reader_simple_mpi.log