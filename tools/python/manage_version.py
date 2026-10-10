"""
EasyKiConverter 版本管理工具
============================================================================
说明:
    本工具用于自动化同步更新项目各处的版本信息，确保 vcpkg、CMake 和源码中的版本号一致。

主要功能:
    1. 集中管理: 从 vcpkg.json 读取并同步版本号到各关键配置文件。
    2. 自动更新:
       - vcpkg.json: 更新 "version-string" 字段。
       - CMakeLists.txt: 更新 VERSION_FROM_CI 及 qt_add_qml_module 的版本。
         (版本通过 configure_file 自动生成 src/core/Version.h，main.cpp 读取该头文件)
       - deploy/metainfo/io.github.tangsangsimida.easykiconverter.metainfo.xml: 更新截图 URL 和 release 描述。
       - deploy/archlinux/PKGBUILD 及 .SRCINFO: 更新 AUR 打包版本。
    3. 智能检查: 在更新前检查每个文件中的版本是否已经是目标版本，避免不必要的修改。
    4. 自动生成 release 描述: 从 Git 提交历史自动提取从上个版本到新版本的所有提交内容。
    5. 校验: 强制要求版本号符合 X.Y.Z (语义化版本) 格式。
    6. AUR 发布: 自动将 PKGBUILD 等文件推送到 AUR 仓库。

环境要求:
    - Python: 3.6+
    - 依赖: 标准库 (argparse, json, re, os, sys, datetime, subprocess)
    - 运行位置: 建议在项目根目录运行，或确保脚本相对于项目的路径结构不发生变化。
    - AUR 推送需要 SSH 密钥已配置（~/.ssh/config 中有 aur.archlinux.org 条目）。

用法示例:
    1. 检查当前版本:
       python tools/python/manage_version.py --check
    2. 更新所有文件到新版本（仅添加默认 release 条目）:
       python tools/python/manage_version.py 3.1.0
    3. 更新所有文件并自动生成详细 release 描述:
       python tools/python/manage_version.py 3.1.0 --generate-release-notes
    4. 仅更新 metainfo.xml:
       python tools/python/manage_version.py 3.1.0 --metainfo-only
    5. 更新版本并推送到 AUR:
       python tools/python/manage_version.py 3.2.0 --push-aur
    6. 仅推送当前版本到 AUR:
       python tools/python/manage_version.py --push-aur

注意事项:
    - 脚本会直接修改文件，建议在 Git 工作区干净的状态下运行以便回滚。
    - 自动生成 release 描述功能需要 Git 环境。
============================================================================
"""

import argparse
import json
import re
import os
import sys
import shutil
import subprocess
import tempfile
from datetime import datetime

# 文件路径配置 (相对于项目根目录)
PROJECT_ROOT = os.path.dirname(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
)
VCPKG_JSON_PATH = os.path.join(PROJECT_ROOT, "vcpkg.json")
CMAKE_LISTS_PATH = os.path.join(PROJECT_ROOT, "CMakeLists.txt")
VERSION_H_IN_PATH = os.path.join(PROJECT_ROOT, "src", "core", "Version.h.in")
METAINFO_XML_PATH = os.path.join(
    PROJECT_ROOT,
    "deploy",
    "metainfo",
    "io.github.tangsangsimida.easykiconverter.metainfo.xml",
)
MKDOCS_YAML_PATH = os.path.join(PROJECT_ROOT, "mkdocs.yml")
APPX_MANIFEST_PATH = os.path.join(PROJECT_ROOT, "deploy", "windows", "AppxManifest.xml")
FLATPAK_MANIFEST_PATH = os.path.join(
    PROJECT_ROOT, "deploy", "flatpak", "io.github.tangsangsimida.easykiconverter.yml"
)
PKGBUILD_PATH = os.path.join(PROJECT_ROOT, "deploy", "archlinux", "PKGBUILD")
SRCINFO_PATH = os.path.join(PROJECT_ROOT, "deploy", "archlinux", ".SRCINFO")


def get_current_version():
    """
    从 vcpkg.json 读取当前版本号

    Requirements:
    - 必须在项目 tools/python 目录下运行，或通过其完整路径调用
    - 依赖于根目录下的 vcpkg.json 作为版本源
    """
    if not os.path.exists(VCPKG_JSON_PATH):
        print(f"错误: 找不到文件 {VCPKG_JSON_PATH}")
        return None

    try:
        with open(VCPKG_JSON_PATH, "r", encoding="utf-8") as f:
            data = json.load(f)
            return data.get("version-string")
    except Exception as e:
        print(f"读取 vcpkg.json 失败: {e}")
        return None


def check_vcpkg_json_version(version):
    """检查 vcpkg.json 中的版本是否已经是目标版本"""
    if not os.path.exists(VCPKG_JSON_PATH):
        return None

    try:
        with open(VCPKG_JSON_PATH, "r", encoding="utf-8") as f:
            data = json.load(f)
            current = data.get("version-string")
            return current == version
    except Exception as e:
        print(f"检查 vcpkg.json 失败: {e}")
        return None


def check_cmake_version(version):
    """检查 CMakeLists.txt 中的版本是否已经是目标版本"""
    if not os.path.exists(CMAKE_LISTS_PATH):
        return None

    try:
        with open(CMAKE_LISTS_PATH, "r", encoding="utf-8") as f:
            content = f.read()
            # 检查 VERSION_FROM_CI 默认值
            pattern = r'set\(VERSION_FROM_CI\s*"([^"]+)"\)'
            match = re.search(pattern, content)
            if match:
                return match.group(1) == version
    except Exception as e:
        print(f"检查 CMakeLists.txt 失败: {e}")
        return None
    return False


