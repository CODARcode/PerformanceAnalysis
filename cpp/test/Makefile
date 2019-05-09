CXX := g++
CXX_FLAGS := -Wall -Wextra -std=c++17 -O3 

INCS := -I../include -I/usr/include
LIBS := -L/usr/lib
OBJS := test_utils.o ../src/util/RunStats.o

#CXX_FLAGS += -lgtest -lpthread
LIBRARIES := -lpthread -lgtest

testAll: $(OBJS)
	$(CXX) $(CXX_FLAGS) $(INCS) $(LIBS) -o testAll test_main.cpp $(OBJS) $(LIBRARIES)

.cpp.o:
	$(CXX) $(CXX_FLAGS) -c $< -o $@ $(INCS) $(LIBRARIES)

clean:
	rm testAll *.o testAll.xml


