#include "internal.h"
#include "VulkanContext.h"
#include <vector>
#include <stdexcept>
#include <iostream>
#include <algorithm>

static void updateDescriptorSet(VkDevice device, VkDescriptorSet set, const std::vector<VkDescriptorBufferInfo>& bufferInfos) {
    std::vector<VkWriteDescriptorSet> writes(bufferInfos.size());
    for (size_t i = 0; i < bufferInfos.size(); ++i) {
        writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].pNext = nullptr;
        writes[i].dstSet = set;
        writes[i].dstBinding = static_cast<uint32_t>(i);
        writes[i].dstArrayElement = 0;
        writes[i].descriptorCount = 1;
        writes[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[i].pImageInfo = nullptr;
        writes[i].pBufferInfo = &bufferInfos[i];
        writes[i].pTexelBufferView = nullptr;
    }
    vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

void pyrDown(const DeviceArray2D<unsigned short> &src, DeviceArray2D<unsigned short> &dst) {
    VulkanContext& ctx = VulkanContext::getInstance();
    VkDevice device = ctx.getDevice();

    ctx.resetDescriptorPool();

    VkDescriptorSetLayout layout = ctx.getDescriptorSetLayout(PIPELINE_PYRDOWN);
    VkDescriptorSet descriptorSet = ctx.allocateDescriptorSet(layout);

    VkDescriptorBufferInfo srcInfo{};
    srcInfo.buffer = src.getVulkanBuffer()->getBuffer();
    srcInfo.offset = 0;
    srcInfo.range = src.getVulkanBuffer()->getSize();

    VkDescriptorBufferInfo dstInfo{};
    dstInfo.buffer = dst.getVulkanBuffer()->getBuffer();
    dstInfo.offset = 0;
    dstInfo.range = dst.getVulkanBuffer()->getSize();

    updateDescriptorSet(device, descriptorSet, {srcInfo, dstInfo});

    struct {
        uint32_t src_step;
        uint32_t dst_step;
        uint32_t src_cols;
        uint32_t src_rows;
        uint32_t dst_cols;
        uint32_t dst_rows;
        float sigma_color;
    } pcs;
    pcs.src_step = src.elem_step();
    pcs.dst_step = dst.elem_step();
    pcs.src_cols = src.cols();
    pcs.src_rows = src.rows();
    pcs.dst_cols = dst.cols();
    pcs.dst_rows = dst.rows();
    pcs.sigma_color = 30.f;

    VkCommandBuffer cb = ctx.beginSingleTimeCommands();
    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, ctx.getPipeline(PIPELINE_PYRDOWN));
    vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_COMPUTE, ctx.getPipelineLayout(PIPELINE_PYRDOWN), 0, 1, &descriptorSet, 0, nullptr);
    vkCmdPushConstants(cb, ctx.getPipelineLayout(PIPELINE_PYRDOWN), VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pcs), &pcs);

    uint32_t groupCountX = divUp(dst.cols(), 32);
    uint32_t groupCountY = divUp(dst.rows(), 8);
    vkCmdDispatch(cb, groupCountX, groupCountY, 1);

    ctx.endSingleTimeCommands(cb);
}

void createVMap(const Intr &intr, const DeviceArray2D<unsigned short> &depth, DeviceArray2D<float> &vmap, const float depthCutoff) {
    VulkanContext& ctx = VulkanContext::getInstance();
    VkDevice device = ctx.getDevice();

    ctx.resetDescriptorPool();

    VkDescriptorSetLayout layout = ctx.getDescriptorSetLayout(PIPELINE_CREATE_VMAP);
    VkDescriptorSet descriptorSet = ctx.allocateDescriptorSet(layout);

    VkDescriptorBufferInfo srcInfo{};
    srcInfo.buffer = depth.getVulkanBuffer()->getBuffer();
    srcInfo.offset = 0;
    srcInfo.range = depth.getVulkanBuffer()->getSize();

    VkDescriptorBufferInfo dstInfo{};
    dstInfo.buffer = vmap.getVulkanBuffer()->getBuffer();
    dstInfo.offset = 0;
    dstInfo.range = vmap.getVulkanBuffer()->getSize();

    updateDescriptorSet(device, descriptorSet, {srcInfo, dstInfo});

    struct {
        float fx, fy, cx, cy;
        uint32_t depth_step;
        uint32_t vmap_step;
        uint32_t cols;
        uint32_t rows;
        float depthCutoff;
    } pcs;
    pcs.fx = intr.fx;
    pcs.fy = intr.fy;
    pcs.cx = intr.cx;
    pcs.cy = intr.cy;
    pcs.depth_step = depth.elem_step();
    pcs.vmap_step = vmap.elem_step();
    pcs.cols = depth.cols();
    pcs.rows = depth.rows();
    pcs.depthCutoff = depthCutoff;

    VkCommandBuffer cb = ctx.beginSingleTimeCommands();
    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, ctx.getPipeline(PIPELINE_CREATE_VMAP));
    vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_COMPUTE, ctx.getPipelineLayout(PIPELINE_CREATE_VMAP), 0, 1, &descriptorSet, 0, nullptr);
    vkCmdPushConstants(cb, ctx.getPipelineLayout(PIPELINE_CREATE_VMAP), VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pcs), &pcs);

    uint32_t groupCountX = divUp(depth.cols(), 32);
    uint32_t groupCountY = divUp(depth.rows(), 8);
    vkCmdDispatch(cb, groupCountX, groupCountY, 1);

    ctx.endSingleTimeCommands(cb);
}

