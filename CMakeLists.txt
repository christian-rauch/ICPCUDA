cmake_minimum_required(VERSION 3.17)

project(ICP LANGUAGES CXX)

find_package(CUDAToolkit REQUIRED)

set(CMAKE_CUDA_COMPILER ${CUDAToolkit_NVCC_EXECUTABLE})
enable_language(CUDA)

find_package(Eigen3 REQUIRED NO_MODULE)
find_package(Sophus REQUIRED NO_MODULE)
find_package(OpenCV REQUIRED COMPONENTS core imgcodecs)

include_directories(${Sophus_INCLUDE_DIR})

file(GLOB srcs *.cpp *.cu *.h *.cuh)
file(GLOB cuda Cuda/*.cu Cuda/*.cpp Cuda/*.cuh Cuda/*.h)
file(GLOB containers Cuda/containers/*.cpp Cuda/containers/*.h Cuda/containers/*.cu Cuda/containers/*.cuh)

set(CUDA_NVCC_FLAGS ${CUDA_NVCC_FLAGS}  "-Xcompiler;-fPIC;--expt-relaxed-constexpr")

add_library(cuda_objs ${cuda})
target_link_libraries(cuda_objs CUDA::cudart Eigen3::Eigen)

set(CMAKE_CXX_FLAGS "-O3 -msse2 -msse3 -Wall")

add_executable(ICP
               ${srcs}
               ${containers}
)

target_link_libraries(ICP
                      Eigen3::Eigen
                      cuda_objs
                      opencv_core
                      opencv_imgcodecs
                      Sophus::Sophus
)

