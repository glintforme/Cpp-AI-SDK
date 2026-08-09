#设置Cmake最小版本
cmake_minimum_required(VERSION 3.10)

#项目名称
project(testLLM)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置构建类型Debug
set(CMAKE_BUILD_TYPE Debug)

# 添加可执行文件
add_executable(testLLM testLLM.cpp
../sdk/src/util/MyLog.cpp
../sdk/src/DeepSeekProvider.cpp
../sdk/src/KIMI3Provider.cpp
../sdk/src/GeminiProvider.cpp
../sdk/src/OllamaLLMProvider.cpp
../sdk/src/DataManager.cpp
../sdk/src/SessionManager.cpp
../sdk/src/LLMManager.cpp
../sdk/src/ChatSDK.cpp)

# 设置输出目录
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})

# 添加头文件搜索路径
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../sdk/include)
include_directories(/usr/include/jsoncpp)

find_package(OpenSSL REQUIRED)
include_directories(${OPENSSL_INCLUDE_DIR})

find_package(PkgConfig REQUIRED)
pkg_check_modules(JSONCPP REQUIRED jsoncpp)
include_directories(${JSONCPP_INCLUDE_DIRS})

# 添加CPPHTTPLIB_OPENSSL_SUPPORT定义
target_compile_definitions(testLLM PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT)

# 链接库
target_link_libraries(testLLM jsoncpp fmt  spdlog gtest OpenSSL::SSL OpenSSL::Crypto sqlite3)
