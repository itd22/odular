cmake_minimum_required(VERSION 3.16)
project(odular VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets Xml)

find_package(PkgConfig REQUIRED)
pkg_check_modules(POPPLER_QT6 REQUIRED IMPORTED_TARGET poppler-qt6)

add_executable(odular
    src/main.cpp
    src/xmpreader.cpp
    src/pdfpageview.cpp
    src/dualviewwindow.cpp
)

target_include_directories(odular PRIVATE src)

target_link_libraries(odular PRIVATE
    Qt6::Widgets
    Qt6::Xml
    PkgConfig::POPPLER_QT6
)

install(TARGETS odular RUNTIME DESTINATION bin)
