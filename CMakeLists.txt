cmake_minimum_required(VERSION 3.5)

# 项目名称与 C++ 标准（C++11）。
project(cpp_csv_tool CXX)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 头文件搜索路径。
include_directories(${PROJECT_SOURCE_DIR}/include)

# 源文件列表。
set(SOURCES
    src/csv_reader.cpp
    src/main.cpp
)

# 生成可执行文件。
add_executable(csv_tool ${SOURCES})
