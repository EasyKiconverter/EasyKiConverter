"""
EasyKiConverter 代码格式化工具
=========================================================================

工具简介:
    本工具是 EasyKiConverter 项目的统一代码格式化脚本，支持 C++ 和
    QML 文件的格式化检查和修复，支持 CI/CD 集成，提供并行处理和
    哈希缓存加速。

主要功能:
    1. 格式化 C++ 文件 (使用 clang-format)
    2. 格式化 QML 文件 (使用 qmlformat)
    3. --check 模式：仅检查不修改，用于 CI
    4. 统计修改文件列表
    5. 生成格式化报告
    6. 并行处理：多线程并行格式化多个文件
    7. 哈希缓存：跳过未变更文件，加速重复格式化

环境要求:
    - Python: 3.6+
    - clang-format: 13+ (推荐 18+)
    - qmlformat: Qt 6.8+ (推荐 6.10.2)

=========================================================================
使用方法:
=========================================================================

基础用法:
    python tools/python/format_code.py --all           # 格式化所有文件
    python tools/python/format_code.py --cpp           # 仅格式化 C++ 文件
    python tools/python/format_code.py --qml           # 仅格式化 QML 文件
    python tools/python/format_code.py --check --all   # CI 检查模式（仅检查）

格式化目标（互斥选项）:
    --cpp              仅格式化 C++ 文件 (.h, .cpp)
    --qml              仅格式化 QML 文件 (.qml)
    --all              格式化所有文件 (C++ 和 QML)
    -s, --fix-empty-lines  修复 QML 文件中的多余空行

运行模式:
    --check            检查模式：仅检查不修改，返回退出码
    -v, --verbose      详细输出模式
    --report PATH      生成报告文件

目录配置:
    --dirs DIR [DIR]   指定格式化目录（默认: src, tests, src/ui/qml）

=========================================================================
使用示例:
=========================================================================

示例 1: 格式化所有代码
    $ python tools/python/format_code.py --all

示例 2: 仅检查代码格式（CI 模式）
    $ python tools/python/format_code.py --check --all

示例 3: 详细输出查看变化
    $ python tools/python/format_code.py --all -v

示例 4: 仅格式化 C++ 或 QML
    $ python tools/python/format_code.py --cpp
    $ python tools/python/format_code.py --qml

示例 5: 修复 QML 空行
    $ python tools/python/format_code.py --qml -s

示例 6: 指定目录格式化
    $ python tools/python/format_code.py --all --dirs src tests

示例 7: 生成格式化报告
    $ python tools/python/format_code.py --all --report logs/format.log

=========================================================================
缓存机制:
=========================================================================

哈希缓存:
    本工具使用 MD5 哈希缓存避免重复格式化未变更的文件。
    缓存文件位于: build/.cache/format_cache.json

    首次运行后会生成缓存，之后运行时会自动跳过未变更的文件。
    如果需要强制重新格式化，可以删除缓存文件。

CI/CD 集成:
    在 CI 环境中使用 --check 模式：
    - 格式正确：退出码 0
    - 需要格式化：退出码 1（显示需要格式化的文件列表）

=========================================================================
返回值:
=========================================================================

    0   格式化成功或格式检查通过
    1   检查到格式问题或发生错误

=========================================================================
"""

import os
import sys
import subprocess
import argparse
import logging
import time
import shutil
import hashlib
import json
from datetime import datetime
from pathlib import Path
from typing import Optional, List, Tuple, Dict
from concurrent.futures import ThreadPoolExecutor, as_completed

# ============================================================================
# ANSI 颜色支持
# ============================================================================


class Colors:
    """ANSI 颜色代码"""

    RESET = "\033[0m"
    RED = "\033[91m"
    GREEN = "\033[92m"
    YELLOW = "\033[93m"
    BLUE = "\033[94m"
    CYAN = "\033[96m"
    BOLD = "\033[1m"

    @classmethod
    def supports_color(cls) -> bool:
        """检测终端是否支持颜色"""
        # Windows 10+ 支持 ANSI 颜色
        if sys.platform == "win32":
            try:
                import ctypes

                kernel32 = ctypes.windll.kernel32
                # 启用 ANSI 转义序列
                kernel32.SetConsoleMode(kernel32.GetStdHandle(-11), 7)
                return True
            except:
                return False
        return hasattr(sys.stdout, "isatty") and sys.stdout.isatty()


