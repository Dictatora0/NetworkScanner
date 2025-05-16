cmake_minimum_required(VERSION 3.16)
project(NetScanner VERSION 1.0 LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 COMPONENTS Core Widgets Network Concurrent REQUIRED)

add_executable(NetScanner
    main.cpp
    mainwindow.cpp
    mainwindow.h
    networkscanner.cpp
    networkscanner.h
)

target_link_libraries(NetScanner PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
    Qt6::Concurrent
)

# 如果使用的是Qt5，请使用以下代码替换上面的find_package和target_link_libraries
# find_package(Qt5 COMPONENTS Core Widgets Network Concurrent REQUIRED)
# target_link_libraries(NetScanner PRIVATE Qt5::Core Qt5::Widgets Qt5::Network Qt5::Concurrent) 