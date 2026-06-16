#ifndef DEVICE_ARRAY_HPP_
#define DEVICE_ARRAY_HPP_

#include "device_memory.hpp"
#include <vector>

template<class T>
class DeviceArray : public DeviceMemory
{
    public:
        typedef T type;
        enum { elem_size = sizeof(T) };

        DeviceArray();
        DeviceArray(size_t size);
        DeviceArray(T *ptr, size_t size);
        DeviceArray(const DeviceArray& other);
        DeviceArray& operator = (const DeviceArray& other);

        void create(size_t size);
        void release();
        void copyTo(DeviceArray& other) const;
        void upload(const T *host_ptr, size_t size);
        void download(T *host_ptr) const;

        template<class A>
        void upload(const std::vector<T, A>& data);

        template<typename A>
        void download(std::vector<T, A>& data) const;

        void swap(DeviceArray& other_arg);

        T* ptr();
        const T* ptr() const;

        operator T*();
        operator const T*() const;

        size_t size() const;
};

template<class T>
class DeviceArray2D : public DeviceMemory2D
{
    public:
        typedef T type;
        enum { elem_size = sizeof(T) };

        DeviceArray2D();
        DeviceArray2D(int rows, int cols);
        DeviceArray2D(int rows, int cols, void *data, size_t stepBytes);
        DeviceArray2D(const DeviceArray2D& other);
        DeviceArray2D& operator = (const DeviceArray2D& other);

        void create(int rows, int cols);
        void release();
        void copyTo(DeviceArray2D& other) const;

        void upload(const void *host_ptr, size_t host_step, int rows, int cols);
        void download(void *host_ptr, size_t host_step) const;

        template<class A>
        void upload(const std::vector<T, A>& data, int cols);

        template<class A>
        void download(std::vector<T, A>& data, int& cols) const;

        void swap(DeviceArray2D& other_arg);

        T* ptr(int y = 0);
        const T* ptr(int y = 0) const;
        
        operator T*();
        operator const T*() const;
        
        int cols() const;
        int rows() const;
        size_t elem_step() const;
};

#include "device_array_impl.hpp"

#endif /* DEVICE_ARRAY_HPP_ */
