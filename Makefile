# 简单 Makefile，兼容 Linux g++，使用 C++11 标准。
CXX      ?= g++
CXXFLAGS ?= -std=c++11 -Wall -Wextra -O2
INCLUDES  = -Iinclude
TARGET    = csv_tool
SRCS      = src/csv_reader.cpp src/main.cpp
OBJS      = $(SRCS:.cpp=.o)

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

run: $(TARGET)
	./$(TARGET) example.csv

clean:
	rm -f $(OBJS) $(TARGET)
