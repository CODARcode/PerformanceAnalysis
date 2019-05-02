ADIOS2_DIR=/opt/adios2
override ADIOS2_INC=`${ADIOS2_DIR}/bin/adios2-config --cxx-flags`
override ADIOS2_LIB=`${ADIOS2_DIR}/bin/adios2-config --cxx-libs`

CXX		  := g++
# CXX_FLAGS := -Wall -Wextra -std=c++17 -ggdb
CXX_FLAGS := -Wall -Wextra -std=c++17 -O3
MPICC     := mpic++

BIN     := bin
SRC     := src
INCLUDE	:= include
LIB		:= lib

#INCLUDE += /opt/adios2/include
CXX_FLAGS += -isystem /opt/adios2/include
# LIB += /opt/adios2/lib

LIBRARIES  := `curl-config --libs`
#EXECUTABLE := perfanal

SRC_PERFANAL := $(SRC)/ADEvent.cpp $(SRC)/ADio.cpp $(SRC)/ADOutlier.cpp $(SRC)/ADParser.cpp $(SRC)/DispatchQueue.cpp $(SRC)/ExecData.cpp $(SRC)/main.cpp $(SRC)/RunStats.cpp $(SRC)/utils.cpp
SRC_PARAM_WEB := $(SRC)/param_web.cpp $(SRC)/RunStats.cpp $(SRC)/ParamServer.cpp
SRC_PARAM_WEB_CLIENT := $(SRC)/param_web_client.cpp $(SRC)/RunStats.cpp
CROW_INC := extern
BOOST_LIB := /usr/lib/x86_64-linux-gnu

all: $(BIN)/perfanal $(BIN)/param_web $(BIN)/param_web_client

# run: clean all
# 	clear
# 	mpirun -n 1 ./$(BIN)/$(EXECUTABLE)

$(BIN)/perfanal: $(SRC_PERFANAL)
	$(MPICC) $(CXX_FLAGS) -I$(INCLUDE) -L$(LIB) $^ -o $@ $(LIBRARIES) $(ADIOS2_LIB) 

$(BIN)/param_web: $(SRC_PARAM_WEB)
	$(CXX) $(CXX_FLAGS) -I$(INCLUDE) -I$(CROW_INC) -L$(LIB) -L$(BOOST_LIB) $^ -o $@ $(LIBRARIES) -lpthread -lboost_system -lboost_thread -lboost_chrono -lboost_date_time -lboost_atomic

$(BIN)/param_web_client: $(SRC_PARAM_WEB_CLIENT)
	$(CXX) $(CXX_FLAGS) -I$(INCLUDE) -I$(CROW_INC) -L$(LIB) $^ -o $@ $(LIBRARIES)

clean:
	rm $(BIN)/*





# BIN		:= bin
# SRC		:= src
# INCLUDE	:= include
# LIB		:= lib

# LIBRARIES	:=
# EXECUTABLE	:= main


# all: $(BIN)/$(EXECUTABLE)

# run: clean all
# 	clear
# 	./$(BIN)/$(EXECUTABLE)


