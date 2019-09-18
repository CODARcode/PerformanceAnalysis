#!/bin/bash


mkdir -p temp

# mpirun --allow-run-as-root -n 1 ../bin/pserver 4 & 
mpirun --allow-run-as-root -n 1 ../bin/pserver 4 &
ps_wid=$!

sleep 5
mpirun --allow-run-as-root -n 4 ./mainAd -n 4

wait $ps_wid
rm -r temp



# MPI_ROOT=/opt/openmpi-4.0.1
# MPIRUN=${MPI_ROOT}/bin/mpirun
# MPISERVER=${MPI_ROOT}/bin/ompi-server

# MPI_URI_PATH=ompi_uri_path.txt

# mkdir -p temp

# ${MPIRUN} -n 1 --ompi-server file:${MPI_URI_PATH} mpiServer & 
# ${MPIRUN} -n 4 --ompi-server file:${MPI_URI_PATH} mainAd -n 4

# rm -r temp