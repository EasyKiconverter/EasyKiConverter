#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

# 固定外部打包工具版本，避免 continuous 构建结果随上游变化。
LINUXDEPLOY_VERSION="1-alpha-20240109-1"
LINUXDEPLOY_PLUGIN_QT_VERSION="1-alpha-20240109-1"
NFPM_VERSION="2.36.0"

usage() {
    cat <<'EOF'
Usage:
  ci-package.sh install-linuxdeploy <x86_64|aarch64>
  ci-package.sh fix-permissions <appdir>
  ci-package.sh package-appimage <appdir> <product> <version> <git_hash> <appimage_arch>
  ci-package.sh package-nfpm <deb|rpm|archlinux> <appdir> <product> <version> <git_hash> <nfpm_arch> <output_arch> <extension>
EOF
}

install_linuxdeploy() {
    local linuxdeploy_arch="$1"

    sudo mkdir -p /opt/linuxdeploy /opt/linuxdeploy-plugin-qt

    wget -q -O /tmp/linuxdeploy \
        "https://github.com/linuxdeploy/linuxdeploy/releases/download/${LINUXDEPLOY_VERSION}/linuxdeploy-${linuxdeploy_arch}.AppImage"
    chmod +x /tmp/linuxdeploy
    /tmp/linuxdeploy --appimage-extract
    sudo cp -r squashfs-root/* /opt/linuxdeploy/
    rm -rf squashfs-root
    sudo ln -sf /opt/linuxdeploy/AppRun /usr/local/bin/linuxdeploy

    wget -q -O /tmp/linuxdeploy-plugin-qt \
        "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/${LINUXDEPLOY_PLUGIN_QT_VERSION}/linuxdeploy-plugin-qt-${linuxdeploy_arch}.AppImage"
    chmod +x /tmp/linuxdeploy-plugin-qt
    /tmp/linuxdeploy-plugin-qt --appimage-extract
    sudo cp -r squashfs-root/* /opt/linuxdeploy-plugin-qt/
    rm -rf squashfs-root
    sudo ln -sf /opt/linuxdeploy-plugin-qt/AppRun /usr/local/bin/linuxdeploy-plugin-qt

    which linuxdeploy || echo "ERROR: linuxdeploy not found"
    /opt/linuxdeploy/AppRun --version || echo "ERROR: linuxdeploy not executable"

    {
        echo "/opt/linuxdeploy"
        echo "/opt/linuxdeploy-plugin-qt"
    } >> "$GITHUB_PATH"
    echo "DEPLOY_MODE=unpackaged" >> "$GITHUB_ENV"
}

fix_permissions() {
    local appdir="$1"

    find "$appdir" -type f -name "easykiconverter" -exec chmod 755 {} \;
    find "$appdir/usr/bin" -type f -exec chmod 755 {} \; 2>/dev/null || true
    find "$appdir/usr/libexec" -type f -exec chmod 755 {} \; 2>/dev/null || true
    chmod 755 "$appdir/AppRun" 2>/dev/null || true
    # linuxdeploy 重新包装 AppRun 时可能生成 AppRun.wrapped，且不会保留执行权限。
    # 如果该文件不可执行，AppImage 解包运行会以 126 退出。
    chmod 755 "$appdir/AppRun.wrapped" 2>/dev/null || true
}

ensure_qt_quick_controls_library() {
    local appdir="$1"
    local required_library="$appdir/usr/lib/libQt6QuickControls2.so.6"

    if [ -f "$required_library" ] && [ ! -L "$required_library" ]; then
        return
    fi

    local qt_libs="${QT_LIBS:-}"
    if [ -z "$qt_libs" ] && command -v qmake >/dev/null 2>&1; then
        qt_libs="$(qmake -query QT_INSTALL_LIBS 2>/dev/null || true)"
    fi

    local source_library=""
    if [ -d "$qt_libs" ]; then
        source_library="$(find "$qt_libs" -maxdepth 1 -type f \
            \( -name "libQt6QuickControls2.so.6" -o -name "libQt6QuickControls2.so.6.*" \) \
            -print -quit)"
    fi
    if [ -z "$source_library" ] && [ -d /opt/qt ]; then
        source_library="$(find /opt/qt -type f \
            \( -name "libQt6QuickControls2.so.6" -o -name "libQt6QuickControls2.so.6.*" \) \
            -print -quit 2>/dev/null)"
    fi
    if [ -z "$source_library" ]; then
        echo "ERROR: Qt Quick Controls 2 library is missing from AppDir and Qt installation" >&2
        exit 1
    fi

    mkdir -p "$(dirname "$required_library")"
    rm -f "$required_library"
    cp -L "$source_library" "$required_library"
    if [ ! -f "$required_library" ] || [ -L "$required_library" ]; then
        echo "ERROR: Qt Quick Controls 2 library was not restored as a regular file" >&2
        exit 1
    fi
    echo "✓ Restored libQt6QuickControls2.so.6 from $source_library"
}

render_nfpm_config() {
    local appdir="$1"
    local version="$2"
    local nfpm_arch="$3"
    local output="$4"

    python3 "$PROJECT_ROOT/tools/python/render_nfpm_config.py" \
        --template "$PROJECT_ROOT/deploy/nfpm.yaml" \
        --output "$output" \
        --project-root "$PROJECT_ROOT" \
        --app-dir "$appdir" \
        --version "$version" \
        --arch "$nfpm_arch"
}

package_appimage() {
    local appdir="$1"
    local product="$2"
    local version="$3"
    local git_hash="$4"
    local appimage_arch="$5"
    local output_name="${product}-${version}-linux-${appimage_arch}.AppImage"

    for qt_lib_dir in /opt/qt/*/*/plugins/sqldrivers; do
        if [ -d "$qt_lib_dir" ]; then
            echo "Removing host Qt SQL drivers from: $qt_lib_dir"
            rm -f "$qt_lib_dir"/libqsql*.so* 2>/dev/null || true
        fi
    done

    fix_permissions "$appdir"

    # build-core 阶段已经完成依赖部署。这里不能再次运行 linuxdeploy，
    # 否则它会重写自定义 AppRun 并生成 AppRun.wrapped，导致 AppImage
    # 启动时绕过 AppRun 中的 LD_LIBRARY_PATH 配置。
    sed -i "s|^Exec=.*easykiconverter|Exec=AppRun|" "$appdir/io.github.tangsangsimida.easykiconverter.desktop"
    sed -i "s|^Exec=.*easykiconverter|Exec=AppRun|" "$appdir/usr/share/applications/io.github.tangsangsimida.easykiconverter.desktop"

    # artifact 传输可能丢失 Qt 主库的符号链接目标。
    # 在 appimagetool 读取 AppDir 前强制恢复为真实文件，避免运行时退出 127。
    ensure_qt_quick_controls_library "$appdir"

    # appimagetool 读取 AppDir 前再次修复启动文件的执行权限。
    fix_permissions "$appdir"

    local appimagetool="/opt/linuxdeploy/plugins/linuxdeploy-plugin-appimage/usr/bin/appimagetool"
    if [ ! -x "$appimagetool" ]; then
        echo "ERROR: appimagetool not found or not executable: $appimagetool" >&2
        exit 1
    fi
    ARCH="$appimage_arch" "$appimagetool" "$appdir"

    chmod +x "${product}"-*.AppImage
    mv "${product}"-*.AppImage "$output_name"
    sha256sum "$output_name" | tee "${output_name}.sha256sum"
}

