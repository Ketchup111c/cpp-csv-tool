// main.cpp
// CSV 工具演示入口：读取表头、逐行读取数据，并演示按列名获取字段。

#include "csv_reader.hpp"

#include <iostream>
#include <string>

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

        // 2) 逐行读取数据，3) 通过 Row 按列名获取字段内容。
        Row row;
        std::size_t lineNo = 0;
        while (reader.readRow(row)) {
            ++lineNo;
            std::cout << "第 " << lineNo << " 行:" << std::endl;
            // 遍历表头列名，演示按列名取值。
            for (const auto& kv : reader.headers()) {
                std::cout << "  " << kv.first << " = " << row.get(kv.first) << std::endl;
            }
        }

        if (lineNo == 0) {
            std::cout << "无数据行。" << std::endl;
        }
    } catch (const CsvException& e) {
        // 捕获 CSV 自定义异常（路径非法 / 打开失败 / 列名不存在等）。
        std::cerr << "CSV 错误: " << e.what() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        // 兜底捕获其他标准异常。
        std::cerr << "标准错误: " << e.what() << std::endl;
        return 2;
    }

    return 0;
}
