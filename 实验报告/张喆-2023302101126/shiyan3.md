# make和cmake使用技术报告
## 目录
1. Make工具与Makefile详解  
1.1 Make的概念  
1.2 Makefile文件的格式  
1.2.1 概述  
1.2.2 目标  
1.2.3 前置条件  
1.2.4 命令  
1.3 Makefile文件的语法  
1.3.1 注释  
1.3.2 回声  
1.3.3 通配符  
1.3.4 模式匹配  
1.3.5 变量和赋值符  
1.3.6 内置变量  
1.3.7 自动变量  
1.3.8 判断和循环  
1.3.9 函数  
1.4 Makefile的实例  
1.4.1 执行多个目标  
1.4.2 编译C语言项目  
2. CMake基础使用  
2.1 CMake简介  
2.2 CMake的简单使用  
2.2.1 准备工作  
2.2.2 开始构建  
2.2.3 解释CMakeLists.txt的内容  
2.3.4 基本语法规则  
3. Make和CMake的不同点  
4. 总结
## Make工具与Makefile详解
### Make的概念
Make这个词，英语的意思是"制作"。Make命令直接用了这个意思，就是要做出某个文件。比如，要做出文件a.txt，就可以执行下面的命令。
```bash
$ make a.txt
``` 
但是，如果你真的输入这条命令，它并不会起作用。因为Make命令本身并不知道如何做出a.txt，需要有人告诉它如何调用其他命令完成这个目标。例如，假设文件a.txt依赖于b.txt和c.txt，是后面两个文件连接（cat命令）的产物。那么，make需要知道下面的规则。
```bash
a.txt: b.txt c.txt   cat b.txt c.txt > a.txt
``` 
也就是说，make a.txt这条命令的背后，实际上分成两步：第一步，确认b.txt和c.txt必须已经存在，第二步使用 cat 命令 将这个两个文件合并，输出为新文件。  

像这样的规则，都写在一个叫做Makefile的文件中，Make命令依赖这个文件进行构建。Makefile文件也可以写为makefile， 或者用命令行参数指定为其他文件名。
```bash
$ make -f rules.txt # 或者 $ make --file=rules.txt
``` 
上面代码指定make命令依据rules.txt文件中的规则，进行构建。  

总之，make只是一个根据指定的Shell命令进行构建的工具。它的规则很简单，你规定要构建哪个文件、它依赖哪些源文件，当那些文件有变动时，如何重新构建它。
### Makefile文件的格式
构建规则都写在Makefile文件里面，要学会如何Make命令，就必须学会如何编写Makefile文件。
#### 概述
Makefile文件由一系列规则（rules）构成。每条规则的形式如下。
```bash
<target> : <prerequisites>  tab  <commands>
``` 
上面第一行冒号前面的部分，叫做"目标"（target），冒号后面的部分叫做"前置条件"（prerequisites）；第二行必须由一个tab键起首，后面跟着"命令（commands）。"目标"是必需的，不可省略；"前置条件"和"命令"都是可选的，但是两者之中必须至少存在一个。  

每条规则就明确两件事：构建目标的前置条件是什么，以及如何构建。下面就详细讲解，每条规则的这三个组成部分。
#### 目标
一个目标（target）就构成一条规则。目标通常是文件名，指明Make命令所要构建的对象，比如上文的 a.txt 。目标可以是一个文件名，也可以是多个文件名，之间用空格分隔。  

除了文件名，目标还可以是某个操作的名字，这称为"伪目标"（phony target）。
```bash
clean:  rm *.o
``` 
上面代码的目标是clean，它不是文件名，而是一个操作的名字，属于"伪目标 "，作用是删除对象文件。
```bash
$ make  clean
``` 
但是，如果当前目录中，正好有一个文件叫做clean，那么这个命令不会执行。因为Make发现clean文件已经存在，就认为没有必要重新构建了，就不会执行指定的rm命令。

