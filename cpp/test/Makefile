MPI_ROOT := /opt/openmpi-4.0.1

CXX := g++
CXX_FLAGS := -Wall -Wextra -std=c++17 -O3 

MPICXX := $(MPI_ROOT)/bin/mpic++
MPI_COMPILE_FLAGS := `$(MPICXX) --showme:compile`
MPI_LINK_FLAGS := `$(MPICXX) --showme:link`

INCS := -I../include -I/usr/include
LIBS := 
OBJS := test_utils.o test_param.o ../src/util/RunStats.o ../src/param.o ../src/param/sstd_param.o

OBJS_MPI := test_mpinet.o ../src/net.o ../src/net/mpi_net.o ../src/util/RunStats.o ../src/param.o ../src/param/sstd_param.o
MPI_FLAG := -DCHIMBUKO_USE_MPINET

#CXX_FLAGS += -lgtest -lpthread
LIBRARIES := -lpthread -lgtest

all: testAll testMpiAll

testAll: $(OBJS)
	$(CXX) $(CXX_FLAGS) $(INCS) $(LIBS) -o testAll test_main.cpp $(OBJS) $(LIBRARIES)

testMpiAll: $(OBJS_MPI)
	$(MPICXX) $(CXX_FLAGS) $(MPI_COMPILE_FLAGS) $(INCS) $(LIBS) $(MPI_FLAG) -o testMpiAll test_main_mpi.cpp $(OBJS_MPI) $(LIBRARIES) $(MPI_LINK_FLAGS)

.cpp.o:
	$(MPICXX) $(CXX_FLAGS) $(MPI_COMPILE_FLAGS) $(INCS) $(LIBS) $(MPI_FLAG) -c $< -o $@ $(LIBRARIES) $(MPI_LINK_FLAGS)
	#$(CXX) $(CXX_FLAGS) -c $< -o $@ $(INCS) $(LIBRARIES)

clean:
	rm testAll testMpiAll $(OBJS) $(OBJS_MPI)


