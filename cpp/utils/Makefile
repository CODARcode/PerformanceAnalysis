CXX := g++
CXX_FLAGS := -Wall -Wextra -std=c++17 -O3

BIN := .
SRC := .

all: $(BIN)/threadPool_ex

$(BIN)/threadPool_ex: $(SRC)/threadPool_ex.cpp
	$(CXX) $(CXX_FLAGS) -I.. -L. $^ -o $@ -lpthread

