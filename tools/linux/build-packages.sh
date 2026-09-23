#!/bin/bash
# 本地打包脚本 - 用于在本地测试 DEB 和 AppImage 打包流程
# 用法: ./tools/linux/build-packages.sh [appimage|deb|all]

set -e

# ==================== 配置变量 ====================
# 可执行文件名（小写，保持与 desktop 文件一致）
BINARY_NAME="easykiconverter"
# 应用标识符（反向域名格式，全小写）
APP_ID="io.github.tangsangsimida.easykiconverter"
# =================================================

# 获取脚本所在目录的绝对路径
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# 项目根目录是脚本目录向上两级（tools/linux -> tools -> 项目根）
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
cd "$PROJECT_ROOT"

HOST_ARCH="$(uname -m)"
case "$HOST_ARCH" in
    x86_64|amd64)
        LINUXDEPLOY_ARCH="x86_64"
        APPIMAGE_ARCH="x86_64"
        NFPM_ARCH="amd64"
        ;;
    aarch64|arm64)
        LINUXDEPLOY_ARCH="aarch64"
        APPIMAGE_ARCH="aarch64"
        NFPM_ARCH="arm64"
        ;;
    *)
        echo "不支持的架构: $HOST_ARCH" >&2
        exit 1
        ;;
esac

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# 打印带颜色的消息
print_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

print_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# 检查必要的工具
check_tools() {
    print_info "检查必要的工具..."
    
    local missing_tools=()
    local need_install=false
    
    if ! command -v cmake &> /dev/null; then
        missing_tools+=("cmake")
        need_install=true
    fi
    
    if ! command -v linuxdeploy &> /dev/null; then
        missing_tools+=("linuxdeploy")
        need_install=true
    fi
    
    if ! command -v linuxdeploy-plugin-qt &> /dev/null; then
        missing_tools+=("linuxdeploy-plugin-qt")
        need_install=true
    fi
    
    if ! command -v nfpm &> /dev/null; then
        missing_tools+=("nfpm")
        need_install=true
    fi
    
    if [ "$need_install" = true ]; then
        print_error "缺少必要的工具: ${missing_tools[*]}"
        echo ""
        print_warn "尝试自动安装缺失的工具（需要 sudo 权限）..."
        echo ""
        
        # 尝试自动安装
        if command -v sudo &> /dev/null; then
            install_tools || exit 1
        else
            print_error "需要 sudo 权限才能安装工具"
            echo ""
            echo "手动安装方法："
            echo ""
            echo "1. 安装 linuxdeploy（从 AppImage 提取，与 CI 一致）:"
            echo "   sudo mkdir -p /opt/linuxdeploy"
            echo "   wget https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-${LINUXDEPLOY_ARCH}.AppImage"
            echo "   chmod +x linuxdeploy-${LINUXDEPLOY_ARCH}.AppImage"
            echo "   ./linuxdeploy-${LINUXDEPLOY_ARCH}.AppImage --appimage-extract"
            echo "   sudo cp -r squashfs-root/* /opt/linuxdeploy/"
            echo "   sudo ln -s /opt/linuxdeploy/AppRun /usr/local/bin/linuxdeploy"
            echo "   rm -rf squashfs-root linuxdeploy-${LINUXDEPLOY_ARCH}.AppImage"
            echo ""
            echo "2. 安装 linuxdeploy Qt 插件:"
            echo "   sudo mkdir -p /opt/linuxdeploy-plugin-qt"
            echo "   wget https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-${LINUXDEPLOY_ARCH}.AppImage"
            echo "   chmod +x linuxdeploy-plugin-qt-${LINUXDEPLOY_ARCH}.AppImage"
            echo "   ./linuxdeploy-plugin-qt-${LINUXDEPLOY_ARCH}.AppImage --appimage-extract"
            echo "   sudo cp -r squashfs-root/* /opt/linuxdeploy-plugin-qt/"
            echo "   sudo ln -s /opt/linuxdeploy-plugin-qt/AppRun /usr/local/bin/linuxdeploy-plugin-qt"
            echo "   rm -rf squashfs-root linuxdeploy-plugin-qt-${LINUXDEPLOY_ARCH}.AppImage"
            echo ""
            echo "3. 安装 nfpm（从 goreleaser 官方源）:"
            echo "   echo 'deb [trusted=yes] https://repo.goreleaser.com/apt/ /' | sudo tee /etc/apt/sources.list.d/goreleaser.list"
            echo "   sudo apt-get update"
            echo "   sudo apt-get install -y nfpm"
            echo ""
            exit 1
        fi
    fi
    
    print_info "所有必要的工具已安装"
}

