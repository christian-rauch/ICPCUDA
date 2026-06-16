#include "VulkanContext.h"
#include <stdexcept>
#include <iostream>
#include <algorithm>
#include <cstring>

// Include compiled SPIR-V headers generated during CMake build
const uint32_t pyrdown_spv[] =
#include "pyrdown_spv.h"
;

const uint32_t create_vmap_spv[] =
#include "create_vmap_spv.h"
;

const uint32_t create_nmap_spv[] =
#include "create_nmap_spv.h"
;

const uint32_t estimate_spv[] =
#include "estimate_spv.h"
;

const uint32_t reduce_spv[] =
#include "reduce_spv.h"
;

VulkanContext& VulkanContext::getInstance() {
    static VulkanContext instance;
    return instance;
}

VulkanContext::VulkanContext() {
    initVulkan();
    createPipelines();
}

VulkanContext::~VulkanContext() {
    VkDevice dev = device;
    if (dev != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(dev);

        destroyPipelines();

        if (descriptorPool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(dev, descriptorPool, nullptr);
        }
        if (commandPool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(dev, commandPool, nullptr);
        }
        vkDestroyDevice(dev, nullptr);
    }
    if (instance != VK_NULL_HANDLE) {
        vkDestroyInstance(instance, nullptr);
    }
}

void VulkanContext::initVulkan() {
    // 1. Create Vulkan Instance
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "icpcuda-vulkan";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_1; // Use Vulkan 1.1

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    // We do not require any instance layers or extensions
    createInfo.enabledExtensionCount = 0;
    createInfo.enabledLayerCount = 0;

    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
        throw std::runtime_error("failed to create Vulkan instance!");
    }

    // 2. Select Physical Device
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    if (deviceCount == 0) {
        throw std::runtime_error("failed to find GPUs with Vulkan support!");
    }
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    for (const auto& dev : devices) {
        VkPhysicalDeviceProperties deviceProperties;
        vkGetPhysicalDeviceProperties(dev, &deviceProperties);

        // Check for compute queue family
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &queueFamilyCount, queueFamilies.data());

        bool foundCompute = false;
        uint32_t computeIdx = 0;
        for (uint32_t i = 0; i < queueFamilyCount; ++i) {
            if (queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
                computeIdx = i;
                foundCompute = true;
                break;
            }
        }

        if (foundCompute) {
            physicalDevice = dev;
            queueFamilyIndex = computeIdx;
            deviceName = deviceProperties.deviceName;
            // Prioritize integrated/discrete GPUs
            if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ||
                deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
                break;
            }
        }
    }

    if (physicalDevice == VK_NULL_HANDLE) {
        throw std::runtime_error("failed to find a suitable Vulkan physical device!");
    }

    // 3. Create Logical Device
    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = queueFamilyIndex;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    std::vector<const char*> deviceExtensions = {
        VK_KHR_16BIT_STORAGE_EXTENSION_NAME,
        VK_KHR_STORAGE_BUFFER_STORAGE_CLASS_EXTENSION_NAME
    };

    VkPhysicalDevice16BitStorageFeatures features16{};
    features16.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES;
    features16.storageBuffer16BitAccess = VK_TRUE;

    VkDeviceCreateInfo deviceCreateInfo{};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.pNext = &features16;
    deviceCreateInfo.queueCreateInfoCount = 1;
    deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
    deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

    if (vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device) != VK_SUCCESS) {
        throw std::runtime_error("failed to create Vulkan logical device!");
    }

    vkGetDeviceQueue(device, queueFamilyIndex, 0, &queue);

    // 4. Create Command Pool
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = queueFamilyIndex;

    if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create Vulkan command pool!");
    }

    // 5. Create Descriptor Pool
    VkDescriptorPoolSize poolSizes[] = {
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 200 }
    };
    VkDescriptorPoolCreateInfo descPoolInfo{};
    descPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descPoolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    descPoolInfo.maxSets = 100;
    descPoolInfo.poolSizeCount = 1;
    descPoolInfo.pPoolSizes = poolSizes;

    if (vkCreateDescriptorPool(device, &descPoolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create Vulkan descriptor pool!");
    }
}

uint32_t VulkanContext::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    throw std::runtime_error("failed to find suitable memory type!");
}

VkCommandBuffer VulkanContext::beginSingleTimeCommands() {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);
    return commandBuffer;
}

void VulkanContext::endSingleTimeCommands(VkCommandBuffer commandBuffer) {
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

VkDescriptorSet VulkanContext::allocateDescriptorSet(VkDescriptorSetLayout layout) {
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layout;

    VkDescriptorSet set;
    if (vkAllocateDescriptorSets(device, &allocInfo, &set) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate Vulkan descriptor set!");
    }
    return set;
}

void VulkanContext::resetDescriptorPool() {
    vkResetDescriptorPool(device, descriptorPool, 0);
}

static VkShaderModule createShaderModule(VkDevice device, const uint32_t* code, size_t sizeBytes) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = sizeBytes;
    createInfo.pCode = code;

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("failed to create shader module!");
    }
    return shaderModule;
}

