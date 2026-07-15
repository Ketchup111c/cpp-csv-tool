// main.cpp
// CSV 工具演示入口：读取表头、清洗脏数据、统计过滤行数，并演示数值列统计。

#include "csv_reader.hpp"

#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

int main(int argc, char* argv[]) {
    // 默认示例文件路径；可通过命令行参数指定。
    std::string path = "example.csv";
    if (argc > 1) {
        path = argv[1];
    }

    try {
        // 构造时即打开文件，路径非法或打开失败会抛出 CsvException。
        CSVReader reader(path);

        // 1) 读取第一行作为表头，建立列名 -> 列索引映射。
        if (!reader.readHeader()) {
            std::cerr << "文件为空，无法读取表头。" << std::endl;
            return 1;
        }
        std::cout << "表头（列名 -> 列索引）:" << std::endl;
        for (const auto& kv : reader.headers()) {
            std::cout << "  [" << kv.second << "] " << kv.first << std::endl;
        }
        std::cout << std::endl;

        // 2) 读取并清洗数据：过滤全空行及含空字段的脏数据行。
        std::vector<Row> validRows;
        std::size_t filteredCount = 0;
        std::size_t validCount = reader.readCleanRows(validRows, filteredCount);

        // 3) 统计并打印被过滤的无效行数。
        std::cout << "有效数据行数: " << validCount << std::endl;
        std::cout << "被过滤的无效行数: " << filteredCount << std::endl;
        std::cout << std::endl;

        // 4) 异常保护：后续统计前检查是否有有效数据，避免对空数据做统计。
        if (validRows.empty()) {
            throw CsvException("无有效数据，无法进行统计");
        }

        // 打印保留下来的有效数据，验证清洗未误删有效行。
        std::cout << "清洗后保留的有效数据:" << std::endl;
        for (std::size_t i = 0; i < validRows.size(); ++i) {
            std::cout << "  第 " << (i + 1) << " 行:" << std::endl;
            for (const auto& kv : reader.headers()) {
                std::cout << "    " << kv.first << " = "
                          << validRows[i].get(kv.first) << std::endl;
            }
        }

        // 演示基于有效数据的统计（空数据已被上面的异常拦截）。
        std::size_t totalFields = 0;
        for (const auto& r : validRows) {
            totalFields += r.size();
        }
        std::cout << std::endl
                  << "统计：有效行 " << validRows.size()
                  << " 行，字段合计 " << totalFields << " 个。" << std::endl;

        // 5) 数值统计：对指定数值列遍历有效数据行，求总和与平均值。
        // 非数字/乱码字段会在内部被捕获并转换为 CsvException 抛出，防止崩溃。
        std::string statCol = "id";
        double sum = 0.0, avg = 0.0;
        reader.computeColumnStats(statCol, validRows, sum, avg);

        std::cout << std::endl;
        std::cout << "数值统计（列 \"" << statCol << "\"）:" << std::endl;
        std::cout << "  行数 = " << validRows.size() << std::endl;
        std::cout << "  总和 = " << std::fixed << std::setprecision(2) << sum
                  << std::endl;
        std::cout << "  平均值 = " << std::setprecision(4) << avg << std::endl;
    } catch (const CsvException& e) {
        // 捕获 CSV 自定义异常（路径非法 / 打开失败 / 列名不存在 / 空数据等）。
        std::cerr << "CSV 错误: " << e.what() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        // 兜底捕获其他标准异常。
        std::cerr << "标准错误: " << e.what() << std::endl;
        return 2;
    }

    return 0;
}