def colorize(text: str, color: str) -> str:
    """添加颜色"""
    if Colors.supports_color():
        return f"{color}{text}{Colors.RESET}"
    return text


# ============================================================================
# 统计信息
# ============================================================================


class FormatStatistics:
    """格式化统计信息"""

    def __init__(self):
        self.start_time: float = time.time()
        self.total_files: int = 0
        self.modified_files: List[str] = []
        self.unchanged_files: List[str] = []
        self.failed_files: List[Tuple[str, str]] = []  # (file, error_message)

    @property
    def success_count(self) -> int:
        return len(self.modified_files) + len(self.unchanged_files)

    @property
    def elapsed_time(self) -> float:
        return time.time() - self.start_time

    def report(self, logger: logging.Logger, verbose: bool = False) -> None:
        """生成统计报告"""
        logger.info("")
        logger.info("=" * 60)
        logger.info("格式化统计报告")
        logger.info("=" * 60)
        logger.info(f"  总文件数:   {self.total_files}")
        logger.info(
            f"  已修改:     {colorize(str(len(self.modified_files)), Colors.YELLOW)} 个文件"
        )
        logger.info(
            f"  无变化:     {colorize(str(len(self.unchanged_files)), Colors.GREEN)} 个文件"
        )

        if self.failed_files:
            logger.info(
                f"  失败:       {colorize(str(len(self.failed_files)), Colors.RED)} 个文件"
            )

        logger.info(f"  耗时:       {self.elapsed_time:.2f}s")
        logger.info("=" * 60)

        # 显示修改文件列表
        if self.modified_files:
            logger.info("")
            logger.info(colorize("[修改文件列表]", Colors.YELLOW))
            for i, f in enumerate(self.modified_files, 1):
                logger.info(f"  {i}. {f}")

        # 显示失败文件列表
        if self.failed_files:
            logger.info("")
            logger.info(colorize("[失败文件列表]", Colors.RED))
            for f, err in self.failed_files:
                logger.error(f"  {f}: {err}")

        # 详细模式显示无变化文件
        if verbose and self.unchanged_files:
            logger.info("")
            logger.info(colorize("[无变化文件]", Colors.GREEN))
            for f in self.unchanged_files:
                logger.debug(f"  {f}")


# ============================================================================
# 代码格式化器
# ============================================================================