void createNMap(const DeviceArray2D<float> &vmap, DeviceArray2D<float> &nmap) {
    VulkanContext& ctx = VulkanContext::getInstance();
    VkDevice device = ctx.getDevice();

    ctx.resetDescriptorPool();

    VkDescriptorSetLayout layout = ctx.getDescriptorSetLayout(PIPELINE_CREATE_NMAP);
    VkDescriptorSet descriptorSet = ctx.allocateDescriptorSet(layout);

    VkDescriptorBufferInfo srcInfo{};
    srcInfo.buffer = vmap.getVulkanBuffer()->getBuffer();
    srcInfo.offset = 0;
    srcInfo.range = vmap.getVulkanBuffer()->getSize();

    VkDescriptorBufferInfo dstInfo{};
    dstInfo.buffer = nmap.getVulkanBuffer()->getBuffer();
    dstInfo.offset = 0;
    dstInfo.range = nmap.getVulkanBuffer()->getSize();

    updateDescriptorSet(device, descriptorSet, {srcInfo, dstInfo});

    struct {
        uint32_t vmap_step;
        uint32_t nmap_step;
        uint32_t cols;
        uint32_t rows;
    } pcs;
    pcs.vmap_step = vmap.elem_step();
    pcs.nmap_step = nmap.elem_step();
    pcs.cols = vmap.cols();
    pcs.rows = vmap.rows() / 3;

    VkCommandBuffer cb = ctx.beginSingleTimeCommands();
    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, ctx.getPipeline(PIPELINE_CREATE_NMAP));
    vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_COMPUTE, ctx.getPipelineLayout(PIPELINE_CREATE_NMAP), 0, 1, &descriptorSet, 0, nullptr);
    vkCmdPushConstants(cb, ctx.getPipelineLayout(PIPELINE_CREATE_NMAP), VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pcs), &pcs);

    uint32_t groupCountX = divUp(pcs.cols, 32);
    uint32_t groupCountY = divUp(pcs.rows, 8);
    vkCmdDispatch(cb, groupCountX, groupCountY, 1);

    ctx.endSingleTimeCommands(cb);
}

struct EstimatePushConstants {
    float R[12];
    float t[4];
    float fx, fy, cx, cy;
    float dist_thresh, angle_thresh;
    uint32_t cols, rows, N;
    uint32_t vmap_curr_step, nmap_curr_step, vmap_prev_step, nmap_prev_step;
};

