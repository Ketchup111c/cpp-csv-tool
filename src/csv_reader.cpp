// csv_reader.cpp
// CSV 读取/写出实现：文件打开、路径校验、UTF-8 与行列校验、表头解析、
// 按列名取值、流式统计/筛选原语，以及标准 CSV 引号转义写出。

#include "csv_reader.hpp"

#include <cctype>

// ===================== CSVReader 实现 =====================

CSVReader::CSVReader(const std::string& filePath)
    : filePath_(filePath),
      headerColCount_(0),
      headerRead_(false),
      dataRowIndex_(0) {
    validatePath(filePath_);
    // 以二进制模式打开，避免平台相关换行翻译干扰解析。
    headerMap_ = std::make_shared<HeaderMap>();
    file_.open(filePath_, std::ios::in | std::ios::binary);
    if (!file_.is_open()) {
        throw CsvIoException("无法打开文件（路径非法或无访问权限）: " + filePath_);
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

const CSVReader::HeaderMap& CSVReader::headers() const {
    return *headerMap_;
}

void CSVReader::validatePath(const std::string& filePath) {
    if (filePath.empty()) {
        throw CsvIoException("文件路径为空（路径非法）");
    }
    for (char c : filePath) {
        if (std::iscntrl(static_cast<unsigned char>(c))) {
            throw CsvIoException("文件路径包含非法控制字符: " + filePath);
        }
    }
}

std::string CSVReader::trim(const std::string& s) {
    std::size_t begin = 0;
    std::size_t end = s.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(s[begin]))) {
        ++begin;
    }
    while (end > begin && std::isspace(static_cast<unsigned char>(s[end - 1]))) {
        --end;
    }
    return s.substr(begin, end - begin);
}

bool CSVReader::isValidUtf8(const std::string& s) {
    std::size_t i = 0;
    const std::size_t n = s.size();
    while (i < n) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (c < 0x80) {
            ++i; // ASCII
        } else if ((c >> 5) == 0x06) { // 110xxxxx，2 字节
            if (i + 1 >= n || (s[i + 1] & 0xC0) != 0x80) return false;
            i += 2;
        } else if ((c >> 4) == 0x0E) { // 1110xxxx，3 字节
            if (i + 2 >= n || (s[i + 1] & 0xC0) != 0x80 ||
                (s[i + 2] & 0xC0) != 0x80) {
                return false;
            }
            i += 3;
        } else if ((c >> 3) == 0x1E) { // 11110xxx，4 字节
            if (i + 3 >= n || (s[i + 1] & 0xC0) != 0x80 ||
                (s[i + 2] & 0xC0) != 0x80 || (s[i + 3] & 0xC0) != 0x80) {
                return false;
            }
            i += 4;
        } else {
            return false; // 非法前导字节
        }
    }
    return true;
}

void CSVReader::stripBom(std::string& s) {
    const char bom[3] = {'\xEF', '\xBB', '\xBF'};
    if (s.size() >= 3 && s[0] == bom[0] && s[1] == bom[1] && s[2] == bom[2]) {
        s.erase(0, 3);
    }
}

bool CSVReader::readHeader() {
    if (headerRead_) {
        return !headerMap_->empty();
    }
    std::vector<std::string> fields;
    if (!parseNextRow(fields)) {
        headerRead_ = true;
        return false; // 文件为空，无表头
    }
    // 去除文件起始处的 UTF-8 BOM，避免污染首列表头名。
    if (!fields.empty()) {
        stripBom(fields[0]);
    }
    headerMap_->clear();
    for (std::size_t i = 0; i < fields.size(); ++i) {
        (*headerMap_)[fields[i]] = i;
    }
    headerColCount_ = fields.size();
    headerRead_ = true;
    return true;
}

bool CSVReader::readRow(Row& row) {
    // 绑定共享表头，清空旧数据，再流式读取下一行。
    row.header = headerMap_;
    row.values.clear();
    if (!parseNextRow(row.values)) {
        return false;
    }
    ++dataRowIndex_;

    // 编码校验：每个字段必须是合法 UTF-8。
    for (std::size_t i = 0; i < row.values.size(); ++i) {
        if (!isValidUtf8(row.values[i])) {
            throw CsvEncodingException(
                "第 " + std::to_string(dataRowIndex_) +
                " 行含非法 UTF-8 编码字段（索引 " + std::to_string(i) + "）");
        }
    }

    // 行列一致性校验：数据行列数须与表头一致。
    // 但完全空白行（无字段或全为空字段）视为无效脏数据，交由 isRowValid
    // 过滤，不按行列不匹配处理，以容忍文件中常见的空行。
    bool blank = true;
    for (std::size_t i = 0; i < row.values.size(); ++i) {
        if (!row.values[i].empty()) {
            blank = false;
            break;
        }
    }
    if (!blank && headerRead_ && headerColCount_ > 0 &&
        row.values.size() != headerColCount_) {
        throw CsvColumnMismatchException(
            "第 " + std::to_string(dataRowIndex_) + " 行列数不匹配：期望 " +
            std::to_string(headerColCount_) + " 列，实际 " +
            std::to_string(row.values.size()) + " 列");
    }
    return true;
}

bool CSVReader::isRowValid(const Row& row) {
    if (row.values.empty()) {
        return false;
    }
    for (std::size_t i = 0; i < row.values.size(); ++i) {
        if (row.values[i].empty()) {
            return false;
        }
    }
    return true;
}

bool CSVReader::rowMatchesKeyword(const Row& row, const std::string& keyword) {
    for (std::size_t i = 0; i < row.values.size(); ++i) {
        if (row.values[i].find(keyword) != std::string::npos) {
            return true;
        }
    }
    return false;
}

