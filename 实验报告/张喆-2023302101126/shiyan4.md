# doxygen使用技术报告
## 目录
1. Doxygen概述  
2. 安装Doxygen  
2.1 下载Doxygen  
2.2 安装 Graphviz（可选）  
3. 准备代码  
4. 配置Doxygen  
4.1 创建配置文件  
4.2 修改配置文件  
5. 生成文档  
6. 添加注释  
6.1 基本注释格式  
6.2 常用 Doxygen 标签  
7. 示例展示与导航（@example）  
7.1 准备示例文件  
7.2 使用 @example 引用  
7.3 Doxyfile 配置要求  
7.4 常见问题  
8. 高级功能  
8.1 类图和调用图  
8.2 多语言支持  
9. 集成到项目  
9.1 使用 CMake 自动生成文档  
10. 常见问题  
10.1 无法找到 dot 命令  
10.2 中文字符显示问题  
11. 总结
## Doxygen概述
Doxygen 是一个开源的文档生成工具，支持多种编程语言（如 C/C++、Java、Python 等），能够从代码注释中提取结构化信息并生成高质量的文档（HTML、LaTeX、PDF 等）。主要功能包括自动化生成 API 文档，支持跨语言项目文档化，生成调用关系图、继承图等可视化内容。
## 安装Doxygen
### 下载Doxygen
1.访问 Doxygen 官网。  
2.根据操作系统选择合适的版本并安装：  
Windows: 提供可执行安装包。  
Linux: 使用包管理器安装，例如 apt install doxygen。  
macOS: 使用 Homebrew 安装：brew install doxygen。
### 安装 Graphviz（可选）
1.Graphviz 可以用来生成类图、调用图等。  
2.Graphviz 下载
## 准备代码
确保源代码中包含标准的 Doxygen 注释格式：  
使用 /// 或 //! 表示单行注释  
使用 /** */ 或 /*! */ 表示多行注释  
以下是 C++ 的注释示例：
```cpp
/**
 * @brief 计算两个整数的和
 * @param a 第一个整数
 * @param b 第二个整数
 * @return 两个整数的和
 */
int add(int a, int b) {
    return a + b;
}
```
## 配置Doxygen
### 创建配置文件
在项目根目录运行以下命令生成配置文件：
```bash
doxygen -g
``` 
此命令会生成一个 Doxyfile，即 Doxygen 的配置文件。  
### 修改配置文件  
打开 Doxyfile，根据需要编辑以下内容：  
PROJECT_NAME：设置项目名称。
```bash
PROJECT_NAME = "MyProject"
``` 
OUTPUT_DIRECTORY：指定生成文档的输出目录。
```bash
OUTPUT_DIRECTORY = docs
``` 
INPUT：指定源文件目录。
```bash
INPUT = src
``` 
GENERATE_HTML：启用 HTML 文档生成。
```bash
GENERATE_HTML = YES
``` 
GENERATE_LATEX：启用 PDF 文档生成（需要 LaTeX 环境）。
```bash
GENERATE_LATEX = NO
``` 
DOT_PATH：如果安装了 Graphviz，设置 dot 命令路径以生成类图和调用图。
```bash
HAVE_DOT = YES
DOT_PATH = /path/to/graphviz/bin
``` 
OUTPUT_LANGUAGE：修改输出的语言（设置中文）。
```bash
OUTPUT_LANGUAGE = Chinese
``` 
## 生成文档
运行以下命令生成文档：
```bash
doxygen Doxyfile
``` 
成功运行后，docs 目录中会生成文档：  
HTML 文档：docs/html/index.html  
PDF 文档（如果启用 LaTeX）：需要进入 LaTeX 文件夹手动编译。
## 添加注释
### 基本注释格式
Doxygen 支持多种注释格式，以下是常见示例：  
1.文件注释
```cpp
/**
 * @file main.cpp
 * @brief 主程序入口
 */
```
2.类与结构体
```cpp
/**
* @brief 表示一个简单的矩形类
*/
class Rectangle {
public:
   /**
    * @brief 构造函数
    * @param w 矩形的宽度
    * @param h 矩形的高度
    */
   Rectangle(double w, double h);

   /**
    * @brief 获取矩形的面积
    * @return 矩形的面积
    */
   double getArea() const;

private:
   double width;  ///< 矩形的宽度
   double height; ///< 矩形的高度
};
``` 
3.函数注释
```cpp
/**
* @brief 打印一个问候语
* @param name 用户的名字
*/
void sayHello(const std::string& name);
``` 
4.模块与分组  
使用 @defgroup 和 @ingroup 可以组织代码到逻辑模块中：
```cpp
/** @defgroup Math 数学模块 */

/** @ingroup Math */
int add(int a, int b);
``` 
5.图表支持  
Doxygen 支持使用 @dot 或与 Graphviz 配合生成类图、调用图等。
```cpp
/**
 * @dot
 * digraph G { A -> B; }
 * @enddot
 */
``` 
### 常用 Doxygen 标签
| 标签          | 描述                          | 示例 |
|---------------|-------------------------------|------|
| `@mainpage`   | 设置首页内容                  | `@mainpage 项目概述` |
| `@brief`      | 简短描述                      | `@brief 计算两个数的和` |
| `@param`      | 描述函数参数                  | `@param a 第一个参数` |
| `@return`     | 描述返回值                    | `@return 计算结果` |
| `@file`       | 文件级别注释                  | `@file main.cpp` |
| `@class`      | 类级别注释                    | `@class MyClass` |
| `@deprecated` | 标记函数或类已过时            | `@deprecated 请使用新API` |
| `@see`        | 参考相关函数或类              | `@see add()` |
| `@section`    | 分节（一级标题）              | `@section intro 介绍` |
| `@subsection` | 子节（二级标题）              | `@subsection usage 用法` |
| `@ref`        | 创建内部链接                  | `参见 @ref install` |
## 示例展示与导航（@example）
若希望在导航栏中添加 Examples 示例页面，应使用 @example 标签引用示例文件。需要注意，@example 通常用于其他文档或源文件中对示例文件的引用，而不应直接写在示例文件本身内部。
### 准备示例文件
在项目中创建 examples/test.c 文件：
```cpp
/**
 * @file test.c
 * @brief 示例文件：演示加法函数
 */

int add(int a, int b) {
    return a + b;
}

int main() {
    return add(2, 3);
}
``` 
### 使用 @example 引用
在主注释中引用示例文件：
```cpp
/**
 * @example test.c
 * 演示如何使用 add 函数
 */
``` 
### Doxyfile 配置要求
```bash
EXAMPLE_PATH = examples
EXAMPLE_RECURSIVE = YES
``` 
确保 INPUT 也包含 examples 目录。
### 常见问题
| 问题                | 解决方案                                                                 |
|---------------------|--------------------------------------------------------------------------|
| 示例页面显示为空     | 检查文件是否位于 `EXAMPLE_PATH` 下，并重新执行 `doxygen`                 |
| 修改未生效           | 确保已重新运行 `doxygen`，并删除之前生成的 `html` 文件夹                 |
| 导航栏无 Examples    | 检查是否使用了 `@example` 标记，并确认 `EXTRACT_ALL = YES` 已设置        |
## 高级功能
### 类图和调用图
启用 Graphviz 后，Doxygen 会自动生成类图和调用图，图形会嵌入到 HTML 文档中。