def check_version_header():
    """
    检查 Version.h.in 模板是否存在且格式正确。

    版本值由 CMakeLists.txt 的 PROJECT_VERSION 通过 configure_file 注入，
    此处仅验证模板文件的完整性，不检查具体版本号。
    """
    if not os.path.exists(VERSION_H_IN_PATH):
        return False

    try:
        with open(VERSION_H_IN_PATH, "r", encoding="utf-8") as f:
            content = f.read()
            return "@PROJECT_VERSION@" in content
    except Exception as e:
        print(f"检查 Version.h.in 失败: {e}")
        return None


def check_metainfo_version(version):
    """检查 metainfo.xml 中的版本是否已经是目标版本"""
    if not os.path.exists(METAINFO_XML_PATH):
        return None

    try:
        with open(METAINFO_XML_PATH, "r", encoding="utf-8") as f:
            content = f.read()
            # 检查截图 URL 中的版本
            pattern = r"https://raw\.githubusercontent\.com/EasyKiconverter/EasyKiConverter/v([^/]+)/"
            matches = re.findall(pattern, content)
            if matches:
                # 所有截图 URL 都应该是相同的版本
                return all(match == version for match in matches)
    except Exception as e:
        print(f"检查 metainfo.xml 失败: {e}")
        return None
    return False


def update_vcpkg_json(new_version, force=False):
    """更新 vcpkg.json 中的版本号"""
    print(f"正在更新 {VCPKG_JSON_PATH}...")

    # 检查版本是否已经是目标版本（除非是强制模式）
    if not force:
        check_result = check_vcpkg_json_version(new_version)
        if check_result is True:
            print(f"  ✓ {VCPKG_JSON_PATH} 版本已经是 {new_version}，跳过更新")
            return True
        elif check_result is None:
            print(f"  ✗ 无法检查 {VCPKG_JSON_PATH} 版本")
            return False

    try:
        with open(VCPKG_JSON_PATH, "r", encoding="utf-8") as f:
            content = f.read()

        # 使用正则替换，保留原有格式
        # "version-string": "3.0.5",
        pattern = r'("version-string":\s*")([^"]+)(")'
        replacement = f"\\g<1>{new_version}\\g<3>"

        new_content = re.sub(pattern, replacement, content)

        if content == new_content:
            if force:
                print(f"  ✓ {VCPKG_JSON_PATH} 版本已经是 {new_version}")
                return True
            else:
                print(f"  ✗ {VCPKG_JSON_PATH} 未发生变化 (可能版本号未变或格式不匹配)")
                return False
        else:
            with open(VCPKG_JSON_PATH, "w", encoding="utf-8") as f:
                f.write(new_content)
            print(f"  ✓ 成功更新 {VCPKG_JSON_PATH}")
            return True
    except Exception as e:
        print(f"  ✗ 更新 {VCPKG_JSON_PATH} 失败: {e}")
        return False


def update_cmake_lists(new_version, force=False):
    """更新 CMakeLists.txt 中的版本号"""
    print(f"正在更新 {CMAKE_LISTS_PATH}...")

    # 检查版本是否已经是目标版本（除非是强制模式）
    if not force:
        check_result = check_cmake_version(new_version)
        if check_result is True:
            print(f"  ✓ {CMAKE_LISTS_PATH} 版本已经是 {new_version}，跳过更新")
            return True
        elif check_result is None:
            print(f"  ✗ 无法检查 {CMAKE_LISTS_PATH} 版本")
            return False

    try:
        with open(CMAKE_LISTS_PATH, "r", encoding="utf-8") as f:
            original_content = f.read()

        content = original_content

        # 1. 更新 VERSION_FROM_CI 默认值
        # set(VERSION_FROM_CI "3.0.13")
        pattern1 = r'(set\(VERSION_FROM_CI\s+")([^"]+)("\))'
        content = re.sub(pattern1, f"\\g<1>{new_version}\\g<3>", content)

        # 2. 更新 qt_add_qml_module 中的 VERSION (只保留 Major.Minor)
        # VERSION 3.0
        major_minor = ".".join(new_version.split(".")[:2])
        pattern2 = r"(qt_add_qml_module\(appEasyKiconverter_Cpp_Version.*?VERSION\s+)(\d+\.\d+)"
        content = re.sub(pattern2, f"\\g<1>{major_minor}", content, flags=re.DOTALL)

        # 3. 更新 project() 中的 VERSION (如果存在显式指定)
        # project(EasyKiconverter_Cpp_Version VERSION 3.0.13 LANGUAGES CXX)
        # 注意：这个通常由 PROJECT_VERSION_SANITIZED 动态生成，但如果有硬编码也需要更新
        pattern3 = r"(project\(EasyKiconverter_Cpp_Version\s+VERSION\s+)(\d+\.\d+\.\d+)(\s+LANGUAGES)"
        content = re.sub(pattern3, f"\\g<1>{new_version}\\g<3>", content)

        if original_content == content:
            if force:
                print(f"  ✓ {CMAKE_LISTS_PATH} 版本已经是 {new_version}")
                return True
            else:
                print(f"  ✗ {CMAKE_LISTS_PATH} 未发生变化")
                return False
        else:
            with open(CMAKE_LISTS_PATH, "w", encoding="utf-8") as f:
                f.write(content)
            print(f"  ✓ 成功更新 {CMAKE_LISTS_PATH}")
            return True

    except Exception as e:
        print(f"  ✗ 更新 {CMAKE_LISTS_PATH} 失败: {e}")
        import traceback

        traceback.print_exc()
        return False


