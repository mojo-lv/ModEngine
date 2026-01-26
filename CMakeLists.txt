# CMake 最低版本要求
cmake_minimum_required(VERSION 3.10)

# 项目名称和版本
project(dinput8 VERSION 1.0 LANGUAGES C CXX)

# 设置 C++ 标准为 C++17
set(CMAKE_CXX_STANDARD 17)
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
    DllExports/exports.def
    DllExports/exported.cpp
    pch.cpp
    dllmain.cpp
    D3D11Hook/D3D11Hook.cpp
    DebugMenu/DebugMenu.cpp
    DebugMenu/Graphics.cpp
    FilesMod/FilesMod.cpp
    InputProcess/InputProcess.cpp
    MemoryPatch/MemoryPatch.cpp
    ThirdParty/imgui/imgui.cpp
    ThirdParty/imgui/imgui_draw.cpp
    ThirdParty/imgui/imgui_tables.cpp
    ThirdParty/imgui/imgui_widgets.cpp
    ThirdParty/imgui/imgui_impl_dx11.cpp
    ThirdParty/imgui/imgui_impl_win32.cpp
)

# 添加动态链接库目标
add_library(${PROJECT_NAME} SHARED ${SOURCES})

# 添加预编译头
target_precompile_headers(${PROJECT_NAME} PRIVATE pch.h)

# 添加头文件包含目录
target_include_directories(${PROJECT_NAME} PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

# 为 MSVC 添加优化标志
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE
        # 最大速度优化
        $<$<CONFIG:Release>:/O2>
        # 函数级别链接
        $<$<CONFIG:Release>:/Gy>
        # 全局数据优化
        $<$<CONFIG:Release>:/Gw>
        # 启用全程序编译优化（链接时代码生成）
        $<$<CONFIG:Release>:/GL>
    )
    target_link_options(${PROJECT_NAME} PRIVATE
        # 减少未用代码和数据
        $<$<CONFIG:Release>:/OPT:REF>
        # 折叠相同代码
        $<$<CONFIG:Release>:/OPT:ICF>
        # 禁用增量链接
        $<$<CONFIG:Release>:/INCREMENTAL:NO>
        # 启用全程序优化（链接时代码生成）
        $<$<CONFIG:Release>:/LTCG>
    )
endif()

# 从源码构建 minhook
add_subdirectory(ThirdParty/minhook)

# 链接到编译后的 minhook 库以及其他系统库
target_link_libraries(${PROJECT_NAME} PRIVATE minhook d3d11)

# 打印一些信息以供调试
message(STATUS "Project Name: ${PROJECT_NAME}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID}")
if(MSVC)
    message(STATUS "MSVC Runtime Library: ${CMAKE_MSVC_RUNTIME_LIBRARY}")
    message(STATUS "Generator Toolset: ${CMAKE_GENERATOR_TOOLSET}")
endif()
