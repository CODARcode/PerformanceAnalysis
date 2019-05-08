OMPI := /opt/openmpi-4.0.1

CXX := g++
CXX_FLAGS := -Wall -Wextra -std=c++17 -O3
#MPICC := mpic++
MPICC := $(OMPI)/bin/mpic++

MPI_COMPILE_FLAGS := `$(MPICC) --showme:compile`
MPI_LINK_FLAGS := `$(MPICC) --showme:link`

BIN := .
SRC := .

all: $(BIN)/ps_zmq_server $(BIN)/ps_zmq_client $(BIN)/ps_mpi_server $(BIN)/ps_mpi_client $(BIN)/ps_nng $(BIN)/tpool_ex

$(BIN)/ps_zmq_server: $(SRC)/ps_zmq_server.cpp
	$(CXX) $(CXX_FLAGS) -I. -L. $^ -o $@ -lzmq -lboost_thread -lboost_system

$(BIN)/ps_zmq_client: $(SRC)/ps_zmq_client.cpp
	$(CXX) $(CXX_FLAGS) -I. -L. $^ -o $@ -lzmq

$(BIN)/ps_mpi_server: $(SRC)/ps_mpi_server.cpp
	$(MPICC) $(CXX_FLAGS) $(MPI_COMPILE_FLAGS) -I. -L. $^ -o $@ $(MPI_LINK_FLAGS) -lboost_thread -lboost_system

$(BIN)/ps_mpi_client: $(SRC)/ps_mpi_client.cpp
	$(MPICC) $(CXX_FLAGS) $(MPI_COMPILE_FLAGS) -I. -L. $^ -o $@ $(MPI_LINK_FLAGS)

$(BIN)/ps_nng: $(SRC)/ps_nng.cpp
	$(CXX) $(CXX_FLAGS) -I. -L. $^ -o $@ -lnng -lpthread

$(BIN)/tpool_ex: $(SRC)/ThreadPool.cpp $(SRC)/ThreadPoolEx.cpp
	$(CXX) $(CXX_FLAGS) -I. -L. $^ -o $@  -lpthread

clean:
	rm $(BIN)/ps_zmq_server $(BIN)/ps_zmq_client $(BIN)/ps_mpi_client $(BIN)/ps_mpi_server $(BIN)/ps_nng $(BIN)/tpool_ex
