CXX ?= clang++
SRC := ../firmware/badge-pet/src
CXXFLAGS := -std=c++17 -O1 -g -Wall -Wextra -Werror -I$(SRC) -isystem third_party
PURE := $(wildcard $(SRC)/pet/*.cpp) $(SRC)/net/Protocol.cpp $(SRC)/input/Gesture.cpp $(SRC)/input/Shake.cpp $(SRC)/input/ButtonArbiter.cpp
TESTS := $(wildcard test_*.cpp)
build/run_tests: $(TESTS) $(wildcard $(PURE)) $(wildcard $(SRC)/pet/*.h) $(wildcard $(SRC)/input/*.h)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) $(TESTS) $(wildcard $(PURE)) -o $@
test: build/run_tests
	./build/run_tests
.PHONY: test
