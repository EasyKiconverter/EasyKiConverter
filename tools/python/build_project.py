"""
EasyKiConverter 项目构建管理工具
=========================================================================

工具简介:
    本工具是 EasyKiConverter 项目的自动化构建管理脚本，提供环境检测、
    CMake 配置、项目构建、安装部署、测试运行等完整构建流程支持。

    支持 Windows (MSVC/MinGW) 和 Linux/macOS (GCC/Clang) 平台，
    自动检测 Qt6 环境并配置 CMake，构建完成后自动生成统计报告。

主要功能:
    1. 环境检查: 验证 CMake、编译器、Qt6 等依赖 (支持 VS 2019/2022)
    2. CMake 配置: 支持多种构建类型和自定义选项 (POSIX 路径解析)
    3. 项目构建: 并行编译，实时进度条显示，预估剩余时间
    4. 日志管理: 历史日志轮换 (保留最近 10 次)，失败日志自动备份
    5. 安装测试: 自动化安装和测试流程，支持清理确认
    6. 增量构建: 通过源码哈希检测，支持跳过未变更的构建
    7. 构建缓存: 自动保存构建状态，加快重复构建速度

环境要求:
    - Python: 3.6+
    - CMake: 3.16+
    - Qt6: 6.6+ (推荐 6.10.2)
    - 编译器: MSVC 2019/2022 (Windows), GCC/Clang (Linux/macOS)

=========================================================================
使用方法:
=========================================================================

基础用法:
    python tools/python/build_project.py                    # 默认 Debug 构建
    python tools/python/build_project.py -t Release        # Release 构建
    python tools/python/build_project.py -t RelWithDebInfo  # 带调试信息的发布构建
    python tools/python/build_project.py -t MinSizeRel     # 最小体积构建

构建选项:
    -c, --clean          构建前清理 build 目录（需确认）
    -y, --yes            自动确认所有提示（包括清理确认）
    -j, --jobs N         指定并行编译任务数（默认: CPU 核心数）
    -v, --verbose        显示详细构建输出

配置选项:
    --check              仅执行环境检查，不进行构建
    --config-only        仅执行 CMake 配置，不执行编译

构建后操作:
    -i, --install        构建完成后执行安装（输出到 build/install 目录）
    --test               构建完成后运行测试 (ctest)

=========================================================================
使用示例:
=========================================================================

示例 1: 快速构建（默认 Debug）
    $ python tools/python/build_project.py

示例 2: 发布版本构建
    $ python tools/python/build_project.py -t Release

示例 3: 清理并重新构建
    $ python tools/python/build_project.py -c -y

示例 4: 指定并行任务数
    $ python tools/python/build_project.py -t Release -j 8

示例 5: 构建并运行测试
    $ python tools/python/build_project.py -t Release --test

示例 6: 构建并安装
    $ python tools/python/build_project.py -t Release -i

示例 7: 仅检查构建环境
    $ python tools/python/build_project.py --check

示例 8: 完整开发环境检查（包含翻译工具、代码格式化工具）
    $ python tools/python/build_project.py --env-check

示例 9: 仅配置不构建（查看 CMake 配置是否正确）
    $ python tools/python/build_project.py --config-only

示例 10: 完整构建流程（清理 + 构建 + 测试 + 安装）
    $ python tools/python/build_project.py -c -t Release --test -i -y

示例 11: 详细输出模式
    $ python tools/python/build_project.py -t Release -v

=========================================================================
环境变量:
=========================================================================

Qt6 路径环境变量（按优先级排序）:
    1. Qt6_DIR          Qt6 CMake 配置文件目录
    2. QTDIR            Qt 安装根目录
    3. QT_PREFIX_PATH   Qt 前缀路径
    4. Qt6_ROOT         Qt6 根目录
    5. QT_ROOT          Qt 根目录
    6. CMAKE_PREFIX_PATH CMake 查找路径

示例:
    export Qt6_ROOT=/opt/Qt6/6.10.2
    export Qt6_DIR=/opt/Qt6/6.10.2/lib/cmake/Qt6
    python tools/python/build_project.py -t Release

=========================================================================
构建缓存:
=========================================================================

增量构建:
    本工具支持增量构建检测，通过计算源码文件的 MD5 哈希值判断是否需要
    重新编译。如果源码未变更，可以跳过构建直接使用之前的编译结果。

    缓存文件: .build_cache.json（位于项目根目录，已加入 .gitignore）

    注意: 修改 CMakeLists.txt 或任何源码文件后，构建系统会自动检测
    并触发完整构建。

=========================================================================
日志文件:
=========================================================================

构建日志（位于 build/ 目录）:
    - build.log              当前构建日志（覆盖写入）
    - logs/build_*.log       历史构建日志（保留最近 10 次）
    - build_last_failed.log  最近一次失败构建的日志备份

构建缓存（位于 build/.cache/ 目录）:
    - build_cache.json       增量构建缓存

=========================================================================
返回值:
=========================================================================

    0   构建/操作成功
    1   构建/操作失败（环境检查失败、编译错误、测试失败等）

=========================================================================
"""

