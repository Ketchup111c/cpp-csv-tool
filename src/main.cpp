// main.cpp
// CSV 工具命令行入口：支持 -f 输入文件、-col 数值列、-filter 筛选关键词、
// -out 导出文件、-h 帮助。采用流式逐行处理，适配大文件；统一全局异常处理。

#include "csv_reader.hpp"

#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {

// 命令行参数。
struct Args {
    std::string input;   // -f 输入文件（必填）
    std::string col;     // -col 数值统计列
    std::string filter;  // -filter 筛选关键词
    std::string out;     // -out 导出文件
    bool hasCol = false;
    bool hasFilter = false;
    bool hasOut = false;
    bool help = false;
};

// 打印帮助文档。
void printHelp() {
    std::cout
        << "CSV 工具 - 读取 / 清洗 / 统计 / 筛选 / 导出（流式处理大文件）" << std::endl
        << std::endl
        << "用法:" << std::endl
        << "  csv_tool -f <输入CSV> [-col <数值列名>] [-filter <关键词>] "
           "[-out <导出CSV>] [-h]"
        << std::endl
        << std::endl
        << "参数:" << std::endl
        << "  -f <path>      必填，原始 CSV 文件路径" << std::endl
        << "  -col <name>    可选，指定用于求和/均值的数值列名" << std::endl
        << "  -filter <kw>   可选，按关键词筛选（任意列包含即匹配，区分大小写）" << std::endl
        << "  -out <path>    可选，将结果（筛选后或清洗后）导出为 CSV" << std::endl
        << "  -h             打印本帮助文档" << std::endl
        << std::endl
        << "示例:" << std::endl
        << "  csv_tool -f data.csv -col id -filter e -out result.csv" << std::endl;
}

// 解析命令行参数；出错抛 CsvArgumentException。
void parseArgs(int argc, char* argv[], Args& args) {
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto needValue = [&](const char* name) -> std::string {
            if (i + 1 >= argc) {
                throw CsvArgumentException(std::string("参数 ") + name +
                                          " 缺少取值");
            }
            return std::string(argv[++i]);
        };

        if (a == "-h" || a == "--help") {
            args.help = true;
        } else if (a == "-f") {
            args.input = needValue("-f");
        } else if (a == "-col") {
            args.col = needValue("-col");
            args.hasCol = true;
        } else if (a == "-filter") {
            args.filter = needValue("-filter");
            args.hasFilter = true;
        } else if (a == "-out") {
            args.out = needValue("-out");
            args.hasOut = true;
        } else {
            throw CsvArgumentException("未知参数: " + a);
        }
    }
}

// 核心处理流程（已处于参数的 try 保护内）。
void runTool(const Args& args) {
    CSVReader reader(args.input);

    if (!reader.readHeader()) {
        std::cerr << "文件为空或无表头，结束。" << std::endl;
        return;
    }

    std::cout << "表头（列名 -> 列索引）:" << std::endl;
    for (const auto& kv : reader.headers()) {
        std::cout << "  [" << kv.second << "] " << kv.first << std::endl;
    }
    std::cout << std::endl;

    // 可选的输出文件写出器（流式写出，避免内存堆积）。
    std::unique_ptr<CsvWriter> writer;
    if (args.hasOut) {
        writer.reset(new CsvWriter(args.out));
        writer->writeHeader(reader.headerFields()); // 保留原表头
    }

    // 流式逐行处理：每行即时清洗/筛选/统计/导出，不缓存全部内容。
    // readRow 会自动将 Row 绑定到 reader 共享的表头映射。
    Row row;
    std::size_t kept = 0;       // 保留（有效且匹配）的行数
    std::size_t dropped = 0;    // 被过滤（空行/空字段/不匹配）的行数
    double sum = 0.0;
    std::size_t statCount = 0;  // 参与统计的行数

    while (reader.readRow(row)) {
        bool valid = CSVReader::isRowValid(row);
        bool match = !args.hasFilter ||
                     CSVReader::rowMatchesKeyword(row, args.filter);

        if (valid && match) {
            ++kept;
            if (writer) {
                writer->writeRow(row);
            }
            if (args.hasCol) {
                double v = CSVReader::parseDouble(row.get(args.col), kept,
                                                  args.col);
                sum += v;
                ++statCount;
            }
        } else {
            ++dropped;
        }
    }

    // 打印清洗与筛选结果。
    std::cout << "有效并匹配行数: " << kept << std::endl;
    std::cout << "被过滤的无效/不匹配行数: " << dropped << std::endl;

    if (args.hasCol) {
        double avg = (statCount > 0) ? (sum / static_cast<double>(statCount)) : 0.0;
        std::cout << std::endl
                  << "数值统计（列 \"" << args.col << "\"）:" << std::endl;
        std::cout << "  行数 = " << statCount << std::endl;
        std::cout << "  总和 = " << std::fixed << std::setprecision(2) << sum
                  << std::endl;
        std::cout << "  平均值 = " << std::setprecision(4) << avg << std::endl;
    }

    if (writer) {
        std::cout << std::endl << "已导出结果至: " << args.out << std::endl;
    }
}

} // namespace

int main(int argc, char* argv[]) {
    // 统一全局异常处理：所有 CSV 错误集中捕获并按类别友好提示。
    try {
        Args args;
        parseArgs(argc, argv, args);

        if (args.help) {
            printHelp();
            return 0;
        }
        if (args.input.empty()) {
            throw CsvArgumentException("缺少必填参数 -f <CSV文件路径>");
        }

        runTool(args);
        return 0;
    } catch (const CsvException& e) {
        std::cerr << "[CSV " << e.category() << "] " << e.what() << std::endl;
        // 参数类错误附带帮助文档。
        if (e.category() == "参数错误") {
            printHelp();
        }
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "[标准错误] " << e.what() << std::endl;
        return 2;
    }
}
