OMPI_ROOT := /opt/openmpi-4.0.1

CXX := g++
MPICXX := $(OMPI_ROOT)/bin/mpic++

CXX_FLAGS := -Wall -Wextra -std=c++17 -O3 -DCHIMBUKO_USE_MPINET
MPI_COMPILE_FLAGS := `$(MPICXX) --showme:compile`
MPI_LINK_FLAGS := `$(MPICXX) --showme:link`

INCS := -I./include -I/usr/include
LIBS := 
LIBRARIES := -lpthread 

TARGET = main


all: $(TARGET)

$(TARGET): main.cpp src/net.cpp src/net/mpi_net.cpp
	$(MPICXX) $(CXX_FLAGS) $(MPI_COMPILE_FLAGS) $(INCS) $(LIBS) $^ -o $@ $(LIBRARIES) $(MPI_LINK_FLAGS)


# %.o: %.cpp
# 	$(MPICXX) $(CXX_FLAGS) $(MPI_COMPILE_FLAGS) $(INCS) $(LIBS) $^ -o $@ $(LIBRARIES) $(MPI_LINK_FLAGS)



