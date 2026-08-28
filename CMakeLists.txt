cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME SmartHome)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

file(GLOB_RECURSE USER_SOURCES
    "User/Src/*.c"
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user sources here
    ${USER_SOURCES}
)

# mystm32-sdk 组件（sdk.toml 拉取至 MySDK/，源文件与头文件清单由 SDK 自带的
# CMakeLists.txt 维护并随拉取更新；须位于 add_subdirectory(cmake/stm32cubemx)
# 之后——组件需要其中的 stm32cubemx INTERFACE 目标获取 HAL/RTOS/LwIP 依赖）
add_subdirectory(MySDK)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
    User                          # board_cfg.h（sdk.toml: board_cfg = "User/board_cfg.h"）
    User/Inc
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx
    mystm32

    # Add user defined libraries
)

# 排除 LwIP 自带 MQTT 应用：其 mqtt_client_connect / mqtt_client_is_connected
# 与 User/mystm32-sdk/protocols/mqtt 的同名符号在链接期冲突；
# 工程统一使用 SDK 的 MQTT 实现，LwIP 的 apps/mqtt 不参与链接。
# 注意放在 add_subdirectory(cmake/stm32cubemx) 之后（此处即满足）。
if(TARGET LwIP)
    get_target_property(LWIP_SOURCES LwIP SOURCES)
    if(LWIP_SOURCES)
        list(FILTER LWIP_SOURCES EXCLUDE REGEX "/apps/mqtt/mqtt\\.c$")
        set_target_properties(LwIP PROPERTIES SOURCES "${LWIP_SOURCES}")
        message(STATUS "Excluded LwIP apps/mqtt/mqtt.c (clashes with mystm32-sdk protocols/mqtt)")
    endif()
endif()

# 编译后自动生成 .hex 和 .bin 文件
add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:${CMAKE_PROJECT_NAME}> $<TARGET_FILE_DIR:${CMAKE_PROJECT_NAME}>/${CMAKE_PROJECT_NAME}.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${CMAKE_PROJECT_NAME}> $<TARGET_FILE_DIR:${CMAKE_PROJECT_NAME}>/${CMAKE_PROJECT_NAME}.bin
    COMMENT "Generating HEX and BIN firmware"
)