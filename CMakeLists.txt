cmake_minimum_required(VERSION 3.17)

project(icpcuda LANGUAGES CXX)

find_package(Vulkan REQUIRED)
find_program(GLSLC_EXECUTABLE glslc REQUIRED)

find_package(Eigen3 REQUIRED NO_MODULE)
set(Unwind_INCLUDE_DIR "/usr/include/libunwind" CACHE PATH "Path to unwind include directory" FORCE)
find_package(glog REQUIRED)
find_package(Sophus REQUIRED NO_MODULE)
find_package(OpenCV REQUIRED COMPONENTS core imgcodecs)

set(CMAKE_CXX_FLAGS "-O3 -msse2 -msse3 -Wall")

# Setup GLSL shader compiler custom commands
set(SHADERS
    Vulkan/shaders/pyrdown.comp
    Vulkan/shaders/create_vmap.comp
    Vulkan/shaders/create_nmap.comp
    Vulkan/shaders/estimate.comp
    Vulkan/shaders/reduce.comp
)

set(SHADER_HEADERS "")
foreach(SHADER ${SHADERS})
    get_filename_component(SHADER_NAME ${SHADER} NAME_WE)
    set(HEADER_OUT "${CMAKE_CURRENT_BINARY_DIR}/${SHADER_NAME}_spv.h")
    add_custom_command(
        OUTPUT ${HEADER_OUT}
        COMMAND ${GLSLC_EXECUTABLE} -mfmt=c -O -o ${HEADER_OUT} ${CMAKE_CURRENT_SOURCE_DIR}/${SHADER}
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${SHADER}
        COMMENT "Compiling shader ${SHADER} to header ${HEADER_OUT}"
    )
    list(APPEND SHADER_HEADERS ${HEADER_OUT})
endforeach()

add_library(icpcuda SHARED
    ICPOdometry.cpp
    Vulkan/vulkan_impl.cpp
    Vulkan/VulkanContext.cpp
    Vulkan/VulkanBuffer.cpp
    Vulkan/containers/device_memory.cpp
    ${SHADER_HEADERS}
)

target_link_libraries(icpcuda
    PUBLIC
        Eigen3::Eigen
    PRIVATE
        Vulkan::Vulkan Sophus::Sophus
)

include(GNUInstallDirs)
file(GLOB_RECURSE VULKAN_HEADERS
    "${CMAKE_SOURCE_DIR}/Vulkan/*.h"
    "${CMAKE_SOURCE_DIR}/Vulkan/*.hpp"
)
foreach(HEADER ${VULKAN_HEADERS})
    file(RELATIVE_PATH HEADER_REL "${CMAKE_SOURCE_DIR}/Vulkan" "${HEADER}")
    get_filename_component(HEADER_DIR "${HEADER_REL}" DIRECTORY)
    install(FILES "${HEADER}"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/Vulkan/${HEADER_DIR}"
    )
endforeach()

file(GLOB PROJECT_HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
)
foreach(HEADER ${PROJECT_HEADERS})
    install(FILES "${HEADER}"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    )
endforeach()
target_include_directories(icpcuda PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

install(TARGETS icpcuda
    EXPORT icpcudaTargets
)

add_executable(ICP ICP.cpp)
target_link_libraries(ICP icpcuda opencv_core opencv_imgcodecs Vulkan::Vulkan)
install(TARGETS ICP)

include(CMakePackageConfigHelpers)
configure_package_config_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/Config.cmake.in
  "${CMAKE_CURRENT_BINARY_DIR}/icpcudaConfig.cmake"
  INSTALL_DESTINATION share/${PROJECT_NAME}/cmake
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/icpcudaConfig.cmake"
    DESTINATION share/${PROJECT_NAME}/cmake
)

install(EXPORT icpcudaTargets
    NAMESPACE ${PROJECT_NAME}::
    DESTINATION share/${PROJECT_NAME}/cmake
    FILE ${PROJECT_NAME}Targets.cmake
)