void VulkanContext::createPipelines() {
    // Define layout configurations for all 5 shaders
    struct LayoutConfig {
        uint32_t bindingsCount;
        uint32_t pushConstantSize;
    } configs[PIPELINE_COUNT] = {
        {2, 28}, // PYRDOWN: 2 buffers, push constants size = 28
        {2, 36}, // CREATE_VMAP: 2 buffers, push constants size = 36
        {2, 16}, // CREATE_NMAP: 2 buffers, push constants size = 16
        {5, 120}, // ESTIMATE: 5 buffers, push constants size = 120 (aligned)
        {2, 4}   // REDUCE: 2 buffers, push constants size = 4
    };

    const uint32_t* shaderCodes[PIPELINE_COUNT] = {
        pyrdown_spv,
        create_vmap_spv,
        create_nmap_spv,
        estimate_spv,
        reduce_spv
    };

    size_t shaderSizes[PIPELINE_COUNT] = {
        sizeof(pyrdown_spv),
        sizeof(create_vmap_spv),
        sizeof(create_nmap_spv),
        sizeof(estimate_spv),
        sizeof(reduce_spv)
    };

    for (int p = 0; p < PIPELINE_COUNT; ++p) {
        // 1. Create Descriptor Set Layout
        std::vector<VkDescriptorSetLayoutBinding> bindings(configs[p].bindingsCount);
        for (uint32_t b = 0; b < configs[p].bindingsCount; ++b) {
            bindings[b].binding = b;
            bindings[b].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            bindings[b].descriptorCount = 1;
            bindings[b].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
            bindings[b].pImmutableSamplers = nullptr;
        }

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();

        if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayouts[p]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create descriptor set layout!");
        }

        // 2. Create Pipeline Layout
        VkPushConstantRange pushRange{};
        pushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        pushRange.offset = 0;
        pushRange.size = configs[p].pushConstantSize;

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &descriptorSetLayouts[p];
        pipelineLayoutInfo.pushConstantRangeCount = configs[p].pushConstantSize > 0 ? 1 : 0;
        pipelineLayoutInfo.pPushConstantRanges = &pushRange;

        if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayouts[p]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create pipeline layout!");
        }

        // 3. Create Compute Pipeline (skip ESTIMATE for static creation since it's dynamic)
        if (p == PIPELINE_ESTIMATE) {
            continue;
        }

        VkShaderModule shaderModule = createShaderModule(device, shaderCodes[p], shaderSizes[p]);

        VkPipelineShaderStageCreateInfo stageInfo{};
        stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        stageInfo.module = shaderModule;
        stageInfo.pName = "main";

        VkComputePipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipelineInfo.stage = stageInfo;
        pipelineInfo.layout = pipelineLayouts[p];

        if (vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipelines[p]) != VK_SUCCESS) {
            vkDestroyShaderModule(device, shaderModule, nullptr);
            throw std::runtime_error("failed to create compute pipeline!");
        }

        vkDestroyShaderModule(device, shaderModule, nullptr);
    }
}

void VulkanContext::destroyPipelines() {
    VkDevice dev = device;
    for (int p = 0; p < PIPELINE_COUNT; ++p) {
        if (pipelines[p] != VK_NULL_HANDLE) {
            vkDestroyPipeline(dev, pipelines[p], nullptr);
            pipelines[p] = VK_NULL_HANDLE;
        }
        if (pipelineLayouts[p] != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(dev, pipelineLayouts[p], nullptr);
            pipelineLayouts[p] = VK_NULL_HANDLE;
        }
        if (descriptorSetLayouts[p] != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(dev, descriptorSetLayouts[p], nullptr);
            descriptorSetLayouts[p] = VK_NULL_HANDLE;
        }
    }

    // Clean up dynamic cached pipelines
    for (auto& entry : estimatePipelineCache) {
        vkDestroyPipeline(dev, entry.pipeline, nullptr);
    }
    estimatePipelineCache.clear();
}

VkPipeline VulkanContext::getOrCreateEstimatePipeline(uint32_t workgroupSize) {
    // 1. Check cache
    for (const auto& entry : estimatePipelineCache) {
        if (entry.workgroupSize == workgroupSize) {
            return entry.pipeline;
        }
    }

    // 2. Create new estimate pipeline using specialization constants
    VkShaderModule shaderModule = createShaderModule(device, estimate_spv, sizeof(estimate_spv));

    VkSpecializationMapEntry entry{};
    entry.constantID = 0;
    entry.offset = 0;
    entry.size = sizeof(uint32_t);

    VkSpecializationInfo specInfo{};
    specInfo.mapEntryCount = 1;
    specInfo.pMapEntries = &entry;
    specInfo.dataSize = sizeof(uint32_t);
    specInfo.pData = &workgroupSize;

    VkPipelineShaderStageCreateInfo stageInfo{};
    stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stageInfo.module = shaderModule;
    stageInfo.pName = "main";
    stageInfo.pSpecializationInfo = &specInfo;

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage = stageInfo;
    pipelineInfo.layout = pipelineLayouts[PIPELINE_ESTIMATE];

    VkPipeline newPipeline;
    if (vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &newPipeline) != VK_SUCCESS) {
        vkDestroyShaderModule(device, shaderModule, nullptr);
        throw std::runtime_error("failed to create specialized estimate pipeline!");
    }

    vkDestroyShaderModule(device, shaderModule, nullptr);

    // Save to cache
    estimatePipelineCache.push_back({workgroupSize, newPipeline});
    return newPipeline;
}
