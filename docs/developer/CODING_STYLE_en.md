# EasyKiConverter Coding Style

This document provides a unified set of coding standards and best practices for developers of the EasyKiConverter project, aiming to ensure code consistency, readability, and maintainability.

This guide is primarily based on the [Qt Coding Style](https://wiki.qt.io/Qt_Coding_Style), combined with the project's own established practices.

## 1. Naming Conventions

### 1.1. General Rules

*   Use clear and descriptive English names.
*   Avoid abbreviations unless they are widely known (e.g., `UI`, `API`).

### 1.2. File Names

*   C++ source and header files should use PascalCase, e.g., `ExportServicePipeline.cpp` and `ExportServicePipeline.h`. The main entry point is `src/main.cpp`.
*   QML files should use PascalCase, e.g., `MainWindow.qml`. The main entry point is `src/ui/qml/Main.qml`.
*   Test files usually use snake_case, e.g., `test_component_service.cpp`.

### 1.3. Classes, Structs, and Namespaces

*   Use PascalCase.
*   **Examples**: `ExportServicePipeline`, `ComponentData`, `EasyKiConverter`.

### 1.4. Functions and Methods

*   Use camelCase.
*   **Examples**: `executeExportPipeline()`, `handleFetchCompleted()`.

### 1.5. Variables

*   **Local Variables**: Use camelCase.
    *   **Examples**: `int totalCount = 0;`, `QMutexLocker locker(m_mutex);`
*   **Member Variables**:
    *   **Public members**: Use camelCase without any prefix.
    *   **Private/Protected members**: Use the `m_` prefix, followed by camelCase.
    *   **Examples**: `QThreadPool *m_fetchThreadPool;`, `int m_successCount;`
*   **Global Variables**: Avoid global variables whenever possible. If one must be used, it should be prefixed with `g_`.
*   **Constants and Enums**:
    *   **Constants**: Use UPPER_SNAKE_CASE.
        *   **Example**: `const int MAX_THREADS = 8;`
    *   **Enum Names**: Use PascalCase.
    *   **Enum Values**: Use UPPER_SNAKE_CASE.

### 1.6. QML

*   **IDs**: Use camelCase.
*   **Properties**: Use camelCase.

## 2. Formatting

### 2.1. Indentation

*   Use **4 spaces** for indentation. Do not use tabs.

### 2.2. Braces `{}`

*   **All Blocks (Functions, Classes, Namespaces, Control Statements)**: The opening brace `{` goes on the **same line** (K&R style).
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
*   **Exceptions**:
    *   Extremely simple inline functions can stay on the same line.
    *   Braces for QML elements usually stay on the same line.
    *   Braces for Lambda expressions can stay on the same line.
*   **Single-Line Statements**: Braces are mandatory even if the body of a control statement is a single line.
    ```cpp
    // Correct
    if (condition)
    {
        return;
    }

    // Incorrect
    if (condition) return;
    ```

### 2.3. Line Length

*   Try to keep the line length limited to **120 characters**.

### 2.4. Spaces

*   Use a space around binary operators (`+`, `-`, `*`, `/`, `=`, `==`, `!=`, `&&`, etc.).
*   Use a space after commas `,` and semicolons `;` (in for loops).
*   Use a space between a keyword (`if`, `for`, `while`) and the following parenthesis `(`.
*   Do **not** use a space between a function name and its argument list's parenthesis `(`.

### 2.5. File Encoding

*   **All source files** (`.cpp`, `.h`, `.qml`, `CMakeLists.txt`, etc.) MUST use **UTF-8 (without BOM)** encoding.
*   **Strictly avoid** UTF-8 with BOM or other encodings (e.g., GBK) to ensure that comments and string literals are displayed correctly across different platforms.
*   **Mandatory Tool Validation**: Developers MUST use the provided conversion tool for encoding compliance before contributing or submitting a PR:
    *   **Tool Path**: `tools/python/convert_to_utf8.py`
    *   **Usage**: `python tools/python/convert_to_utf8.py` (defaults to scanning the entire project root).
*   **Comment Language**: While technical terms should remain in English, descriptive comments in Chinese (Simplified) are allowed and encouraged for internal development efficiency, provided the file is saved as pure UTF-8.
*   **Compiler Compatibility**: The project's `CMakeLists.txt` is configured with the `/utf-8` flag for MSVC to ensure correct parsing of No-BOM UTF-8 files on Windows.

## 3. Comments

*   Prefer self-documenting code. Add comments only when the intent of the code is not obvious.
*   Use C++-style `//` for single-line and end-of-line comments.
*   **All public interfaces (functions, classes, structs, enums) MUST use Doxygen-style comments.**
    ```cpp
    /**
     * @brief A brief description.
     *
     * A more detailed description goes here.
     * @param param1 Description of parameter 1.
     * @return Description of the return value.
     */
    ```
*   **Doxygen comment conventions**:
    *   **File-level**: Every `.cpp` / `.h` / `.cxx` / `.hxx` file must start with `@file` and `@brief` tags.
    *   **Classes/Structs**: Every class and struct definition must have a `@brief` comment; complex types need `@details`.
    *   **Functions**: All public functions must have `@brief`; functions with parameters need `@param` tags; functions with return values need `@return` tags.
    *   **Member variables**: Use end-of-line `///< comment` syntax.
    *   **Constants and enum values**: Use end-of-line `///< comment` or block `/** @brief ... */` comments.
    *   **Test methods**: Test functions must have `@brief` explaining the test purpose; complex scenarios need `@details` explaining the verification logic.
*   **Variable comments**: Every new or modified variable, including member variables, parameters, local temporaries, intermediate results, and constants, must have a concise Simplified Chinese comment. Use trailing `///<` for declarations and a nearby `//` or Doxygen comment for local-variable, temporary-variable, and parameter semantics.

## 4. Modern C++ Features

### 4.1. Resource Management

*   **Strongly recommend** using the RAII (Resource Acquisition Is Initialization) idiom.
*   **Prefer smart pointers** to manage dynamically allocated memory over raw pointers and manual `new`/`delete`.
    *   Use `std::unique_ptr` or `QScopedPointer` to represent exclusive ownership.
    *   Use `QSharedPointer` to represent shared ownership across thread / Qt signal-slot boundaries.
    *   **Never** use `std::shared_ptr` in cross-thread Qt signal scenarios — it will break the signal-slot system.
    *   **Example**:
    ```cpp
    // Recommended
    std::unique_ptr<MyObject> obj = std::make_unique<MyObject>();

    // Not recommended
    MyObject* obj = new MyObject();
    // ...
    delete obj;
    ```

### 4.2. `nullptr`

*   Always use `nullptr`, not `0` or `NULL`, to represent a null pointer.

### 4.3. Lambda Expressions

*   Lambda expressions are encouraged, especially for signal-slot connections and simple asynchronous tasks.
*   Keep lambdas short and easy to understand. If the logic is complex, refactor it into a separate member or static function.

### 4.4. `auto`

*   Use `auto` judiciously. It can improve readability when the variable type is long or clearly inferable from the initialization expression.
*   Do not overuse `auto` to the point where it obscures the code's clarity.

## 5. Qt Specific Practices

### 5.1. QObject Parent-Child Relationship

*   Whenever possible, leverage Qt's parent-child object tree to manage the lifetime of `QObject` derivatives.
*   When creating an instance of a `QObject` derived class, assign a `parent` if possible.
    ```cpp
    // Correct: `service` will be automatically deleted when its parent is destroyed.
    auto service = new MyService(this);
    ```

### 5.2. Signals and Slots

*   **Prefer** the new signal-slot connection syntax (based on function pointers) as it provides compile-time checking.
    ```cpp
    // Recommended
    connect(sender, &Sender::valueChanged, receiver, &Receiver::updateValue);

    // Not recommended (unless necessary)
    connect(sender, SIGNAL(valueChanged(int)), receiver, SLOT(updateValue(int)));
    ```

### 5.3. Strings

*   Use `QString` for UI elements and in parts that require internationalization.
*   Consider using `std::string` when dealing with raw data or interacting with non-Qt libraries, and convert to/from `QString` when needed.

### 5.4. Logging

*   Use Qt's logging framework (`qDebug`, `qInfo`, `qWarning`, `qCritical`).
*   `qDebug` messages are automatically removed in release builds, so they can be used freely for development and debugging.
*   Important errors and warnings should use `qWarning` or `qCritical`.
