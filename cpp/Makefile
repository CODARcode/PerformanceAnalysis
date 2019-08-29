LIB_NAME = chimbuko

INC_DIRS := include 3rdparty
INC_FLAGS := $(addprefix -I,$(INC_DIRS))

BIN_DIR ?= bin
BUILD_DIR ?= build
SRC_DIRS ?= src
TEST_DIR ?= test
LIB_DIR ?= lib

# for MPINet, need the latest OpenMPI (>= 3.x)
#CXX := /opt/openmpi-4.0.1/bin/mpic++
CXX := mpic++
CXX_FLAGS := $(INC_FLAGS) -MMD -MP -Wall -Wextra -std=c++17 -O3
LDFLAGS := `$(CXX) --showme:link` -L$(LIB_DIR) -l$(LIB_NAME) -Wl,-rpath,\$$ORIGIN/../${LIB_DIR}

ADIOS = /opt/adios2
ADIOS_FLAGS = -isystem ${ADIOS}/include
ADIOS_LIBS = `${ADIOS}/bin/adios2-config --cxx-libs`

CURL_FLAGS = `curl-config --libs`

#PS_FLAGS = -D_USE_MPINET
PS_FLAGS = -D_USE_ZMQNET

TARGET_LIB := lib/lib${LIB_NAME}.so
EXEC_SRCS := $(wildcard *.cpp)
EXEC_TARS := $(patsubst %.cpp,bin/%,$(EXEC_SRCS))
SRCS = $(shell find $(SRC_DIRS) -name "*.cpp")
OBJS = $(SRCS:%.cpp=$(BUILD_DIR)/%.o)
DEPS = $(OBJS:.o=.d)

TEST_SIMPLE := test/mainSimple
TEST_SIMPLE_SRCS := test/mainSimple.cpp test/test_utils.cpp test/test_param.cpp

TEST_MPINET := test/mainNet
TEST_MPINET_SRCS := test/mainNet.cpp test/test_net.cpp

TEST_AD := test/mainAd
TEST_AD_SRCS := test/mainAD.cpp test/test_ad.cpp

all: $(EXEC_TARS) $(TARGET_LIB) $(TEST_SIMPLE) $(TEST_MPINET) $(TEST_AD)
	cp ws_flask_stat.py bin

debugvars:
	@echo $(SRCS)
	@echo $(OBJS)
	@echo $(EXEC_SRCS)
	@echo $(EXEC_TARS)
	@echo $(TARGET_LIB)

bin/%: build/%.o | $(TARGET_LIB)
	$(MKDIR_P) $(@D)
	$(CXX) $(CXX_FLAGS) $(PS_FLAGS) $< $(LDFLAGS) $(ADIOS_LIBS) $(CURL_FLAGS) -lzmq -o $@ 

$(TEST_SIMPLE): $(TEST_SIMPLE_SRCS) | $(TARGET_LIB) 
	$(CXX) $(CXX_FLAGS) $(PS_FLAGS) $^ $(LDFLAGS) $(ADIOS_LIBS) $(CURL_FLAGS) -lgtest -o $@ 

$(TEST_MPINET): $(TEST_MPINET_SRCS) | $(TARGET_LIB)
	$(CXX) $(CXX_FLAGS) $(PS_FLAGS) $^ $(LDFLAGS) $(ADIOS_LIBS) $(CURL_FLAGS) -lgtest -o $@ 

$(TEST_AD): $(TEST_AD_SRCS) | $(TARGET_LIB)
	$(CXX) $(CXX_FLAGS) $(PS_FLAGS) $^ $(LDFLAGS) $(ADIOS_LIBS) $(CURL_FLAGS) $(ADIOS_FLAGS) -lgtest -o $@ 

lib/lib$(LIB_NAME).so: $(OBJS)
	$(MKDIR_P) $(@D)
	$(CXX) $(CXX_FLAGS) $(PS_FLAGS) $^ -shared -Wl,-soname,lib$(LIB_NAME).so -o $@

$(BUILD_DIR)/%.o: %.cpp
	$(MKDIR_P) $(@D)
	$(CXX) -c -fPIC $(CXX_FLAGS) $(PS_FLAGS) $< -o $@ $(ADIOS_FLAGS) $(CURL_FLAGS)

test/%.o: %.cpp
	$(CXX) -c $(CXX_FLAGS) $(PS_FLAGS) $< -o $@ $(ADIOS_FLAGS) $(CURL_FLAGS)


.PHONY: clean

clean:
	$(RM) -r $(BUILD_DIR) $(BIN_DIR) $(LIB_DIR)
	$(RM) $(TEST_AD) $(TEST_SIMPLE) $(TEST_MPINET) test/*.d test/*.txt
	$(RM) -rf scripts/docker/test

-include $(DEPS)

MKDIR_P ?= mkdir -p

RM ?= rm
