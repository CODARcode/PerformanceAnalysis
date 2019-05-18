#!/bin/bash
# MPI_ROOT=/opt/openmpi-4.0.1
# MPIRUN=${MPI_ROOT}/bin/mpirun
# MPISERVER=${MPI_ROOT}/bin/ompi-server

# MPI_URI_PATH=ompi_uri_path.txt

# # ./testAll

# #echo "run testMpiAll"
# ${MPIRUN} -n 1 --ompi-server file:${MPI_URI_PATH} mainMpinet -n 1 &

# #echo "run mpiClient"
# sleep 1
# ${MPIRUN} -n 1 --ompi-server file:${MPI_URI_PATH} mpiClient 

# #echo "run mpiClient"
# sleep 1
# ${MPIRUN} -n 10 --ompi-server file:${MPI_URI_PATH} mpiClient 

# sleep 1
# ${MPIRUN} -n 10 --ompi-server file:${MPI_URI_PATH} mpiClient 


echo "run test ZMQNet"
mpirun --allow-run-as-root -n 1 mainNet -n 1 &

sleep 1
mpirun --allow-run-as-root -n 1 ../bin/pclient "tcp://localhost:5559"

sleep 1
mpirun --allow-run-as-root -n 10 ../bin/pclient "tcp://localhost:5559"

sleep 1
mpirun --allow-run-as-root -n 10 ../bin/pclient "tcp://localhost:5559"