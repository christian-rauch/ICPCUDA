#pragma once
#include <vulkan/vulkan.h>
#include <string>
#include <vector>
#include <memory>

enum PipelineType {
    PIPELINE_PYRDOWN = 0,
    PIPELINE_CREATE_VMAP,
    PIPELINE_CREATE_NMAP,
    PIPELINE_ESTIMATE,
    PIPELINE_REDUCE,
    PIPELINE_COUNT
};

class VulkanContext {
public:
    static VulkanContext& getInstance();

    ~VulkanContext();

    VkDevice getDevice() const { return device; }
    VkQueue getQueue() const { return queue; }
    VkCommandPool getCommandPool() const { return commandPool; }
    std::string getDeviceName() const { return deviceName; }

    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;

    VkCommandBuffer beginSingleTimeCommands();
    void endSingleTimeCommands(VkCommandBuffer commandBuffer);

    // Descriptor pool management
    VkDescriptorSet allocateDescriptorSet(VkDescriptorSetLayout layout);
    void resetDescriptorPool();

    // Pipeline getters
    VkPipeline getPipeline(PipelineType type) const { return pipelines[type]; }
    VkPipelineLayout getPipelineLayout(PipelineType type) const { return pipelineLayouts[type]; }
    VkDescriptorSetLayout getDescriptorSetLayout(PipelineType type) const { return descriptorSetLayouts[type]; }
    
    // Helper to compile/recreate the estimate pipeline with custom workgroup size
    VkPipeline getOrCreateEstimatePipeline(uint32_t workgroupSize);

private:
    VulkanContext();
    void initVulkan();
    void createPipelines();
    void destroyPipelines();

    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;
    uint32_t queueFamilyIndex = 0;
    std::string deviceName;

    VkCommandPool commandPool = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;

    VkPipeline pipelines[PIPELINE_COUNT] = {VK_NULL_HANDLE};
    VkPipelineLayout pipelineLayouts[PIPELINE_COUNT] = {VK_NULL_HANDLE};
    VkDescriptorSetLayout descriptorSetLayouts[PIPELINE_COUNT] = {VK_NULL_HANDLE};

    // Cache of estimate pipelines by workgroup size (specialization constants)
    struct PipelineCacheEntry {
        uint32_t workgroupSize;
        VkPipeline pipeline;
    };
    std::vector<PipelineCacheEntry> estimatePipelineCache;
};
