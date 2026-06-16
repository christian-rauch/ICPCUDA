#ifndef DEVICE_MEMORY_HPP_
#define DEVICE_MEMORY_HPP_

#include "kernel_containers.hpp"
#include "../VulkanBuffer.h"
#include <memory>

class DeviceMemory
{
    public:
        DeviceMemory();
        ~DeviceMemory();
        DeviceMemory(size_t sizeBytes_arg);
        DeviceMemory(void *ptr_arg, size_t sizeBytes_arg);
        DeviceMemory(const DeviceMemory& other_arg);
        DeviceMemory& operator=(const DeviceMemory& other_arg);

        void create(size_t sizeBytes_arg);
        void release();
        void copyTo(DeviceMemory& other) const;
        void upload(const void *host_ptr_arg, size_t sizeBytes_arg);
        void download(void *host_ptr_arg) const;
        void swap(DeviceMemory& other_arg);

        template<class T> T* ptr() { return nullptr; }
        template<class T> const T* ptr() const { return nullptr; }

        template <class U> operator PtrSz<U>() const {
            return PtrSz<U>(nullptr, sizeBytes_ / sizeof(U));
        }

        bool empty() const;
        size_t sizeBytes() const;

        std::shared_ptr<VulkanBuffer> getVulkanBuffer() const { return buffer; }

    private:
        std::shared_ptr<VulkanBuffer> buffer;
        size_t sizeBytes_;
};

class DeviceMemory2D
{
    public:
        DeviceMemory2D();
        ~DeviceMemory2D();
        DeviceMemory2D(int rows_arg, int colsBytes_arg);
        DeviceMemory2D(int rows_arg, int colsBytes_arg, void *data_arg, size_t step_arg);
        DeviceMemory2D(const DeviceMemory2D& other_arg);
        DeviceMemory2D& operator=(const DeviceMemory2D& other_arg);

        void create(int rows_arg, int colsBytes_arg);
        void release();
        void copyTo(DeviceMemory2D& other) const;
        void upload(const void *host_ptr_arg, size_t host_step_arg, int rows_arg, int colsBytes_arg);
        void download(void *host_ptr_arg, size_t host_step_arg) const;
        void swap(DeviceMemory2D& other_arg);

        template<class T> T* ptr(int y_arg = 0) { return nullptr; }
        template<class T> const T* ptr(int y_arg = 0) const { return nullptr; }

        template <class U> operator PtrStep<U>() const {
            return PtrStep<U>(nullptr, step_);
        }
        template <class U> operator PtrStepSz<U>() const {
            return PtrStepSz<U>(rows_, colsBytes_ / sizeof(U), nullptr, step_);
        }

        bool empty() const;
        int colsBytes() const;
        int rows() const;
        size_t step() const;

        std::shared_ptr<VulkanBuffer> getVulkanBuffer() const { return buffer; }

    private:
        std::shared_ptr<VulkanBuffer> buffer;
        size_t step_;
        int colsBytes_;
        int rows_;
};

#include "device_memory_impl.hpp"

#endif /* DEVICE_MEMORY_HPP_ */