# 计算源码文件哈希（用于增量构建检测）
calculate_source_hash() {
    local hash_file="${PROJECT_ROOT}/build/.cache/source_hash"
    local new_hash=""
    
    # 计算源码哈希
    new_hash=$(find src tests CMakeLists.txt -type f \( -name "*.cpp" -o -name "*.h" -o -name "*.qml" -o -name "*.cmake" \) 2>/dev/null | \
        xargs md5sum 2>/dev/null | \
        sort | \
        md5sum | \
        awk '{print $1}')
    
    echo "$new_hash"
}

# 检查源码是否变更，返回 0 表示有变更，1 表示无变更
check_source_changed() {
    local hash_file="${PROJECT_ROOT}/build/.cache/source_hash"
    local current_hash=$(calculate_source_hash)
    
    # 如果缓存文件不存在，认为有变更
    if [ ! -f "$hash_file" ]; then
        return 0
    fi
    
    local saved_hash=$(cat "$hash_file" 2>/dev/null)
    
    if [ "$current_hash" != "$saved_hash" ]; then
        return 0  # 有变更
    else
        return 1  # 无变更
    fi
}

# 保存源码哈希
save_source_hash() {
    local hash_file="${PROJECT_ROOT}/build/.cache/source_hash"
    mkdir -p "${PROJECT_ROOT}/build/.cache"
    calculate_source_hash > "$hash_file"
}

