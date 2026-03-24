cmake_minimum_required(VERSION 4.1)
project(CopilotClaw)

if (NOT WIN32)
    message(FATAL_ERROR "CopilotClaw supports only Windows 11 x86_64.")
endif ()

if (NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "CopilotClaw supports only x86_64.")
endif ()

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if (MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif ()

set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO ON)

add_executable(CopilotClaw WIN32 main.cpp app.rc)
target_link_libraries(CopilotClaw PRIVATE ws2_32)

if (MSVC)
    target_compile_definitions(CopilotClaw PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
    target_compile_options(CopilotClaw PRIVATE /O2 /Oi /Ot /Gy /Gw /permissive-)
    target_link_options(CopilotClaw PRIVATE /SUBSYSTEM:WINDOWS)
endif ()