package_nfpm() {
    local packager="$1"
    local appdir="$2"
    local product="$3"
    local version="$4"
    local git_hash="$5"
    local nfpm_arch="$6"
    local output_arch="$7"
    local extension="$8"
    local nfpm_config="nfpm_temp.yaml"
    local output_name="${product}-${version}-linux-${output_arch}.${extension}"

    fix_permissions "$appdir"
    render_nfpm_config "$appdir" "$version" "$nfpm_arch" "$nfpm_config"
    nfpm pkg --packager "$packager" --target . --config "$nfpm_config"

    find . -maxdepth 1 -name "*.${extension}" -exec mv {} "$output_name" \;
    sha256sum "$output_name" | tee "${output_name}.sha256sum"
}

main() {
    if [ "$#" -lt 1 ]; then
        usage
        exit 2
    fi

    local command="$1"
    shift

    case "$command" in
        install-linuxdeploy)
            [ "$#" -eq 1 ] || { usage; exit 2; }
            install_linuxdeploy "$1"
            ;;
        fix-permissions)
            [ "$#" -eq 1 ] || { usage; exit 2; }
            fix_permissions "$1"
            ;;
        package-appimage)
            [ "$#" -eq 5 ] || { usage; exit 2; }
            package_appimage "$@"
            ;;
        package-nfpm)
            [ "$#" -eq 8 ] || { usage; exit 2; }
            package_nfpm "$@"
            ;;
        *)
            usage
            exit 2
            ;;
    esac
}

main "$@"
