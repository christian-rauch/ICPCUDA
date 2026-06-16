#include "device_memory.hpp"
#include <algorithm>
#include <cassert>
#include <cstring>
#include "../VulkanContext.h"

////////////////////////    DeviceMemory    /////////////////////////////

DeviceMemory::DeviceMemory() : sizeBytes_(0) {}

DeviceMemory::~DeviceMemory() { release(); }

DeviceMemory::DeviceMemory(size_t sizeBytes_arg) : sizeBytes_(0) {
    create(sizeBytes_arg);
}

DeviceMemory::DeviceMemory(void *ptr_arg, size_t sizeBytes_arg) : sizeBytes_(sizeBytes_arg) {
    // User pointer constructor is a stub for Vulkan
}

DeviceMemory::DeviceMemory(const DeviceMemory &other_arg)
    : buffer(other_arg.buffer), sizeBytes_(other_arg.sizeBytes_) {}

DeviceMemory &DeviceMemory::operator=(const DeviceMemory &other_arg) {
    if (this != &other_arg) {
        buffer = other_arg.buffer;
        sizeBytes_ = other_arg.sizeBytes_;
    }
    return *this;
}

void DeviceMemory::create(size_t sizeBytes_arg) {
    if (sizeBytes_arg == sizeBytes_ && buffer)
        return;

    if (sizeBytes_arg > 0) {
        sizeBytes_ = sizeBytes_arg;
        buffer = std::make_shared<VulkanBuffer>(
            sizeBytes_,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );
    } else {
        release();
    }
}

void DeviceMemory::copyTo(DeviceMemory &other) const {
    if (empty()) {
        other.release();
    } else {
        other.create(sizeBytes_);
        buffer->copyTo(*other.buffer, sizeBytes_);
    }
}

void DeviceMemory::release() {
    buffer.reset();
    sizeBytes_ = 0;
}

void DeviceMemory::upload(const void *host_ptr_arg, size_t sizeBytes_arg) {
    create(sizeBytes_arg);
    if (buffer) {
        buffer->upload(host_ptr_arg, sizeBytes_arg);
    }
}

void DeviceMemory::download(void *host_ptr_arg) const {
    if (buffer) {
        buffer->download(host_ptr_arg, sizeBytes_);
    }
}

void DeviceMemory::swap(DeviceMemory &other_arg) {
    std::swap(buffer, other_arg.buffer);
    std::swap(sizeBytes_, other_arg.sizeBytes_);
}

bool DeviceMemory::empty() const { return !buffer; }
size_t DeviceMemory::sizeBytes() const { return sizeBytes_; }


////////////////////////    DeviceMemory2D    /////////////////////////////

DeviceMemory2D::DeviceMemory2D()
    : step_(0), colsBytes_(0), rows_(0) {}

DeviceMemory2D::~DeviceMemory2D() { release(); }

DeviceMemory2D::DeviceMemory2D(int rows_arg, int colsBytes_arg)
    : step_(0), colsBytes_(0), rows_(0) {
    create(rows_arg, colsBytes_arg);
}

DeviceMemory2D::DeviceMemory2D(int rows_arg, int colsBytes_arg, void *data_arg, size_t step_arg)
    : step_(step_arg), colsBytes_(colsBytes_arg), rows_(rows_arg) {
    // Stub
}

DeviceMemory2D::DeviceMemory2D(const DeviceMemory2D &other_arg)
    : buffer(other_arg.buffer), step_(other_arg.step_),
      colsBytes_(other_arg.colsBytes_), rows_(other_arg.rows_) {}

DeviceMemory2D &DeviceMemory2D::operator=(const DeviceMemory2D &other_arg) {
    if (this != &other_arg) {
        buffer = other_arg.buffer;
        step_ = other_arg.step_;
        colsBytes_ = other_arg.colsBytes_;
        rows_ = other_arg.rows_;
    }
    return *this;
}

void DeviceMemory2D::create(int rows_arg, int colsBytes_arg) {
    if (colsBytes_ == colsBytes_arg && rows_ == rows_arg && buffer)
        return;

    if (rows_arg > 0 && colsBytes_arg > 0) {
        colsBytes_ = colsBytes_arg;
        rows_ = rows_arg;
        
        // Align row step to 256 bytes for performance/alignment
        step_ = ((colsBytes_ + 255) / 256) * 256;

        buffer = std::make_shared<VulkanBuffer>(
            step_ * rows_,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );
    } else {
        release();
    }
}

void DeviceMemory2D::release() {
    buffer.reset();
    colsBytes_ = 0;
    rows_ = 0;
    step_ = 0;
}

void DeviceMemory2D::copyTo(DeviceMemory2D &other) const {
    if (empty()) {
        other.release();
    } else {
        other.create(rows_, colsBytes_);
        buffer->copyTo(*other.buffer, step_ * rows_);
    }
}

void DeviceMemory2D::upload(const void *host_ptr_arg, size_t host_step_arg,
                            int rows_arg, int colsBytes_arg) {
    create(rows_arg, colsBytes_arg);
    if (!buffer) return;

    // Create a host-visible staging buffer representing the pitched layout
    size_t totalStagingSize = step_ * rows_;
    VulkanBuffer stagingBuffer(totalStagingSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    VkDevice device = VulkanContext::getInstance().getDevice();
    void* mapped = nullptr;
    vkMapMemory(device, stagingBuffer.getMemory(), 0, totalStagingSize, 0, &mapped);

    // Copy row by row (performing pitch conversion)
    for (int r = 0; r < rows_; ++r) {
        std::memcpy(static_cast<char*>(mapped) + r * step_,
                    static_cast<const char*>(host_ptr_arg) + r * host_step_arg,
                    colsBytes_);
    }
    vkUnmapMemory(device, stagingBuffer.getMemory());

    // Copy to device local buffer
    stagingBuffer.copyTo(*buffer, totalStagingSize);
}

void DeviceMemory2D::download(void *host_ptr_arg, size_t host_step_arg) const {
    if (!buffer) return;

    size_t totalStagingSize = step_ * rows_;
    VulkanBuffer stagingBuffer(totalStagingSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    // Copy from device local to staging
    buffer->copyTo(stagingBuffer, totalStagingSize);

    VkDevice device = VulkanContext::getInstance().getDevice();
    void* mapped = nullptr;
    vkMapMemory(device, stagingBuffer.getMemory(), 0, totalStagingSize, 0, &mapped);

    // Copy row by row from staging (pitched) to host memory
    for (int r = 0; r < rows_; ++r) {
        std::memcpy(static_cast<char*>(host_ptr_arg) + r * host_step_arg,
                    static_cast<const char*>(mapped) + r * step_,
                    colsBytes_);
    }
    vkUnmapMemory(device, stagingBuffer.getMemory());
}

void DeviceMemory2D::swap(DeviceMemory2D &other_arg) {
    std::swap(buffer, other_arg.buffer);
    std::swap(step_, other_arg.step_);
    std::swap(colsBytes_, other_arg.colsBytes_);
    std::swap(rows_, other_arg.rows_);
}

bool DeviceMemory2D::empty() const { return !buffer; }
int DeviceMemory2D::colsBytes() const { return colsBytes_; }
int DeviceMemory2D::rows() const { return rows_; }
size_t DeviceMemory2D::step() const { return step_; }
