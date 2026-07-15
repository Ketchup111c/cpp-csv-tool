# 简单 Makefile，兼容 Linux g++，使用 C++11 标准。
CXX      ?= g++
CXXFLAGS ?= -std=c++11 -Wall -Wextra -O2
INCLUDES  = -Iinclude
TARGET    = csv_tool
TEST      = test_edge
SRCS      = src/csv_reader.cpp src/main.cpp
TEST_SRC  = tests/test_edge.cpp src/csv_reader.cpp
OBJS      = $(SRCS:.cpp=.o)
TEST_OBJS = $(TEST_SRC:.cpp=.o)

.PHONY: all clean run test

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(TEST): $(TEST_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

run: $(TARGET)
	./$(TARGET) example.csv

test: $(TEST)
	./$(TEST)

clean:
	rm -f $(OBJS) $(TEST_OBJS) $(TARGET) $(TEST)
