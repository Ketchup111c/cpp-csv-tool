// main.cpp
// CSV 工具命令行入口：支持 -f 输入文件、-col 数值列、-filter 筛选关键词、
// -out 导出文件、-h 帮助。校验必填参数，参数或运行错误均友好提示。

#include "csv_reader.hpp"

#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

// 打印帮助文档。
static void printHelp() {
    std::cout
        << "CSV 工具 - 读取 / 清洗 / 统计 / 筛选 / 导出" << std::endl
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

// 解析命令行参数；出错时抛出带说明的异常。
static void parseArgs(int argc, char* argv[], std::string& input, bool& hasCol,
                      std::string& col, bool& hasFilter, std::string& filter,
                      bool& hasOut, std::string& out, bool& help) {
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        // 取当前参数的取值；缺失则抛异常。
        auto needValue = [&](const char* name) -> std::string {
            if (i + 1 >= argc) {
                throw std::runtime_error(std::string("参数 ") + name +
                                         " 缺少取值");
            }
            return std::string(argv[++i]);
        };

        if (a == "-h" || a == "--help") {
            help = true;
        } else if (a == "-f") {
            input = needValue("-f");
        } else if (a == "-col") {
            col = needValue("-col");
            hasCol = true;
        } else if (a == "-filter") {
            filter = needValue("-filter");
            hasFilter = true;
        } else if (a == "-out") {
            out = needValue("-out");
            hasOut = true;
        } else {
            throw std::runtime_error("未知参数: " + a);
        }
    }
}

int main(int argc, char* argv[]) {
    std::string input, col, filter, out;
    bool hasCol = false, hasFilter = false, hasOut = false, help = false;

    // 1) 解析参数（参数层错误先友好提示并退出）。
    try {
        parseArgs(argc, argv, input, hasCol, col, hasFilter, filter, hasOut,
                  out, help);
    } catch (const std::exception& e) {
        std::cerr << "参数错误: " << e.what() << std::endl;
        printHelp();
        return 1;
    }

    if (help) {
        printHelp();
        return 0;
    }

    // 校验必填参数 -f。
    if (input.empty()) {
        std::cerr << "参数错误: 缺少必填参数 -f <CSV文件路径>" << std::endl;
        printHelp();
        return 1;
    }

    try {
        // 2) 打开并读取表头。
        CSVReader reader(input);
        if (!reader.readHeader()) {
            std::cerr << "文件为空，无法读取表头。" << std::endl;
            return 1;
        }
        std::cout << "表头（列名 -> 列索引）:" << std::endl;
        for (const auto& kv : reader.headers()) {
            std::cout << "  [" << kv.second << "] " << kv.first << std::endl;
        }
        std::cout << std::endl;

        // 3) 清洗：过滤全空行及含空字段的脏数据行。
        std::vector<Row> validRows;
        std::size_t filteredCount = 0;
        reader.readCleanRows(validRows, filteredCount);
        std::cout << "有效数据行数: " << validRows.size() << std::endl;
        std::cout << "被过滤的无效行数: " << filteredCount << std::endl;

        // 紧凑打印清洗后的有效数据，便于核对保留结果。
        for (std::size_t i = 0; i < validRows.size(); ++i) {
            std::cout << "  有效[" << (i + 1) << "] ";
            for (std::size_t c = 0; c < validRows[i].size(); ++c) {
                if (c > 0) std::cout << " | ";
                std::cout << validRows[i].get(c);
            }
            std::cout << std::endl;
        }
        std::cout << std::endl;

        // 4) 数值统计（可选 -col）。
        if (hasCol) {
            double sum = 0.0, avg = 0.0;
            reader.computeColumnStats(col, validRows, sum, avg);
            std::cout << "数值统计（列 \"" << col << "\"）:" << std::endl;
            std::cout << "  行数 = " << validRows.size() << std::endl;
            std::cout << "  总和 = " << std::fixed << std::setprecision(2) << sum
                      << std::endl;
            std::cout << "  平均值 = " << std::setprecision(4) << avg << std::endl;
            std::cout << std::endl;
        }

        // 5) 筛选（可选 -filter，任意列包含关键词即匹配）。
        std::vector<Row> result = validRows; // 默认导出清洗后的全部有效行
        if (hasFilter) {
            CSVReader::filterRowsAnyColumn(validRows, filter, result);
            std::cout << "筛选（任意列含关键词 \"" << filter << "\"）匹配 "
                      << result.size() << " 行。" << std::endl;
            std::cout << std::endl;
        }

        // 6) 导出（可选 -out）。CsvWriter 构造/写入失败会抛 CsvException。
        if (hasOut) {
            CsvWriter writer(out);
            writer.writeHeader(reader.headerFields()); // 保留原表头
            for (const auto& r : result) {
                writer.writeRow(r);
            }
            std::cout << "已导出结果至: " << out << std::endl;
        }
    } catch (const CsvException& e) {
        std::cerr << "CSV 错误: " << e.what() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "标准错误: " << e.what() << std::endl;
        return 2;
    }

    return 0;
}