def update_metainfo_xml(new_version, generate_release=False, force=False):
    """
    更新 metainfo.xml 中的版本号和 release 描述

    Args:
        new_version: 新版本号
        generate_release: 是否自动生成 release 描述（默认 False）
        force: 是否强制更新，跳过版本检查
    """
    print(f"正在更新 {METAINFO_XML_PATH}...")

    if not os.path.exists(METAINFO_XML_PATH):
        print(f"  ✗ 找不到文件 {METAINFO_XML_PATH}")
        return False

    # 检查版本是否已经是目标版本（除非是强制模式）
    if not force:
        check_result = check_metainfo_version(new_version)
        if check_result is True:
            print(f"  ✓ {METAINFO_XML_PATH} 版本已经是 {new_version}，跳过更新")
            return True
        elif check_result is None:
            print(f"  ✗ 无法检查 {METAINFO_XML_PATH} 版本")
            return False

    try:
        with open(METAINFO_XML_PATH, "r", encoding="utf-8") as f:
            content = f.read()

        # 获取第一个 release 版本
        release_pattern = r'<release version="([^"]+)" date="[^"]+">'
        first_release_match = re.search(release_pattern, content)

        # 获取上个版本号
        old_version = None
        if first_release_match:
            old_version = first_release_match.group(1)

        # 更新截图 URL 中的版本标签
        # https://raw.githubusercontent.com/EasyKiconverter/EasyKiConverter/v3.0.11/resources/imgs/screenshot1.png
        pattern = r"(https://raw\.githubusercontent\.com/EasyKiconverter/EasyKiConverter/v)([^/]+)(/resources/imgs/screenshot\d+\.png)"
        new_content = re.sub(pattern, f"\\g<1>{new_version}\\g<3>", content)

        # 检查是否需要添加新的 release 条目
        if first_release_match:
            current_release_version = first_release_match.group(1)
            if current_release_version != new_version:
                # 需要添加新的 release 条目
                if generate_release:
                    print(f"  ℹ  自动生成 release 描述...")
                    # 获取提交历史
                    commits = get_git_commits_since_version(old_version, new_version)
                    print(f"  ℹ  找到 {len(commits)} 个提交")

                    # 生成 release 描述
                    release_description = generate_release_description(
                        old_version, new_version, commits
                    )
                    print(f"  ✓ 已自动添加详细 release 描述")
                else:
                    # 生成简单的默认 release 描述
                    release_description = generate_release_description(
                        old_version, new_version, []
                    )
                    print(f"  ℹ  已添加默认 release 描述（使用 --generate-release-notes 生成详细描述）")

                # 在 <releases> 标签后插入新的 release
                releases_pattern = r"(<releases>)"
                new_content = re.sub(
                    releases_pattern,
                    f"\\g<1>\n{release_description}",
                    new_content,
                    count=1,
                )
            else:
                if force:
                    print(
                        f"  ✓ {METAINFO_XML_PATH} 第一个 release 版本已经是 {new_version}"
                    )
                    # 在强制模式下，需要检查截图 URL 是否也需要更新
                    # 继续执行后续的截图 URL 更新逻辑
                else:
                    print(f"  ✓ 第一个 release 版本已经是 {new_version}")
                    # 非强制模式下，检查截图 URL 版本
                    screenshot_pattern = r"https://raw\.githubusercontent\.com/EasyKiconverter/EasyKiConverter/v([^/]+)/"
                    screenshot_matches = re.findall(screenshot_pattern, new_content)
                    if screenshot_matches and all(
                        match == new_version for match in screenshot_matches
                    ):
                        print(f"  ✓ {METAINFO_XML_PATH} 所有版本都是 {new_version}")
                        return True
        else:
            print(f"  ℹ  注意: 未找到 release 条目，建议手动添加")

        if content == new_content:
            if force:
                print(f"  ✓ {METAINFO_XML_PATH} 版本已经是 {new_version}")
                return True
            else:
                print(f"  ✗ {METAINFO_XML_PATH} 未发生变化")
                return False
        else:
            with open(METAINFO_XML_PATH, "w", encoding="utf-8") as f:
                f.write(new_content)
            print(f"  ✓ 成功更新 {METAINFO_XML_PATH}")
            return True

    except Exception as e:
        print(f"  ✗ 更新 {METAINFO_XML_PATH} 失败: {e}")
        import traceback

        traceback.print_exc()
        return False