为了避免这种情况，可以明确声明clean是"伪目标"，写法如下。
```bash
.PHONY: clean clean:   rm *.o temp
``` 
声明clean是"伪目标"之后，make就不会去检查是否存在一个叫做clean的文件，而是每次运行都执行对应的命令。像.PHONY这样的内置目标名还有不少，可以查看手册。

如果Make命令运行时没有指定目标，默认会执行Makefile文件的第一个目标。
```bash
$ make
``` 
上面代码执行Makefile文件的第一个目标。
#### 前置条件
前置条件通常是一组文件名，之间用空格分隔。它指定了"目标"是否重新构建的判断标准：只要有一个前置文件不存在，或者有过更新（前置文件的last-modification时间戳比目标的时间戳新），"目标"就需要重新构建。
```bash
result.txt: source.txt     cp source.txt result.txt
``` 
上面代码中，构建 result.txt 的前置条件是 source.txt 。如果当前目录中，source.txt 已经存在，那么make result.txt可以正常运行，否则必须再写一条规则，来生成 source.txt 。
```bash
source.txt:   echo "this is the source" > source.txt
``` 
上面代码中，source.txt后面没有前置条件，就意味着它跟其他文件都无关，只要这个文件还不存在，每次调用make source.txt，它都会生成。
```bash
$ make result.txt $ make result.txt
``` 
上面命令连续执行两次make result.txt。第一次执行会先新建 source.txt，然后再新建 result.txt。第二次执行，Make发现 source.txt 没有变动（时间戳晚于 result.txt），就不会执行任何操作，result.txt  也不会重新生成。

如果需要生成多个文件，往往采用下面的写法。
```bash
source: file1 file2 file3
``` 
上面代码中，source 是一个伪目标，只有三个前置文件，没有任何对应的命令。
```bash
$ make source
``` 
执行make source命令后，就会一次性生成 file1，file2，file3 三个文件。这比下面的写法要方便很多。
```bash
$ make file1 $ make file2 $ make file3
``` 
#### 命令
命令（commands）表示如何更新目标文件，由一行或多行的Shell命令组成。它是构建"目标"的具体指令，它的运行结果通常就是生成目标文件。

每行命令之前必须有一个tab键。如果想用其他键，可以用内置变量.RECIPEPREFIX声明。
```bash
.RECIPEPREFIX = > all: > echo Hello, world
``` 
上面代码用.RECIPEPREFIX指定，大于号（>）替代tab键。所以，每一行命令的起首变成了大于号，而不是tab键。

需要注意的是，每行命令在一个单独的shell中执行。这些Shell之间没有继承关系。
```bash
var-lost:     export foo=bar     echo "foo=$$foo"
``` 
上面代码执行后（make var-lost），取不到foo的值。因为两行命令在两个不同的进程执行。一个解决办法是将两行命令写在一行，中间用分号分隔。
```bash
var-kept:     export foo=bar; echo "foo=$$foo"
``` 
另一个解决办法是在换行符前加反斜杠转义。
```bash
var-kept:     export foo=bar; \     echo "foo=$$foo"
```
最后一个方法是加上.ONESHELL:命令。
```bash
.ONESHELL: var-kept:     export foo=bar;      echo "foo=$$foo"
``` 
### Makefile文件的语法
#### 注释
井号（#）在Makefile中表示注释。
```bash
这是注释 result.txt: source.txt     # 这是注释     cp source.txt result.txt # 这也是注释
``` 
#### 回声
正常情况下，make会打印每条命令，然后再执行，这就叫做回声（echoing）。
```bash
test:     # 这是测试
``` 
执行上面的规则，会得到下面的结果。
```bash
$ make test # 这是测试
``` 
在命令的前面加上@，就可以关闭回声。
```bash
test:     @# 这是测试
``` 
现在再执行make test，就不会有任何输出。

