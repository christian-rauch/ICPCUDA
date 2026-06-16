#pragma once
#include <vulkan/vulkan.h>
#include <cstddef>

class VulkanBuffer {
public:
    VulkanBuffer(size_t size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);
    ~VulkanBuffer();

    // Prevent copying to avoid double frees
    VulkanBuffer(const VulkanBuffer&) = delete;
    VulkanBuffer& operator=(const VulkanBuffer&) = delete;

    void upload(const void* data, size_t size);
    void download(void* data, size_t size) const;
    void copyTo(VulkanBuffer& dest, size_t size) const;

    VkBuffer getBuffer() const { return buffer; }
    VkDeviceMemory getMemory() const { return memory; }
    size_t getSize() const { return sizeBytes; }

private:
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    size_t sizeBytes = 0;
};