def get_git_commits_since_version(old_version, new_version):
    """
    获取从上个版本到当前版本的所有提交

    Args:
        old_version: 上个版本号
        new_version: 新版本号

    Returns:
        list: 提交信息列表，每个元素为 (commit_hash, commit_message)
    """
    try:
        # 尝试找到上个版本的标签
        tag_pattern = f"v{old_version}"

        # 获取从上个版本标签到 HEAD 的所有提交
        cmd = [
            "git",
            "log",
            "--oneline",
            "--no-merges",
            f"{tag_pattern}..HEAD" if old_version else "--all",
        ]

        result = subprocess.run(
            cmd, cwd=PROJECT_ROOT, capture_output=True, text=True, check=True
        )

        commits = []
        for line in result.stdout.strip().split("\n"):
            if line:
                # 解析提交格式: hash commit_message
                parts = line.split(" ", 1)
                if len(parts) >= 2:
                    commit_hash = parts[0]
                    commit_message = parts[1]
                    commits.append((commit_hash, commit_message))

        return commits

    except subprocess.CalledProcessError as e:
        print(f"  ⚠ 获取 Git 提交历史失败: {e}")
        print(f"  ⚠ 将使用空提交列表")
        return []
    except Exception as e:
        print(f"  ⚠ 获取 Git 提交历史时出错: {e}")
        print(f"  ⚠ 将使用空提交列表")
        return []


def generate_release_description(old_version, new_version, commits):
    """
    生成 release 描述

    Args:
        old_version: 上个版本号
        new_version: 新版本号
        commits: 提交信息列表

    Returns:
        str: 格式化的 release 描述 XML
    """
    if not commits:
        # 如果没有提交，使用默认描述
        return f"""    <release version="{new_version}" date="{datetime.now().strftime("%Y-%m-%d")}">
      <description>
        <p>Version {new_version} release</p>
        <p xml:lang="zh_CN">{new_version} 版本发布</p>
      </description>
    </release>"""

    # 分类提交
    categories = {
        "feat": [],
        "fix": [],
        "refactor": [],
        "chore": [],
        "docs": [],
        "perf": [],
        "test": [],
        "i18n": [],
        "other": [],
    }

    for commit_hash, commit_message in commits:
        # 解析提交类型 (遵循约定式提交)
        commit_type = "other"
        if commit_message.startswith("feat(") or commit_message.startswith("feat:"):
            commit_type = "feat"
        elif commit_message.startswith("fix(") or commit_message.startswith("fix:"):
            commit_type = "fix"
        elif commit_message.startswith("refactor(") or commit_message.startswith(
            "refactor:"
        ):
            commit_type = "refactor"
        elif commit_message.startswith("chore(") or commit_message.startswith("chore:"):
            commit_type = "chore"
        elif commit_message.startswith("docs(") or commit_message.startswith("docs:"):
            commit_type = "docs"
        elif commit_message.startswith("perf(") or commit_message.startswith("perf:"):
            commit_type = "perf"
        elif commit_message.startswith("test(") or commit_message.startswith("test:"):
            commit_type = "test"
        elif commit_message.startswith("i18n(") or commit_message.startswith("i18n:"):
            commit_type = "i18n"

        # 提取提交主体（移除类型前缀）
        message_body = commit_message
        if "(" in commit_message and ")" in commit_message:
            # 格式: type(scope): message
            parts = commit_message.split(")", 1)
            if len(parts) >= 2:
                message_body = parts[1].strip(": ")
        elif ":" in commit_message:
            # 格式: type: message
            parts = commit_message.split(":", 1)
            if len(parts) >= 2:
                message_body = parts[1].strip()

        categories[commit_type].append(message_body)

    # 生成 XML 描述
    xml_parts = []
    xml_parts.append(
        f'    <release version="{new_version}" date="{datetime.now().strftime("%Y-%m-%d")}">'
    )
    xml_parts.append("      <description>")

    # 添加版本描述
    if old_version:
        xml_parts.append(f"        <p>Changes from {old_version} to {new_version}</p>")
        xml_parts.append(
            f'        <p xml:lang="zh_CN">从 {old_version} 到 {new_version} 的变更</p>'
        )
    else:
        xml_parts.append(f"        <p>Version {new_version} release</p>")
        xml_parts.append(f'        <p xml:lang="zh_CN">{new_version} 版本发布</p>')

    # 生成分类列表
    category_order = [
        "feat",
        "fix",
        "perf",
        "refactor",
        "test",
        "docs",
        "i18n",
        "chore",
        "other",
    ]
    category_names = {
        "feat": ("New Features", "新功能"),
        "fix": ("Bug Fixes", "修复"),
        "perf": ("Performance Improvements", "性能改进"),
        "refactor": ("Refactoring", "重构"),
        "test": ("Testing", "测试"),
        "docs": ("Documentation", "文档"),
        "i18n": ("Internationalization", "国际化"),
        "chore": ("Chores", "其他"),
        "other": ("Other Changes", "其他变更"),
    }

    has_items = False
    for cat_type in category_order:
        items = categories[cat_type]
        if items:
            has_items = True
            cat_name_en, cat_name_zh = category_names[cat_type]
            xml_parts.append(f"        <p>{cat_name_en}:</p>")
            xml_parts.append(f'        <p xml:lang="zh_CN">{cat_name_zh}:</p>')
            xml_parts.append("        <ul>")
            for item in items:
                # 简单的中文翻译检测（如果包含中文字符）
                if any("\u4e00" <= char <= "\u9fff" for char in item):
                    xml_parts.append(f'          <li xml:lang="zh_CN">{item}</li>')
                else:
                    xml_parts.append(f"          <li>{item}</li>")
            xml_parts.append("        </ul>")

    if not has_items:
        xml_parts.append("        <p>No significant changes</p>")
        xml_parts.append('        <p xml:lang="zh_CN">无重大变更</p>')

    xml_parts.append("      </description>")
    xml_parts.append("    </release>")

    return "\n".join(xml_parts)