由于在构建过程中，需要了解当前在执行哪条命令，所以通常只在注释和纯显示的echo命令前面加上@。
```bash
test:     @# 这是测试     @echo TODO
``` 
#### 通配符
通配符（wildcard）用来指定一组符合条件的文件名。Makefile 的通配符与 Bash 一致，主要有星号（*）、问号（？）和 ... 。比如， *.o 表示所有后缀名为o的文件。
```bash
clean: rm -f *.o
``` 
#### 模式匹配
Make命令允许对文件名，进行类似正则运算的匹配，主要用到的匹配符是%。比如，假定当前目录下有 f1.c 和 f2.c 两个源码文件，需要将它们编译为对应的对象文件。
```bash
%.o: %.c
``` 
等同于下面的写法。
```bash
f1.o: f1.c f2.o: f2.c
``` 
使用匹配符%，可以将大量同类型的文件，只用一条规则就完成构建。
#### 变量和赋值符
Makefile 允许使用等号自定义变量。
```bash
txt = Hello World test:     @echo $(txt)
``` 
上面代码中，变量 txt 等于 Hello World。调用时，变量需要放在 $( ) 之中。

调用Shell变量，需要在美元符号前，再加一个美元符号，这是因为Make命令会对美元符号转义。
```bash
test:  @echo $$HOME
``` 
有时，变量的值可能指向另一个变量。
```bash
v1 = $(v2)
``` 
上面代码中，变量 v1 的值是另一个变量 v2。这时会产生一个问题，v1 的值到底在定义时扩展（静态扩展），还是在运行时扩展（动态扩展）？如果 v2 的值是动态的，这两种扩展方式的结果可能会差异很大。

为了解决类似问题，Makefile一共提供了四个赋值运算符 （=、:=、？=、+=），它们的区别请看StackOverflow。
```bash
VARIABLE = value # 在执行时扩展，允许递归扩展。  VARIABLE := value # 在定义时扩展。  VARIABLE ?= value # 只有在该变量为空时才设置值。  VARIABLE += value # 将值追加到变量的尾端。
``` 
#### 内置变量
Make命令提供一系列内置变量，比如，$(CC) 指向当前使用的编译器，$(MAKE) 指向当前使用的Make工具。这主要是为了跨平台的兼容性。
```bash
output: $(CC) -o output input.c
``` 
#### 自动变量
Make命令还提供一些自动变量，它们的值与当前规则有关。下面是自动变量的一个例子。
```bash
dest/%.txt: src/%.txt   @ -d dest  || mkdir dest  cp $< $@  #将 src 目录下的 txt 文件，拷贝到 dest 目录下。首先判断 dest 目录是否存在，如果不存在就新建，然后，$< 指代前置文件（src/%.txt），$@ 指代目标文件（dest/%.
``` 
#### 判断和循环
Makefile使用 Bash 语法，完成判断和循环。
```bash
ifeq ($(CC),gcc)   libs=$(libs_for_gcc) else   libs=$(normal_libs) endif
``` 
上面代码判断当前编译器是否 gcc ，然后指定不同的库文件。
```bash
LIST = one two three all:     for i in $(LIST); do \         echo $$i; \     done  # 等同于  all:     for i in one two three; do \         echo $i; \     done
``` 
上面代码的运行结果。
```bash
one two three
``` 
#### 函数
Makefile 还可以使用函数，格式如下。
```bash
$(function arguments) # 或者 ${function arguments}
``` 
Makefile提供了许多内置函数，可供调用。下面是几个常用的内置函数。

