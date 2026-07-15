// csv_reader.hpp
// CSV 读取/写出工具头文件：定义统一异常体系、Row 单行结构体、CSVReader 与 CsvWriter。
// 设计要点：
//  - 仅依赖 C++ 标准库，符合 C++11 标准，可在 Linux g++ 下编译。
//  - 逐行流式读取（状态机 + 二进制模式），不一次性加载整个文件，适合大文件。
//  - 统一异常体系，区分 IO / 格式 / 行列不匹配 / 编码 / 参数 等错误类别。
//  - Row 通过 shared_ptr 共享表头映射，避免悬空指针，无手动内存管理。

#ifndef CSV_READER_HPP
#define CSV_READER_HPP

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <stdexcept>
#include <fstream>

// ===================== 统一异常体系 =====================

// 所有 CSV 异常的基类，携带“错误类别”便于统一处理与打印。
class CsvException : public std::runtime_error {
public:
    explicit CsvException(const std::string& message,
                          const std::string& category = "CSV错误")
        : std::runtime_error(message), category_(category) {}

    // 错误类别（如 IO错误 / 格式错误 / 行列不匹配 / 编码异常 / 参数错误）。
    const std::string& category() const { return category_; }

private:
    std::string category_;
};

// 文件打开/写入/路径等 IO 相关错误。
class CsvIoException : public CsvException {
public:
    explicit CsvIoException(const std::string& m) : CsvException(m, "IO错误") {}
};

// 非法 CSV 格式 / 数值解析失败等格式错误。
class CsvFormatException : public CsvException {
public:
    explicit CsvFormatException(const std::string& m) : CsvException(m, "格式错误") {}
};

// 数据行列数与表头不一致。
class CsvColumnMismatchException : public CsvException {
public:
    explicit CsvColumnMismatchException(const std::string& m)
        : CsvException(m, "行列不匹配") {}
};

// 字段含非法 UTF-8 编码。
class CsvEncodingException : public CsvException {
public:
    explicit CsvEncodingException(const std::string& m) : CsvException(m, "编码异常") {}
};

// 命令行参数错误。
class CsvArgumentException : public CsvException {
public:
    explicit CsvArgumentException(const std::string& m) : CsvException(m, "参数错误") {}
};

// ===================== 单行数据结构 =====================

// 单行数据：保存本行各字段取值，并共享表头映射以支持“按列名取值”。
struct Row {
    using HeaderMap = std::map<std::string, std::size_t>;

    // 共享的“列名 -> 列索引”映射表（由 CSVReader 持有，Row 仅引用，避免拷贝与悬空）。
    std::shared_ptr<const HeaderMap> header;

    // 本行各字段内容（顺序与 CSV 列一致）。
    std::vector<std::string> values;

    Row() {}
    explicit Row(std::shared_ptr<const HeaderMap> h) : header(std::move(h)) {}

    // 字段个数。
    std::size_t size() const { return values.size(); }

    // 按列索引获取字段；索引越界抛 CsvException。
    const std::string& get(std::size_t index) const;

    // 按列名获取字段；未建立表头或列名不存在时抛异常。
    const std::string& get(const std::string& colName) const;
};

// ===================== CSV 读取器 =====================

// 逐行流式读取 CSV：状态机按字符解析，支持字段内逗号/双引号/换行转义，
// 并在读取时完成编码校验与行列一致性校验。
class CSVReader {
public:
    using HeaderMap = Row::HeaderMap;

    // 构造时以二进制模式打开文件；路径非法或打开失败抛 CsvIoException。
    explicit CSVReader(const std::string& filePath);

    // 析构时关闭文件流（RAII，无手动资源释放）。
    ~CSVReader();

    // 读取第一行作为表头，建立“列名 -> 列索引”映射表，并去除可能的 UTF-8 BOM。
    // 读取成功（至少读到一行）返回 true；文件为空返回 false。
    bool readHeader();

    // 流式读取下一行数据填充 row（不含表头行）。到达文件末尾返回 false。
    // 读取过程中会校验 UTF-8 编码与行列数一致性，异常时抛出对应 CsvException。
    bool readRow(Row& row);

    // 获取表头映射表（未调用 readHeader 时为空）。
    const HeaderMap& headers() const;

    // 按列索引顺序返回表头列名列表（用于导出时保留原表头）。
    std::vector<std::string> headerFields() const;

    // 表头列数（未读表头时为 0）。
    std::size_t columnCount() const { return headerColCount_; }

    // 文件是否已成功打开且可读。
    bool isOpen() const;

    // 判断一行是否为有效数据行：至少含一个字段且所有字段均非空。
    static bool isRowValid(const Row& row);

    // 任意列包含关键词（子串，区分大小写）即匹配。
    static bool rowMatchesKeyword(const Row& row, const std::string& keyword);

    // 严格解析数值字段（整串须为合法数字，否则抛 CsvFormatException）。
    static double parseDouble(const std::string& raw, std::size_t rowIdx,
                              const std::string& colName);

private:
    // 核心状态机：读取一行（含字段内换行）并拆分为字段列表。
    bool parseNextRow(std::vector<std::string>& outFields);

    // 去除字符串首尾空白字符（空格、制表符等）。
    static std::string trim(const std::string& s);

    // 校验路径是否合法（非空且不含控制字符），非法抛 CsvIoException。
    static void validatePath(const std::string& filePath);

    // 校验字符串是否为合法 UTF-8。
    static bool isValidUtf8(const std::string& s);

    // 去除行首 UTF-8 BOM（EF BB BF）。
    static void stripBom(std::string& s);

    std::string filePath_;                     // 记录文件路径，便于异常信息
    std::ifstream file_;                       // 文件输入流（RAII）
    std::shared_ptr<HeaderMap> headerMap_;     // 列名 -> 列索引（共享所有权）
    std::size_t headerColCount_;               // 表头列数
    bool headerRead_;                          // 是否已读取过表头
    std::size_t dataRowIndex_;                 // 已读取的数据行计数（从 1 起）
};

// ===================== CSV 写出器 =====================

// 将表头与数据行按标准 CSV 规则转义后写入文件。
// 字段含逗号、双引号、换行或首尾空白时自动加双引号包裹，
// 字段内双引号转义为两个双引号（""）。
class CsvWriter {
public:
    // 构造时打开输出文件；打开失败（路径非法/无写权限）抛 CsvIoException。
    explicit CsvWriter(const std::string& filePath);

    // 析构时关闭文件流（RAII）。
    ~CsvWriter();

    // 写入表头（一列名一行，逗号分隔）。
    void writeHeader(const std::vector<std::string>& header);

    // 写入一行（字段列表），自动处理引号转义。
    void writeRow(const std::vector<std::string>& fields);

    // 写入一行（来自 Row 结构体，使用其 values）。
    void writeRow(const Row& row);

    // 文件是否已成功打开且可写。
    bool isOpen() const;

private:
    // 按 CSV 规则转义单个字段。
    static std::string escapeField(const std::string& field);

    // 校验输出路径是否合法（非空且不含控制字符），非法抛 CsvIoException。
    static void validatePath(const std::string& filePath);

    std::string filePath_;   // 记录输出路径，便于异常信息
    std::ofstream file_;     // 文件输出流（RAII）
};

#endif // CSV_READER_HPP
