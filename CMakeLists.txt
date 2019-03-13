cmake_minimum_required(VERSION 3.5)
set(CMAKE_CXX_STANDARD 11)
project(demo LANGUAGES CXX)

find_package(AWSSDK COMPONENTS transcribestreaming)

add_executable(${PROJECT_NAME} "record.cpp" "main.cpp")
# add_executable(${PROJECT_NAME} "main.cpp")

target_link_libraries(${PROJECT_NAME} PRIVATE portaudio ${AWSSDK_LINK_LIBRARIES})

target_compile_options(${PROJECT_NAME} PRIVATE
    "-Wall"
    "-Wextra"
    "-Wconversion"
    "-Wshadow"
    "-Wno-sign-conversion")