double CSVReader::parseDouble(const std::string& raw, std::size_t rowIdx,
                              const std::string& colName) {
    std::size_t pos = 0;
    double val = 0.0;
    try {
        // stod 可能因非数字内容抛 invalid_argument / out_of_range。
        val = std::stod(raw, &pos);
    } catch (const std::exception& e) {
        throw CsvFormatException(
            "第 " + std::to_string(rowIdx) + " 行数值解析失败（非数字/乱码字段）[" +
            colName + "] = \"" + raw + "\": " + e.what());
    }
    // 整串未被完全消费（如 "12abc"），视为含非数值内容。
    if (pos != raw.size()) {
        throw CsvFormatException("第 " + std::to_string(rowIdx) +
                                 " 行含非数值内容 [" + colName + "] = \"" +
                                 raw + "\"");
    }
    return val;
}

std::vector<std::string> CSVReader::headerFields() const {
    std::vector<std::string> out;
    if (headerMap_->empty()) {
        return out;
    }
    std::size_t maxIdx = 0;
    for (const auto& kv : *headerMap_) {
        if (kv.second > maxIdx) {
            maxIdx = kv.second;
        }
    }
    out.resize(maxIdx + 1);
    for (const auto& kv : *headerMap_) {
        out[kv.second] = kv.first;
    }
    return out;
}

// 核心状态机：逐字符读取一行（含字段内换行），拆分为字段列表并 trim。
bool CSVReader::parseNextRow(std::vector<std::string>& outFields) {
    outFields.clear();
    if (file_.eof()) {
        return false;
    }

    bool inQuotes = false;
    bool rowStarted = false;
    std::string field;
    char ch = 0;

    while (file_.get(ch)) {
        rowStarted = true;
        if (inQuotes) {
            if (ch == '"') {
                if (file_.peek() == '"') {
                    field.push_back('"'); // 转义引号 ""
                    file_.get(ch);        // 消费第二个引号
                } else {
                    inQuotes = false;     // 字段结束引号
                }
            } else {
                field.push_back(ch);      // 引号内字符原样保留（含换行）
            }
        } else {
            if (ch == '"') {
                inQuotes = true;
            } else if (ch == ',') {
                outFields.push_back(trim(field));
                field.clear();
            } else if (ch == '\n') {
                break;
            } else if (ch == '\r') {
                if (file_.peek() == '\n') {
                    file_.get(ch);
                }
                break;
            } else {
                field.push_back(ch);
            }
        }
    }

    if (!rowStarted && file_.eof()) {
        return false;
    }
    outFields.push_back(trim(field));
    return true;
}

const std::string& Row::get(std::size_t index) const {
    if (index >= values.size()) {
        throw CsvException("列索引越界: " + std::to_string(index));
    }
    return values[index];
}

const std::string& Row::get(const std::string& colName) const {
    if (!header) {
        throw CsvException("尚未建立表头，无法按列名取值");
    }
    auto it = header->find(colName);
    if (it == header->end()) {
        throw CsvFormatException("列名不存在: " + colName);
    }
    if (it->second >= values.size()) {
        throw CsvColumnMismatchException("列数据缺失: " + colName);
    }
    return values[it->second];
}

// ===================== CsvWriter 实现 =====================

CsvWriter::CsvWriter(const std::string& filePath) : filePath_(filePath) {
    validatePath(filePath_);
    file_.open(filePath_, std::ios::out | std::ios::trunc);
    if (!file_.is_open()) {
        throw CsvIoException(
            "无法创建/写入文件（路径非法或无写权限）: " + filePath_);
    }
}

CsvWriter::~CsvWriter() {
    if (file_.is_open()) {
        file_.close();
    }
}

bool CsvWriter::isOpen() const {
    return file_.is_open();
}

void CsvWriter::validatePath(const std::string& filePath) {
    if (filePath.empty()) {
        throw CsvIoException("输出文件路径为空（路径非法）");
    }
    for (char c : filePath) {
        if (std::iscntrl(static_cast<unsigned char>(c))) {
            throw CsvIoException("输出文件路径包含非法控制字符: " + filePath);
        }
    }
}

std::string CsvWriter::escapeField(const std::string& field) {
    bool needQuote = false;
    for (char c : field) {
        if (c == ',' || c == '"' || c == '\n' || c == '\r') {
            needQuote = true;
            break;
        }
    }
    if (!field.empty()) {
        if (std::isspace(static_cast<unsigned char>(field.front())) ||
            std::isspace(static_cast<unsigned char>(field.back()))) {
            needQuote = true;
        }
    }
    if (!needQuote) {
        return field;
    }
    // 引号包裹，内部双引号转义为 ""。
    std::string out = "\"";
    for (char c : field) {
        if (c == '"') {
            out += "\"\"";
        } else {
            out += c;
        }
    }
    out += "\"";
    return out;
}

void CsvWriter::writeHeader(const std::vector<std::string>& header) {
    writeRow(header);
}

void CsvWriter::writeRow(const std::vector<std::string>& fields) {
    if (fields.empty()) {
        file_ << '\n';
        if (!file_.good()) {
            throw CsvIoException("写入文件失败: " + filePath_);
        }
        return;
    }
    for (std::size_t i = 0; i < fields.size(); ++i) {
        if (i > 0) {
            file_ << ',';
        }
        file_ << escapeField(fields[i]);
    }
    file_ << '\n';
    if (!file_.good()) {
        throw CsvIoException("写入文件失败: " + filePath_);
    }
}

void CsvWriter::writeRow(const Row& row) {
    writeRow(row.values);
}
