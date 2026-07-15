// csv_reader.hpp
// CSV 读取器头文件：定义 CSV 异常类型与 CSVReader 类。
// 仅依赖 C++ 标准库，符合 C++11 标准，可在 Linux g++ 下编译。

#ifndef CSV_READER_HPP
#define CSV_READER_HPP

#include <string>
#include <vector>
#include <stdexcept>
#include <fstream>

// CSV 自定义异常：用于统一表达文件打开失败、路径非法等错误。
class CsvException : public std::runtime_error {
public:
    explicit CsvException(const std::string& message)
        : std::runtime_error(message) {}
};

// CSV 读取器：逐行读取 CSV 文件，并解析标准 CSV 转义规则。
// 支持字段内包含逗号、双引号（用两个双引号 "" 转义）以及换行。
class CSVReader {
public:
    // 构造时打开文件。
    // 若路径为空或包含非法字符，抛出 CsvException（路径非法）。
    // 若文件无法打开，抛出 CsvException（打开失败）。
    explicit CSVReader(const std::string& filePath);

    // 析构时关闭文件流。
    ~CSVReader();

    // 读取下一行并解析为字段列表。
    // 成功读取到一行返回 true，并将结果写入 outRow；
    // 到达文件末尾返回 false。
    bool readRow(std::vector<std::string>& outRow);

    // 判断文件是否已成功打开且处于可读状态。
    bool isOpen() const;

private:
    // 校验路径是否合法（非空且不含控制字符）。
    static void validatePath(const std::string& filePath);

    std::string filePath_;   // 记录文件路径，便于异常信息输出
    std::ifstream file_;     // 文件输入流
};

#endif // CSV_READER_HPP