调用图：显示函数的调用关系。  
被调用图：显示函数被哪些函数调用。

确保以下选项启用：
```bash
HAVE_DOT = YES
CALL_GRAPH = YES
CALLER_GRAPH = YES
``` 
### 多语言支持
通过修改 LANGUAGE 选项支持多种编程语言（如 C++、Python、Java）：
```bash
OPTIMIZE_OUTPUT_FOR_C = YES
``` 
## 集成到项目
### 使用 CMake 自动生成文档
在 CMake 文件中添加以下内容：
```cmake
find_package(Doxygen REQUIRED)

set(DOXYGEN_INPUT_DIR "${CMAKE_SOURCE_DIR}/src")
set(DOXYGEN_OUTPUT_DIR "${CMAKE_BINARY_DIR}/docs")

set(DOXYGEN_CONFIG_FILE "${CMAKE_BINARY_DIR}/Doxyfile")

add_custom_target(doc
    COMMAND doxygen ${DOXYGEN_CONFIG_FILE}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Generating API documentation with Doxygen"
    VERBATIM)
```
运行 make doc 即可生成文档。
## 常见问题
### 无法找到 dot 命令
确保安装了 Graphviz，并将其路径添加到环境变量中。  
修改 Doxyfile 中的 DOT_PATH 选项。
### 中文字符显示问题
将 Doxyfile 中的编码设置为 UTF-8：
```bash
INPUT_ENCODING = UTF-8
``` 
## 总结
Doxygen 是一个功能强大的文档生成工具，结合良好的代码注释，可以大大提高项目的可维护性。使用 Doxygen，您可以轻松生成包含类图、调用关系图的 HTML 和 PDF 文档，并将其集成到项目的 CI/CD 管道中。