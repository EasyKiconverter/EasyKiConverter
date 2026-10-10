# EasyKiConverter 编码规范

本文档旨在为 EasyKiConverter 项目的开发者提供一套统一的编码规范和最佳实践，以确保代码的一致性、可读性和可维护性。

本规范主要基于 [Qt 编码风格](https://wiki.qt.io/Qt_Coding_Style)，并结合项目自身的实践。

## 1. 命名约定 (Naming Conventions)

### 1.1. 通用规则

*   使用清晰、有描述性的英文名称。
*   避免使用缩写，除非是广为人知的（例如 `UI`, `API`）。

### 1.2. 文件名

*   C++ 源文件和头文件使用大驼峰命名法（PascalCase），例如 `ExportServicePipeline.cpp` 和 `ExportServicePipeline.h`。主入口文件为 `src/main.cpp`。
*   QML 文件使用大驼峰命名法（PascalCase），例如 `MainWindow.qml`。主入口文件为 `src/ui/qml/Main.qml`。
*   测试文件通常使用下划线命名法（snake_case），例如 `test_component_service.cpp`。

### 1.3. 类、结构体和命名空间

*   使用大驼峰命名法（PascalCase）。
*   **示例**: `ExportServicePipeline`, `ComponentData`, `EasyKiConverter`。

### 1.4. 函数和方法

*   使用小驼峰命名法（camelCase）。
*   **示例**: `executeExportPipeline()`, `handleFetchCompleted()`。

### 1.5. 变量

*   **局部变量**: 使用小驼峰命名法（camelCase）。
    *   **示例**: `int totalCount = 0;`, `QMutexLocker locker(m_mutex);`
*   **成员变量**:
    *   **公共成员**: 使用小驼峰命名法，不带任何前缀。
    *   **私有/保护成员**: 使用 `m_` 前缀，后跟小驼峰命名法。
    *   **示例**: `QThreadPool *m_fetchThreadPool;`, `int m_successCount;`
*   **全局变量**: 尽量避免使用全局变量。如果必须使用，应使用 `g_` 前缀。
*   **常量和枚举**:
    *   **常量**: 使用全大写字母和下划线（UPPER_SNAKE_CASE）。
        *   **示例**: `const int MAX_THREADS = 8;`
    *   **枚举名**: 使用大驼峰命名法。
    *   **枚举值**: 使用全大写字母和下划线。

### 1.6. QML

*   **ID**: 使用小驼峰命名法。
*   **属性 (Properties)**: 使用小驼峰命名法。

## 2. 格式化 (Formatting)

### 2.1. 缩进

*   使用 **4个空格** 进行缩进，不要使用制表符（Tab）。

### 2.2. 大括号 `{}`

*   **所有块 (函数、类、命名空间、控制语句)**: 左大括号 `{` **与控制语句同一行**（K&R 风格）。
    ```cpp
    class MyClass {
    public:
        void myFunction() {
            if (condition) {
                // ...
            } else {
                // ...
            }
        }
    };
    ```
*   **单行语句**: 即使控制语句体只有一行，也必须使用大括号。
    ```cpp
    // 正确
    if (condition)
    {
        return;
    }

    // 错误
    if (condition) return;
    ```

### 2.3. 行长度

*   尽量将行长度限制在 **120个字符** 以内。

### 2.4. 空格

*   在二元运算符（`+`, `-`, `*`, `/`, `=`, `==`, `!=`, `&&` 等）两边使用空格。
*   在逗号 `,` 和分号 `;` (在 for 循环中) 后使用空格。
*   在关键字（`if`, `for`, `while`）和其后的括号 `(` 之间使用空格。
*   在函数名和参数列表的括号 `(` 之间**不**使用空格。

### 2.5. 文件编码 (File Encoding)

*   **所有源文件**（`.cpp`, `.h`, `.qml`, `CMakeLists.txt` 等）必须使用 **UTF-8 (无 BOM)** 编码。
*   **严禁使用** UTF-8 with BOM 或 GBK 等其他编码，以确保跨平台协作时注释和字符串字面量不会出现乱码。
*   **强制工具校验**: 开发者在贡献代码和提交 PR 前，**必须使用**项目中提供的转换工具进行编码规范校验：
    *   **工具路径**: `tools/python/convert_to_utf8.py`
    *   **运行命令**: `python tools/python/convert_to_utf8.py` (默认扫描整个项目根目录)。
*   **注释规范**: 鼓励在注释中使用中文（简体）以提高国内开发者的沟通效率，但必须确保文件保存为纯 UTF-8 编码。
*   **编译器兼容性**: 项目在 `CMakeLists.txt` 中已配置 `/utf-8` 选项，确保 MSVC 在 Windows 下能正确解析无 BOM 的 UTF-8 源文件。

## 3. 注释 (Comments)

*   优先编写自解释的代码。当代码的意图不够清晰时，才添加注释。
*   使用 C++ 风格的 `//` 进行单行和行尾注释。
*   **所有公开接口（函数、类、结构体、枚举）必须使用 Doxygen 风格注释。** 注释使用中文编写。
    ```cpp
    /**
     * @brief 这是一个简要说明.
     *
     * 这里是更详细的描述。
     * @param param1 参数1的说明。
     * @return 返回值的说明。
     */
    ```
*   **Doxygen 注释规范**：
    *   **文件级**：每个 `.cpp` / `.h` / `.cxx` / `.hxx` 文件的开头必须包含 `@file` 和 `@brief` 标签。
    *   **类/结构体**：每个类和结构体定义前必须有 `@brief` 注释；复杂类型需添加 `@details` 说明。
    *   **函数**：所有公开函数必须有 `@brief`；有参数的函数需添加 `@param` 标签；有返回值的需添加 `@return` 标签。
    *   **成员变量**：使用行尾 `///< 注释` 语法标注成员变量。
    *   **常量和枚举值**：使用行尾 `///< 注释` 或块注释 `/** @brief ... */` 标注。
    *   **测试方法**：测试函数必须有 `@brief` 说明测试目的，复杂场景需 `@details` 说明验证逻辑。
*   **变量注释规范**：所有新增或修改的变量（包括成员变量、函数参数、函数内部临时变量、中间结果和常量）必须有简洁的简体中文注释；声明使用行尾 `///<`，局部变量、临时变量或参数使用紧邻的 `//` 或 Doxygen 注释。

## 4. C++ 现代特性 (Modern C++ Features)

### 4.1. 资源管理

*   **强烈推荐** 使用 RAII (资源获取即初始化) 技术。
*   **优先使用智能指针** 管理动态分配的内存，而不是裸指针和手动的 `new`/`delete`。
    *   使用 `std::unique_ptr` 或 `QScopedPointer` 来表示独占所有权。
    *   使用 `QSharedPointer` 来表示跨线程 / Qt 信号槽边界的共享所有权。
    *   **严禁** 在涉及 Qt 信号的跨线程场景中使用 `std::shared_ptr`——它会破坏信号槽系统。
    *   **示例**:
    ```cpp
    // 推荐
    std::unique_ptr<MyObject> obj = std::make_unique<MyObject>();

    // 不推荐
    MyObject* obj = new MyObject();
    // ...
    delete obj;
    ```

### 4.2. `nullptr`

*   总是使用 `nullptr`，而不是 `0` 或 `NULL` 来表示空指针。

### 4.3. Lambda 表达式

*   鼓励使用 Lambda 表达式，尤其是在处理信号槽连接和简单的异步任务时。
*   保持 Lambda 表达式简短且易于理解。如果逻辑复杂，应将其重构为一个独立的成员函数或静态函数。

### 4.4. `auto`

*   谨慎使用 `auto`。当变量类型非常长或从初始化表达式中可以清晰地推断出类型时，可以使用 `auto` 来提高可读性。
*   不要过度使用 `auto`，以免降低代码的清晰度。

## 5. Qt 特定实践 (Qt Specific Practices)

### 5.1. QObject 父子关系

*   尽可能利用 Qt 的父子对象树来管理 `QObject` 派生类的生命周期。
*   在创建 `QObject` 派生类的实例时，如果可能，请为其指定一个 `parent`。
    ```cpp
    // 正确：当 parent 被销毁时，service 也会被自动销毁。
    auto service = new MyService(this);
    ```

### 5.2. 信号和槽

*   **优先使用** 新的信号槽连接语法（基于函数指针），因为它提供了编译时检查。
    ```cpp
    // 推荐
    connect(sender, &Sender::valueChanged, receiver, &Receiver::updateValue);

    // 不推荐 (除非必要)
    connect(sender, SIGNAL(valueChanged(int)), receiver, SLOT(updateValue(int)));
    ```

### 5.3. 字符串

*   在 UI 和需要国际化的部分，使用 `QString`。
*   在处理原始数据或与非 Qt 库交互时，可以考虑使用 `std::string`，并在需要时与 `QString` 进行转换。

### 5.4. 日志

*   使用 Qt 的日志框架 (`qDebug`, `qInfo`, `qWarning`, `qCritical`)。
*   在发布版本中，`qDebug` 信息会被自动移除，因此可以放心用于开发和调试。
*   重要的错误和警告应使用 `qWarning` 或 `qCritical`。
