#!/usr/bin/env python3
"""
EasyKiConverter 代码注释覆盖率检查工具。

本工具检查 C++ 可维护性边界，而不是把重复的行尾注释当作质量指标：

1. 函数定义前必须存在 Doxygen 或行注释；
2. 会引入多路分支的 switch 块前必须存在解释其映射目的的注释；
3. 文件中至少有一个文件级说明，避免新增模块缺少整体设计背景。
4. 使用 --check-variables 时，检查可识别的简单变量声明是否有中文注释。
5. 使用 --check-params 时，检查函数 Doxygen 是否覆盖全部参数。
6. 使用 --check-doxygen 时，检查每个函数定义是否紧邻多行 Doxygen 注释，并包含规范的 ``@brief``。

检查结果称为“逻辑代码注释率”。它统计已经识别的函数和控制流逻辑单元，
阈值默认是 90%，用于约束新增和改动代码中的可维护性说明。

启用 --forbid-external-references 后，还会拒绝源码注释中的外部项目引用，
用于保持注释只描述本项目的功能、约束和实现语义。
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path


# 函数定义模式覆盖普通函数、成员函数和带 const/override 的声明实现。
FUNCTION_RE = re.compile(r"^\s*[\w:<>*&]+\s+[\w:~]+(?:::\w+)?\s*\([^;]*\)\s*(?:const)?\s*\{")
# 多行函数签名的首行，例如 `void addLine(QJsonObject& lines,`。
FUNCTION_START_RE = re.compile(r"^\s*(?:(?:[\w:<>*&]+)\s+)+[\w:~]+(?:::\w+)?\s*\(")
FUNCTION_KEYWORDS = {"if", "for", "while", "switch", "catch", "return"}

# 只统计 switch 这种需要说明映射策略的控制流块，不把循环和普通条件实现
# 当作逐行注释对象，避免检查结果鼓励无意义的重复注释。
CONTROL_RE = re.compile(r"^\s*switch\s*\(")

# 检查单行的简单变量声明；复杂的宏、模板和跨行初始化交由编译器/人工审查，避免正则误报。
VARIABLE_DECLARATION_RE = re.compile(
    r"^\s*(?:(?:static|const|constexpr|volatile|mutable|inline)\s+)*"
    r"(?:[A-Za-z_]\w*(?:::[A-Za-z_]\w*)*(?:<[^;{}()]+>)?[*&\s]+)"
    r"([A-Za-z_]\w*)\s*(?:=[^;]*)?;\s*(?://.*)?$"
)

# 识别以赋值表达式开头、在后续行结束的简单变量声明；用于覆盖长表达式和链式初始化。
MULTILINE_VARIABLE_START_RE = re.compile(
    r"^\s*(?:(?:static|const|constexpr|volatile|mutable|inline)\s+)*"
    r"(?:[A-Za-z_]\w*(?:::[A-Za-z_]\w*)*(?:<[^;{}()]+>)?[*&\s]+)"
    r"([A-Za-z_]\w*)\s*=.*[^;{}]\s*$"
)

# 仅匹配明确的外部项目引用表达，避免误报普通的“reference”技术术语。
FORBIDDEN_REFERENCE_RE = re.compile(
    r"(?:参考\s*(?:其他)?项目|参考\s*(?:其他)?实现|reference\s+implementation)",
    re.IGNORECASE,
)


def has_comment_before(lines: list[str], index: int) -> bool:
    """检查目标代码单元前的连续注释区域，允许空行和注释结束标记。"""
    cursor = index - 1
    while cursor >= 0 and (not lines[cursor].strip() or lines[cursor].lstrip().startswith("template ")):
        cursor -= 1
    if cursor < 0:
        return False
    return lines[cursor].lstrip().startswith(("//", "/*", "*", "*/"))


def inspect_file(path: Path) -> tuple[int, int, list[str]]:
    """统计一个文件的逻辑单元和有注释单元，并返回缺失位置。"""
    lines = path.read_text(encoding="utf-8").splitlines()
    total = 0
    documented = 0
    missing: list[str] = []
    pending_function: int | None = None
    in_block_comment = False
    for index, line in enumerate(lines):
        stripped = line.lstrip()
        if in_block_comment:
            if "*/" in line:
                in_block_comment = False
            continue
        if "/*" in line:
            if "*/" not in line[line.find("/*") + 2 :]:
                in_block_comment = True
            continue
        if stripped.startswith("//"):
            continue
        if pending_function is not None:
            if ";" in line and "{" not in line:
                pending_function = None
                continue
            if "{" in line:
                total += 1
                if has_comment_before(lines, pending_function):
                    documented += 1
                else:
                    missing.append(f"{path}:{pending_function + 1}")
                pending_function = None
            continue
        if FUNCTION_RE.match(
            line
        ) and not line.lstrip().startswith((":", "if ", "for ", "while ", "switch ", "catch ", "else ")):
            total += 1
            if has_comment_before(lines, index):
                documented += 1
            else:
                missing.append(f"{path}:{index + 1}")
        elif CONTROL_RE.match(line):
            total += 1
            if has_comment_before(lines, index):
                documented += 1
            else:
                missing.append(f"{path}:{index + 1}")
        elif (
            FUNCTION_START_RE.match(line)
            and not line.lstrip().startswith("return ")
            and not line.lstrip().startswith(":")
            and not line.lstrip().startswith(("if ", "if(", "else ", "else{"))
            and "<<" not in line
            and ("{" in line or ";" not in line)
        ):
            name = FUNCTION_START_RE.match(line).group(0).split("(", 1)[0].strip().split()[-1]
            if name not in FUNCTION_KEYWORDS:
                pending_function = index
    return total, documented, missing


def inspect_variable_comments(path: Path) -> tuple[int, int, list[str]]:
    """检查可识别的单行变量声明是否有紧邻或行尾中文注释。"""
    lines = path.read_text(encoding="utf-8").splitlines()
    total = 0
    documented = 0
    missing: list[str] = []
    index = 0
    while index < len(lines):
        line = lines[index]
        match = VARIABLE_DECLARATION_RE.match(line)
        if not match or line.lstrip().startswith(
            ("#", "return ", "delete ", "class ", "struct ", "enum ", "namespace ")
        ):
            multiline_match = MULTILINE_VARIABLE_START_RE.match(line)
            if (
                multiline_match
                and not line.lstrip().startswith(("#", "return ", "delete "))
                and not is_documented_parameter(lines, index)
            ):
                end = index
                while end + 1 < len(lines) and ";" not in lines[end]:
                    end += 1
                    if "{" in lines[end] or ("}" in lines[end] and ";" not in lines[end]):
                        break
                if end > index and ";" in lines[end]:
                    total += 1
                    if "//" in line or has_comment_before(lines, index):
                        documented += 1
                    else:
                        missing.append(f"{path}:{index + 1}")
                    index = end + 1
                    continue
            index += 1
            continue
        # 函数参数由所属函数的 Doxygen ``@param`` 统一说明，不要求在参数列表中重复插入注释。
        if is_documented_parameter(lines, index):
            index += 1
            continue
        total += 1
        if "//" in line or has_comment_before(lines, index):
            documented += 1
        else:
            missing.append(f"{path}:{index + 1}")
        index += 1
    return total, documented, missing


def is_documented_parameter(lines: list[str], index: int) -> bool:
    """判断多行函数签名中的参数是否已由前置 Doxygen ``@param`` 说明。"""
    cursor = index - 1
    while cursor >= 0 and index - cursor <= 40:
        line = lines[cursor]
        if "@param" in line:
            return True
        if "{" in line or ";" in line:
            return False
        cursor -= 1
    return False


def split_parameters(signature: str) -> list[str]:
    """按顶层逗号拆分函数参数，避免模板和初始化列表中的逗号误拆分。"""
    parameters: list[str] = []
    current: list[str] = []
    angle_depth = 0
    brace_depth = 0
    for character in signature:
        if character == "<":
            angle_depth += 1
        elif character == ">" and angle_depth:
            angle_depth -= 1
        elif character == "{" :
            brace_depth += 1
        elif character == "}" and brace_depth:
            brace_depth -= 1
        if character == "," and angle_depth == 0 and brace_depth == 0:
            parameters.append("".join(current))
            current = []
        else:
            current.append(character)
    if current:
        parameters.append("".join(current))
    return parameters


def function_parameter_names(signature: str) -> list[str]:
    """从函数签名中提取具名参数；匿名参数不要求额外的 ``@param``。"""
    opening = signature.find("(")
    closing = signature.rfind(")")
    if opening < 0 or closing <= opening:
        return []
    names: list[str] = []
    for parameter in split_parameters(signature[opening + 1 : closing]):
        parameter = parameter.split("=", 1)[0].strip()
        if not parameter or parameter.endswith(("&", "*")):
            continue
        identifiers = re.findall(r"[A-Za-z_]\w*", parameter)
        if len(identifiers) >= 2:
            names.append(identifiers[-1])
    return names


def inspect_parameter_comments(path: Path) -> tuple[int, int, list[str]]:
    """检查带 Doxygen 注释的函数参数是否都有对应的 ``@param`` 说明。"""
    lines = path.read_text(encoding="utf-8").splitlines()
    total = 0
    documented = 0
    missing: list[str] = []
    index = 0
    while index < len(lines):
        stripped = lines[index].lstrip()
        if (
            not FUNCTION_START_RE.match(lines[index])
            or stripped.startswith(("return ", "if ", "for ", "while ", "switch ", "catch ", "else ", "."))
        ):
            index += 1
            continue
        signature_lines = [lines[index]]
        end = index
        while end < len(lines) and ")" not in lines[end]:
            end += 1
            if end < len(lines):
                signature_lines.append(lines[end])
        if end >= len(lines):
            index += 1
            continue
        # 参数契约只检查紧邻 Doxygen 注释的函数；未注释函数由逻辑注释率检查报告，
        # 避免把函数体内的调用表达式误识别为新的函数声明。
        cursor = index - 1
        while cursor >= 0 and not lines[cursor].strip():
            cursor -= 1
        if cursor < 0 or not lines[cursor].strip().endswith("*/"):
            index = end + 1
            continue
        block_start = cursor
        while block_start >= 0 and "/**" not in lines[block_start]:
            block_start -= 1
        if block_start < 0:
            index = end + 1
            continue
        # 过滤 `object.method(QStringLiteral(...));` 和构造对象初始化等调用表达式，
        # 它们不是函数声明，不能被当作需要 @param 的接口。
        if "QStringLiteral(" in " ".join(signature_lines) and ";" in lines[end] and "{" not in lines[end]:
            index = end + 1
            continue
        # 函数体可能在同一行出现调用表达式；参数解析只应读取声明部分。
        signature = " ".join(signature_lines)
        if "{" in signature:
            signature = signature.split("{", 1)[0]
        names = function_parameter_names(signature)
        if not names:
            index = end + 1
            continue
        total += len(names)
        documentation = "\n".join(lines[max(0, index - 40) : index])
        documented_names = set(re.findall(r"@param\s+([A-Za-z_]\w*)", documentation))
        for name in names:
            if name in documented_names:
                documented += 1
            else:
                missing.append(f"{path}:{index + 1}: {name}")
        index = end + 1
    return total, documented, missing


def inspect_doxygen_comments(path: Path) -> tuple[int, int, list[str]]:
    """检查每个函数定义是否紧邻规范的多行 Doxygen 注释。"""
    lines = path.read_text(encoding="utf-8").splitlines()
    total = 0
    documented = 0
    missing: list[str] = []

    def preceding_doxygen(function_line: int) -> tuple[bool, bool]:
        """返回函数前是否存在多行 Doxygen 及其是否包含 ``@brief``。"""
        cursor = function_line - 1
        while cursor >= 0 and (not lines[cursor].strip() or lines[cursor].lstrip().startswith("template ")):
            cursor -= 1
        if cursor < 0 or not lines[cursor].strip().endswith("*/"):
            return False, False
        end = cursor
        while cursor >= 0 and "/**" not in lines[cursor]:
            cursor -= 1
        if cursor < 0:
            return False, False
        block = lines[cursor : end + 1]
        return len(block) > 1, any("@brief" in line for line in block)

    def record_function(function_line: int) -> None:
        """记录一个函数定义及其 Doxygen 格式结果。"""
        nonlocal total, documented
        total += 1
        is_multiline, has_brief = preceding_doxygen(function_line)
        if is_multiline and has_brief:
            documented += 1
        else:
            missing.append(f"{path}:{function_line + 1}")

    pending_function: int | None = None
    in_block_comment = False
    for index, line in enumerate(lines):
        stripped = line.lstrip()
        if in_block_comment:
            if "*/" in line:
                in_block_comment = False
            continue
        if "/*" in line:
            if "*/" not in line[line.find("/*") + 2 :]:
                in_block_comment = True
            continue
        if stripped.startswith("//"):
            continue
        if pending_function is not None:
            if ";" in line and "{" not in line:
                pending_function = None
                continue
            if "{" in line:
                record_function(pending_function)
                pending_function = None
            continue
        if FUNCTION_RE.match(
            line
        ) and not line.lstrip().startswith((":", "if ", "for ", "while ", "switch ", "catch ", "else ")):
            record_function(index)
        elif (
            FUNCTION_START_RE.match(line)
            and not line.lstrip().startswith("return ")
            and not line.lstrip().startswith(":")
            and not line.lstrip().startswith(("if ", "if(", "else ", "else{"))
            and "<<" not in line
            and ("{" in line or ";" not in line)
        ):
            name = FUNCTION_START_RE.match(line).group(0).split("(", 1)[0].strip().split()[-1]
            if name not in FUNCTION_KEYWORDS:
                pending_function = index
    return total, documented, missing


def find_forbidden_references(path: Path) -> list[str]:
    """查找文件注释中的外部项目引用，并返回带行号的位置。"""
    lines = path.read_text(encoding="utf-8").splitlines()
    locations: list[str] = []
    in_block_comment = False
    for index, line in enumerate(lines):
        comment_text = ""
        cursor = 0
        while cursor < len(line):
            if in_block_comment:
                end = line.find("*/", cursor)
                if end < 0:
                    comment_text += line[cursor:]
                    break
                comment_text += line[cursor:end]
                cursor = end + 2
                in_block_comment = False
                continue
            block_start = line.find("/*", cursor)
            line_start = line.find("//", cursor)
            if line_start >= 0 and (block_start < 0 or line_start < block_start):
                comment_text += line[line_start + 2 :]
                break
            if block_start < 0:
                break
            cursor = block_start + 2
            in_block_comment = True
        if FORBIDDEN_REFERENCE_RE.search(comment_text):
            locations.append(f"{path}:{index + 1}")
    return locations


def main() -> int:
    """解析参数并以 90% 作为默认门槛执行检查。"""
    parser = argparse.ArgumentParser(description="检查 C++ 逻辑代码注释覆盖率")
    parser.add_argument("files", nargs="+", type=Path, help="需要检查的 C++ 文件")
    parser.add_argument("--threshold", type=float, default=90.0, help="最低注释覆盖率，默认 90")
    parser.add_argument(
        "--forbid-external-references",
        action="store_true",
        help="拒绝源码注释中的外部项目引用",
    )
    parser.add_argument(
        "--check-variables",
        action="store_true",
        help="检查可识别的简单变量声明是否有注释",
    )
    parser.add_argument(
        "--check-params",
        action="store_true",
        help="检查函数参数是否有 Doxygen @param 说明",
    )
    parser.add_argument(
        "--check-doxygen",
        action="store_true",
        help="检查函数 Doxygen 注释块是否使用多行格式并包含 @brief",
    )
    args = parser.parse_args()

    total = 0
    documented = 0
    missing: list[str] = []
    forbidden_references: list[str] = []
    variable_total = 0
    variable_documented = 0
    missing_variables: list[str] = []
    parameter_total = 0
    parameter_documented = 0
    missing_parameters: list[str] = []
    doxygen_total = 0
    doxygen_documented = 0
    missing_doxygen: list[str] = []
    for path in args.files:
        file_total, file_documented, file_missing = inspect_file(path)
        total += file_total
        documented += file_documented
        missing.extend(file_missing)
        if args.forbid_external_references:
            forbidden_references.extend(find_forbidden_references(path))
        if args.check_variables:
            file_total, file_documented, file_missing = inspect_variable_comments(path)
            variable_total += file_total
            variable_documented += file_documented
            missing_variables.extend(file_missing)
        if args.check_params:
            file_total, file_documented, file_missing = inspect_parameter_comments(path)
            parameter_total += file_total
            parameter_documented += file_documented
            missing_parameters.extend(file_missing)
        if args.check_doxygen:
            file_total, file_documented, file_missing = inspect_doxygen_comments(path)
            doxygen_total += file_total
            doxygen_documented += file_documented
            missing_doxygen.extend(file_missing)

    rate = 100.0 if total == 0 else documented * 100.0 / total
    print(f"逻辑代码注释率: {rate:.1f}% ({documented}/{total})，门槛: {args.threshold:.1f}%")
    if missing:
        print("缺少注释的逻辑单元:")
        for location in missing:
            print(f"- {location}")
    if forbidden_references:
        print("检测到外部项目引用注释:")
        for location in forbidden_references:
            print(f"- {location}")
    if args.check_variables:
        variable_rate = 100.0 if variable_total == 0 else variable_documented * 100.0 / variable_total
        print(f"变量注释率: {variable_rate:.1f}% ({variable_documented}/{variable_total})")
        if missing_variables:
            print("缺少注释的变量声明:")
            for location in missing_variables:
                print(f"- {location}")
    failed = rate + 1e-9 < args.threshold
    if forbidden_references:
        failed = True
    if args.check_variables and variable_documented < variable_total:
        failed = True
    if args.check_params:
        parameter_rate = 100.0 if parameter_total == 0 else parameter_documented * 100.0 / parameter_total
        print(f"参数注释率: {parameter_rate:.1f}% ({parameter_documented}/{parameter_total})")
        if missing_parameters:
            print("缺少 @param 说明的参数:")
            for location in missing_parameters:
                print(f"- {location}")
        if parameter_documented < parameter_total:
            failed = True
    if args.check_doxygen:
        doxygen_rate = 100.0 if doxygen_total == 0 else doxygen_documented * 100.0 / doxygen_total
        print(f"Doxygen 注释格式率: {doxygen_rate:.1f}% ({doxygen_documented}/{doxygen_total})")
        if missing_doxygen:
            print("缺少 @brief 的 Doxygen 注释:")
            for location in missing_doxygen:
                print(f"- {location}")
        if doxygen_documented < doxygen_total:
            failed = True
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
