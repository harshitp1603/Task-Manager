cmake_minimum_required(VERSION 3.16)
project(TaskManager VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 COMPONENTS Core Gui Widgets Charts REQUIRED)

# Define source files
set(SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/mainwindow.h
    src/systeminfo.cpp
    src/systeminfo.h
    src/processcategorizer.cpp
    src/processcategorizer.h
)

# Define resource files
set(RESOURCES
    resources/resources.qrc
)

add_executable(TaskManager
    ${SOURCES}
    ${RESOURCES}
)

target_link_libraries(TaskManager PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Charts
)

if(WIN32)
    target_link_libraries(TaskManager PRIVATE
        pdh
        psapi
    )
endif() 