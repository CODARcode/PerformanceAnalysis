MPI_ROOT := /opt/openmpi-4.0.1
ADIOS_ROOT := /opt/adios2

CXX := g++
CXX_FLAGS := -Wall -Wextra -std=c++17 -O3 

MPICXX := $(MPI_ROOT)/bin/mpic++
MPI_COMPILE_FLAGS := `$(MPICXX) --showme:compile`
MPI_LINK_FLAGS := `$(MPICXX) --showme:link`

ADIOS2_INC := `${ADIOS_ROOT}/bin/adios2-config --cxx-flags`
ADIOS2_LIB := `${ADIOS_ROOT}/bin/adios2-config --cxx-libs`
ADIOS2_FLAGS := -isystem ${ADIOS_ROOT}/include

INCS := -I../include -I/usr/include
LIBS := 
OBJS := test_utils.o test_param.o ../src/util/RunStats.o ../src/param.o ../src/param/sstd_param.o ../src/message.o

OBJS_MPI := test_mpinet.o ../src/net.o ../src/net/mpi_net.o ../src/util/RunStats.o ../src/param.o ../src/param/sstd_param.o ../src/message.o
MPI_FLAG := -DCHIMBUKO_USE_MPINET

OBJS_AD := test_ad.o ../src/ad/ADParser.o ../src/ad/ADEvent.o ../src/ad/utils.o ../src/ad/ExecData.o ../src/ad/ADOutlier.o ../src/util/RunStats.o ../src/util/DispatchQueue.o ../src/ad/ADio.o

#CXX_FLAGS += -lgtest -lpthread
LIBRARIES := -lpthread -lgtest `curl-config --libs`

all: testAll testMpiNet mpiClient testAD

testAll: $(OBJS)
	$(CXX) $(CXX_FLAGS) $(INCS) $(LIBS) -o testAll test_main.cpp $(OBJS) $(LIBRARIES)

testMpiNet: $(OBJS_MPI)
	$(MPICXX) $(CXX_FLAGS) $(MPI_COMPILE_FLAGS) $(INCS) $(LIBS) $(MPI_FLAG) -o testMpiNet test_main_mpinet.cpp $(OBJS_MPI) $(LIBRARIES) $(MPI_LINK_FLAGS)

mpiClient: $(OBJS_MPI)
	$(MPICXX) $(CXX_FLAGS) $(MPI_COMPILE_FLAGS) $(INCS) $(LIBS) $(MPI_FLAG) -o mpiClient ./clients/mpi_client.cpp $(OBJS_MPI) $(LIBRARIES) $(MPI_LINK_FLAGS)

testAD: $(OBJS_AD)
	$(MPICXX) $(CXX_FLAGS) $(ADIOS2_FLAGS) $(MPI_COMPILE_FLAGS) $(INCS) $(LIBS) $(MPI_FLAG) -o testAD test_main_ad.cpp $(OBJS_AD) $(LIBRARIES) $(MPI_LINK_FLAGS) $(ADIOS2_LIB)

.cpp.o:
	$(MPICXX) $(CXX_FLAGS) $(ADIOS2_FLAGS) $(MPI_COMPILE_FLAGS) $(INCS) $(LIBS) $(MPI_FLAG) -c $< -o $@ $(LIBRARIES) $(MPI_LINK_FLAGS) $(ADIOS2_LIB)
	#$(CXX) $(CXX_FLAGS) -c $< -o $@ $(INCS) $(LIBRARIES)

clean:
	rm testAll testMpiNet testAD mpiClient $(OBJS) $(OBJS_MPI) $(OBJS_AD)


