#!/bin/bash
MPI_ROOT=/opt/openmpi-4.0.1
MPIRUN=${MPI_ROOT}/bin/mpirun
MPISERVER=${MPI_ROOT}/bin/ompi-server

MPI_URI_PATH=ompi_uri_path.txt

#${MPISERVER} --no-daemonize -r ${MPI_URI_PATH} & 

${MPIRUN} -n 1 --ompi-server file:${MPI_URI_PATH} testMpiAll -n 1