（1）shell 函数  
shell 函数用来执行 shell 命令
```bash
srcfiles := $(shell echo src/{00..99}.txt)
``` 
（2）wildcard 函数  
wildcard 函数用来在 Makefile 中，替换 Bash 的通配符。
```bash
srcfiles := $(wildcard src/*.txt)
``` 
（3）subst 函数  
subst 函数用来文本替换，格式如下。
```bash
$(subst from,to,text)
``` 
下面的例子将字符串"feet on the street"替换成"fEEt on the strEEt"。
```bash
$(subst ee,EE,feet on the street)
``` 
下面是一个稍微复杂的例子。
```bash
comma:= , empty:= # space变量用两个空变量作为标识符，当中是一个空格 space:= $(empty) $(empty) foo:= a b c bar:= $(subst $(space),$(comma),$(foo)) # bar is now `a,b,c'.
``` 
（4）patsubst函数  
patsubst 函数用于模式匹配的替换，格式如下。
```bash
$(patsubst pattern,replacement,text)
``` 
下面的例子将文件名"x.c.c bar.c"，替换成"x.c.o bar.o"。
```bash
$(patsubst %.c,%.o,x.c.c bar.c)
``` 
（5）替换后缀名  
替换后缀名函数的写法是：变量名 + 冒号 + 后缀名替换规则。它实际上patsubst函数的一种简写形式。
```bash
min: $(OUTPUT:.js=.min.js)
``` 
上面代码的意思是，将变量OUTPUT中的后缀名 .js 全部替换成 .min.js 。
### Makefile的实例
#### 执行多个目标
```bash
.PHONY: cleanall cleanobj cleandiff  cleanall : cleanobj cleandiff   rm program  cleanobj :     rm *.o  cleandiff :    rm *.diff
``` 
上面代码可以调用不同目标，删除不同后缀名的文件，也可以调用一个目标（cleanall），删除所有指定类型的文件。
#### 编译C语言项目
```bash
edit : main.o kbd.o command.o display.o      cc -o edit main.o kbd.o command.o display.o  main.o : main.c defs.h     cc -c main.c kbd.o : kbd.c defs.h command.h     cc -c kbd.c command.o : command.c defs.h command.h     cc -c command.c display.o : display.c defs.h     cc -c display.c  clean :      rm edit main.o kbd.o command.o display.o  .PHONY: edit clean
``` 
## CMake基础使用
### CMake简介
CMake是一个跨平台的安装（编译）工具，可以用简单的语句来描述所有平台的安装(编译过程)。他能够输出各种各样的makefile或者project文件，能测试编译器所支持的C++特性,类似UNIX下的automake。只是 CMake 的组态档取名为CMakeLists.txt。

Cmake 并不直接建构出最终的软件，而是产生标准的建构档（如 Unix 的 Makefile 或 Windows Visual C++ 的 projects/workspaces），然后再以一般的建构方式使用。
### CMake的简单使用
#### 准备工作
（1）首先，建立一个文件夹，用来存放工程文件。
```bash
cd cmake
mkdir t1
cd t1
``` 
（2）在 t1 目录建立main.c 和 CMakeLists.txt(注意文件名大小写)：  
main.c 文件内容：
```bash
//main.c
#include <stdio.h>
int main()
{
    printf(“Hello World from t1 Main!\n”); 
    return 0;
}
``` 
CMakeLists.txt 文件内容：
```bash
PROJECT(HELLO)
SET(SRC_LIST main.c)
MESSAGE(STATUS "This is BINARY dir " ${HELLO_BINARY_DIR})
MESSAGE(STATUS "This is SOURCE dir "${HELLO_SOURCE_DIR})
ADD_EXECUTABLE(hello ${SRC_LIST})
ADD_EXECUTABLE(hello2 ${SRC_LIST})
``` 
#### 开始构建
```bash
cmake .
``` 
注意命令后面的点号，代表本目录。

执行结果：
```bash
CMake Warning (dev) at CMakeLists.txt:4:
  Syntax Warning in cmake code at column 37

  Argument not separated from preceding token by whitespace.
This warning is for project developers.  Use -Wno-dev to suppress it.