def update_mkdocs_yaml(new_version, force=False):
    """更新 mkdocs.yml 中的版本号"""
    print(f"正在更新 {MKDOCS_YAML_PATH}...")

    # 检查版本是否已经是目标版本（除非是强制模式）
    if not force:
        check_result = check_mkdocs_version(new_version)
        if check_result is True:
            print(f"  ✓ {MKDOCS_YAML_PATH} 版本已经是 {new_version}，跳过更新")
            return True
        elif check_result is None:
            print(f"  ✗ 无法检查 {MKDOCS_YAML_PATH} 版本")
            return False

    try:
        with open(MKDOCS_YAML_PATH, "r", encoding="utf-8") as f:
            content = f.read()

        # 更新 extra.version
        # extra:
        #   version: 3.0.5
        pattern = r"(extra:\s*\n\s*version:\s*)([^\s\n]+)"
        replacement = f"\\g<1>{new_version}"
        new_content = re.sub(pattern, replacement, content)

        if content == new_content:
            if force:
                print(f"  ✓ {MKDOCS_YAML_PATH} 版本已经是 {new_version}")
                return True
            else:
                print(f"  ✗ {MKDOCS_YAML_PATH} 未发生变化")
                return False
        else:
            with open(MKDOCS_YAML_PATH, "w", encoding="utf-8") as f:
                f.write(new_content)
            print(f"  ✓ 成功更新 {MKDOCS_YAML_PATH}")
            return True
    except Exception as e:
        print(f"  ✗ 更新 {MKDOCS_YAML_PATH} 失败: {e}")
        return False


def update_appx_manifest(new_version, force=False):
    """更新 AppxManifest.xml 中的版本号"""
    print(f"正在更新 {APPX_MANIFEST_PATH}...")

    if not os.path.exists(APPX_MANIFEST_PATH):
        print(f"  ℹ  文件 {APPX_MANIFEST_PATH} 不存在，跳过更新")
        return True

    # 转换为 MSIX 版本格式 (X.Y.Z -> X.Y.Z.0)
    msix_version = convert_to_msix_version(new_version)

    # 检查版本是否已经是目标版本（除非是强制模式）
    if not force:
        check_result = check_appx_version(new_version)
        if check_result is True:
            print(f"  ✓ {APPX_MANIFEST_PATH} 版本已经是 {msix_version}，跳过更新")
            return True
        elif check_result is None:
            print(f"  ✗ 无法检查 {APPX_MANIFEST_PATH} 版本")
            return False

    try:
        with open(APPX_MANIFEST_PATH, "r", encoding="utf-8") as f:
            content = f.read()

        # 更新 Identity Version（只匹配 Identity 标签内的 Version）
        # <Identity ... Version="3.0.9.0" ...>
        pattern = r'(<Identity[^>]*\sVersion=")(\d+\.\d+\.\d+\.\d+)("[^>]*>)'
        replacement = f"\\g<1>{msix_version}\\g<3>"
        new_content = re.sub(pattern, replacement, content)

        if content == new_content:
            if force:
                print(f"  ✓ {APPX_MANIFEST_PATH} 版本已经是 {msix_version}")
                return True
            else:
                print(f"  ✗ {APPX_MANIFEST_PATH} 未发生变化")
                return False
        else:
            with open(APPX_MANIFEST_PATH, "w", encoding="utf-8") as f:
                f.write(new_content)
            print(f"  ✓ 成功更新 {APPX_MANIFEST_PATH}")
            return True
    except Exception as e:
        print(f"  ✗ 更新 {APPX_MANIFEST_PATH} 失败: {e}")
        return False


def update_flatpak_manifest(new_version, force=False):
    """更新 Flatpak 清单中的 Git 标签版本"""
    print(f"正在更新 {FLATPAK_MANIFEST_PATH}...")

    if not os.path.exists(FLATPAK_MANIFEST_PATH):
        print(f"  ℹ  文件 {FLATPAK_MANIFEST_PATH} 不存在，跳过更新")
        return True

    # 检查版本是否已经是目标版本（除非是强制模式）
    if not force:
        check_result = check_flatpak_version(new_version)
        if check_result is True:
            print(
                f"  ✓ {FLATPAK_MANIFEST_PATH} Git 标签已经是 v{new_version}，跳过更新"
            )
            return True
        elif check_result is None:
            print(f"  ✗ 无法检查 {FLATPAK_MANIFEST_PATH} 版本")
            return False

    try:
        with open(FLATPAK_MANIFEST_PATH, "r", encoding="utf-8") as f:
            content = f.read()

        # 更新 tag
        # tag: v3.0.13
        pattern = r"(tag:\s*)v([^\s\n]+)"
        replacement = f"\\g<1>v{new_version}"
        new_content = re.sub(pattern, replacement, content)

        if content == new_content:
            if force:
                print(f"  ✓ {FLATPAK_MANIFEST_PATH} Git 标签已经是 v{new_version}")
                return True
            else:
                print(f"  ✗ {FLATPAK_MANIFEST_PATH} 未发生变化")
                return False
        else:
            with open(FLATPAK_MANIFEST_PATH, "w", encoding="utf-8") as f:
                f.write(new_content)
            print(f"  ✓ 成功更新 {FLATPAK_MANIFEST_PATH}")
            return True
    except Exception as e:
        print(f"  ✗ 更新 {FLATPAK_MANIFEST_PATH} 失败: {e}")
        return False


