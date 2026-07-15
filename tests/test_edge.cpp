// test_edge.cpp
// CSV 工具边界/异常用例测试：统一异常体系、UTF-8 编码、行列不匹配、
// 空文件、BOM、转义、大文件流式读取、文件读写异常等。

#include "csv_reader.hpp"

#include <cstdio>
#include <fstream>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

namespace {

int g_fail = 0;

// 写临时文件，返回路径。
std::string writeTemp(const std::string& name, const std::string& content) {
    std::ofstream f(name, std::ios::binary);
    f << content;
    f.close();
    return name;
}

void removeTemp(const std::string& name) {
    std::remove(name.c_str());
}

// 期望 expr 抛出类型为 T 的异常。
template <typename T>
void expectThrow(const std::string& label, const std::string& expectCat,
                 const std::function<void()>& fn) {
    try {
        fn();
        std::cout << "[FAIL] " << label << " : 未抛出异常" << std::endl;
        ++g_fail;
    } catch (const T& e) {
        if (e.category() != expectCat) {
            std::cout << "[FAIL] " << label << " : 类别错误，得到 " << e.category()
                      << " 期望 " << expectCat << std::endl;
            ++g_fail;
        } else {
            std::cout << "[PASS] " << label << " : [" << e.category() << "] "
                      << e.what() << std::endl;
        }
    } catch (const std::exception& e) {
        std::cout << "[FAIL] " << label << " : 抛出了非预期异常类型: " << e.what()
                  << std::endl;
        ++g_fail;
    }
}

void expectTrue(const std::string& label, bool cond) {
    std::cout << (cond ? "[PASS] " : "[FAIL] ") << label << std::endl;
    if (!cond) ++g_fail;
}

} // namespace

int main() {
    // 1) 文件不存在 -> IO 错误
    expectThrow<CsvIoException>("打开不存在文件", "IO错误", []() {
        CSVReader r("__not_exist__.csv");
    });

    // 2) 空文件 -> readHeader 返回 false
    {
        std::string f = writeTemp("__empty__.csv", "");
        bool ok = false;
        try {
            CSVReader r(f);
            ok = (r.readHeader() == false);
        } catch (...) {
            ok = false;
        }
        expectTrue("空文件 readHeader 返回 false", ok);
        removeTemp(f);
    }

    // 3) 行列不匹配 -> 行列不匹配异常
    {
        std::string f = writeTemp("__mismatch__.csv",
                                  "id,name,note\n1,Alice\n");
        expectThrow<CsvColumnMismatchException>("数据行列数不匹配", "行列不匹配",
                                                [f]() {
                                                    CSVReader r(f);
                                                    r.readHeader();
                                                    Row row;
                                                    r.readRow(row);
                                                });
        removeTemp(f);
    }

    // 4) 非法 UTF-8 编码 -> 编码异常
    {
        std::string bad = "id,note\n1,";
        bad.push_back(static_cast<char>(0xFF)); // 非法前导字节
        bad.push_back('\n');
        std::string f = writeTemp("__enc__.csv", bad);
        expectThrow<CsvEncodingException>("非法 UTF-8 字段", "编码异常", [f]() {
            CSVReader r(f);
            r.readHeader();
            Row row;
            r.readRow(row);
        });
        removeTemp(f);
    }

    // 5) 数值列含非数字 -> 格式异常
    {
        std::string f = writeTemp("__badnum__.csv", "id,note\nabc,ok\n");
        expectThrow<CsvFormatException>("非数字数值字段", "格式错误", [f]() {
            CSVReader r(f);
            r.readHeader();
            Row row;
            r.readRow(row);
            (void)CSVReader::parseDouble(row.get("id"), 1, "id");
        });
        removeTemp(f);
    }

    // 6) UTF-8 BOM 被剥离
    {
        std::string content = "\xEF\xBB\xBFid,name\n1,Alice\n";
        std::string f = writeTemp("__bom__.csv", content);
        bool ok = false;
        try {
            CSVReader r(f);
            r.readHeader();
            auto hdr = r.headerFields();
            ok = (!hdr.empty() && hdr[0] == "id");
        } catch (...) {
            ok = false;
        }
        expectTrue("UTF-8 BOM 被剥离（首列名为 id）", ok);
        removeTemp(f);
    }

    // 7) 含逗号/引号/换行的转义字段保持正确列数
    {
        std::string f = writeTemp(
            "___esc__.csv",
            "id,name,note\n1,Alice,\"你好,世界\"\n2,Bob,\"他说:\"\"你好\"\"\"\n"
            "3,Charlie,\"第一行\n第二行\"\n");
        bool ok = false;
        try {
            CSVReader r(f);
            r.readHeader();
            int n = 0;
            Row row;
            while (r.readRow(row)) {
                ok = (row.size() == 3) && CSVReader::isRowValid(row);
                ++n;
            }
            ok = ok && (n == 3);
        } catch (...) {
            ok = false;
        }
        expectTrue("转义字段解析正确且列数一致（3 行）", ok);
        removeTemp(f);
    }

    // 8) 空行/空字段被清洗过滤（isRowValid），有效行保留
    {
        std::string f = writeTemp("__clean__.csv",
                                  "id,name,note\n1,Alice,ok\n\n2,,miss\n3,Bob,ok\n");
        int valid = 0, invalid = 0;
        try {
            CSVReader r(f);
            r.readHeader();
            Row row;
            while (r.readRow(row)) {
                if (CSVReader::isRowValid(row)) ++valid;
                else ++invalid;
            }
        } catch (...) {
        }
        expectTrue("清洗：2 有效 + 2 无效（空行/空字段）", valid == 2 && invalid == 2);
        removeTemp(f);
    }

    // 9) 写入到非法路径 -> IO 错误
    expectThrow<CsvIoException>("写入非法路径", "IO错误", []() {
        CsvWriter w("");
    });

    // 10) 大文件流式读取：生成 10 万行，逐行读取并统计，不应一次性占满内存
    {
        std::string f = "__big__.csv";
        {
            std::ofstream of(f, std::ios::binary);
            of << "id,val\n";
            for (int i = 1; i <= 100000; ++i) {
                of << i << ",row" << i << "\n";
            }
        }
        bool ok = false;
        std::size_t count = 0;
        try {
            CSVReader r(f);
            r.readHeader();
            Row row;
            while (r.readRow(row)) {
                ++count; // 不保存，仅流式计数
            }
            ok = (count == 100000);
        } catch (const std::exception& e) {
            std::cout << "  大文件读取异常: " << e.what() << std::endl;
            ok = false;
        }
        expectTrue("大文件流式读取 10 万行全部成功", ok);
        removeTemp(f);
    }

    std::cout << std::endl;
    if (g_fail == 0) {
        std::cout << "全部边界用例通过。" << std::endl;
        return 0;
    }
    std::cout << g_fail << " 个用例失败。" << std::endl;
    return 1;
}
