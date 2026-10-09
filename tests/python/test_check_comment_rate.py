#!/usr/bin/env python3
"""验证 C++ 注释检查器能够识别单行和多行函数定义。"""

import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools" / "python"))

from check_comment_rate import inspect_doxygen_comments, inspect_file, inspect_parameter_comments, inspect_variable_comments


class CheckCommentRateTest(unittest.TestCase):
    """确保多行函数不会绕过注释覆盖率检查。"""

    def test_multiline_function_is_counted(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "sample.cpp"
            path.write_text(
                "/** @brief 已记录的函数。 */\n"
                "void addLine(\n"
                "    int first,\n"
                "    int second) {\n"
                "}\n",
                encoding="utf-8",
            )
            self.assertEqual(inspect_file(path), (1, 1, []))

    def test_uncommented_multiline_function_is_reported(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "sample.cpp"
            path.write_text(
                "void addArc(\n"
                "    int first,\n"
                "    int second) {\n"
                "}\n",
                encoding="utf-8",
            )
            total, documented, missing = inspect_file(path)
            self.assertEqual((total, documented), (1, 0))
            self.assertEqual(missing, [f"{path}:1"])

    def test_commented_global_variable_is_counted(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "sample.cpp"
            path.write_text(
                "QMutex g_mutex;  ///< 保护共享状态的互斥量。\n"
                "QHash<QString, int> g_counts;  ///< 状态到计数的映射。\n",
                encoding="utf-8",
            )
            self.assertEqual(inspect_variable_comments(path), (2, 2, []))

    def test_uncommented_local_variable_is_reported(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "sample.cpp"
            path.write_text(
                "void run() {\n"
                "    int count = 0;\n"
                "}\n",
                encoding="utf-8",
            )
            total, documented, missing = inspect_variable_comments(path)
            self.assertEqual((total, documented), (1, 0))
            self.assertEqual(missing, [f"{path}:2"])

    def test_documented_function_parameters_are_not_required_to_repeat_comments(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "sample.cpp"
            path.write_text(
                "/**\n"
                " * @brief 设置数据。\n"
                " * @param first 第一个参数。\n"
                " * @param second 第二个参数。\n"
                " */\n"
                "void setData(\n"
                "    int first,\n"
                "    int second);\n",
                encoding="utf-8",
            )
            self.assertEqual(inspect_variable_comments(path), (0, 0, []))

    def test_multiline_variable_initializer_is_reported(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "sample.cpp"
            path.write_text(
                "QString key = first() +\n"
                "               second();\n",
                encoding="utf-8",
            )
            total, documented, missing = inspect_variable_comments(path)
            self.assertEqual((total, documented), (1, 0))
            self.assertEqual(missing, [f"{path}:1"])

    def test_multiline_variable_initializer_accepts_declaration_comment(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "sample.cpp"
            path.write_text(
                "QString key = first() +  // 组合稳定键。\n"
                "               second();\n",
                encoding="utf-8",
            )
            total, documented, missing = inspect_variable_comments(path)
            self.assertEqual((total, documented, missing), (1, 1, []))

    def test_parameter_comments_are_checked_separately(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "sample.cpp"
            path.write_text(
                "/**\n"
                " * @brief 设置数据。\n"
                " * @param first 第一个参数。\n"
                " */\n"
                "void setData(int first, int second) {}\n",
                encoding="utf-8",
            )
            total, documented, missing = inspect_parameter_comments(path)
            self.assertEqual((total, documented), (2, 1))
            self.assertEqual(missing, [f"{path}:5: second"])

    def test_doxygen_and_parameter_checks_cover_complete_function_contract(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "sample.cpp"
            path.write_text(
                "/**\n"
                " * @brief 写入数据。\n"
                " * @param value 待写入的数据。\n"
                " * @return 是否写入成功。\n"
                " */\n"
                "bool writeData(const QString& value) { return !value.isEmpty(); }\n",
                encoding="utf-8",
            )
            self.assertEqual(inspect_doxygen_comments(path), (1, 1, []))
            self.assertEqual(inspect_parameter_comments(path), (1, 1, []))

    def test_doxygen_requires_brief(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "sample.cpp"
            path.write_text("/** 创建对象。 */\nvoid create() {}\n", encoding="utf-8")
            self.assertEqual(inspect_doxygen_comments(path)[0:2], (1, 0))

    def test_doxygen_brief_is_accepted(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "sample.cpp"
            path.write_text("/**\n * @brief 创建对象。\n */\nvoid create() {}\n", encoding="utf-8")
            self.assertEqual(inspect_doxygen_comments(path), (1, 1, []))

    def test_single_line_doxygen_brief_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "sample.cpp"
            path.write_text("/** @brief 创建对象。 */\nvoid create() {}\n", encoding="utf-8")
            self.assertEqual(inspect_doxygen_comments(path)[0:2], (1, 0))

    def test_line_comment_before_function_is_rejected_by_doxygen_check(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "sample.cpp"
            path.write_text("// 创建对象。\nvoid create() {}\n", encoding="utf-8")
            self.assertEqual(inspect_doxygen_comments(path)[0:2], (1, 0))

    def test_missing_function_comment_is_rejected_by_doxygen_check(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "sample.cpp"
            path.write_text("void create() {}\n", encoding="utf-8")
            self.assertEqual(inspect_doxygen_comments(path)[0:2], (1, 0))


if __name__ == "__main__":
    unittest.main()
