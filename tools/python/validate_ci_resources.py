#!/usr/bin/env python3
"""校验独立静态资源，避免资源-only PR 被无关 C++ 构建阻塞。"""

from __future__ import annotations

import argparse
import sys
import xml.etree.ElementTree as ElementTree
from pathlib import Path

from classify_ci_changes import changed_paths, is_static_resource


def validate_resource(path: Path) -> str | None:
    """检查资源存在、非空，并对 SVG 执行基本 XML 结构校验。"""
    if not path.exists():
        return None
    if not path.is_file():
        return "不是普通文件"
    if path.stat().st_size == 0:
        return "文件为空"
    if path.suffix.lower() == ".svg":
        try:
            root = ElementTree.parse(path).getroot()
        except (ElementTree.ParseError, OSError) as error:
            return f"SVG XML 无法解析：{error}"
        if root.tag.rsplit("}", 1)[-1].lower() != "svg":
            return "SVG 根元素不是 svg"
    return None


def main() -> int:
    """读取完整 diff 并验证其中的静态资源，删除的文件不再检查内容。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base", required=True)
    parser.add_argument("--head", required=True)
    parser.add_argument("--repository", type=Path, default=Path.cwd())
    args = parser.parse_args()

    paths, fallback, reason = changed_paths(args.base, args.head, args.repository)
    if fallback:
        print(f"无法安全读取资源变更范围：{reason}", file=sys.stderr)
        return 1

    failures: list[str] = []
    for relative_path in paths:
        if not is_static_resource(relative_path):
            continue
        error = validate_resource(args.repository / relative_path)
        if error:
            failures.append(f"{relative_path}: {error}")
    if failures:
        print("资源检查失败：")
        print("\n".join(f"- {failure}" for failure in failures))
        return 1
    print("静态资源检查通过。")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
