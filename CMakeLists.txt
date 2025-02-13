cmake_minimum_required(VERSION 3.17)

project(icpcuda LANGUAGES CXX)

find_package(CUDAToolkit REQUIRED)

set(CMAKE_CUDA_COMPILER ${CUDAToolkit_NVCC_EXECUTABLE})
enable_language(CUDA)

find_package(Eigen3 REQUIRED NO_MODULE)
find_package(Sophus REQUIRED NO_MODULE)
find_package(OpenCV REQUIRED COMPONENTS core imgcodecs)

set(CUDA_NVCC_FLAGS ${CUDA_NVCC_FLAGS}  "-Xcompiler;-fPIC;--expt-relaxed-constexpr")
set(CMAKE_CXX_FLAGS "-O3 -msse2 -msse3 -Wall")

add_library(icpcuda SHARED
    ICPOdometry.cpp
    Cuda/estimate.cu
    Cuda/pyrdown.cu
    Cuda/containers/device_memory.cpp
    Cuda/containers/initialization.cpp
)
target_link_libraries(icpcuda
    PUBLIC
        CUDA::cudart Eigen3::Eigen
    PRIVATE
        Sophus::Sophus
)
set_target_properties(icpcuda PROPERTIES PUBLIC_HEADER ICPOdometry.h)
target_include_directories(icpcuda PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)
install(TARGETS icpcuda
    EXPORT icpcudaTargets
)

add_executable(ICP ICP.cpp)
target_link_libraries(ICP icpcuda opencv_core opencv_imgcodecs)
install(TARGETS ICP)

install(EXPORT icpcudaTargets
    NAMESPACE ${PROJECT_NAME}::
    DESTINATION share/${PROJECT_NAME}/cmake
    FILE ${PROJECT_NAME}Config.cmake
)
