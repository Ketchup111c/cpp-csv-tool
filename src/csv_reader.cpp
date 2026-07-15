// csv_reader.cpp
// CSV 读取器实现：文件打开、路径校验、表头解析、按列名取值与标准 CSV 转义解析。

#include "csv_reader.hpp"

#include <cctype>

CSVReader::CSVReader(const std::string& filePath)
    : filePath_(filePath), headerRead_(false) {
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

const std::map<std::string, std::size_t>& CSVReader::headers() const {
    return headerMap_;
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

std::string CSVReader::trim(const std::string& s) {
    std::size_t begin = 0;
    std::size_t end = s.size();
    // 跳过首部空白字符。
    while (begin < end && std::isspace(static_cast<unsigned char>(s[begin]))) {
        ++begin;
    }
    // 跳过尾部空白字符。
    while (end > begin && std::isspace(static_cast<unsigned char>(s[end - 1]))) {
        --end;
    }
    return s.substr(begin, end - begin);
}

bool CSVReader::readHeader() {
    // 已读取过表头则直接返回（避免重复读取导致跳过数据行）。
    if (headerRead_) {
        return !headerMap_.empty();
    }

    std::vector<std::string> fields;
    if (!parseNextRow(fields)) {
        headerRead_ = true;
        return false; // 文件为空，无表头
    }

    // 建立“列名 -> 列索引”映射（重复列名以最后一次出现为准）。
    headerMap_.clear();
    for (std::size_t i = 0; i < fields.size(); ++i) {
        headerMap_[fields[i]] = i;
    }
    headerRead_ = true;
    return true;
}

bool CSVReader::readRow(Row& row) {
    // 绑定表头映射并清空旧数据，再读取下一行。
    row.header = &headerMap_;
    row.values.clear();
    return parseNextRow(row.values);
}

bool CSVReader::isRowValid(const Row& row) {
    // 无字段（如空行）视为无效。
    if (row.values.empty()) {
        return false;
    }
    // 任一字段为空（缺失值）视为脏数据，无效。
    for (std::size_t i = 0; i < row.values.size(); ++i) {
        if (row.values[i].empty()) {
            return false;
        }
    }
    return true;
}

std::size_t CSVReader::readCleanRows(std::vector<Row>& validRows,
                                     std::size_t& filteredCount) {
    validRows.clear();
    filteredCount = 0;

    Row row;
    while (readRow(row)) {
        // 有效行保留，脏数据行（全空或含空字段）计入过滤数。
        if (isRowValid(row)) {
            validRows.push_back(row);
        } else {
            ++filteredCount;
        }
    }
    return validRows.size();
}

bool CSVReader::computeColumnStats(const std::string& colName,
                                   const std::vector<Row>& rows,
                                   double& outSum, double& outAvg) const {
    // 校验列名是否在表头中存在。
    if (headerMap_.find(colName) == headerMap_.end()) {
        throw CsvException("列名不存在，无法统计: " + colName);
    }
    // 无有效数据时不允许统计，避免对空数据做除法。
    if (rows.empty()) {
        throw CsvException("无有效数据，无法统计列: " + colName);
    }

    double sum = 0.0;
    std::size_t numericCount = 0;

    for (std::size_t i = 0; i < rows.size(); ++i) {
        const std::string& raw = rows[i].get(colName);

        std::size_t pos = 0;
        double val = 0.0;
        try {
            // 严格解析：stod 可能因非数字内容抛 invalid_argument / out_of_range。
            val = std::stod(raw, &pos);
        } catch (const std::exception& e) {
            throw CsvException("第 " + std::to_string(i + 1) +
                               " 行数值解析失败（非数字/乱码字段）[" +
                               colName + "] = \"" + raw + "\": " + e.what());
        }
        // 若整串未被完全消费（如 "12abc"），视为含有非数值内容。
        if (pos != raw.size()) {
            throw CsvException("第 " + std::to_string(i + 1) +
                               " 行含非数值内容 [" + colName +
                               "] = \"" + raw + "\"");
        }

        sum += val;
        ++numericCount;
    }

    outSum = sum;
    outAvg = (numericCount > 0)
                 ? (sum / static_cast<double>(numericCount))
                 : 0.0;
    return true;
}

void CSVReader::filterRows(const std::vector<Row>& src,
                           const std::string& colName,
                           const std::string& keyword,
                           std::vector<Row>& out) {
    out.clear();
    for (std::size_t i = 0; i < src.size(); ++i) {
        // 列名不存在时 Row::get 会抛出 CsvException。
        const std::string& val = src[i].get(colName);
        // 子串包含匹配（区分大小写）。
        if (val.find(keyword) != std::string::npos) {
            out.push_back(src[i]);
        }
    }
}

std::vector<std::string> CSVReader::headerFields() const {
    std::vector<std::string> out;
    if (headerMap_.empty()) {
        return out;
    }
    // 依列索引顺序还原表头列名。
    std::size_t maxIdx = 0;
    for (const auto& kv : headerMap_) {
        if (kv.second > maxIdx) {
            maxIdx = kv.second;
        }
    }
    out.resize(maxIdx + 1);
    for (const auto& kv : headerMap_) {
        out[kv.second] = kv.first;
    }
    return out;
}

bool CSVReader::parseNextRow(std::vector<std::string>& outFields) {
    outFields.clear();

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
                outFields.push_back(trim(field)); // 分隔符：收尾并清除首尾空格
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
    if (header == nullptr) {
        throw CsvException("尚未建立表头，无法按列名取值");
    }
    auto it = header->find(colName);
    if (it == header->end()) {
        throw CsvException("列名不存在: " + colName);
    }
    if (it->second >= values.size()) {
        throw CsvException("列数据缺失: " + colName);
    }
    return values[it->second];
}

// ===================== CsvWriter 实现 =====================

CsvWriter::CsvWriter(const std::string& filePath)
    : filePath_(filePath) {
    // 先校验输出路径合法性，再尝试打开文件，错误信息更明确。
    validatePath(filePath_);
    file_.open(filePath_, std::ios::out | std::ios::trunc);
    if (!file_.is_open()) {
        throw CsvException("无法创建/写入文件（路径非法或无写权限）: " + filePath_);
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
        throw CsvException("输出文件路径为空（路径非法）");
    }
    for (char c : filePath) {
        if (std::iscntrl(static_cast<unsigned char>(c))) {
            throw CsvException("输出文件路径包含非法控制字符: " + filePath);
        }
    }
}

std::string CsvWriter::escapeField(const std::string& field) {
    // 含逗号、双引号、换行之一，或含首尾空白时，需要加双引号包裹。
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
            throw CsvException("写入文件失败: " + filePath_);
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
    // 写入后检查流状态，捕获磁盘满等写入异常。
    if (!file_.good()) {
        throw CsvException("写入文件失败: " + filePath_);
    }
}

void CsvWriter::writeRow(const Row& row) {
    writeRow(row.values);
}
