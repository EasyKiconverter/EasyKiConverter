#!/usr/bin/env python3
"""验证 Python 构建工具的 Qt 版本检测边界。"""

import subprocess
import sys
import tempfile
import unittest
from os import environ
from pathlib import Path
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools" / "python"))

from build_project import BuildManager, MIN_QT_VERSION, QtInstallation

# 清空 Qt 环境变量，让 PATH 用例独立于开发者本机配置。
QT_ENV = dict.fromkeys(("Qt6_DIR", "QTDIR", "QT_PREFIX_PATH", "Qt6_ROOT", "QT_ROOT", "CMAKE_PREFIX_PATH"), "")


class BuildProjectQtDetectionTest(unittest.TestCase):
    """覆盖 qmake6、缺少版本工具和低版本 Qt 的构建检查行为。"""

    def _create_qt_layout(self, root: Path, qmake_name: str = "") -> Path:
        qt_path = root / "qt"
        (qt_path / "bin").mkdir(parents=True)
        (qt_path / "lib/cmake/Qt6").mkdir(parents=True)
        (qt_path / "lib/cmake/Qt6/Qt6Config.cmake").write_text("# test Qt config\n", encoding="utf-8")
        if qmake_name:
            (qt_path / "bin" / qmake_name).touch()
        return qt_path

    def _create_manager(self, root: Path, qt_path: Path = None) -> BuildManager:
        return BuildManager(build_dir=root / "build", qt_path=str(qt_path) if qt_path else None)

    def test_qmake6_is_used_when_qmake_is_missing(self):
        for system, qmake_name in (("Linux", "qmake6"), ("Windows", "qmake6.exe")):
            with self.subTest(system=system), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                qt_path = self._create_qt_layout(root, qmake_name)
                manager = self._create_manager(root, qt_path)
                qmake_path = qt_path / "bin" / qmake_name
                completed = subprocess.CompletedProcess(
                    args=[str(qmake_path)], returncode=0, stdout="6.10.2\n", stderr=""
                )

                with mock.patch("build_project.platform.system", return_value=system), mock.patch(
                    "build_project.subprocess.run", return_value=completed
                ) as run:
                    self.assertEqual(manager._get_qt_version(manager._find_qt_installation()), (6, 10, 2))

                self.assertEqual(run.call_args.args[0][0], str(qmake_path))

    def test_path_qmake_without_qt_cmake_config_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            shim_path = root / "shims" / "qmake6.exe"
            shim_path.parent.mkdir(parents=True)
            shim_path.touch()
            manager = self._create_manager(root)
            manager.config["qt_path"] = ""
            completed = subprocess.CompletedProcess(
                args=[str(shim_path)], returncode=0, stdout=str(root / "qt-without-cmake"), stderr=""
            )

            with mock.patch.dict(environ, QT_ENV), mock.patch(
                "build_project.platform.system", return_value="Windows"
            ), mock.patch(
                "build_project.shutil.which",
                side_effect=lambda name: str(shim_path) if name == "qmake6.exe" else None,
            ), mock.patch("build_project.subprocess.run", return_value=completed):
                self.assertIsNone(manager._find_qt_installation())

    def test_path_prefers_qmake6_over_qmake(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)  # 两套 Qt 安装所在的临时目录。
            preferred = self._create_qt_layout(root / "preferred", "qmake6")  # qmake6 对应的安装。
            fallback = self._create_qt_layout(root / "fallback", "qmake")  # qmake 对应的安装。
            qmake6 = preferred / "bin/qmake6"  # 预期优先使用的工具路径。
            tools = {"qmake6": str(qmake6), "qmake": str(fallback / "bin/qmake")}  # 模拟 PATH 命令。
            manager = self._create_manager(root)  # 被测构建管理器。
            manager.config["qt_path"] = ""

            properties = {"QT_INSTALL_PREFIX": str(preferred), "QT_INSTALL_LIBS": str(preferred / "lib")}
            with mock.patch.dict(environ, QT_ENV), mock.patch(
                "build_project.platform.system", return_value="Linux"
            ), mock.patch("build_project.shutil.which", side_effect=tools.get), mock.patch.object(
                manager, "_query_qmake", side_effect=lambda path, key: properties[key]
            ) as query:
                self.assertEqual(
                    manager._find_qt_installation(),
                    QtInstallation(preferred, preferred / "lib/cmake/Qt6", qmake6),
                )
            self.assertEqual(query.call_args_list, [
                mock.call(qmake6, "QT_INSTALL_PREFIX"),
                mock.call(qmake6, "QT_INSTALL_LIBS"),
            ])

    def test_path_multiarch_qt_config_is_passed_to_cmake(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)  # 模拟系统安装的临时目录。
            prefix = root / "usr"  # qmake 报告的安装前缀。
            lib_dir = prefix / "lib/x86_64-linux-gnu"  # 架构专用的库目录。
            config_dir = lib_dir / "cmake/Qt6"  # 需要传给 CMake 的 Qt 配置目录。
            config_dir.mkdir(parents=True)
            (config_dir / "Qt6Config.cmake").touch()
            shim = root / "qmake6"  # 位于安装前缀之外的 PATH 工具。
            shim.touch()
            manager = self._create_manager(root)  # 被测构建管理器。
            manager.config["qt_path"] = ""
            properties = {  # 模拟 qmake 报告的安装和版本信息。
                "QT_INSTALL_PREFIX": str(prefix),
                "QT_INSTALL_LIBS": str(lib_dir),
                "QT_VERSION": "6.10.2",
            }

            with mock.patch.dict(environ, QT_ENV), mock.patch(
                "build_project.platform.system", return_value="Linux"
            ), mock.patch(
                "build_project.shutil.which", side_effect=lambda name: str(shim) if name == "qmake6" else None
            ), mock.patch.object(
                manager, "_query_qmake", side_effect=lambda path, key: properties.get(key) if path == shim else None
            ), mock.patch.object(manager, "_get_cmake_generator", return_value="Ninja"), mock.patch(
                "build_project.os.chdir"
            ), mock.patch("build_project.subprocess.run", return_value=mock.Mock(returncode=0)) as run:
                installation = manager._find_qt_installation()  # PATH 探测选中的安装。
                self.assertEqual(installation, QtInstallation(prefix, config_dir, shim))
                self.assertEqual(manager._get_qt_version(installation), (6, 10, 2))
                self.assertTrue(manager.configure_cmake("Release"))

            self.assertIn(f"-DQt6_DIR={config_dir.as_posix()}", run.call_args.args[0])

    def test_missing_qmake_version_tool_returns_unknown(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            qt_path = self._create_qt_layout(root)
            manager = self._create_manager(root, qt_path)

            with mock.patch("build_project.shutil.which", return_value=None):
                self.assertIsNone(manager._get_qt_version(manager._find_qt_installation()))

            with mock.patch.object(manager, "_get_qt_version", return_value=None):
                self.assertTrue(manager.check_environment())

    def test_qt_below_minimum_version_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            qt_path = self._create_qt_layout(root, "qmake6")
            manager = self._create_manager(root, qt_path)

            with mock.patch.object(manager, "_get_qt_version", return_value=(6, 7, 3)):
                self.assertFalse(manager.check_environment())

            self.assertLess((6, 7, 3), MIN_QT_VERSION)


if __name__ == "__main__":
    unittest.main()
