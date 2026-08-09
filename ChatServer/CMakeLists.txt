# 设置Cmake的最小版本
cmake_minimum_required(VERSION 3.10)

# 项目名称
project(AIChatServer)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置构建类型Debug
set(CMAKE_BUILD_TYPE Debug)

# 添加可执行文件
add_executable(AIChatServer main.cpp ChatServer.cpp)

# 设置输出目录
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})

# 查找依赖库
find_package(OpenSSL REQUIRED)

# 添加CPPHTTPLIB_OPENSSL_SUPPORT定义
target_compile_definitions(AIChatServer PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT)

# ====== 静态库查找 ======
# ai_chat_sdk.a 优先使用刚刚在 SDK 构建目录编译的最新版本，
# 避免链接到 /usr/local/lib 里已安装的旧版本。
set(AI_CHAT_SDK_LIB "${CMAKE_CURRENT_SOURCE_DIR}/../sdk/build/libai_chat_sdk.a")
if(NOT EXISTS "${AI_CHAT_SDK_LIB}")
    # 回退：直接用系统路径查找
    find_library(AI_CHAT_SDK_LIB_PATH ai_chat_sdk PATHS /usr/local/lib)
    if(AI_CHAT_SDK_LIB_PATH)
        set(AI_CHAT_SDK_LIB "${AI_CHAT_SDK_LIB_PATH}")
    else()
        message(FATAL_ERROR "找不到 libai_chat_sdk.a，请先在 sdk/build 执行 make 构建 SDK")
    endif()
endif()
message(STATUS "[AIChatServer] 将链接 ai_chat_sdk: ${AI_CHAT_SDK_LIB}")

# 其他系统库的查找目录
link_directories(/usr/local/lib)

# 添加 SDK 头文件路径（优先使用本地构建的最新头文件）
target_include_directories(AIChatServer PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../sdk/include")

# 链接库（ai_chat_sdk 使用绝对路径放在最前，确保用最新构建）
target_link_libraries(AIChatServer
    "${AI_CHAT_SDK_LIB}"
    jsoncpp
    fmt
    spdlog
    gflags
    OpenSSL::SSL
    OpenSSL::Crypto
    sqlite3
    dl
    pthread
    gflags)

# ==========================================================
# 复制前端静态资源 (ChatServer/www) 到构建目录 (build/www)
# 这样运行 ./AIChatServer 时可以直接读取 ./www/index.html
# ==========================================================
set(WWW_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/www)
set(WWW_TARGET_DIR ${CMAKE_BINARY_DIR}/www)

# 1) 配置阶段先创建空目录 + 拷贝一次，确保首次构建就存在
file(MAKE_DIRECTORY ${WWW_TARGET_DIR})
if(EXISTS ${WWW_SOURCE_DIR})
    file(GLOB WWW_FILES
        ${WWW_SOURCE_DIR}/*.html
        ${WWW_SOURCE_DIR}/*.css
        ${WWW_SOURCE_DIR}/*.js
        ${WWW_SOURCE_DIR}/*.svg
        ${WWW_SOURCE_DIR}/*.png
        ${WWW_SOURCE_DIR}/*.ico
    )
    foreach(F IN LISTS WWW_FILES)
        file(COPY ${F} DESTINATION ${WWW_TARGET_DIR})
    endforeach()
    message(STATUS "[AIChatServer] 已拷贝前端资源到: ${WWW_TARGET_DIR}")
else()
    message(WARNING "[AIChatServer] 源码目录未找到 www: ${WWW_SOURCE_DIR}，前端页面可能缺失")
endif()

# 2) 构建时自定义命令，每次构建都同步最新文件
add_custom_target(copy_www ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory ${WWW_TARGET_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${WWW_SOURCE_DIR}
            ${WWW_TARGET_DIR}
    COMMENT "[AIChatServer] 同步前端资源 www -> build/www"
    DEPENDS ${WWW_SOURCE_DIR}
)
add_dependencies(AIChatServer copy_www)

# 3) 默认配置文件 ChatServer.conf 也拷贝到构建目录
set(CONF_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/ChatServer.conf)
set(CONF_TARGET ${CMAKE_BINARY_DIR}/ChatServer.conf)
if(EXISTS ${CONF_SOURCE})
    file(COPY ${CONF_SOURCE} DESTINATION ${CMAKE_BINARY_DIR})
endif()
