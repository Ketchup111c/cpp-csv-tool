// main.cpp
// CSV 工具演示入口：打开 CSV 文件并逐行读取，展示异常捕获。

#include "csv_reader.hpp"

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

        std::vector<std::string> row;
        std::size_t lineNo = 0;

        // 逐行读取并输出字段。
        while (reader.readRow(row)) {
            ++lineNo;
            std::cout << "第 " << lineNo << " 行，共 " << row.size() << " 个字段:" << std::endl;
            for (std::size_t i = 0; i < row.size(); ++i) {
                std::cout << "  [" << i << "] " << row[i] << std::endl;
            }
        }

        if (lineNo == 0) {
            std::cout << "文件为空或无可读内容。" << std::endl;
        }
    } catch (const CsvException& e) {
        // 捕获 CSV 自定义异常（路径非法 / 打开失败等）。
        std::cerr << "CSV 错误: " << e.what() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        // 兜底捕获其他标准异常。
        std::cerr << "标准错误: " << e.what() << std::endl;
        return 2;
    }

    return 0;
}
