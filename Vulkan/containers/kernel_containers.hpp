#ifndef KERNEL_CONTAINERS_HPP_
#define KERNEL_CONTAINERS_HPP_

#include <cstddef>

template<typename T> struct DevPtr
{
    typedef T elem_type;
    const static size_t elem_size = sizeof(elem_type);

    T* data;

    DevPtr() : data(0) {}
    DevPtr(T* data_arg) : data(data_arg) {}

    size_t elemSize() const { return elem_size; }
    operator       T*()       { return data; }
    operator const T*() const { return data; }
};

template<typename T> struct PtrSz : public DevPtr<T>
{
    PtrSz() : size(0) {}
    PtrSz(T* data_arg, size_t size_arg) : DevPtr<T>(data_arg), size(size_arg) {}

    size_t size;
};

template<typename T>  struct PtrStep : public DevPtr<T>
{
    PtrStep() : step(0) {}
    PtrStep(T* data_arg, size_t step_arg) : DevPtr<T>(data_arg), step(step_arg) {}

    size_t step;

    T* ptr(int y = 0)       { return nullptr; }
    const T* ptr(int y = 0) const { return nullptr; }
};

template <typename T> struct PtrStepSz : public PtrStep<T>
{
    PtrStepSz() : cols(0), rows(0) {}
    PtrStepSz(int rows_arg, int cols_arg, T* data_arg, size_t step_arg)
        : PtrStep<T>(data_arg, step_arg), cols(cols_arg), rows(rows_arg) {}

    int cols;
    int rows;
};

#endif /* KERNEL_CONTAINERS_HPP_ */