import os
import sys
import subprocess
import argparse
import logging
import shutil
import time
import json
import platform
import re
import hashlib
from datetime import datetime
from pathlib import Path, PurePath
from typing import Optional, Dict, Any, List, Tuple

# ============================================================================
# 进度条显示
# ============================================================================


class ProgressBar:
    """简单的进度条显示（纯标准库实现）"""

    def __init__(self, total: int = 100, width: int = 40):
        self.total = total
        self.current = 0
        self.width = width
        self.start_time = time.time()

    def update(self, current: int, prefix: str = "构建中") -> None:
        """更新进度条"""
        self.current = current
        if self.total == 0:
            return

        percent = min(100, int(100 * current / self.total))
        filled = int(self.width * current / self.total)
        bar = "█" * filled + "░" * (self.width - filled)

        elapsed = time.time() - self.start_time
        if current > 0:
            eta = elapsed * (self.total - current) / current
            eta_str = self._format_time(eta)
        else:
            eta_str = "--:--"

        elapsed_str = self._format_time(elapsed)

        line = f"\r{prefix} |{bar}| {percent:3d}% [{current}/{self.total}] 耗时: {elapsed_str} 剩余: {eta_str}"
        print(line, end="", flush=True)

    def _format_time(self, seconds: float) -> str:
        """格式化时间"""
        if seconds < 60:
            return f"{int(seconds):2d}s"
        elif seconds < 3600:
            m = int(seconds // 60)
            s = int(seconds % 60)
            return f"{m:02d}:{s:02d}"
        else:
            h = int(seconds // 3600)
            m = int((seconds % 3600) // 60)
            return f"{h}h{m:02d}m"

    def finish(self) -> None:
        """完成进度条"""
        self.update(self.total, "完成  ")
        print()


# ============================================================================
# 异常定义
# ============================================================================


class BuildError(Exception):
    """构建错误基类"""

    pass


class EnvironmentError(BuildError):
    """环境错误"""

    pass


class ConfigurationError(BuildError):
    """配置错误"""

    pass


class CompilationError(BuildError):
    """编译错误"""

    pass


# ============================================================================
# 统计信息
# ============================================================================


class BuildStatistics:
    """构建统计信息"""

    def __init__(self):
        self.start_time: float = time.time()
        self.configure_time: float = 0
        self.build_time: float = 0
        self.warnings: int = 0
        self.errors: int = 0

    def report(self, logger: logging.Logger):
        """生成构建统计报告"""
        total_time = time.time() - self.start_time
        logger.info("=" * 60)
        logger.info("构建统计报告")
        logger.info("=" * 60)
        logger.info(f"  总耗时:     {total_time:.2f}s")
        logger.info(f"  配置耗时:   {self.configure_time:.2f}s")
        logger.info(f"  构建耗时:   {self.build_time:.2f}s")
        logger.info(f"  警告数量:   {self.warnings}")
        logger.info(f"  错误数量:   {self.errors}")
        logger.info("=" * 60)


# ============================================================================
# 构建管理器
# ============================================================================


class BuildManager:
    def __init__(self, verbose: bool = False, build_dir: Optional[Path] = None, qt_path: Optional[str] = None):
        self.project_root = Path(__file__).parent.parent.parent.absolute()
        self.build_dir = build_dir if build_dir else self.project_root / "build"
        self.build_cache_dir = self.build_dir / ".cache"
        self.build_logs_dir = self.build_dir / "logs"
        self.verbose = verbose
        self.qt_path = qt_path
        self.config = self._load_config()
        self.stats = BuildStatistics()
        self._ensure_build_dirs()
        self.logger = self._setup_logging(verbose)

        # 初始 CMake 选项
        self.cmake_options = self.config.get("cmake_options", {})

    def _ensure_build_dirs(self) -> None:
        """确保构建相关目录存在"""
        self.build_dir.mkdir(parents=True, exist_ok=True)
        self.build_cache_dir.mkdir(parents=True, exist_ok=True)
        self.build_logs_dir.mkdir(parents=True, exist_ok=True)

    def _setup_logging(self, verbose: bool) -> logging.Logger:
        """配置日志系统，含日志轮转边界优化"""
        level = logging.DEBUG if verbose else logging.INFO
        logger = logging.getLogger("BuildManager")
        logger.setLevel(level)

        formatter = logging.Formatter("%(asctime)s - %(levelname)s - %(message)s")

        # 日志轮转：严格保留最近 10 个历史日志
        try:
            log_files = sorted(
                self.build_logs_dir.glob("build_*.log"), key=lambda f: f.stat().st_mtime
            )
            max_logs = 10
            if len(log_files) > max_logs:
                for old_file in log_files[:-max_logs]:
                    old_file.unlink()
        except Exception as e:
            print(f"警告: 清理旧日志失败: {e}")

        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        log_filename = f"build_{timestamp}.log"

        file_handler = logging.FileHandler(
            self.build_dir / "build.log", encoding="utf-8", mode="w"
        )
        file_handler.setFormatter(formatter)
        logger.addHandler(file_handler)

        hist_handler = logging.FileHandler(
            self.build_logs_dir / log_filename, encoding="utf-8"
        )
        hist_handler.setFormatter(formatter)
        logger.addHandler(hist_handler)

        stream_handler = logging.StreamHandler(sys.stdout)
        stream_handler.setFormatter(formatter)
        logger.addHandler(stream_handler)

        return logger

    def _load_config(self) -> Dict[str, Any]:
        """加载构建配置，支持平台特定设置"""
        config_file = self.project_root / "tools" / "config" / "build_config.json"
        default_config = {
            "qt_version": "6.10.2",
            "cmake_options": {
                "ENABLE_SYMBOL_FOOTPRINT_DEBUG_EXPORT": True,
                "EASYKICONVERTER_BUILD_TESTS": False,
            },
            "build_types": ["Debug", "Release", "RelWithDebInfo", "MinSizeRel"],
        }

        if config_file.exists():
            try:
                with open(config_file, "r", encoding="utf-8") as f:
                    user_config = json.load(f)

                    # 确定当前平台
                    current_platform = platform.system().lower()
                    platform_key = {
                        "windows": "windows",
                        "linux": "linux",
                        "darwin": "macos",
                    }.get(current_platform)

                    # 获取平台映射表
                    platform_map = user_config.get("_platform_map", {})

                    # 应用平台特定的 qt_path（通过映射表查找）
                    if platform_key and platform_map:
                        mapping = platform_map.get(platform_key, {})
                        qt_path_key = mapping.get("qt_path_key")
                        generator_key = mapping.get("generator_key")

                        if qt_path_key and qt_path_key in user_config:
                            default_config["qt_path"] = user_config[qt_path_key]

                        if generator_key and generator_key in user_config:
                            default_config["cmake_generator"] = user_config[generator_key]

                    # 应用 cmake_options
                    if "cmake_options" in user_config:
                        if isinstance(user_config["cmake_options"], dict):
                            default_config["cmake_options"].update(
                                user_config["cmake_options"]
                            )

                    # 应用其他顶层配置
                    for key in ["qt_version", "build_types"]:
                        if key in user_config:
                            default_config[key] = user_config[key]

            except Exception as e:
                print(f"警告: 加载配置文件失败, 使用默认配置: {e}")

        return default_config

    def _get_cmake_generator(self) -> str:
        """获取 CMake 生成器，优先使用配置值，与 VSCode 保持一致"""
        # 优先使用配置文件中的生成器设置
        configured_generator = self.config.get("cmake_generator")
        if configured_generator:
            return configured_generator

        # 自动检测逻辑（备用）
        if platform.system() == "Windows":
            gen_info = self._get_msvc_generator()
            if gen_info:
                return gen_info
            if shutil.which("mingw32-make") or shutil.which("make"):
                return "MinGW Makefiles"
            return "NMake Makefiles"

        return "Unix Makefiles"

    def _get_msvc_generator(self) -> Optional[str]:
        """检测编译器生成器，支持 MSVC 和 MinGW"""
        # 1. 检查环境变量
        if "VCINSTALLDIR" in os.environ:
            return "Visual Studio"

        # 2. 检查常见 MSVC 安装路径
        for ver_year, ver_id in [("2022", "17"), ("2019", "16")]:
            for edition in ["Community", "Professional", "Enterprise"]:
                p = Path(
                    rf"C:\Program Files\Microsoft Visual Studio\{ver_year}\{edition}\VC\Tools\MSVC"
                )
                if p.exists():
                    return f"Visual Studio {ver_id} {ver_year}"

        # 3. 检查 MinGW/GCC
        gcc_path = shutil.which("gcc")
        if gcc_path:
            return "MinGW Makefiles"

        return None

    def _get_qt_prefix_path(self) -> Tuple[bool, Optional[str]]:
        """检测 Qt 并验证版本匹配性"""
        # 优先级：1. 命令行参数 2. 配置文件中的 qt_path 3. 环境变量/自动检测
        if self.qt_path:
            p = Path(self.qt_path)
            if (p / "lib" / "cmake" / "Qt6" / "Qt6Config.cmake").exists():
                return True, self.qt_path

        # 配置文件中的平台特定 qt_path
        config_qt_path = self.config.get("qt_path")
        if config_qt_path:
            p = Path(config_qt_path)
            if (p / "lib" / "cmake" / "Qt6" / "Qt6Config.cmake").exists():
                return True, str(p)

        qt_candidates = []

        env_vars = ["Qt6_DIR", "QTDIR", "QT_PREFIX_PATH", "Qt6_ROOT", "QT_ROOT"]
        for var in env_vars:
            env_qt = os.environ.get(var)
            if env_qt:
                qt_candidates.append(env_qt)

        cmake_qt = os.environ.get("CMAKE_PREFIX_PATH")
        if cmake_qt:
            qt_candidates.append(cmake_qt)

        if platform.system() == "Windows":
            for base in [r"C:\Qt", r"D:\Qt"]:
                base_path = Path(base)
                if base_path.exists():
                    versions = sorted(
                        [
                            d
                            for d in base_path.iterdir()
                            if d.is_dir() and d.name.startswith("6.")
                        ],
                        key=lambda d: d.name,
                        reverse=True,
                    )
                    for v in versions:
                        for arch in ["msvc2019_64", "msvc2022_64", "mingw_64"]:
                            qt_candidates.append(str(v / arch))
        else:
            qt_candidates.extend(["/usr/lib/qt6", "/usr/local/Qt-6.10.2", "/opt/Qt6"])

        expected_version = str(self.config.get("qt_version", "6.10.2"))

        # 2. 尝试从 PATH 中的 qmake 查找
        qmake_path = shutil.which("qmake")
        if qmake_path:
            qmake_dir = Path(qmake_path).parent
            qt_from_path = qmake_dir.parent
            if (qt_from_path / "lib" / "cmake" / "Qt6" / "Qt6Config.cmake").exists():
                qt_candidates.insert(0, str(qt_from_path))

        # 3. 检查候选路径
        for qt_path in qt_candidates:
            p = Path(qt_path)
            # 更宽松的检查：只要包含 6. 就可以
            if (p / "lib" / "cmake" / "Qt6" / "Qt6Config.cmake").exists():
                is_matched = (
                    expected_version.split(".")[0:2]
                    == str(p).split("/")[-1].split(".")[0:2]
                )
                return is_matched, qt_path

        return False, None

    def _get_file_hash(self, file_path: Path) -> str:
        """计算文件 MD5 哈希"""
        try:
            with open(file_path, "rb") as f:
                return hashlib.md5(f.read()).hexdigest()
        except Exception:
            return ""

    def _load_build_cache(self) -> Dict[str, str]:
        """加载构建缓存"""
        cache_file = self.build_cache_dir / "build_cache.json"
        if cache_file.exists():
            try:
                with open(cache_file, "r", encoding="utf-8") as f:
                    return json.load(f)
            except Exception:
                pass
        return {}

    def _save_build_cache(self, cache: Dict[str, str]) -> None:
        """保存构建缓存"""
        cache_file = self.build_cache_dir / "build_cache.json"
        try:
            self.build_cache_dir.mkdir(parents=True, exist_ok=True)
            with open(cache_file, "w", encoding="utf-8") as f:
                json.dump(cache, f, indent=2)
        except Exception as e:
            self.logger.warning(f"缓存保存失败: {e}")

    def _get_source_files_hash(self) -> str:
        """计算源码文件哈希"""
        source_dirs = ["src", "tests", "CMakeLists.txt"]
        hash_dict = {}

        for item in source_dirs:
            path = self.project_root / item
            if path.is_file():
                hash_dict[str(path)] = self._get_file_hash(path)
            elif path.is_dir():
                for ext in [".cpp", ".h", ".qml", ".cmake"]:
                    for f in path.rglob(f"*{ext}"):
                        rel_path = str(f.relative_to(self.project_root))
                        if not any(
                            x in rel_path for x in ["build/", "node_modules/", ".git/"]
                        ):
                            hash_dict[rel_path] = self._get_file_hash(f)

        return json.dumps(hash_dict, sort_keys=True)

    def _check_incremental_build_possible(self) -> Tuple[bool, Optional[str]]:
        """
        检查是否可以使用增量构建
        返回: (possible, reason)
        """
        cache = self._load_build_cache()
        last_build_hash = cache.get("source_hash", "")
        current_hash = self._get_source_files_hash()

        if not last_build_hash:
            return False, "首次构建"

        if last_build_hash != current_hash:
            return False, "源码已变更"

        if (
            not (self.build_dir / "Makefile").exists()
            and not (self.build_dir / "build.ninja").exists()
        ):
            return False, "构建目录配置不完整"

        return True, "源码未变更，可跳过构建"

    def check_environment(self, full_check: bool = False) -> bool:
        """
        检查构建环境

        Args:
            full_check: 是否执行完整检查（包括翻译工具、代码格式化工具）
        """
        self.logger.info("=" * 60)
        self.logger.info("正在检查构建环境...")
        checks = []

        self.logger.info("")
        self.logger.info("[基础构建工具]")
        # 1. CMake
        cmake_path = shutil.which("cmake")
        if cmake_path:
            res = subprocess.run(["cmake", "--version"], capture_output=True, text=True)
            version = res.stdout.splitlines()[0]
            self.logger.info(f"  ✓ CMake: {version}")
            checks.append(("CMake", True))
        else:
            self.logger.error("  ✗ CMake: 未找到 (建议安装 CMake 3.16+)")
            checks.append(("CMake", False))

        # 2. Git
        git_path = shutil.which("git")
        if git_path:
            res = subprocess.run(["git", "--version"], capture_output=True, text=True)
            version = res.stdout.strip()
            self.logger.info(f"  ✓ Git: {version}")
        else:
            self.logger.warning("  ⚠ Git: 未找到 (可选，用于版本管理)")

        # 3. Ninja (可选)
        ninja_path = shutil.which("ninja")
        if ninja_path:
            self.logger.info(f"  ✓ Ninja: 已安装")
        else:
            self.logger.warning("  ⚠ Ninja: 未找到 (可选，使用 Makefiles)")

        self.logger.info("")
        self.logger.info("[编译器]")
        # 4. 编译器检查
        if platform.system() == "Windows":
            gen = self._get_msvc_generator()
            if not gen:
                gcc_path = shutil.which("gcc")
                if gcc_path:
                    try:
                        res = subprocess.run(
                            ["gcc", "--version"], capture_output=True, text=True
                        )
                        version = res.stdout.splitlines()[0]
                        self.logger.info(f"  ✓ GCC: {version}")
                        gen = "MinGW Makefiles"
                    except:
                        pass
            checks.append(("编译器", bool(gen)))
            if gen:
                self.logger.info(f"  ✓ 编译器生成器: {gen}")
        else:
            compiler = shutil.which("gcc") or shutil.which("clang")
            if compiler:
                try:
                    res = subprocess.run(
                        [compiler, "--version"], capture_output=True, text=True
                    )
                    version = res.stdout.splitlines()[0]
                    compiler_name = "GCC" if "gcc" in compiler else "Clang"
                    self.logger.info(f"  ✓ {compiler_name}: {version}")
                except:
                    pass
            else:
                self.logger.error("  ✗ C++ 编译器: 未找到")
                checks.append(("C++编译器", False))

        self.logger.info("")
        self.logger.info("[Qt6 环境]")
        # 5. Qt6 检查
        is_matched, qt_path = self._get_qt_prefix_path()
        if not qt_path:
            qmake_path = shutil.which("qmake")
            if qmake_path:
                try:
                    res = subprocess.run(
                        ["qmake", "-v"], capture_output=True, text=True
                    )
                    if "Qt version" in res.stdout:
                        self.logger.debug(f"qmake: {res.stdout.splitlines()[-1]}")
                        qt_path = str(Path(qmake_path).parent.parent)
                        is_matched = True
                except:
                    pass

        checks.append(("Qt6", bool(qt_path)))
        if qt_path:
            if is_matched:
                self.logger.info(f"  ✓ Qt6: {qt_path}")
            else:
                self.logger.info(f"  ✓ Qt6: {qt_path} (版本可能不匹配)")
        else:
            self.logger.error("  ✗ Qt6: 未找到")

        if full_check:
            self.logger.info("")
            self.logger.info("[翻译工具]")
            # 6. lupdate
            lupdate_path = self._find_qt_tool("lupdate", qt_path)
            if lupdate_path:
                self.logger.info(f"  ✓ lupdate: {lupdate_path}")
            else:
                self.logger.warning("  ⚠ lupdate: 未找到 (翻译提取需要)")

            # 7. lrelease
            lrelease_path = self._find_qt_tool("lrelease", qt_path)
            if lrelease_path:
                self.logger.info(f"  ✓ lrelease: {lrelease_path}")
            else:
                self.logger.warning("  ⚠ lrelease: 未找到 (翻译编译需要)")

            self.logger.info("")
            self.logger.info("[代码格式化工具]")
            # 8. clang-format
            clang_format_path = shutil.which("clang-format")
            if clang_format_path:
                try:
                    res = subprocess.run(
                        ["clang-format", "--version"], capture_output=True, text=True
                    )
                    version = res.stdout.splitlines()[0] if res.stdout else "已安装"
                    self.logger.info(f"  ✓ clang-format: {version}")
                except:
                    self.logger.info("  ✓ clang-format: 已安装")
            else:
                self.logger.warning("  ⚠ clang-format: 未找到 (代码格式化需要)")

            # 9. qmlformat
            qmlformat_path = self._find_qt_tool("qmlformat", qt_path)
            if qmlformat_path:
                try:
                    res = subprocess.run(
                        [qmlformat_path, "--version"], capture_output=True, text=True
                    )
                    version = res.stdout.splitlines()[0] if res.stdout else "已安装"
                    self.logger.info(f"  ✓ qmlformat: {version}")
                except:
                    self.logger.info("  ✓ qmlformat: 已安装")
            else:
                self.logger.warning("  ⚠ qmlformat: 未找到 (QML 格式化需要)")

        failed_checks = [name for name, success in checks if not success]
        self.logger.info("")
        if failed_checks:
            self.logger.error(f"环境检查失败: {', '.join(failed_checks)}")
            self.logger.info("=" * 60)
            return False

        self.logger.info("环境检查通过")
        self.logger.info("=" * 60)
        return True

    def _find_qt_tool(self, tool: str, qt_path: Optional[str]) -> Optional[str]:
        """查找项目 Qt 安装中的工具，避免依赖系统 PATH。"""
        path_tool = shutil.which(tool)
        if path_tool:
            return path_tool
        if qt_path:
            candidate = Path(qt_path) / "bin" / tool
            if candidate.is_file() and os.access(candidate, os.X_OK):
                return str(candidate)
        return None

    def clean_build_dir(self, force: bool = False) -> bool:
        """清理构建目录，含确认提示"""
        if not self.build_dir.exists():
            return True

        if not force:
            try:
                confirm = input(f"确定要清理构建目录 {self.build_dir} 吗? (y/N): ")
                if confirm.lower() != "y":
                    self.logger.info("已取消清理操作")
                    return True
            except EOFError:
                # 在非交互环境下默认不清理
                self.logger.warning("非交互环境，跳过清理确认")
                return False

        self.logger.info(f"正在清理构建目录: {self.build_dir}")
        try:
            shutil.rmtree(self.build_dir)
            return True
        except Exception as e:
            self.logger.error(f"清理失败: {e}")
            return False

    def configure_cmake(self, build_type: str) -> bool:
        """配置 CMake (POSIX 路径处理)"""
        start_time = time.time()
        self.logger.info("=" * 60)
        self.logger.info(f"正在配置 CMake (类型: {build_type})")

        if not self.build_dir.exists():
            self.build_dir.mkdir(parents=True)

        os.chdir(self.build_dir)
        generator = self._get_cmake_generator()
        self.logger.info(f"选择生成器: {generator}")

        cmake_cmd = [
            "cmake",
            PurePath(self.project_root).as_posix(),
            f"-DCMAKE_BUILD_TYPE={build_type}",
            "-G",
            generator,
        ]

        if "Visual Studio" in generator:
            cmake_cmd.extend(["-A", "x64"])

        _, qt_prefix = self._get_qt_prefix_path()
        if qt_prefix:
            posix_qt = PurePath(qt_prefix).as_posix()
            cmake_cmd.append(f"-DCMAKE_PREFIX_PATH={posix_qt}")
            self.logger.info(f"  关联 Qt6: {posix_qt}")

        for opt, val in self.cmake_options.items():
            cmake_cmd.append(f"-D{opt}={'ON' if val else 'OFF'}")

        self.logger.debug(f"执行命令: {' '.join(cmake_cmd)}")

        try:
            result = subprocess.run(cmake_cmd, capture_output=True, text=True)
            if result.returncode == 0:
                self.stats.configure_time = time.time() - start_time
                self.logger.info(
                    f"CMake 配置成功 (耗时: {self.stats.configure_time:.2f}s)"
                )
                self.logger.info("=" * 60)
                return True
            else:
                self.logger.error(f"CMake 配置失败:\n{result.stderr}")
                return False
        except Exception as e:
            self.logger.error(f"CMake 配置异常: {e}")
            return False

    def build_project(self, build_type: str, jobs: Optional[int] = None) -> bool:
        """执行构建，优化进度输出"""
        start_time = time.time()
        self.logger.info("=" * 60)
        self.logger.info(f"开始构建项目 (类型: {build_type})")

        can_incremental, reason = self._check_incremental_build_possible()
        if can_incremental:
            self.logger.info(f"增量构建: {reason}")
            self.logger.info("使用 'cmake --build . --parallel' 继续构建")
        else:
            self.logger.info(f"完整构建: {reason}")

        if jobs is None:
            jobs = os.cpu_count() or 4
        self.logger.info(f"并行度: {jobs}")

        build_cmd = ["cmake", "--build", ".", "--parallel", str(jobs)]

        try:
            process = subprocess.Popen(
                build_cmd,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                universal_newlines=True,
                bufsize=1,
                cwd=self.build_dir,
            )

            progress_bar = None
            last_update_time = 0

            if process.stdout:
                for line in process.stdout:
                    line = line.strip()
                    if not line:
                        continue

                    is_progress = re.match(r"^\[(\d+)/(\d+)\]", line)

                    if is_progress:
                        current = int(is_progress.group(1))
                        total = int(is_progress.group(2))

                        if not self.verbose:
                            current_time = time.time()
                            if progress_bar is None:
                                progress_bar = ProgressBar(total=total, width=35)
                            if (
                                current_time - last_update_time > 0.1
                                or current == total
                            ):
                                progress_bar.update(current, "构建中")
                                last_update_time = current_time
                        else:
                            self.logger.info(line)
                    # 只匹配编译器或构建工具的诊断格式，避免把源文件名中的
                    # ComponentValidationErrorPolicy 等普通文本误判为错误。
                    lower_line = line.lower()
                    is_warning = bool(
                        re.search(r"\bwarning(?:\s+[a-z]\d+)?\s*:", lower_line)
                        or re.search(r"警告\s*[:：]", line)
                    )
                    is_error = bool(
                        re.search(r"\berror(?:\s+[a-z]\d+)?\s*:", lower_line)
                        or re.search(r"\bfatal error\b", lower_line)
                        or re.search(r"错误\s*[:：]", line)
                        or re.search(r"^\s*failed(?:\s*:|\s|$)", lower_line)
                    )

                    if is_warning:
                        self.stats.warnings += 1
                        self.logger.warning(line)
                    elif is_error:
                        self.stats.errors += 1
                        self.logger.error(line)
                    else:
                        self.logger.info(line)

            process.wait()
            if progress_bar and not self.verbose:
                progress_bar.finish()
            elif not self.verbose:
                print()
            self.stats.build_time = time.time() - start_time

            if process.returncode == 0:
                self.logger.info("=" * 60)
                self.logger.info("构建完成")
                cache = self._load_build_cache()
                cache["source_hash"] = self._get_source_files_hash()
                cache["build_type"] = build_type
                self._save_build_cache(cache)
                self.stats.report(self.logger)
                return True
            else:
                self.logger.error(f"构建失败，退出码: {process.returncode}")
                # 备份失败日志
                try:
                    shutil.copy(
                        self.build_dir / "build.log",
                        self.build_dir / "build_last_failed.log",
                    )
                    self.logger.info(
                        f"失败日志已保存至: {self.build_dir / 'build_last_failed.log'}"
                    )
                except:
                    pass
                return False
        except Exception as e:
            self.logger.error(f"构建过程中出现异常: {e}")
            return False

    def install_project(self) -> bool:
        """安装项目"""
        self.logger.info("=" * 60)
        self.logger.info("正在安装项目...")
        install_cmd = ["cmake", "--install", ".", "--prefix", "install"]
        try:
            res = subprocess.run(
                install_cmd, capture_output=True, text=True, cwd=self.build_dir
            )
            if res.returncode == 0:
                self.logger.info("安装成功 (目录: build/install)")
                self.logger.info("=" * 60)
                return True
            else:
                self.logger.error(f"安装失败: {res.stderr}")
                return False
        except Exception as e:
            self.logger.error(f"安装异常: {e}")
            return False

    def run_tests(self) -> bool:
        """运行测试，丰富反馈信息"""
        self.logger.info("=" * 60)
        self.logger.info("正在运行项目测试...")
        try:
            res = subprocess.run(
                ["ctest", "--output-on-failure"],
                capture_output=True,
                text=True,
                cwd=self.build_dir,
            )
            if res.returncode == 0:
                self.logger.info("所有测试通过！")
                self.logger.debug(f"输出结果:\n{res.stdout}")
                self.logger.info("=" * 60)
                return True
            else:
                self.logger.error(f"测试未通过 (退出码 {res.returncode}):")
                self.logger.error(res.stdout)
                self.logger.info("=" * 60)
                return False
        except Exception as e:
            self.logger.error(f"运行测试异常: {e}")
            return False


def main():
    parser = argparse.ArgumentParser(
        description="EasyKiConverter 项目构建管理工具",
        epilog="""
示例:
  python tools/python/build_project.py                    # 默认 Debug 构建
  python tools/python/build_project.py -t Release        # Release 构建
  python tools/python/build_project.py -c -y            # 强制清理目录并编译
  python tools/python/build_project.py --check           # 仅执行环境 dependencies 检查
  python tools/python/build_project.py --config-only     # 仅执行配置，不进行编译
  python tools/python/build_project.py -t Release -i     # 构建并安装
        """,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        "-t",
        "--type",
        choices=["Debug", "Release", "RelWithDebInfo", "MinSizeRel"],
        default="Debug",
        help="构建类型 (默认: Debug)",
    )
    parser.add_argument("-c", "--clean", action="store_true", help="构建前清理目录")
    parser.add_argument(
        "-y", "--yes", action="store_true", help="自动确认所有提示 (如清理确认)"
    )
    parser.add_argument("-i", "--install", action="store_true", help="构建后执行安装")
    parser.add_argument("--test", action="store_true", help="构建后运行测试")
    parser.add_argument("--coverage", action="store_true", help="构建后生成代码覆盖率报告 (需 gcovr 或 lcov)")
    parser.add_argument(
        "--check", action="store_true", help="仅执行构建环境检查 (CMake/编译器/Qt6)"
    )
    parser.add_argument(
        "--env-check",
        action="store_true",
        help="执行完整开发环境检查 (包含翻译工具和代码格式化工具)",
    )
    parser.add_argument(
        "--config-only", action="store_true", help="仅执行配置，不执行构建"
    )
    parser.add_argument("-j", "--jobs", type=int, help="并行任务数 (默认 CPU 核心数)")
    parser.add_argument(
        "-b",
        "--build-dir",
        type=str,
        help="构建目录 (默认: build，需与 VSCode cmake.buildDirectory 一致)",
    )
    parser.add_argument(
        "-q",
        "--qt-path",
        type=str,
        help="Qt 路径 (需与 VSCode CMAKE_PREFIX_PATH 一致)",
    )
    parser.add_argument("-v", "--verbose", action="store_true", help="显示详细输出")

    args = parser.parse_args()
    build_dir = Path(args.build_dir) if args.build_dir else None
    qt_path = args.qt_path if args.qt_path else None
    manager = BuildManager(verbose=args.verbose, build_dir=build_dir, qt_path=qt_path)

    # Coverage 模式: 启用覆盖率编译选项并强制运行测试
    run_tests = args.test
    if args.coverage:
        manager.cmake_options["ENABLE_COVERAGE"] = True
        manager.cmake_options["EASYKICONVERTER_BUILD_TESTS"] = True
        run_tests = True

    try:
        if args.env_check:
            success = manager.check_environment(full_check=True)
            sys.exit(0 if success else 1)

        if args.check:
            success = manager.check_environment(full_check=False)
            sys.exit(0 if success else 1)

        if not manager.check_environment():
            sys.exit(1)

        if args.clean:
            if not manager.clean_build_dir(force=args.yes):
                sys.exit(1)

        if not manager.configure_cmake(args.type):
            sys.exit(1)

        if args.config_only:
            manager.logger.info("仅执行配置完成。")
            sys.exit(0)

        if not manager.build_project(args.type, args.jobs):
            sys.exit(1)

        if run_tests:
            manager.run_tests()

        if args.coverage:
            coverage_script = (
                Path(__file__).parent / "generate_coverage.py"
            )
            manager.logger.info("=" * 60)
            manager.logger.info("生成覆盖率报告...")
            result = subprocess.run(
                [
                    sys.executable,
                    str(coverage_script),
                    "--report-only",
                    "--build-dir",
                    str(manager.build_dir),
                ],
                capture_output=False,
            )
            if result.returncode != 0:
                manager.logger.warning("覆盖率报告生成失败，但构建已完成")

        if args.install:
            manager.install_project()

    except KeyboardInterrupt:
        print("\n构建被用户中断")
        sys.exit(1)
    except Exception as e:
        print(f"\n发生未处理的错误: {e}")
        sys.exit(1)


if __name__ == "__main__":
    main()