void estimateStep(
    const Eigen::Matrix<float, 3, 3, Eigen::DontAlign> &R_prev_curr,
    const Eigen::Matrix<float, 3, 1, Eigen::DontAlign> &t_prev_curr,
    const DeviceArray2D<float> &vmap_curr,
    const DeviceArray2D<float> &nmap_curr, const Intr &intr,
    const DeviceArray2D<float> &vmap_prev,
    const DeviceArray2D<float> &nmap_prev, float dist_thresh,
    float angle_thresh,
    DeviceArray<Eigen::Matrix<float, 29, 1, Eigen::DontAlign>> &sum,
    DeviceArray<Eigen::Matrix<float, 29, 1, Eigen::DontAlign>> &out,
    float *matrixA_host, float *vectorB_host, float *residual_inliers,
    int threads, int blocks) {

    VulkanContext& ctx = VulkanContext::getInstance();
    VkDevice device = ctx.getDevice();

    ctx.resetDescriptorPool();

    // Pass 1: Local reduction per workgroup
    VkDescriptorSetLayout layout_est = ctx.getDescriptorSetLayout(PIPELINE_ESTIMATE);
    VkDescriptorSet set_est = ctx.allocateDescriptorSet(layout_est);

    VkDescriptorBufferInfo vmapCurrInfo{ vmap_curr.getVulkanBuffer()->getBuffer(), 0, vmap_curr.getVulkanBuffer()->getSize() };
    VkDescriptorBufferInfo nmapCurrInfo{ nmap_curr.getVulkanBuffer()->getBuffer(), 0, nmap_curr.getVulkanBuffer()->getSize() };
    VkDescriptorBufferInfo vmapPrevInfo{ vmap_prev.getVulkanBuffer()->getBuffer(), 0, vmap_prev.getVulkanBuffer()->getSize() };
    VkDescriptorBufferInfo nmapPrevInfo{ nmap_prev.getVulkanBuffer()->getBuffer(), 0, nmap_prev.getVulkanBuffer()->getSize() };
    VkDescriptorBufferInfo sumInfo{ sum.getVulkanBuffer()->getBuffer(), 0, sum.getVulkanBuffer()->getSize() };

    updateDescriptorSet(device, set_est, {vmapCurrInfo, nmapCurrInfo, vmapPrevInfo, nmapPrevInfo, sumInfo});

    EstimatePushConstants pcs{};
    pcs.R[0] = R_prev_curr(0, 0); pcs.R[1] = R_prev_curr(1, 0); pcs.R[2] = R_prev_curr(2, 0); pcs.R[3] = 0.0f;
    pcs.R[4] = R_prev_curr(0, 1); pcs.R[5] = R_prev_curr(1, 1); pcs.R[6] = R_prev_curr(2, 1); pcs.R[7] = 0.0f;
    pcs.R[8] = R_prev_curr(0, 2); pcs.R[9] = R_prev_curr(1, 2); pcs.R[10] = R_prev_curr(2, 2); pcs.R[11] = 0.0f;
    pcs.t[0] = t_prev_curr(0); pcs.t[1] = t_prev_curr(1); pcs.t[2] = t_prev_curr(2); pcs.t[3] = 0.0f;
    pcs.fx = intr.fx;
    pcs.fy = intr.fy;
    pcs.cx = intr.cx;
    pcs.cy = intr.cy;
    pcs.dist_thresh = dist_thresh;
    pcs.angle_thresh = angle_thresh;
    pcs.cols = vmap_curr.cols();
    pcs.rows = vmap_curr.rows() / 3;
    pcs.N = pcs.cols * pcs.rows;
    pcs.vmap_curr_step = vmap_curr.elem_step();
    pcs.nmap_curr_step = nmap_curr.elem_step();
    pcs.vmap_prev_step = vmap_prev.elem_step();
    pcs.nmap_prev_step = nmap_prev.elem_step();

    // Clamp threads to max 256 since shared memory in estimate.comp supports up to 256 threads
    int active_threads = std::min(threads, 256);

    VkPipeline pipeline_est = ctx.getOrCreateEstimatePipeline(active_threads);

    VkCommandBuffer cb = ctx.beginSingleTimeCommands();
    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_est);
    vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_COMPUTE, ctx.getPipelineLayout(PIPELINE_ESTIMATE), 0, 1, &set_est, 0, nullptr);
    vkCmdPushConstants(cb, ctx.getPipelineLayout(PIPELINE_ESTIMATE), VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pcs), &pcs);
    vkCmdDispatch(cb, blocks, 1, 1);
    ctx.endSingleTimeCommands(cb);

    // Pass 2: Global reduction
    VkDescriptorSetLayout layout_red = ctx.getDescriptorSetLayout(PIPELINE_REDUCE);
    VkDescriptorSet set_red = ctx.allocateDescriptorSet(layout_red);

    VkDescriptorBufferInfo redInInfo{ sum.getVulkanBuffer()->getBuffer(), 0, sum.getVulkanBuffer()->getSize() };
    VkDescriptorBufferInfo redOutInfo{ out.getVulkanBuffer()->getBuffer(), 0, out.getVulkanBuffer()->getSize() };

    updateDescriptorSet(device, set_red, {redInInfo, redOutInfo});

    struct {
        uint32_t num_blocks;
    } pcs_red;
    pcs_red.num_blocks = blocks;

    cb = ctx.beginSingleTimeCommands();
    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, ctx.getPipeline(PIPELINE_REDUCE));
    vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_COMPUTE, ctx.getPipelineLayout(PIPELINE_REDUCE), 0, 1, &set_red, 0, nullptr);
    vkCmdPushConstants(cb, ctx.getPipelineLayout(PIPELINE_REDUCE), VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pcs_red), &pcs_red);
    vkCmdDispatch(cb, 1, 1, 1);
    ctx.endSingleTimeCommands(cb);

    // Download result
    float host_data[29];
    out.download((Eigen::Matrix<float, 29, 1, Eigen::DontAlign> *)&host_data[0]);

    int shift = 0;
    for (int i = 0; i < 6; ++i) // rows
    {
      for (int j = i; j < 7; ++j) // cols + b
      {
        float value = host_data[shift++];
        if (j == 6) // vector b
          vectorB_host[i] = value;
        else
          matrixA_host[j * 6 + i] = matrixA_host[i * 6 + j] = value;
      }
    }

    residual_inliers[0] = host_data[27];
    residual_inliers[1] = host_data[28];
}
