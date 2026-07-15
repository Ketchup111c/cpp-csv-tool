// csv_reader.hpp
// CSV 读取器头文件：定义 CSV 异常类型、Row 单行结构体与 CSVReader 类。
// 仅依赖 C++ 标准库，符合 C++11 标准，可在 Linux g++ 下编译。

#ifndef CSV_READER_HPP
#define CSV_READER_HPP

#include <string>
#include <vector>
#include <map>
#include <stdexcept>
#include <fstream>

// CSV 自定义异常：用于统一表达文件打开失败、路径非法、列名不存在等错误。
class CsvException : public std::runtime_error {
public:
    explicit CsvException(const std::string& message)
        : std::runtime_error(message) {}
};

// 单行数据：保存本行各字段取值，并通过表头映射支持“按列名取值”。
struct Row {
    // 指向 CSVReader 维护的“列名 -> 列索引”映射表（不拥有该资源）。
    const std::map<std::string, std::size_t>* header;

    // 本行各字段内容（顺序与 CSV 列一致）。
    std::vector<std::string> values;

    // 构造时绑定表头映射（可为 nullptr，此时按列名取值会抛异常）。
    Row() : header(nullptr) {}
    explicit Row(const std::map<std::string, std::size_t>* h) : header(h) {}

    // 字段个数。
    std::size_t size() const { return values.size(); }

    // 按列索引获取字段；索引越界抛 CsvException。
    const std::string& get(std::size_t index) const;

    // 按列名获取字段；未建立表头或列名不存在时抛 CsvException。
    const std::string& get(const std::string& colName) const;
};

// CSV 读取器：逐行读取 CSV 文件，解析标准 CSV 转义规则，
// 支持将首行作为表头建立列名映射，并清除字段首尾空白。
class CSVReader {
public:
    // 构造时打开文件。
    // 若路径为空或包含非法字符，抛出 CsvException（路径非法）。
    // 若文件无法打开，抛出 CsvException（打开失败）。
    explicit CSVReader(const std::string& filePath);

    // 析构时关闭文件流。
    ~CSVReader();

    // 读取第一行作为表头，建立“列名 -> 列索引”映射表。
    // 读取成功（至少读到一行）返回 true；文件为空返回 false。
    // 可重复调用：第二次起直接返回是否曾成功读取过表头。
    bool readHeader();

    // 读取下一行数据并填充到 row（不含表头行）。
    // 到达文件末尾返回 false。
    bool readRow(Row& row);

    // 读取并清洗剩余所有数据行：丢弃全空行及含空字段的脏数据行。
    // 有效行存入 validRows，被过滤的无效行数写入 filteredCount。
    // 返回有效行数量（validRows.size()）。
    std::size_t readCleanRows(std::vector<Row>& validRows, std::size_t& filteredCount);

    // 判断一行是否为有效数据行：至少含一个字段且所有字段均非空。
    static bool isRowValid(const Row& row);

    // 获取表头映射表（未调用 readHeader 时为空）。
    const std::map<std::string, std::size_t>& headers() const;

    // 判断文件是否已成功打开且处于可读状态。
    bool isOpen() const;

private:
    // 核心状态机：读取一行（含字段内换行）并拆分为字段列表。
    bool parseNextRow(std::vector<std::string>& outFields);

    // 去除字符串首尾空白字符（空格、制表符等）。
    static std::string trim(const std::string& s);

    // 校验路径是否合法（非空且不含控制字符）。
    static void validatePath(const std::string& filePath);

    std::string filePath_;                          // 记录文件路径，便于异常信息
    std::ifstream file_;                            // 文件输入流
    std::map<std::string, std::size_t> headerMap_;  // 列名 -> 列索引
    bool headerRead_;                               // 是否已读取过表头
};

#endif // CSV_READER_HPP