class CodeFormatter:
    def __init__(self, check_mode: bool = False, verbose: bool = False):
        self.project_root = Path(__file__).parent.parent.parent.absolute()
        self.build_dir = self.project_root / "build"
        self.build_cache_dir = self.build_dir / ".cache"
        self.check_mode = check_mode
        self.verbose = verbose
        self.stats = FormatStatistics()
        self.logger = self._setup_logging()

        self._ensure_build_dirs()
        self.clang_format_version = self._get_tool_version("clang-format")
        self.qmlformat_path = self._find_qmlformat()
        self.qmlformat_version = self._get_tool_version(self.qmlformat_path) if self.qmlformat_path else None
        self.hash_cache = self._load_cache()
        self.cache_updated = False

    def _ensure_build_dirs(self) -> None:
        """确保构建相关目录存在"""
        self.build_dir.mkdir(parents=True, exist_ok=True)
        self.build_cache_dir.mkdir(parents=True, exist_ok=True)

    def _setup_logging(self) -> logging.Logger:
        """配置日志系统"""
        level = logging.DEBUG if self.verbose else logging.INFO
        logger = logging.getLogger("CodeFormatter")
        logger.setLevel(level)

        # 控制台处理器
        console_handler = logging.StreamHandler(sys.stdout)
        console_handler.setLevel(level)

        # 简洁格式
        formatter = logging.Formatter("%(message)s")
        console_handler.setFormatter(formatter)
        logger.addHandler(console_handler)

        return logger

    def _find_qmlformat(self) -> Optional[str]:
        """查找 qmlformat，优先使用项目配置指定的 Qt。"""
        path_tool = shutil.which("qmlformat")
        if path_tool:
            return path_tool

        qt_roots = []
        for variable in ("Qt6_DIR", "QTDIR", "QT_PREFIX_PATH", "Qt6_ROOT", "QT_ROOT", "CMAKE_PREFIX_PATH"):
            value = os.environ.get(variable)
            if value:
                qt_roots.append(Path(value))

        config_file = self.project_root / "tools" / "config" / "build_config.json"
        try:
            with config_file.open(encoding="utf-8") as file:
                config = json.load(file)
            platform_name = {
                "linux": "qt_path_linux",
                "darwin": "qt_path_macos",
                "win32": "qt_path_windows",
            }.get(sys.platform)
            if platform_name and config.get(platform_name):
                qt_roots.append(Path(config[platform_name]))
        except (OSError, json.JSONDecodeError):
            pass

        for root in qt_roots:
            candidates = [root / "bin" / "qmlformat"]
            if root.name == "Qt6":
                candidates.append(root.parent / "bin" / "qmlformat")
            for candidate in candidates:
                if candidate.is_file() and os.access(candidate, os.X_OK):
                    return str(candidate)
        return None

    def _get_tool_version(self, tool: str) -> Optional[str]:
        """获取工具版本"""
        try:
            result = subprocess.run(
                [tool, "--version"], capture_output=True, text=True, timeout=10
            )
            if result.returncode == 0:
                # 提取版本号
                output = result.stdout.strip()
                if tool == "clang-format":
                    # clang-format version 21.1.0
                    import re

                    match = re.search(r"version\s+(\d+\.\d+\.\d+)", output)
                    return match.group(1) if match else output.split("\n")[0]
                elif Path(tool).name == "qmlformat":
                    # qmlformat 6.10.2
                    import re

                    match = re.search(r"(\d+\.\d+\.\d+)", output)
                    return match.group(1) if match else output.split("\n")[0]
                return output.split("\n")[0]
        except Exception:
            pass
        return None

    def _get_file_hash(self, file_path: Path) -> str:
        """计算文件 MD5 哈希"""
        try:
            with open(file_path, "rb") as f:
                return hashlib.md5(f.read()).hexdigest()
        except Exception:
            return ""

    def _load_cache(self) -> Dict[str, str]:
        """加载哈希缓存"""
        cache_file = self.build_cache_dir / "format_cache.json"
        if cache_file.exists():
            try:
                with open(cache_file, "r", encoding="utf-8") as f:
                    return json.load(f)
            except Exception:
                pass
        return {}

    def _save_cache(self, cache: Dict[str, str]) -> None:
        """保存哈希缓存"""
        cache_file = self.build_cache_dir / "format_cache.json"
        try:
            self.build_cache_dir.mkdir(parents=True, exist_ok=True)
            with open(cache_file, "w", encoding="utf-8") as f:
                json.dump(cache, f, indent=2)
        except Exception as e:
            self.logger.warning(f"缓存保存失败: {e}")

    def _is_file_changed(self, file_path: Path, cached_hash: str) -> bool:
        """检查文件是否已变更"""
        current_hash = self._get_file_hash(file_path)
        return current_hash != cached_hash

    def _find_files(self, directories: List[str], extensions: List[str]) -> List[Path]:
        """查找文件"""
        files = []
        for directory in directories:
            dir_path = self.project_root / directory
            if not dir_path.exists():
                self.logger.warning(f"目录不存在: {directory}")
                continue

            for ext in extensions:
                files.extend(dir_path.rglob(f"*{ext}"))

        return sorted(set(files))

    def _check_file_needs_formatting(
        self, file_path: Path, tool: str
    ) -> Tuple[bool, Optional[str]]:
        """检查文件是否需要格式化"""
        try:
            result = subprocess.run(
                [tool, "--dry-run", "--Werror", str(file_path)],
                capture_output=True,
                text=True,
                timeout=30,
            )
            # 退出码非 0 表示需要格式化
            return result.returncode != 0, None
        except subprocess.TimeoutExpired:
            return False, "超时"
        except Exception as e:
            return False, str(e)

    def _format_file(
        self, file_path: Path, tool: str
    ) -> Tuple[bool, bool, Optional[str]]:
        """
        格式化单个文件
        返回: (success, was_modified, error_message)
        """
        try:
            if self.check_mode:
                # 检查模式：仅检查是否需要格式化
                needs_formatting, error = self._check_file_needs_formatting(
                    file_path, tool
                )
                if error:
                    return False, False, error
                return True, needs_formatting, None
            else:
                # 格式化模式：先读取原内容用于比较
                original_content = file_path.read_text(encoding="utf-8")

                # 执行格式化
                result = subprocess.run(
                    [tool, "-i", str(file_path)],
                    capture_output=True,
                    text=True,
                    timeout=30,
                )

                if result.returncode != 0:
                    return False, False, result.stderr

                # 比较是否有变化
                new_content = file_path.read_text(encoding="utf-8")
                was_modified = original_content != new_content

                return True, was_modified, None

        except subprocess.TimeoutExpired:
            return False, False, "格式化超时"
        except UnicodeDecodeError:
            return False, False, "编码错误"
        except Exception as e:
            return False, False, str(e)

    def _format_qml_file(
        self, file_path: Path, config_file: Path
    ) -> Tuple[bool, bool, Optional[str]]:
        """格式化 QML 文件（支持并行）"""
        try:
            config_arg = ["-s", str(config_file)] if config_file.exists() else []

            if self.check_mode:
                import tempfile

                original = file_path.read_text(encoding="utf-8")

                with tempfile.NamedTemporaryFile(
                    mode="w", suffix=".qml", delete=False, encoding="utf-8"
                ) as tmp:
                    tmp.write(original)
                    tmp_path = tmp.name

                try:
                    result = subprocess.run(
                        [self.qmlformat_path, "-i"] + config_arg + [tmp_path],
                        capture_output=True,
                        text=True,
                        timeout=30,
                    )

                    if result.returncode != 0:
                        return False, False, result.stderr.strip() or "格式化失败"

                    formatted = Path(tmp_path).read_text(encoding="utf-8")
                    was_modified = formatted != original
                    return True, was_modified, None
                finally:
                    Path(tmp_path).unlink(missing_ok=True)
            else:
                original = file_path.read_text(encoding="utf-8")
                result = subprocess.run(
                    [self.qmlformat_path, "-i"] + config_arg + [str(file_path)],
                    capture_output=True,
                    text=True,
                    timeout=30,
                )

                if result.returncode != 0:
                    return False, False, result.stderr

                new_content = file_path.read_text(encoding="utf-8")
                was_modified = original != new_content
                return True, was_modified, None

        except subprocess.TimeoutExpired:
            return False, False, "超时"
        except UnicodeDecodeError:
            return False, False, "编码错误"
        except Exception as e:
            return False, False, str(e)

    def _process_qml_files_parallel(
        self, files: List[Path], config_file: Path, max_workers: int = 4
    ) -> None:
        """并行处理 QML 文件列表"""
        cache_key_prefix = "qmlformat_"

        files_to_process = []
        for file_path in files:
            relative_path = str(file_path.relative_to(self.project_root))
            cache_key = f"{cache_key_prefix}{relative_path}"
            cached_hash = self.hash_cache.get(cache_key, "")

            if cached_hash and not self._is_file_changed(file_path, cached_hash):
                self.stats.unchanged_files.append(relative_path)
                if self.verbose:
                    self.logger.debug(f"    ✓ {relative_path} - 无变化 (缓存命中)")
                continue
            files_to_process.append((file_path, relative_path, cache_key))

        if not files_to_process:
            return

        def format_with_config(file_path: Path) -> Tuple[bool, bool, Optional[str]]:
            return self._format_qml_file(file_path, config_file)

        with ThreadPoolExecutor(max_workers=max_workers) as executor:
            future_to_info = {
                executor.submit(format_with_config, f): (f, rel, ck)
                for f, rel, ck in files_to_process
            }
            for future in as_completed(future_to_info):
                file_path, relative_path, cache_key = future_to_info[future]
                self.stats.total_files += 1
                try:
                    success, was_modified, error = future.result()
                    if not success:
                        self.stats.failed_files.append((relative_path, error))
                        self.logger.error(f"    ✗ {relative_path} - {error}")
                    elif was_modified:
                        self.stats.modified_files.append(relative_path)
                        self.hash_cache[cache_key] = self._get_file_hash(file_path)
                        self.cache_updated = True
                        if self.check_mode:
                            self.logger.warning(f"    ⚡ {relative_path} - 需要格式化")
                        else:
                            self.logger.info(f"    ⚡ {relative_path} - 已修改")
                    else:
                        self.stats.unchanged_files.append(relative_path)
                        self.hash_cache[cache_key] = self._get_file_hash(file_path)
                        self.cache_updated = True
                        if self.verbose:
                            self.logger.debug(f"    ✓ {relative_path} - 无变化")
                except Exception as e:
                    self.stats.failed_files.append((relative_path, str(e)))
                    self.logger.error(f"    ✗ {relative_path} - {e}")

    def _process_files_parallel(
        self, files: List[Path], tool: str, max_workers: int = 4
    ) -> None:
        """并行处理文件列表"""
        cache_key_prefix = f"{tool}_"
        files_to_process = []

        for file_path in files:
            relative_path = str(file_path.relative_to(self.project_root))
            cache_key = f"{cache_key_prefix}{relative_path}"
            cached_hash = self.hash_cache.get(cache_key, "")

            if cached_hash and not self._is_file_changed(file_path, cached_hash):
                self.stats.unchanged_files.append(relative_path)
                if self.verbose:
                    self.logger.debug(f"    ✓ {relative_path} - 无变化 (缓存命中)")
                continue
            files_to_process.append((file_path, relative_path, cache_key))

        if not files_to_process:
            return

        with ThreadPoolExecutor(max_workers=max_workers) as executor:
            future_to_info = {
                executor.submit(self._format_file, f, tool): (f, rel, ck)
                for f, rel, ck in files_to_process
            }
            for future in as_completed(future_to_info):
                file_path, relative_path, cache_key = future_to_info[future]
                self.stats.total_files += 1
                try:
                    success, was_modified, error = future.result()
                    if not success:
                        self.stats.failed_files.append((relative_path, error))
                        self.logger.error(f"    ✗ {relative_path} - {error}")
                    elif was_modified:
                        self.stats.modified_files.append(relative_path)
                        self.hash_cache[cache_key] = self._get_file_hash(file_path)
                        self.cache_updated = True
                        if self.check_mode:
                            self.logger.warning(f"    ⚡ {relative_path} - 需要格式化")
                        else:
                            self.logger.info(f"    ⚡ {relative_path} - 已修改")
                    else:
                        self.stats.unchanged_files.append(relative_path)
                        self.hash_cache[cache_key] = self._get_file_hash(file_path)
                        self.cache_updated = True
                        if self.verbose:
                            self.logger.debug(f"    ✓ {relative_path} - 无变化")
                except Exception as e:
                    self.stats.failed_files.append((relative_path, str(e)))
                    self.logger.error(f"    ✗ {relative_path} - {e}")

    def format_cpp(self, directories: List[str] = None) -> bool:
        """格式化 C++ 文件"""
        if directories is None:
            directories = ["src", "tests"]

        self.logger.info("")
        self.logger.info(colorize("[C++ 格式化]", Colors.CYAN))
        self.logger.info(
            f"  工具: clang-format {self.clang_format_version or '未找到'}"
        )
        self.logger.info(f"  模式: {'检查' if self.check_mode else '格式化'}")
        self.logger.info(f"  目录: {', '.join(directories)}")

        if not self.clang_format_version:
            self.logger.error("  错误: 未找到 clang-format")
            return False

        # 检查 .clang-format 配置文件
        config_file = self.project_root / ".clang-format"
        if not config_file.exists():
            self.logger.warning("  警告: 未找到 .clang-format 配置文件")

        # 查找文件
        files = self._find_files(directories, [".h", ".cpp"])
        self.logger.info(f"  扫描到 {len(files)} 个文件")
        self.logger.info("")

        if not files:
            self.logger.info("  没有找到 C++ 文件")
            return True

        self._process_files_parallel(files, "clang-format")
        if self.cache_updated and not self.check_mode:
            self._save_cache(self.hash_cache)
        return len(self.stats.failed_files) == 0

    def format_qml(self, directories: List[str] = None) -> bool:
        """格式化 QML 文件"""
        if directories is None:
            directories = ["src/ui/qml"]

        self.logger.info("")
        self.logger.info(colorize("[QML 格式化]", Colors.CYAN))
        self.logger.info(f"  工具: qmlformat {self.qmlformat_version or '未找到'}")
        self.logger.info(f"  模式: {'检查' if self.check_mode else '格式化'}")
        self.logger.info(f"  目录: {', '.join(directories)}")

        if not self.qmlformat_version:
            self.logger.error("  错误: 未找到 qmlformat")
            return False

        # 检查 .qmlformat.json 配置文件
        config_file = self.project_root / ".qmlformat.json"
        if not config_file.exists():
            self.logger.warning("  警告: 未找到 .qmlformat.json 配置文件")

        # 查找文件
        files = self._find_files(directories, [".qml"])
        self.logger.info(f"  扫描到 {len(files)} 个文件")
        self.logger.info("")

        if not files:
            self.logger.info("  没有找到 QML 文件")
            return True

        self._process_qml_files_parallel(files, config_file)
        if self.cache_updated and not self.check_mode:
            self._save_cache(self.hash_cache)
        return len(self.stats.failed_files) == 0

    def fix_qml_empty_lines(self, directories: List[str] = None) -> bool:
        """修复 QML 文件中的多余空行"""
        if directories is None:
            directories = ["src/ui/qml"]

        self.logger.info("")
        self.logger.info(colorize("[修复 QML 空行]", Colors.CYAN))
        self.logger.info(f"  目录: {', '.join(directories)}")

        # 查找文件
        files = self._find_files(directories, [".qml"])
        self.logger.info(f"  扫描到 {len(files)} 个文件")
        self.logger.info("")

        if not files:
            self.logger.info("  没有找到 QML 文件")
            return True

        def remove_extra_empty_lines(content: str) -> str:
            """删除多余的空行"""
            lines = content.split("\n")
            result = []

            i = 0
            while i < len(lines):
                line = lines[i]
                stripped = line.strip()

                if stripped == "":
                    # 空行：检查是否需要保留
                    should_keep = False

                    # 检查上一行
                    if i > 0:
                        prev_line = lines[i - 1].strip()
                        if prev_line.endswith("}"):
                            should_keep = True
                        if (
                            prev_line.startswith("//")
                            or prev_line.startswith("/*")
                            or prev_line.endswith("*/")
                        ):
                            should_keep = True
                        if prev_line.startswith("import "):
                            should_keep = True

                    # 检查下一行
                    if i + 1 < len(lines):
                        next_line = lines[i + 1].strip()
                        if next_line == "":
                            should_keep = False
                        if next_line == "}":
                            should_keep = False

                    if should_keep:
                        result.append(line)
                else:
                    result.append(line)
                i += 1

            # 删除文件末尾的空行
            while result and result[-1].strip() == "":
                result.pop()

            # 确保文件以换行符结尾
            if result:
                result.append("")

            return "\n".join(result)

        # 处理文件
        for file_path in files:
            relative_path = file_path.relative_to(self.project_root)
            self.stats.total_files += 1

            try:
                content = file_path.read_text(encoding="utf-8")
                original_lines = len(content.split("\n"))

                fixed_content = remove_extra_empty_lines(content)
                fixed_lines = len(fixed_content.split("\n"))

                if fixed_content != content:
                    if not self.check_mode:
                        file_path.write_text(fixed_content, encoding="utf-8")
                    self.stats.modified_files.append(str(relative_path))
                    self.logger.info(
                        f"    ⚡ {relative_path} ({original_lines} → {fixed_lines} 行)"
                    )
                else:
                    self.stats.unchanged_files.append(str(relative_path))
                    if self.verbose:
                        self.logger.debug(f"    ✓ {relative_path} - 无变化")

            except Exception as e:
                self.stats.failed_files.append((str(relative_path), str(e)))
                self.logger.error(f"    ✗ {relative_path} - {e}")

        return len(self.stats.failed_files) == 0

    def generate_report(self, output_path: Optional[str] = None) -> None:
        """生成报告文件"""
        if output_path is None:
            log_dir = self.project_root / "logs"
            log_dir.mkdir(parents=True, exist_ok=True)
            timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
            output_path = log_dir / f"format_{timestamp}.log"
        else:
            output_path = Path(output_path)

        try:
            with open(output_path, "w", encoding="utf-8") as f:
                f.write("=" * 60 + "\n")
                f.write("EasyKiConverter 代码格式化报告\n")
                f.write("=" * 60 + "\n")
                f.write(f"时间: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
                f.write(f"模式: {'检查' if self.check_mode else '格式化'}\n")
                f.write(f"clang-format: {self.clang_format_version or '未找到'}\n")
                f.write(f"qmlformat: {self.qmlformat_version or '未找到'}\n")
                f.write(f"\n统计:\n")
                f.write(f"  总文件数: {self.stats.total_files}\n")
                f.write(f"  已修改: {len(self.stats.modified_files)}\n")
                f.write(f"  无变化: {len(self.stats.unchanged_files)}\n")
                f.write(f"  失败: {len(self.stats.failed_files)}\n")
                f.write(f"  耗时: {self.stats.elapsed_time:.2f}s\n")

                if self.stats.modified_files:
                    f.write(f"\n修改文件列表:\n")
                    for i, file in enumerate(self.stats.modified_files, 1):
                        f.write(f"  {i}. {file}\n")

                if self.stats.failed_files:
                    f.write(f"\n失败文件列表:\n")
                    for file, err in self.stats.failed_files:
                        f.write(f"  {file}: {err}\n")

            self.logger.info(f"\n报告已保存: {output_path}")
        except Exception as e:
            self.logger.error(f"保存报告失败: {e}")


# ============================================================================
# 主函数
# ============================================================================


def main():
    parser = argparse.ArgumentParser(
        description="EasyKiConverter 代码格式化工具",
        epilog="""
示例:
  python tools/python/format_code.py --all           # 格式化所有文件
  python tools/python/format_code.py --cpp           # 仅格式化 C++ 文件
  python tools/python/format_code.py --qml           # 仅格式化 QML 文件
  python tools/python/format_code.py --check --all   # CI 检查模式
  python tools/python/format_code.py --all -v        # 详细模式
  python tools/python/format_code.py --all --report logs/format.log  # 生成报告
  python tools/python/format_code.py --qml -s  # 修复 QML 空行
        """,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )

    # 格式化目标
    target_group = parser.add_mutually_exclusive_group()
    target_group.add_argument("--cpp", action="store_true", help="格式化 C++ 文件")
    target_group.add_argument("--qml", action="store_true", help="格式化 QML 文件")
    target_group.add_argument(
        "--all", action="store_true", help="格式化所有文件 (C++ + QML)"
    )

    # 运行模式
    parser.add_argument(
        "--check", action="store_true", help="检查模式：仅检查不修改，用于 CI"
    )
    parser.add_argument("-v", "--verbose", action="store_true", help="显示详细输出")
    parser.add_argument(
        "-s",
        "--fix-empty-lines",
        action="store_true",
        help="修复 QML 文件中的多余空行（每行代码后的空行）",
    )

    # 目录配置
    parser.add_argument("--dirs", nargs="+", help="指定格式化目录 (默认: src, tests)")

    # 报告生成
    parser.add_argument("--report", type=str, help="生成报告文件路径")

    args = parser.parse_args()

    # 检查参数：如果没有指定任何操作，显示帮助
    if not (args.cpp or args.qml or args.all or args.fix_empty_lines):
        parser.print_help()
        sys.exit(1)

    # 创建格式化器
    formatter = CodeFormatter(check_mode=args.check, verbose=args.verbose)

    # 显示标题
    formatter.logger.info("")
    formatter.logger.info("=" * 60)
    formatter.logger.info(colorize("  EasyKiConverter 代码格式化工具", Colors.BOLD))
    formatter.logger.info("=" * 60)

    # 执行格式化
    success = True

    if args.fix_empty_lines:
        # 仅修复空行
        if not formatter.fix_qml_empty_lines(
            args.dirs if args.dirs else ["src/ui/qml"]
        ):
            success = False
    else:
        # 正常格式化
        if args.cpp or args.all:
            if not formatter.format_cpp(args.dirs):
                success = False

        if args.qml or args.all:
            if not formatter.format_qml(args.dirs if args.dirs else ["src/ui/qml"]):
                success = False

    # 生成报告
    if args.report:
        formatter.generate_report(args.report)

    # 显示统计
    formatter.stats.report(formatter.logger, args.verbose)

    # 显示结果
    formatter.logger.info("")
    if formatter.check_mode:
        if formatter.stats.modified_files:
            formatter.logger.error(colorize("✗ 代码格式检查失败！", Colors.RED))
            formatter.logger.info("")
            formatter.logger.info("请运行以下命令格式化代码:")
            formatter.logger.info(
                colorize("  python tools/python/format_code.py --all", Colors.CYAN)
            )
            sys.exit(1)
        else:
            formatter.logger.info(colorize("✓ 代码格式检查通过！", Colors.GREEN))
            sys.exit(0)
    else:
        if success:
            formatter.logger.info(colorize("✓ 格式化完成！", Colors.GREEN))
            sys.exit(0)
        else:
            formatter.logger.error(colorize("✗ 格式化过程中出现错误！", Colors.RED))
            sys.exit(1)


if __name__ == "__main__":
    main()
