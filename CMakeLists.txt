cmake_minimum_required(VERSION 3.21)
project(LamportAuthQt LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Ensure Qt’s MOC/UIC/RCC are run automatically (needed for Q_OBJECT in mainwindow.h)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Use vcpkg toolchain automatically if available (still OK when using presets)
if (DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets Network)
find_package(cryptopp CONFIG REQUIRED)

add_executable(LamportAuthQt
  src/main.cpp
  src/mainwindow.h
  src/mainwindow.cpp
  src/lamport.h
  src/lamport.cpp
)

target_link_libraries(LamportAuthQt PRIVATE
  Qt6::Widgets
  Qt6::Network
  cryptopp::cryptopp
)

# Deploy Qt DLLs next to the exe after build (Release)
if(WIN32)
  add_custom_command(TARGET LamportAuthQt POST_BUILD
    COMMAND "${Qt6_DIR}/../../../bin/windeployqt.exe" --network --compiler-runtime "$<TARGET_FILE:LamportAuthQt>"
    COMMENT "Running windeployqt"
  )
endif()
