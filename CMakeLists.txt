cmake_minimum_required(VERSION 3.11)
set(CMAKE_CXX_STANDARD 11)
project(demo LANGUAGES CXX)

find_package(AWSSDK COMPONENTS transcribestreaming)

add_executable(${PROJECT_NAME} "main.cpp" "audio-capture.cpp")

target_link_libraries(${PROJECT_NAME} PRIVATE portaudio ${AWSSDK_LINK_LIBRARIES})

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE "/W4" "/Wx")
else()
    target_compile_options(${PROJECT_NAME} PRIVATE "-Wall" "-Wextra" "-Werror")
endif()