def update_pkgbuild(new_version, force=False):
    """更新 PKGBUILD 和 .SRCINFO 中的版本号"""
    print(f"正在更新 {PKGBUILD_PATH}...")

    if not os.path.exists(PKGBUILD_PATH):
        print(f"  ℹ  文件 {PKGBUILD_PATH} 不存在，跳过更新")
        return True

    try:
        # 更新 PKGBUILD
        with open(PKGBUILD_PATH, "r", encoding="utf-8") as f:
            content = f.read()

        # pkgver=3.1.11
        pattern = r"(pkgver=)([^\s\n]+)"
        match = re.search(pattern, content)
        if match:
            current_version = match.group(2)
            if current_version == new_version and not force:
                print(f"  ✓ {PKGBUILD_PATH} 版本已经是 {new_version}，跳过更新")
            else:
                new_content = re.sub(pattern, f"\\g<1>{new_version}", content)
                with open(PKGBUILD_PATH, "w", encoding="utf-8") as f:
                    f.write(new_content)
                print(f"  ✓ 成功更新 {PKGBUILD_PATH}")
        else:
            print(f"  ⚠  未找到 pkgver 字段")
            return False

        # 更新 .SRCINFO
        if os.path.exists(SRCINFO_PATH):
            with open(SRCINFO_PATH, "r", encoding="utf-8") as f:
                srcinfo_content = f.read()

            original_srcinfo = srcinfo_content

            # 1. 更新 pkgver = 3.1.11
            pkgver_pattern = r"(pkgver\s*=\s*)([^\s\n]+)"
            match = re.search(pkgver_pattern, srcinfo_content)
            if match:
                current_version = match.group(2)
                if current_version == new_version and not force:
                    print(f"  ✓ {SRCINFO_PATH} pkgver 已经是 {new_version}，跳过更新")
                else:
                    srcinfo_content = re.sub(pkgver_pattern, f"\\g<1>{new_version}", srcinfo_content)
            else:
                print(f"  ⚠  未找到 pkgver 字段")
                return False

            # 2. 更新 source URL 中的版本号
            # source = easykiconverter-3.1.11.tar.gz::https://...v3.1.11.tar.gz
            source_pattern = r"(source\s*=\s*[\w-]+-)(\d+\.\d+\.\d+)(\.tar\.gz::https://[^\s]*?/v)(\d+\.\d+\.\d+)(\.tar\.gz)"
            match = re.search(source_pattern, srcinfo_content)
            if match:
                srcinfo_content = re.sub(
                    source_pattern,
                    f"\\g<1>{new_version}\\g<3>{new_version}\\g<5>",
                    srcinfo_content,
                )

            if original_srcinfo == srcinfo_content:
                if force:
                    print(f"  ✓ {SRCINFO_PATH} 已经是 {new_version}")
                else:
                    print(f"  ✗ {SRCINFO_PATH} 未发生变化")
            else:
                with open(SRCINFO_PATH, "w", encoding="utf-8") as f:
                    f.write(srcinfo_content)
                print(f"  ✓ 成功更新 {SRCINFO_PATH}")

        return True
    except Exception as e:
        print(f"  ✗ 更新 {PKGBUILD_PATH} 失败: {e}")
        return False


def check_pkgbuild_version(version):
    """检查 PKGBUILD 中的版本是否匹配"""
    if not os.path.exists(PKGBUILD_PATH):
        return None  # 文件不存在

    try:
        with open(PKGBUILD_PATH, "r", encoding="utf-8") as f:
            content = f.read()

        pattern = r"pkgver=([^\s\n]+)"
        match = re.search(pattern, content)
        if match:
            return match.group(1) == version
        return False
    except Exception:
        return False


def check_all_versions(version):
    """检查所有文件中的版本是否一致"""
    print(f"检查所有文件中的版本是否为 {version}...")

    results = {
        "vcpkg.json": check_vcpkg_json_version(version),
        "CMakeLists.txt": check_cmake_version(version),
        "Version.h.in": check_version_header(),
        "metainfo.xml": check_metainfo_version(version),
        "mkdocs.yml": check_mkdocs_version(version),
        "AppxManifest.xml": check_appx_version(version),
        "Flatpak manifest": check_flatpak_version(version),
        "PKGBUILD": check_pkgbuild_version(version),
    }

    all_match = True
    for filename, result in results.items():
        if filename == "Version.h.in":
            # Version.h.in 只验证模板完整性，版本值由 CMake 注入
            if result is True:
                print(f"  ✓ {filename}: 模板正确 (版本由 CMakeLists.txt 注入)")
            elif result is False:
                print(f"  ✗ {filename}: 模板缺失或格式错误")
                all_match = False
            else:
                print(f"  ? {filename}: 无法检查模板")
                all_match = False
        elif result is True:
            print(f"  ✓ {filename}: 版本是 {version}")
        elif result is False:
            print(f"  ✗ {filename}: 版本不是 {version}")
            all_match = False
        else:
            print(f"  ? {filename}: 无法检查版本")
            all_match = False

    return all_match


def convert_to_msix_version(version):
    """
    将 X.Y.Z 格式转换为 MSIX 的 X.Y.Z.R 格式
    MSIX 要求 4 段版本号，R 通常设为 0
    """
    parts = version.split(".")
    if len(parts) == 3:
        return f"{version}.0"
    elif len(parts) == 4:
        return version
    else:
        raise ValueError(f"无效的版本格式: {version}")


