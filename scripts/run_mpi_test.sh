#!/bin/bash

export PYTHONPATH=${PYTHONPATH}:/opt/adios2/lib/python3.5/site-packages
export PYTHONPATH=${PYTHONPATH}:../lib
export PYTHONPATH=${PYTHONPATH}:.

rm -f *.bp.sst


# with existing bp file
mpirun -n 2 python3 simple_mpi_writer_sst.py &
mpirun -n 2 python3 simple_mpi_reader_sst.py

# with pseudo data
#mpirun -n 2 python3 sst_writer.py &
#mpirun -n 2 python3 sst_reader.py
