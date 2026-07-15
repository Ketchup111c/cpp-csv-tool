// csv_reader.cpp
// CSV 读取器实现：文件打开、路径校验与标准 CSV 转义解析。

#include "csv_reader.hpp"

#include <cctype>

CSVReader::CSVReader(const std::string& filePath)
    : filePath_(filePath) {
    // 先校验路径合法性，再尝试打开文件，错误信息更明确。
    validatePath(filePath_);
    file_.open(filePath_, std::ios::in);
    if (!file_.is_open()) {
        throw CsvException("无法打开文件（可能不存在或无访问权限）: " + filePath_);
    }
}

CSVReader::~CSVReader() {
    if (file_.is_open()) {
        file_.close();
    }
}

bool CSVReader::isOpen() const {
    return file_.is_open();
}

void CSVReader::validatePath(const std::string& filePath) {
    // 路径为空视为非法。
    if (filePath.empty()) {
        throw CsvException("文件路径为空（路径非法）");
    }
    // 路径中包含 ASCII 控制字符（如 '\0'）视为非法。
    for (char c : filePath) {
        if (std::iscntrl(static_cast<unsigned char>(c))) {
            throw CsvException("文件路径包含非法控制字符: " + filePath);
        }
    }
}

bool CSVReader::readRow(std::vector<std::string>& outRow) {
    outRow.clear();

    // 已到达文件末尾时直接返回 false。
    if (file_.eof()) {
        return false;
    }

    // 状态机逐字符解析一行（含字段内换行），直接生成字段列表。
    // 状态：inQuotes 表示当前处于引号字段内。
    bool inQuotes = false;
    bool rowStarted = false;
    std::string field;
    char ch = 0;

    while (file_.get(ch)) {
        rowStarted = true;

        if (inQuotes) {
            if (ch == '"') {
                // 引号字段内：若后为另一个引号，则为转义引号 ""。
                if (file_.peek() == '"') {
                    field.push_back('"'); // 写入单个引号
                    file_.get(ch);        // 消费第二个引号
                } else {
                    inQuotes = false;     // 否则为字段结束引号
                }
            } else {
                field.push_back(ch);      // 引号内字符原样保留（含换行）
            }
        } else {
            if (ch == '"') {
                inQuotes = true;          // 进入引号字段
            } else if (ch == ',') {
                outRow.push_back(field);  // 分隔符：结束当前字段
                field.clear();
            } else if (ch == '\n') {
                break;                    // 普通换行：一行结束
            } else if (ch == '\r') {
                // 兼容 Windows 行尾 \r\n：忽略 \r，遇 \n 结束。
                if (file_.peek() == '\n') {
                    file_.get(ch);
                }
                break;
            } else {
                field.push_back(ch);
            }
        }
    }

    // 空文件或末尾无内容时返回 false。
    if (!rowStarted && file_.eof()) {
        return false;
    }

    // 收尾最后一个字段（含未闭合引号的情况，保持容错）。
    outRow.push_back(field);
    return true;
}