def parse_msix_version(msix_version):
    """
    将 MSIX 的 X.Y.Z.R 格式转换为 X.Y.Z 格式
    """
    parts = msix_version.split(".")
    if len(parts) == 4:
        return ".".join(parts[:3])
    elif len(parts) == 3:
        return msix_version
    else:
        raise ValueError(f"无效的 MSIX 版本格式: {msix_version}")


def check_mkdocs_version(version):
    """检查 mkdocs.yml 中的版本是否已经是目标版本"""
    if not os.path.exists(MKDOCS_YAML_PATH):
        return None

    try:
        with open(MKDOCS_YAML_PATH, "r", encoding="utf-8") as f:
            content = f.read()
            # 检查 extra.version
            pattern = r"extra:\s*\n\s*version:\s*([^\s\n]+)"
            match = re.search(pattern, content)
            if match:
                return match.group(1) == version
    except Exception as e:
        print(f"检查 mkdocs.yml 失败: {e}")
        return None
    return False


def check_appx_version(version):
    """检查 AppxManifest.xml 中的版本是否已经是目标版本"""
    if not os.path.exists(APPX_MANIFEST_PATH):
        return None

    try:
        msix_version = convert_to_msix_version(version)
        with open(APPX_MANIFEST_PATH, "r", encoding="utf-8") as f:
            content = f.read()
            # 检查 Identity Version（只匹配 Identity 标签内的 Version）
            pattern = r'<Identity[^>]*\sVersion="(\d+\.\d+\.\d+\.\d+)"'
            match = re.search(pattern, content)
            if match:
                return match.group(1) == msix_version
    except Exception as e:
        print(f"检查 AppxManifest.xml 失败: {e}")
        return None
    return False


def check_flatpak_version(version):
    """检查 Flatpak 清单中的 Git 标签是否已经是目标版本"""
    if not os.path.exists(FLATPAK_MANIFEST_PATH):
        return None

    try:
        with open(FLATPAK_MANIFEST_PATH, "r", encoding="utf-8") as f:
            content = f.read()
            # 检查 tag
            pattern = r"tag:\s*v([^\s\n]+)"
            match = re.search(pattern, content)
            if match:
                return match.group(1) == version
    except Exception as e:
        print(f"检查 Flatpak 清单失败: {e}")
        return None
    return False


def push_aur():
    """
    将本地 PKGBUILD、.SRCINFO、easykiconverter.install 推送到 AUR 仓库

    流程:
    1. 克隆 AUR 仓库到临时目录
    2. 复制本地文件到 AUR 仓库
    3. 提交并推送
    4. 清理临时目录

    前提条件:
    - SSH 密钥已配置（~/.ssh/config 中有 aur.archlinux.org 条目）
    - AUR 账户已注册并添加了公钥
    """
    print("正在推送到 AUR 仓库...")

    # 检查本地文件是否存在
    files_to_push = ["PKGBUILD", ".SRCINFO", "easykiconverter.install"]
    for filename in files_to_push:
        filepath = os.path.join(
            PROJECT_ROOT, "deploy", "archlinux", filename
        )
        if not os.path.exists(filepath):
            print(f"  ✗ 找不到文件 {filepath}")
            return False

    # 读取 PKGBUILD 中的维护者信息用于 git commit
    maintainer_name = "dennisreyoonjiho"
    maintainer_email = "dennisreyoonjiho@gmail.com"
    try:
        with open(PKGBUILD_PATH, "r", encoding="utf-8") as f:
            content = f.read()
            match = re.search(r"# Maintainer:\s*(\S+)\s*<([^>]+)>", content)
            if match:
                maintainer_name = match.group(1)
                maintainer_email = match.group(2)
    except Exception:
        pass

    # 获取当前版本号用于 commit message
    version = None
    try:
        with open(PKGBUILD_PATH, "r", encoding="utf-8") as f:
            content = f.read()
            match = re.search(r"pkgver=([^\s\n]+)", content)
            if match:
                version = match.group(1)
    except Exception:
        pass

    tmp_dir = None
    try:
        # 克隆 AUR 仓库到临时目录
        tmp_dir = tempfile.mkdtemp(prefix="aur_push_")
        print(f"  正在克隆 AUR 仓库到 {tmp_dir}...")

        result = subprocess.run(
            [
                "git",
                "clone",
                "ssh://aur@aur.archlinux.org/easykiconverter.git",
                tmp_dir,
            ],
            capture_output=True,
            text=True,
        )
        if result.returncode != 0:
            print(f"  ✗ 克隆 AUR 仓库失败: {result.stderr.strip()}")
            return False

        # 配置 git 用户信息
        subprocess.run(
            ["git", "config", "user.name", maintainer_name],
            cwd=tmp_dir,
            capture_output=True,
            text=True,
        )
        subprocess.run(
            ["git", "config", "user.email", maintainer_email],
            cwd=tmp_dir,
            capture_output=True,
            text=True,
        )

        # 复制文件到 AUR 仓库
        for filename in files_to_push:
            src = os.path.join(PROJECT_ROOT, "deploy", "archlinux", filename)
            dst = os.path.join(tmp_dir, filename)
            shutil.copy2(src, dst)
            print(f"  ✓ 复制 {filename}")

        # 检查是否有变更
        result = subprocess.run(
            ["git", "status", "--porcelain"],
            cwd=tmp_dir,
            capture_output=True,
            text=True,
        )
        if not result.stdout.strip():
            print("  ✓ AUR 仓库已是最新，无需推送")
            return True

        # 提交
        commit_msg = f"Update to version {version}" if version else "Update package files"
        subprocess.run(
            ["git", "add", "PKGBUILD", ".SRCINFO", "easykiconverter.install"],
            cwd=tmp_dir,
            capture_output=True,
            text=True,
        )
        result = subprocess.run(
            ["git", "commit", "-m", commit_msg],
            cwd=tmp_dir,
            capture_output=True,
            text=True,
        )
        if result.returncode != 0:
            print(f"  ✗ 提交失败: {result.stderr.strip()}")
            return False
        print(f"  ✓ 已提交: {commit_msg}")

        # 推送
        result = subprocess.run(
            ["git", "push", "origin", "master"],
            cwd=tmp_dir,
            capture_output=True,
            text=True,
        )
        if result.returncode != 0:
            print(f"  ✗ 推送失败: {result.stderr.strip()}")
            return False

        print(f"  ✓ 已推送到 AUR (版本 {version})")
        return True

    except Exception as e:
        print(f"  ✗ 推送到 AUR 失败: {e}")
        return False
    finally:
        # 清理临时目录
        if tmp_dir and os.path.exists(tmp_dir):
            shutil.rmtree(tmp_dir, ignore_errors=True)


