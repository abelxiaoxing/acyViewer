cmake_minimum_required(VERSION 3.16)
project(acyViewer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 COMPONENTS Core Gui Widgets Network REQUIRED)

set(PROJECT_SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/mainwindow.h
    src/settingsdialog.cpp
    src/settingsdialog.h
    src/imagefetcher.cpp
    src/imagefetcher.h
    resources.qrc
)

qt_add_executable(acyViewer
    ${PROJECT_SOURCES}
)

target_link_libraries(acyViewer PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network)

# Install rules
install(TARGETS acyViewer
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib/archive
)