# 自动安装缺失的工具
install_tools() {
    print_info "开始安装缺失的工具..."
    
    # 验证 sudo 权限并提示用户输入密码
    print_warn "需要 sudo 权限来安装工具，请输入密码..."
    if ! sudo -v; then
        print_error "无法获取 sudo 权限，请检查您的密码或联系系统管理员"
        return 1
    fi
    print_info "sudo 权限已验证，继续安装..."
    
    local temp_dir=$(mktemp -d)
    cd "$temp_dir"
    
    # 安装 cmake（如果缺失）
    if ! command -v cmake &> /dev/null; then
        print_info "安装 cmake..."
        sudo apt-get update -qq
        sudo apt-get install -y -qq cmake ninja-build || {
            print_error "无法安装 cmake"
            cd "$PROJECT_ROOT"
            rm -rf "$temp_dir"
            return 1
        }
    fi
    
    # 安装 linuxdeploy（从 AppImage 提取）
    if ! command -v linuxdeploy &> /dev/null; then
        print_info "安装 linuxdeploy（从 AppImage 提取）..."
        
        wget -q https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-${LINUXDEPLOY_ARCH}.AppImage \
            -O linuxdeploy-${LINUXDEPLOY_ARCH}.AppImage || {
            print_error "下载 linuxdeploy 失败"
            cd "$PROJECT_ROOT"
            rm -rf "$temp_dir"
            return 1
        }
        
        chmod +x linuxdeploy-${LINUXDEPLOY_ARCH}.AppImage
        ./linuxdeploy-${LINUXDEPLOY_ARCH}.AppImage --appimage-extract || {
            print_error "提取 linuxdeploy 失败"
            cd "$PROJECT_ROOT"
            rm -rf "$temp_dir"
            return 1
        }
        
        sudo mkdir -p /opt/linuxdeploy
        sudo cp -r squashfs-root/* /opt/linuxdeploy/
        sudo ln -sf /opt/linuxdeploy/AppRun /usr/local/bin/linuxdeploy
        sudo chmod +x /opt/linuxdeploy/AppRun
        
        rm -rf squashfs-root linuxdeploy-${LINUXDEPLOY_ARCH}.AppImage
        print_info "linuxdeploy 安装完成"
    fi
    
    # 安装 linuxdeploy-plugin-qt（从 AppImage 提取）
    if ! command -v linuxdeploy-plugin-qt &> /dev/null; then
        print_info "安装 linuxdeploy-plugin-qt（从 AppImage 提取）..."
        
        wget -q https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-${LINUXDEPLOY_ARCH}.AppImage \
            -O linuxdeploy-plugin-qt-${LINUXDEPLOY_ARCH}.AppImage || {
            print_error "下载 linuxdeploy-plugin-qt 失败"
            cd "$PROJECT_ROOT"
            rm -rf "$temp_dir"
            return 1
        }
        
        chmod +x linuxdeploy-plugin-qt-${LINUXDEPLOY_ARCH}.AppImage
        ./linuxdeploy-plugin-qt-${LINUXDEPLOY_ARCH}.AppImage --appimage-extract || {
            print_error "提取 linuxdeploy-plugin-qt 失败"
            cd "$PROJECT_ROOT"
            rm -rf "$temp_dir"
            return 1
        }
        
        sudo mkdir -p /opt/linuxdeploy-plugin-qt
        sudo cp -r squashfs-root/* /opt/linuxdeploy-plugin-qt/
        sudo ln -sf /opt/linuxdeploy-plugin-qt/AppRun /usr/local/bin/linuxdeploy-plugin-qt
        sudo chmod +x /opt/linuxdeploy-plugin-qt/AppRun
        
        rm -rf squashfs-root linuxdeploy-plugin-qt-${LINUXDEPLOY_ARCH}.AppImage
        print_info "linuxdeploy-plugin-qt 安装完成"
    fi
    
    # 安装 nfpm（从 goreleaser 官方源）
    if ! command -v nfpm &> /dev/null; then
        print_info "安装 nfpm（从 goreleaser 官方源）..."
        
        echo 'deb [trusted=yes] https://repo.goreleaser.com/apt/ /' | sudo tee /etc/apt/sources.list.d/goreleaser.list > /dev/null
        sudo apt-get update -qq
        sudo apt-get install -y -qq nfpm || {
            print_error "无法安装 nfpm"
            cd "$PROJECT_ROOT"
            rm -rf "$temp_dir"
            return 1
        }
        print_info "nfpm 安装完成"
    fi
    
    cd "$PROJECT_ROOT"
    rm -rf "$temp_dir"
    
    print_info "所有工具安装完成！"
}

# 获取版本信息
get_version() {
    if [ -n "$VERSION" ]; then
        # 去掉版本号中的 v 前缀（如果有）
        echo "$VERSION" | sed 's/^v//'
    else
        # 从 git tag 获取版本号，去掉 v 前缀
        git describe --tags --abbrev=0 2>/dev/null | sed 's/^v//' || echo "3.0.7"
    fi
}

# 构建 AppDir
build_appdir() {
    local VERSION=$(get_version)
    local APP_DIR="${PROJECT_ROOT}/build/EasyKiConverter.AppDir"
    local BUILD_DIR="${PROJECT_ROOT}/build/appimage-build"
    
    # 检查源码是否变更
    if [ -d "$APP_DIR" ]; then
        if check_source_changed; then
            print_warn "源码已变更，需要重新构建..."
            rm -rf "$APP_DIR" "$BUILD_DIR"
            print_info "构建 AppDir..."
        else
            print_info "源码未变更，跳过构建，使用现有 AppDir..."
            return 0
        fi
    else
        print_info "构建 AppDir..."
    fi
    
    # 创建 AppDir 结构
    mkdir -p "$APP_DIR/usr/bin"
    mkdir -p "$APP_DIR/usr/share/applications"
    mkdir -p "$APP_DIR/usr/share/icons/hicolor/512x512/apps"
    
    # 编译项目
    print_info "编译项目..."
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"
    cmake ../.. \
        -DCMAKE_BUILD_TYPE=RelWithDebInfo \
        -DCMAKE_INSTALL_PREFIX=/usr \
        -DENABLE_SYMBOL_FOOTPRINT_DEBUG_EXPORT=OFF \
        -DVERSION_FROM_CI="$VERSION"
    
    cmake --build . --config RelWithDebInfo --parallel
    
    # 安装到 AppDir
    print_info "安装到 AppDir..."
    cmake --install . --prefix "$APP_DIR/usr"
    rm -rf "$APP_DIR/usr/include"
    
    # 保存源码哈希，用于增量构建检测
    save_source_hash
    
    cd "$PROJECT_ROOT"
    
    # 创建桌面文件
    print_info "创建桌面文件..."
    cat > "$APP_DIR/$APP_ID.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=EasyKiConverter
Name[zh_CN]=EasyKiConverter
Comment=Convert LCSC and EasyEDA components to KiCad libraries
Comment[zh_CN]=将嘉立创和 EasyEDA 元件转换为 KiCad 库
Exec=AppRun %F
Icon=$APP_ID
Terminal=false
Categories=Development;Electronics;Engineering;
Keywords=KiCad;LCSC;EasyEDA;Component;Converter;Electronics;
StartupNotify=true
StartupWMClass=easykiconverter
MimeType=application/vnd.easyeda+json;
EOF

    # 复制图标（使用 256x256 尺寸，确保任务栏显示正常）
    print_info "复制图标到 AppDir 根目录..."
    cp "resources/icons/hicolor/256x256/apps/${APP_ID}.png" "$APP_DIR/$APP_ID.png"
    if [ ! -f "$APP_DIR/$APP_ID.png" ]; then
        print_warn "256x256 图标不存在，尝试使用备用路径..."
        cp resources/icons/app_icon.png "$APP_DIR/$APP_ID.png"
    fi

    # 运行 linuxdeploy 填充依赖
    print_info "运行 linuxdeploy 填充依赖..."
    linuxdeploy --appdir "$APP_DIR" --plugin qt \
        --executable "$APP_DIR/usr/bin/$BINARY_NAME" \
        --desktop-file "$APP_DIR/$APP_ID.desktop" \
        --icon-file "$APP_DIR/$APP_ID.png"

    # 修复：linuxdeploy 会覆盖 Exec 字段，需要手动改回 AppRun
    print_info "修复 desktop 文件的 Exec 字段..."
    sed -i "s|^Exec=.*$BINARY_NAME|Exec=AppRun|" "$APP_DIR/$APP_ID.desktop"

    # 安装 hicolor 图标到 AppDir（确保所有尺寸的图标都可用）
    print_info "安装 hicolor 图标（浅色主题）..."
    for size in 16 24 32 48 64 128 256 512; do
        mkdir -p "$APP_DIR/usr/share/icons/hicolor/${size}x${size}/apps"
        cp "resources/icons/hicolor/${size}x${size}/apps/${APP_ID}.png" \
           "$APP_DIR/usr/share/icons/hicolor/${size}x${size}/apps/${APP_ID}.png"
    done

    # 安装 hicolor-dark 图标到 AppDir（暗色主题）
    print_info "安装 hicolor-dark 图标（暗色主题）..."
    if [ -d "resources/icons/hicolor-dark" ]; then
        for size in 16 24 32 48 64 128 256 512; do
            if [ -f "resources/icons/hicolor-dark/${size}x${size}/apps/${APP_ID}.png" ]; then
                mkdir -p "$APP_DIR/usr/share/icons/hicolor-dark/${size}x${size}/apps"
                cp "resources/icons/hicolor-dark/${size}x${size}/apps/${APP_ID}.png" \
                   "$APP_DIR/usr/share/icons/hicolor-dark/${size}x${size}/apps/${APP_ID}.png"
            fi
        done
        print_info "✓ 暗色主题图标已安装"
    else
        print_warn "hicolor-dark 目录不存在，跳过暗色主题图标"
    fi
    
    # 修复：复制 QML 模块到 AppDir（linuxdeploy-plugin-qt 可能不会自动复制）
    print_info "复制 QML 模块到 AppDir..."
    local QT_INSTALL_QML=$(qmake -query QT_INSTALL_QML 2>/dev/null || echo "")
    if [ -n "$QT_INSTALL_QML" ] && [ -d "$QT_INSTALL_QML" ]; then
        rm -rf "$APP_DIR/usr/qml"
        mkdir -p "$APP_DIR/usr/qml"
        cp -r "$QT_INSTALL_QML"/* "$APP_DIR/usr/qml/"
        
        # 修复：创建插件符号链接（Qt 6.10.2 使用 lib 前缀，但 qmldir 引用无前缀名称）
        print_info "创建 QML 插件符号链接..."
        find "$APP_DIR/usr/qml" -name "lib*.so" -type f | while read libfile; do
            plugin_name=$(basename "$libfile" | sed 's/^lib//')
            plugin_dir=$(dirname "$libfile")
            if [ ! -e "$plugin_dir/$plugin_name" ]; then
                ln -sf "$(basename "$libfile")" "$plugin_dir/$plugin_name"
            fi
        done
    fi
    
    # 修复：复制所有缺失的 Qt 库
    print_info "复制缺失的 Qt 库..."
    if [ -d "$APP_DIR/usr/qml" ] && [ -n "$QT_INSTALL_QML" ]; then
        local QT_LIB_DIR="$(dirname "$QT_INSTALL_QML")/lib"
        # 查找所有实际的插件文件（不是符号链接）
        find "$APP_DIR/usr/qml" -name "lib*.so" -type f 2>/dev/null | while read plugin; do
            # 使用 ldd 查找依赖的 Qt 库
            ldd "$plugin" 2>/dev/null | grep "$QT_LIB_DIR" | awk '{print $3}' | sort -u | while read lib; do
                libname=$(basename "$lib")
                if [ ! -f "$APP_DIR/usr/lib/$libname" ]; then
                    cp "$lib" "$APP_DIR/usr/lib/" 2>/dev/null && echo "  复制: $libname" || true
                fi
            done
        done
    fi
    
    # 配置 AppRun 脚本
    print_info "配置 AppRun 脚本..."
    rm -f "$APP_DIR/AppRun"
    cat > "$APP_DIR/AppRun" <<'EOF'
#!/bin/bash

# 获取 AppDir 路径
THIS_SCRIPT=$(readlink -f "$0" 2>/dev/null)
if [ -z "$THIS_SCRIPT" ]; then
    THIS_SCRIPT="$0"
fi
APPDIR="$(dirname "$THIS_SCRIPT")"

# 清除干扰的环境变量
unset QT_PLUGIN_PATH
unset QT_QPA_PLATFORM_PLUGIN_PATH
unset LD_LIBRARY_PATH
unset QTDIR
unset QT_SELECT

# 设置 AppImage 特定的环境变量
export LD_LIBRARY_PATH="${APPDIR}/usr/lib"
export PATH="${APPDIR}/usr/bin:${PATH}"
export QT_PLUGIN_PATH="${APPDIR}/usr/plugins"
export QML2_IMPORT_PATH="${APPDIR}/usr/qml"
export QT_QPA_PLATFORM=xcb
export XDG_DATA_HOME="${XDG_DATA_HOME:-${HOME}/.local/share}"
export XDG_CONFIG_HOME="${XDG_CONFIG_HOME:-${HOME}/.config}"

# 添加 XDG_DATA_DIRS，让系统能够找到 AppImage 内部的图标
# 这样桌面环境才能正确显示任务栏图标
export XDG_DATA_DIRS="${APPDIR}/usr/share:${XDG_DATA_DIRS:-/usr/local/share:/usr/share}"

# 全局变量定义（用于 desktop 文件和图标管理）
export DESKTOP_FILE="$APPDIR/__APP_ID__.desktop"
export USER_DESKTOP_DIR="$HOME/.local/share/applications"
export USER_ICON_DIR="$HOME/.local/share/icons/hicolor"
export USER_DESKTOP_PATH="$USER_DESKTOP_DIR/__APP_ID__.desktop"
export APPIMAGE_PATH="$APPIMAGE"

# 自动安装 desktop 文件和图标到用户目录（支持路径检测和更新）
install_desktop_file() {
    # 只有在 AppImage 模式下才安装 desktop 文件到用户目录
    # DEB/RPM 包使用系统目录 /usr/share/applications/ 中的 desktop 文件
    if [ -z "$APPIMAGE" ]; then
        return 0
    fi
    
    # 创建用户目录
    mkdir -p "$USER_DESKTOP_DIR"
    
    # 如果 desktop 文件不存在，创建它
    if [ ! -f "$USER_DESKTOP_PATH" ]; then
        if [ -f "$DESKTOP_FILE" ]; then
            cp "$DESKTOP_FILE" "$USER_DESKTOP_PATH"
            chmod +x "$USER_DESKTOP_PATH"
            
            # 添加 AppImage 路径标记
            if [ -n "$APPIMAGE_PATH" ]; then
                echo "X-AppImage-Path=${APPIMAGE_PATH}" >> "$USER_DESKTOP_PATH"
            fi
        fi
    fi
    
    # 检查并更新 AppImage 路径（支持 AppImage 移动）
    if [ -f "$USER_DESKTOP_PATH" ] && [ -n "$APPIMAGE_PATH" ]; then
        # 检查 desktop 文件中的 Exec 路径是否需要更新
        local CURRENT_EXEC=$(grep "^Exec=" "$USER_DESKTOP_PATH" | cut -d= -f2 | cut -d' ' -f1)
        
        if [ "$CURRENT_EXEC" != "$APPIMAGE_PATH" ]; then
            echo "AppImage 路径已更新: $CURRENT_EXEC -> $APPIMAGE_PATH" >&2
            sed -i "s|^Exec=.*|Exec=${APPIMAGE_PATH} %F|" "$USER_DESKTOP_PATH"
            
            # 更新 AppImage 路径标记
            if grep -q "^X-AppImage-Path=" "$USER_DESKTOP_PATH"; then
                sed -i "s|^X-AppImage-Path=.*|X-AppImage-Path=${APPIMAGE_PATH}|" "$USER_DESKTOP_PATH"
            else
                echo "X-AppImage-Path=${APPIMAGE_PATH}" >> "$USER_DESKTOP_PATH"
            fi
        fi
    fi
    
    # 复制图标到用户目录（如果缺失）
    for size in 16 24 32 48 64 128 256 512; do
        local ICON_DIR="$USER_ICON_DIR/${size}x${size}/apps"
        local ICON_FILE="$APPDIR/usr/share/icons/hicolor/${size}x${size}/apps/__APP_ID__.png"
        
        if [ -f "$ICON_FILE" ] && [ ! -f "$ICON_DIR/__APP_ID__.png" ]; then
            mkdir -p "$ICON_DIR"
            cp "$ICON_FILE" "$ICON_DIR/__APP_ID__.png"
        fi
    done
    
    # 复制暗色主题图标（如果缺失）
    if [ -d "$APPDIR/usr/share/icons/hicolor-dark" ]; then
        for size in 16 24 32 48 64 128 256 512; do
            local ICON_DIR="$HOME/.local/share/icons/hicolor-dark/${size}x${size}/apps"
            local ICON_FILE="$APPDIR/usr/share/icons/hicolor-dark/${size}x${size}/apps/__APP_ID__.png"
            
            if [ -f "$ICON_FILE" ] && [ ! -f "$ICON_DIR/__APP_ID__.png" ]; then
                mkdir -p "$ICON_DIR"
                cp "$ICON_FILE" "$ICON_DIR/__APP_ID__.png"
            fi
        done
    fi
    
    # 更新桌面数据库
    if command -v update-desktop-database >/dev/null 2>&1; then
        update-desktop-database "$USER_DESKTOP_DIR" >/dev/null 2>&1 &
    fi
    
    # 更新图标缓存
    if command -v gtk-update-icon-cache >/dev/null 2>&1; then
        gtk-update-icon-cache -q "$USER_ICON_DIR" >/dev/null 2>&1 &
    fi
}

# 卸载 desktop 文件和图标
uninstall_desktop_file() {
    # 等待 desktop 文件安装完成
    wait

    echo "正在卸载 EasyKiConverter AppImage..."
    echo ""

    # 删除 desktop 文件
    USER_DESKTOP_PATH="$USER_DESKTOP_DIR/__APP_ID__.desktop"
    if [ -f "$USER_DESKTOP_PATH" ]; then
        rm -f "$USER_DESKTOP_PATH"
        echo "已删除 desktop 文件: $USER_DESKTOP_PATH"
    fi

    # 删除所有尺寸的图标（浅色和暗色主题）
    removed_count=0
    for size in 16 24 32 48 64 128 256 512; do
        icon_file="$USER_ICON_DIR/${size}x${size}/apps/__APP_ID__.png"
        if [ -f "$icon_file" ]; then
            rm -f "$icon_file"
            echo "已删除图标 ($size x $size): $icon_file"
            removed_count=$((removed_count + 1))
        fi
    done

    # 删除暗色主题图标
    if [ -d "$HOME/.local/share/icons/hicolor-dark" ]; then
        for size in 16 24 32 48 64 128 256 512; do
            icon_file="$HOME/.local/share/icons/hicolor-dark/${size}x${size}/apps/__APP_ID__.png"
            if [ -f "$icon_file" ]; then
                rm -f "$icon_file"
                echo "已删除暗色图标 ($size x $size): $icon_file"
                removed_count=$((removed_count + 1))
            fi
        done
    fi

    # 更新桌面数据库
    if command -v update-desktop-database >/dev/null 2>&1; then
        update-desktop-database "$USER_DESKTOP_DIR" 2>/dev/null
        echo "桌面数据库已更新"
    fi

    # 更新图标缓存
    if command -v gtk-update-icon-cache >/dev/null 2>&1; then
        for theme in hicolor hicolor-dark; do
            if [ -d "$USER_ICON_DIR/$theme" ]; then
                gtk-update-icon-cache -q "$USER_ICON_DIR/$theme" 2>/dev/null
                echo "图标缓存已更新: $theme"
            fi
        done
    fi

    echo ""
    echo "卸载完成！"
    echo "已删除 $removed_count 个图标文件"
    echo ""
    echo "提示："
    echo "  1. 您可以手动删除 AppImage 文件"
    echo "  2. 如果任务栏图标仍然显示，请注销并重新登录"
    echo "  3. 或者重启桌面环境以刷新图标缓存"
    echo ""
    exit 0
}

# 后台安装 desktop 文件（不阻塞启动）
install_desktop_file &

# 检查是否需要卸载
if [ "$1" = "--uninstall" ]; then
    uninstall_desktop_file
fi

# 启动应用
exec "${APPDIR}/usr/bin/__BINARY_NAME__" "$@"
EOF

    # 替换可执行文件名占位符
    sed -i "s|__BINARY_NAME__|$BINARY_NAME|g" "$APP_DIR/AppRun"
    sed -i "s|__APP_ID__|$APP_ID|g" "$APP_DIR/AppRun"

    chmod +x "$APP_DIR/AppRun"

    print_info "AppDir 构建完成: $APP_DIR"
}

# 构建 AppImage
build_appimage() {
    print_info "构建 AppImage..."
    
    local VERSION=$(get_version)
    local APP_DIR="${PROJECT_ROOT}/build/EasyKiConverter.AppDir"
    
    if [ ! -d "$APP_DIR" ]; then
        print_info "AppDir 不存在，先构建 AppDir..."
        build_appdir
    fi
    
    # 生成 AppImage（使用已配置的 AppDir）
    print_info "生成 AppImage..."
    linuxdeploy --appdir "$APP_DIR" --output appimage
    
    # 修复：linuxdeploy 会覆盖 Exec 字段，需要再次修复
    # 注意：修复必须在 linuxdeploy 运行后进行
    print_info "修复 AppDir 中 desktop 文件的 Exec 字段..."
    sed -i "s|^Exec=.*$BINARY_NAME|Exec=AppRun|" "$APP_DIR/$APP_ID.desktop"
    sed -i "s|^Exec=.*$BINARY_NAME|Exec=AppRun|" "$APP_DIR/usr/share/applications/$APP_ID.desktop"
    
    # 重新打包（使用修复后的 AppDir）
    print_info "使用修复后的 AppDir 重新生成 AppImage..."
    linuxdeploy --appdir "$APP_DIR" --output appimage
    
    # 重命名
    local output_dir="${PROJECT_ROOT}/build/packages"
    mkdir -p "$output_dir"
    
    mv EasyKiConverter-*.AppImage "$output_dir/EasyKiConverter-${VERSION}-linux-${APPIMAGE_ARCH}.AppImage"
    
    # 生成校验和
    cd "$output_dir"
    sha256sum EasyKiConverter-${VERSION}-linux-${APPIMAGE_ARCH}.AppImage > EasyKiConverter-${VERSION}-linux-${APPIMAGE_ARCH}.AppImage.sha256sum
    
    print_info "AppImage 构建完成: $output_dir/EasyKiConverter-${VERSION}-linux-${APPIMAGE_ARCH}.AppImage"
}

# 构建 DEB 包
build_deb() {
    print_info "构建 DEB 包..."
    
    local VERSION=$(get_version)
    local APP_DIR="${PROJECT_ROOT}/build/EasyKiConverter.AppDir"
    
    if [ ! -d "$APP_DIR" ]; then
        print_info "AppDir 不存在，先构建 AppDir..."
        build_appdir
    fi
    
    local output_dir="${PROJECT_ROOT}/build/packages"
    mkdir -p "$output_dir"
    
    # 渲染 nfpm.yaml 中的环境变量和相对路径
    local temp_nfpm_config="${output_dir}/nfpm_temp.yaml"
    python3 "$PROJECT_ROOT/tools/python/render_nfpm_config.py" \
        --template "$PROJECT_ROOT/deploy/nfpm.yaml" \
        --output "$temp_nfpm_config" \
        --project-root "$PROJECT_ROOT" \
        --app-dir "$APP_DIR" \
        --version "$VERSION" \
        --arch "$NFPM_ARCH"
    
    # 运行 nfpm 构建（必须指定 --packager）
    export NFPM_ARCH
    nfpm package --packager deb --config "$temp_nfpm_config" --target "$output_dir" || {
        print_error "nfpm 构建失败，请检查 nfpm.yaml 配置"
        print_info "临时配置文件保留在: $temp_nfpm_config"
        cd "$PROJECT_ROOT"
        return 1
    }
    
    # 清理临时配置文件
    rm -f "$temp_nfpm_config"
    
    # 重命名
    cd "$output_dir"
    if [ -f "easykiconverter_${VERSION}-1_${NFPM_ARCH}.deb" ]; then
        mv "easykiconverter_${VERSION}-1_${NFPM_ARCH}.deb" "EasyKiConverter-${VERSION}-linux-${NFPM_ARCH}.deb"
        sha256sum "EasyKiConverter-${VERSION}-linux-${NFPM_ARCH}.deb" > "EasyKiConverter-${VERSION}-linux-${NFPM_ARCH}.deb.sha256sum"
        print_info "DEB 包构建完成: $output_dir/EasyKiConverter-${VERSION}-linux-${NFPM_ARCH}.deb"
    else
        print_warn "DEB 包文件未找到，查看生成的文件："
        ls -la *.deb 2>/dev/null || print_error "没有生成 DEB 文件"
        cd "$PROJECT_ROOT"
        return 1
    fi
    
    cd "$PROJECT_ROOT"
}

# 主函数
main() {
    local build_type="${1:-all}"
    
    print_info "开始本地打包流程..."
    print_info "项目根目录: $PROJECT_ROOT"
    print_info "版本: $(get_version)"
    
    check_tools
    
    case "$build_type" in
        appimage)
            build_appimage
            ;;
        deb)
            build_deb
            ;;
        all)
            build_appimage
            build_deb
            ;;
        *)
            print_error "未知的构建类型: $build_type"
            echo "用法: $0 [appimage|deb|all]"
            exit 1
            ;;
    esac
    
    print_info "打包完成！"
    print_info "输出目录: ${PROJECT_ROOT}/build/packages"
}

# 运行主函数
main "$@"
