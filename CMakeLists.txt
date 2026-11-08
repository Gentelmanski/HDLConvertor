cmake_minimum_required(VERSION 3.16)

project(VHDLVerilogConverter LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC OFF)
set(CMAKE_AUTORCC OFF)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(VHDLVerilogConverter
    src/main.cpp

    src/mainwindow.h
    src/mainwindow.cpp

    src/diagnostic.h

    src/lexer.h
    src/lexer.cpp

    src/parser.h
    src/parser.cpp

    src/ir.h

    src/converter.h
    src/converter.cpp
    src/expr.h
)

target_link_libraries(VHDLVerilogConverter
    PRIVATE
    Qt6::Widgets
)