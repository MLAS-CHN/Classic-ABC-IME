cmake_minimum_required(VERSION 3.15)
project(proto CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# DLL: proto IME
add_library(proto SHARED
    src/win/proto.cpp
    src/win/proto_core.cpp
    src/win/proto_engine.cpp
    src/win/proto_ui.cpp
    src/win/proto.def
)
target_include_directories(proto PRIVATE src/win)
target_link_libraries(proto PRIVATE gdiplus shlwapi)

# Installer
add_executable(install_proto src/win/install_proto.cpp)
target_include_directories(install_proto PRIVATE src/win)
if(MINGW)
    set_target_properties(install_proto PROPERTIES LINK_FLAGS -municode)
endif()

# Uninstaller
add_executable(uninstall_proto src/win/uninstall_proto.cpp)
target_include_directories(uninstall_proto PRIVATE src/win)
if(MINGW)
    set_target_properties(uninstall_proto PROPERTIES LINK_FLAGS -municode)
endif()
