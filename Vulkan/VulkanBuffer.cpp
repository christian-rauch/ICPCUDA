#include "VulkanBuffer.h"
#include "VulkanContext.h"
#include <stdexcept>
#include <cstring>

VulkanBuffer::VulkanBuffer(size_t size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties)
    : sizeBytes(size) {
    if (size == 0) return;

    VkDevice device = VulkanContext::getInstance().getDevice();

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to create Vulkan buffer!");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = VulkanContext::getInstance().findMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &memory) != VK_SUCCESS) {
        vkDestroyBuffer(device, buffer, nullptr);
        buffer = VK_NULL_HANDLE;
        throw std::runtime_error("failed to allocate Vulkan buffer memory!");
    }

    vkBindBufferMemory(device, buffer, memory, 0);
}

VulkanBuffer::~VulkanBuffer() {
    VkDevice device = VulkanContext::getInstance().getDevice();
    if (buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, buffer, nullptr);
    }
    if (memory != VK_NULL_HANDLE) {
        vkFreeMemory(device, memory, nullptr);
    }
}

void VulkanBuffer::upload(const void* data, size_t size) {
    if (size == 0 || !data) return;

    // Create staging buffer (host visible and coherent)
    VulkanBuffer stagingBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    VkDevice device = VulkanContext::getInstance().getDevice();
    void* mapped = nullptr;
    vkMapMemory(device, stagingBuffer.getMemory(), 0, size, 0, &mapped);
    std::memcpy(mapped, data, size);
    vkUnmapMemory(device, stagingBuffer.getMemory());

    // Copy to device local buffer
    stagingBuffer.copyTo(*this, size);
}

void VulkanBuffer::download(void* data, size_t size) const {
    if (size == 0 || !data) return;

    // Create staging buffer (host visible and coherent)
    VulkanBuffer stagingBuffer(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    // Copy from device local buffer to staging
    this->copyTo(stagingBuffer, size);

    VkDevice device = VulkanContext::getInstance().getDevice();
    void* mapped = nullptr;
    vkMapMemory(device, stagingBuffer.getMemory(), 0, size, 0, &mapped);
    std::memcpy(data, mapped, size);
    vkUnmapMemory(device, stagingBuffer.getMemory());
}

void VulkanBuffer::copyTo(VulkanBuffer& dest, size_t size) const {
    if (size == 0) return;

    VulkanContext& ctx = VulkanContext::getInstance();
    VkCommandBuffer commandBuffer = ctx.beginSingleTimeCommands();

    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, this->buffer, dest.getBuffer(), 1, &copyRegion);

    ctx.endSingleTimeCommands(commandBuffer);
}