def main():
    parser = argparse.ArgumentParser(description="EasyKiConverter 版本管理工具")
    parser.add_argument("version", nargs="?", help="新的版本号 (例如 3.0.6)")
    parser.add_argument("--check", action="store_true", help="仅检查当前版本")
    parser.add_argument(
        "--metainfo-only", action="store_true", help="仅更新 metainfo.xml"
    )
    parser.add_argument(
        "--generate-release-notes", action="store_true", help="从 Git 提交历史自动生成详细的 release 描述"
    )
    parser.add_argument(
        "--force", action="store_true", help="强制更新所有文件，即使版本相同"
    )
    parser.add_argument(
        "--push-aur", action="store_true", help="更新版本后自动推送到 AUR 仓库"
    )

    args = parser.parse_args()

    current_version = get_current_version()
    print(f"当前版本 (vcpkg.json): {current_version}")

    if args.check:
        if current_version:
            check_all_versions(current_version)
        return

    # 仅推送到 AUR（不更新版本号）
    if args.push_aur and not args.version:
        if push_aur():
            print("\n✓ 已推送到 AUR！")
        else:
            print("\n✗ 推送到 AUR 失败。")
        return

    if not args.version:
        print("请输入新的版本号，例如: python manage_version.py 3.0.6")
        print("\n可用选项:")
        print("  --check              仅检查当前版本")
        print("  --force              强制更新所有文件，即使版本相同")
        print("  --metainfo-only      仅更新 metainfo.xml")
        print("  --push-aur           推送当前/新版本到 AUR 仓库")
        print("  --generate-release-notes  自动生成详细 release 描述")
        return

    new_version = args.version
    if not re.match(r"^\d+\.\d+\.\d+$", new_version):
        print("错误: 版本号格式必须为 X.Y.Z (例如 3.0.6)")
        return

    print(f"\n准备将版本更新为: {new_version}")

    # 检查是否已经是目标版本
    if current_version == new_version and not args.force:
        print(f"\n当前版本已经是 {new_version}，检查所有文件...")
        all_match = check_all_versions(new_version)
        if all_match:
            print("\n✓ 所有文件版本都是最新的，无需更新！")
        else:
            print("\n✗ 部分文件版本不一致，请使用 --force 参数强制更新所有文件！")
        return

    # 仅更新 metainfo.xml
    if args.metainfo_only:
        print("\n仅更新 metainfo.xml...")
        if update_metainfo_xml(
            new_version, generate_release=args.generate_release_notes, force=args.force
        ):
            print("\nmetainfo.xml 更新完成！")
        else:
            print("\n更新失败。")
        return

    # 强制更新所有文件
    if args.force:
        print(f"\n强制更新模式：将所有文件更新到 {new_version}")
        print(f"  (跳过版本检查，确保所有文件版本一致)\n")

    # 更新所有文件
    success = True
    success &= update_vcpkg_json(new_version, force=args.force)
    success &= update_cmake_lists(new_version, force=args.force)
    success &= update_metainfo_xml(
        new_version, generate_release=args.generate_release_notes, force=args.force
    )
    success &= update_mkdocs_yaml(new_version, force=args.force)
    success &= update_appx_manifest(new_version, force=args.force)
    success &= update_flatpak_manifest(new_version, force=args.force)
    success &= update_pkgbuild(new_version, force=args.force)

    if success:
        print("\n✓ 所有文件更新完成！")
        if not args.generate_release_notes:
            print(f"\n提示: 已添加默认 release 条目，如需自动生成详细描述请使用 --generate-release-notes")

        # 验证更新结果
        print("\n正在验证更新结果...")
        all_match = check_all_versions(new_version)
        if all_match:
            print("\n✓ 所有文件版本验证通过！")
        else:
            print("\n⚠ 部分文件版本验证失败，请检查上述信息")

        # 推送到 AUR
        if args.push_aur:
            print("\n正在推送到 AUR...")
            if push_aur():
                print("\n✓ 版本更新并推送到 AUR 完成！")
            else:
                print("\n⚠ 版本更新成功，但推送到 AUR 失败，请手动推送")
    else:
        print("\n✗ 更新过程中出现错误，请检查上述信息。")


if __name__ == "__main__":
    main()
