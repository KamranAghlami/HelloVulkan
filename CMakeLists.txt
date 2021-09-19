cmake_minimum_required(VERSION 3.21.0)

project(HelloVulkan LANGUAGES CXX VERSION 0.0.1)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB_RECURSE SOURCES "${CMAKE_SOURCE_DIR}/src/*.cpp")

add_executable(${PROJECT_NAME} ${SOURCES})

include(cmake/config.cmake)
configure_file(src/${PROJECT_NAME}_config.h.in ${PROJECT_NAME}_config.h)

target_include_directories(${PROJECT_NAME}
  PRIVATE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
    $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>
)

include(FetchContent)
FetchContent_Declare(
  spdlog
  GIT_REPOSITORY https://github.com/gabime/spdlog.git
  GIT_TAG tags/v1.9.2
)
set(SPDLOG_INSTALL OFF CACHE BOOL "disable installing of spdlog.")
FetchContent_MakeAvailable(spdlog)
target_link_libraries(${PROJECT_NAME} PRIVATE spdlog)

find_package(Vulkan)
target_link_libraries(${PROJECT_NAME} PRIVATE Vulkan::Vulkan)

FetchContent_Declare(
  glfw
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG tags/3.3.4
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "disable building of GLFW's documentation.")
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "disable building of GLFW's examples.")
set(GLFW_BUILD_TESTS OFF CACHE BOOL "disable building of GLFW's tests.")
set(GLFW_INSTALL OFF CACHE BOOL "disable installing of GLFW.")
FetchContent_MakeAvailable(glfw)
target_link_libraries(${PROJECT_NAME} PRIVATE glfw)

FetchContent_Declare(
  glm
  GIT_REPOSITORY https://github.com/g-truc/glm.git
  GIT_TAG tags/0.9.9.8
)
FetchContent_MakeAvailable(glm)
target_link_libraries(${PROJECT_NAME} PRIVATE glm)