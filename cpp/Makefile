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
EXECUTABLE := main

all: $(BIN)/$(EXECUTABLE)

run: clean all
	clear
	mpirun -n 1 ./$(BIN)/$(EXECUTABLE)

$(BIN)/$(EXECUTABLE): $(SRC)/*.cpp
	$(MPICC) $(CXX_FLAGS) -I$(INCLUDE) -L$(LIB) $^ -o $@ $(LIBRARIES) $(ADIOS2_LIB) 

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


