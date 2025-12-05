# CMake 最低版本要求
cmake_minimum_required(VERSION 3.10)

# 项目名称和版本
project(ModEngine VERSION 1.0 LANGUAGES C CXX)

# 设置 C++ 标准为 C++14
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 针对 Visual Studio 编译器的特定配置
if(MSVC)
    # 设置 MSVC 运行时库为动态链接
    # 对于 Release 配置，使用 /MD (MultiThreadedDLL)
    # 对于 Debug 配置，使用 /MDd (MultiThreadedDebugDLL)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

# 定义源文件列表
set(SOURCES
    pch.cpp
    dllmain.cpp
    DLLWrapper/DInputWrapper.cpp
    MemoryPatcher/MemoryPatcher.cpp
    LibraryLoader/LibraryLoader.cpp
    DLLWrapper/exports.def
)

# 添加动态链接库目标
add_library(${PROJECT_NAME} SHARED ${SOURCES})

# 添加预编译头
target_precompile_headers(${PROJECT_NAME} PRIVATE pch.h)

# 添加头文件包含目录
target_include_directories(${PROJECT_NAME} PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

# 打印一些信息以供调试
message(STATUS "Project Name: ${PROJECT_NAME}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID}")
if(MSVC)
    message(STATUS "MSVC Runtime Library: ${CMAKE_MSVC_RUNTIME_LIBRARY}")
    message(STATUS "Generator Toolset: ${CMAKE_GENERATOR_TOOLSET}")
endif()