-- The C compiler identification is GNU 8.4.0
-- The CXX compiler identification is GNU 8.4.0
-- Detecting C compiler ABI info
-- Detecting C compiler ABI info - done
-- Check for working C compiler: /usr/bin/cc - skipped
-- Detecting C compile features
-- Detecting C compile features - done
-- Detecting CXX compiler ABI info
-- Detecting CXX compiler ABI info - done
-- Check for working CXX compiler: /usr/bin/c++ - skipped
-- Detecting CXX compile features
-- Detecting CXX compile features - done
-- This is BINARY dir /home/fly/workspace/cmakeProj/t1
-- This is SOURCE dir /home/fly/workspace/cmakeProj/t1
-- Configuring done
-- Generating done
-- Build files have been written to: /home/fly/workspace/cmakeProj/t1
``` 
系统自动生成了： CMakeFiles, CMakeCache.txt, cmake_install.cmake 等文件，并且生成了 Makefile。

不需要理会这些文件的作用，最关键的是，它自动生成了 Makefile。然后进行工程的实际构建，在这个目录输入 make 命令，大概会得到如下的输出：
```bash
[ 25%] Building C object CMakeFiles/hello2.dir/main.c.o
[ 50%] Linking C executable hello2
[ 50%] Built target hello2
[ 75%] Building C object CMakeFiles/hello.dir/main.c.o
[100%] Linking C executable hello
[100%] Built target hello
``` 
如果需要看到make 构建的详细过程，可以使用make VERBOSE=1 或者VERBOSE=1 make 命令来进行构建。

这时候，需要的目标文件hello 已经构建完成，位于当前目录，尝试运行一下：
```bash
$ ./hello
Hello World from t1 main
``` 
#### 解释CMakeLists.txt的内容
CMakeLists.txt这个文件是 cmake 的构建定义文件，文件名是大小写相关的，如果工程存在多个目录，需要确保每个要管理的目录都存在一个CMakeLists.txt。这涉及到关于多目录构建，后面解释。

上面例子中的CMakeLists.txt 文件内容如下:
```bash
PROJECT(HELLO)
SET(SRC_LIST main.c)
MESSAGE(STATUS "This is BINARY dir " ${HELLO_BINARY_DIR})
MESSAGE(STATUS "This is SOURCE dir "${HELLO_SOURCE_DIR})
ADD_EXECUTABLE(hello ${SRC_LIST})
ADD_EXECUTABLE(hello2 ${SRC_LIST})
``` 
（1）PROJECT指令。  
语法是：
```bash
PROJECT(projectname [CXX] [C] [Java])
``` 
可以用这个指令定义工程名称，并可指定工程支持的语言，支持的语言列表是可以忽略的，默认情况表示支持所有语言。这个指令隐式的定义了两个 cmake 变量: <projectname>_BINARY_DIR 以及<projectname>_SOURCE_DIR，这里就是 HELLO_BINARY_DIR 和HELLO_SOURCE_DIR(所以CMakeLists.txt 中两个MESSAGE指令可以直接使用了这两个变量)，因为采用的是内部编译，两个变量目前指的都是工程所在路/t1；注意，在外部编译中，两者所指代的内容会有所不同。

同时cmake 系统也帮助我们预定义了 PROJECT_BINARY_DIR 和PROJECT_SOURCE_DIR变量，他们的值分别跟HELLO_BINARY_DIR 与HELLO_SOURCE_DIR 一致。

为了统一起见，建议以后直接使用PROJECT_BINARY_DIR，PROJECT_SOURCE_DIR，即使修改了工程名称，也不会影响这两个变量。如果使用了<projectname>_SOURCE_DIR，修改工程名称后，需要同时修改这些变量。

（2）SET指令。  
语法是：
```bash
SET(VAR [VALUE] [CACHE TYPE DOCSTRING [FORCE]])
``` 
SET 指令可以用来显式的定义变量即可。比如用到的是SET(SRC_LIST main.c)，如果有多个源文件，也可以定义成：SET(SRC_LIST main.c t1.c t2.c)。

（3）MESSAGE指令。  
语法是：
```bash
MESSAGE([SEND_ERROR | STATUS | FATAL_ERROR] "message to display" ...)
``` 
这个指令用于向终端输出用户定义的信息，包含了三种类型:

SEND_ERROR，产生错误，生成过程被跳过。  
SATUS，输出前缀为--的信息。  
FATAL_ERROR，立即终止所有cmake 过程。

这里使用的是STATUS 信息输出，演示了由PROJECT 指令定义的两个隐式变量HELLO_BINARY_DIR 和HELLO_SOURCE_DIR。

（4）ADD_EXECUTABLE 指令。
```bash
ADD_EXECUTABLE(hello ${SRC_LIST})
``` 
定义了这个工程会生成一个文件名为hello 的可执行文件，相关的源文件是 SRC_LIST 中定义的源文件列表， 本例中也可以直接写成ADD_EXECUTABLE(hello main.c)。
#### 基本语法规则
cmake 其实要使用”cmake 语言和语法”去构建，上面的内容就是所谓的 ”cmake 语言和语法”，最简单的语法规则是：  
（1）变量使用${}方式取值，但是在IF 控制语句中是直接使用变量名 。  

（2）指令(参数1 参数 2...)，参数使用括弧括起，参数之间使用空格或分号分开。以上面的ADD_EXECUTABLE 指令为例，如果存在另外一个 func.c 源文件，就要写成：
```bash
ADD_EXECUTABLE(hello main.c func.c)
# 或者
ADD_EXECUTABLE(hello main.c;func.c)
``` 
（3）指令是大小写无关的，参数和变量是大小写相关的。但，推荐全部使用大写指令。上面的MESSAGE 指令我们已经用到了这条规则：
```bash
MESSAGE(STATUS "This is BINARY dir" ${HELLO_BINARY_DIR})
# 也可以写成：
MESSAGE(STATUS "This is BINARY dir ${HELLO_BINARY_DIR}")
``` 
这里需要特别解释的是作为工程名的HELLO 和生成的可执行文件 hello 是没有任何关系的。

（4）工程名和执行文件。hello 定义了可执行文件的文件名，也完全可以写成：ADD_EXECUTABLE(t1 main.c)编译后会生成一个t1 可执行文件。

（5）关于语法的疑惑：

cmake 的语法还是比较灵活而且考虑到各种情况，比如：
```bash
SET(SRC_LIST main.c)
# 也可以写成 
SET(SRC_LIST "main.c")
``` 
是没有区别的，但是假设一个源文件的文件名是 fu nc.c(文件名中间包含了空格)。这时候就必须使用双引号，如果写成了 SET(SRC_LIST fu nc.c)，就会出现错误，提示找不到fu 文件和nc.c 文件。这种情况，就必须写成：
```bash
SET(SRC_LIST "fu nc.c")
``` 
此外，可以忽略掉source 列表中的源文件后缀，比如可以写成：
```bash
ADD_EXECUTABLE(t1 main)
``` 
cmake 会自动的在本目录查找main.c 或者main.cpp等，当然，最好不要偷这个懒，以免这个目录确实存在一个 main.c和一个main。

同时参数也可以使用分号来进行分割。下面的例子也是合法的：
```bash
ADD_EXECUTABLE(t1 main.c t1.c)
# 可以写成
ADD_EXECUTABLE(t1 main.c;t1.c).
``` 
只需要在编写CMakeLists.txt 时注意形成统一的风格即可。

（6）清理工程。跟经典的autotools 系列工具一样，运行make clean即可对构建结果进行清理。

（7）cmake 并不支持 make distclean，关于这一点，官方是有明确解释的。因为CMakeLists.txt 可以执行脚本并通过脚本生成一些临时文件，但是却没有办法来跟踪这些临时文件到底是哪些。因此，没有办法提供一个可靠的 make distclean 方案。

（8）内部构建。刚才进行的是内部构建(in-source build)，而 cmake 强烈推荐的是外部构建(out-of-source build)。内部构建生成的临时文件可能比您的代码文件还要多，而且它生成了一些无法自动删除的中间文件。

（9）外部构建。外部编译的过程如下：  
首先，请清除t1 目录中除main.c CmakeLists.txt 之外的所有中间文件，最关键的是CMakeCache.txt。  
其次，在 t1 目录中建立build 目录，当然你也可以在任何地方建立build 目录，不一定必须在工程目录中。  
再次，进入 build 目录，运行cmake ..(注意,..代表父目录，因为父目录存在我们需要的CMakeLists.txt，如果你在其他地方建立了build 目录，需要运行cmake <工程的全路径>)，查看一下build 目录，就会发现了生成了编译需要的Makefile 以及其他的中间文件。  
最后，运行 make 构建工程，就会在当前目录(build 目录)中获得目标文件 hello。

上述过程就是所谓的out-of-source 外部编译，一个最大的好处是，对于原有的工程没有任何影响，所有动作全部发生在编译目录。通过这一点，也足以说服我们全部采用外部编译方式构建工程。

这里需要特别注意的是：通过外部编译进行工程构建，HELLO_SOURCE_DIR 仍然指代工程路径，即/t1；而 HELLO_BINARY_DIR 则指代编译路径，即cmake/t1/build。
## Make和CMake的不同点
如果你要使用编译脚本，构建的过程中有一个步骤，也就是需要在命令行中输入”make”。 对于CMake，需要进行2步：第一，你需要配置你的编译环境（可以通过在你的编译目录中输入cmake ，也可以通过使用GUI客户端）。这将创建一个等效的，依赖你选择的编译环境的编译脚本或其他。编译系统可以传递给CMake一个参数。总之，CMake根据你的系统配置选择合理的的默认的选择。第二，你在你选择的编译系统中执行实际的构建。

你将进入GNU构建系统领域。我们可以使用Autotool来比较CMake。当我们这样做的时候，我们会发现Make的缺点，而且这就是Autotool产生的理由。我们可以看到CMake明显比Make优越的理由。Autoconf 解决了一个重要的问题，也就是说与系统有关的构建和运行时信息的的可信赖发现。但是，这仅仅是轻便软件的开发中的一小部分。作为结尾，GNU工程已经开发了一系列集成的实用程序，用于完成Autoconf开始之后的工作：GNU构建系统中最重要的组件是Autoconf, Automake, and Libtool.

“Make”就不能那样做了，至少不修改任何东西是做不到的。你可以自己做所有的跨平台工作，但是这将花费很多时间。CMake解决了这个问题，但是与此同时，它比GNU构建系统更有优势：  
用于编写CMakeLists.txt文件的语言具有可读性和很容易理解。  
可以使用“Make” 来构建工程。  
支持多种生产工具，比如Xcode, Eclipse, Visual Studio, etc.

CMake与Make对比具有以下优点：  
自动发现跨平台系统库。  
自动发现和管理的工具集。  
更容易将文件编译进共享库，以一种平台无关的方式或者以比make更容易使用的的生成方式。

CMake不仅仅只“make”，所以它变得更复杂。如果你仅仅在一个平台上构建小的工程，“Make”更适合完成这部分工作。
## 总结
本技术报告全面系统地介绍了Make和CMake两大构建工具的核心概念、使用方法和实际应用，为开发者提供了从基础到实践的完整指南。Make工具体系作为经典构建工具，Make通过Makefile规则文件实现自动化构建，采用"目标-依赖-命令"的简洁规则定义构建流程，提供变量系统、模式匹配、自动变量等强大功能，特别适合中小型项目的快速构建。CMake现代构建作为跨平台构建系统生成器，支持多种编译环境，通过声明式的CMakeLists.txt描述项目结构，自动处理依赖关系、编译器差异等复杂问题，特别适合中大型跨平台项目开发。对于简单项目或快速原型开发中，推荐使用Make，对于需要支持多平台的中大型项目时，首选CMake，现代C/C++开发中，CMake已成为事实标准，掌握两种工具可以应对不同场景的构建需求。