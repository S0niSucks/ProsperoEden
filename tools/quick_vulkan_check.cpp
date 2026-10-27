// SPDX-License-Identifier: GPL-3.0-or-later
// Offscreen evidence for the real HUD shaders/draw include; not a PS5 driver or game test.
#include <vulkan/vulkan.h>
#include "hud.h"
#include <atomic>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <vector>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "tools/launcher/stb/stb_image_write.h"

namespace {
std::atomic<unsigned> validation_errors{};
VKAPI_ATTR VkBool32 VKAPI_CALL Debug(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
        VkDebugUtilsMessageTypeFlagsEXT, const VkDebugUtilsMessengerCallbackDataEXT* data, void*) {
    if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        ++validation_errors; std::cerr << data->pMessage << '\n';
    }
    return VK_FALSE;
}
void Ok(VkResult result) { assert(result == VK_SUCCESS); }
struct Commands {
    VkCommandBuffer command;
    void SetViewport(uint32_t first, const VkViewport& value) { vkCmdSetViewport(command, first, 1, &value); }
    void SetScissor(uint32_t first, const VkRect2D& value) { vkCmdSetScissor(command, first, 1, &value); }
    void BindPipeline(VkPipelineBindPoint point, VkPipeline pipeline) { vkCmdBindPipeline(command, point, pipeline); }
    void BindDescriptorSets(VkPipelineBindPoint point, VkPipelineLayout layout, uint32_t first,
                            VkDescriptorSet set, std::span<const uint32_t>) {
        vkCmdBindDescriptorSets(command, point, layout, first, 1, &set, 0, nullptr);
    }
    void PushConstants(VkPipelineLayout layout, VkShaderStageFlags stage, const Eden::HudSnapshot& value) {
        vkCmdPushConstants(command, layout, stage, 0, sizeof(value), &value);
    }
    void Draw(uint32_t vertices, uint32_t instances, uint32_t first, uint32_t first_instance) {
        vkCmdDraw(command, vertices, instances, first, first_instance);
    }
};
struct Test {
    VkInstance instance{};
    VkDebugUtilsMessengerEXT messenger{};
    VkPhysicalDevice physical{};
    VkDevice device{};
    uint32_t family{};
    VkQueue queue{};
    VkCommandPool command_pool{};
    VkCommandBuffer command{};
    VkDescriptorSetLayout set_layout{};
    VkPipelineLayout pipeline_layout{};
    VkDescriptorPool pool{};
    VkDescriptorSet set{};
    VkSampler sampler{};
    VkRenderPass pass{};
    VkPipeline pipeline{};
    VkImage photo{};
    VkDeviceMemory photo_memory{};
    VkImageView photo_view{};
    VkBuffer upload{};
    VkDeviceMemory upload_memory{};
    bool uploaded{};

    uint32_t Memory(uint32_t mask, VkMemoryPropertyFlags flags) {
        VkPhysicalDeviceMemoryProperties properties{}; vkGetPhysicalDeviceMemoryProperties(physical, &properties);
        for (uint32_t n = 0; n < properties.memoryTypeCount; ++n)
            if ((mask & (1u << n)) && (properties.memoryTypes[n].propertyFlags & flags) == flags) return n;
        std::abort();
    }
    void Image(unsigned width, unsigned height, VkImageUsageFlags usage, VkImage& image,
               VkDeviceMemory& memory, VkImageView& view) {
        VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        info.imageType = VK_IMAGE_TYPE_2D; info.format = VK_FORMAT_R8G8B8A8_UNORM;
        info.extent = {width, height, 1}; info.mipLevels = info.arrayLayers = 1;
        info.samples = VK_SAMPLE_COUNT_1_BIT; info.tiling = VK_IMAGE_TILING_OPTIMAL; info.usage = usage;
        Ok(vkCreateImage(device, &info, nullptr, &image));
        VkMemoryRequirements requirements{}; vkGetImageMemoryRequirements(device, image, &requirements);
        VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        allocation.allocationSize = requirements.size; allocation.memoryTypeIndex = Memory(requirements.memoryTypeBits, 0);
        Ok(vkAllocateMemory(device, &allocation, nullptr, &memory)); Ok(vkBindImageMemory(device, image, memory, 0));
        VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        vi.image = image; vi.viewType = VK_IMAGE_VIEW_TYPE_2D; vi.format = info.format;
        vi.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        Ok(vkCreateImageView(device, &vi, nullptr, &view));
    }
    void Buffer(size_t size, VkBufferUsageFlags usage, VkBuffer& buffer, VkDeviceMemory& memory) {
        VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO}; info.size = size; info.usage = usage;
        Ok(vkCreateBuffer(device, &info, nullptr, &buffer));
        VkMemoryRequirements requirements{}; vkGetBufferMemoryRequirements(device, buffer, &requirements);
        VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO}; allocation.allocationSize = requirements.size;
        allocation.memoryTypeIndex = Memory(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        Ok(vkAllocateMemory(device, &allocation, nullptr, &memory)); Ok(vkBindBufferMemory(device, buffer, memory, 0));
    }
    VkShaderModule Shader(const char* path) {
        std::ifstream file(path, std::ios::binary | std::ios::ate); assert(file);
        const auto size = file.tellg(); assert(size > 0 && size % 4 == 0);
        std::vector<uint32_t> words(static_cast<size_t>(size) / 4); file.seekg(0);
        file.read(reinterpret_cast<char*>(words.data()), size);
        VkShaderModuleCreateInfo info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        info.codeSize = static_cast<size_t>(size); info.pCode = words.data();
        VkShaderModule module{}; Ok(vkCreateShaderModule(device, &info, nullptr, &module)); return module;
    }
    Test(const char* vertex_path, const char* fragment_path) {
        const char* layer = "VK_LAYER_KHRONOS_validation", *extension = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
        VkApplicationInfo application{VK_STRUCTURE_TYPE_APPLICATION_INFO}; application.apiVersion = VK_API_VERSION_1_1;
        VkInstanceCreateInfo info{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO}; info.pApplicationInfo = &application;
        info.enabledLayerCount = 1; info.ppEnabledLayerNames = &layer;
        info.enabledExtensionCount = 1; info.ppEnabledExtensionNames = &extension;
        Ok(vkCreateInstance(&info, nullptr, &instance));
        VkDebugUtilsMessengerCreateInfoEXT debug{VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
        debug.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debug.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debug.pfnUserCallback = Debug;
        Ok(reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"))(instance, &debug, nullptr, &messenger));
        uint32_t count{}; Ok(vkEnumeratePhysicalDevices(instance, &count, nullptr)); assert(count);
        std::vector<VkPhysicalDevice> devices(count); Ok(vkEnumeratePhysicalDevices(instance, &count, devices.data()));
        physical = devices[0]; VkPhysicalDeviceProperties properties{}; vkGetPhysicalDeviceProperties(physical, &properties);
        std::cout << "Vulkan evidence device: " << properties.deviceName << '\n';
        vkGetPhysicalDeviceQueueFamilyProperties(physical, &count, nullptr);
        std::vector<VkQueueFamilyProperties> families(count); vkGetPhysicalDeviceQueueFamilyProperties(physical, &count, families.data());
        while (family < count && !(families[family].queueFlags & VK_QUEUE_GRAPHICS_BIT)) ++family; assert(family < count);
        const float priority = 1;
        VkDeviceQueueCreateInfo qi{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO}; qi.queueFamilyIndex = family; qi.queueCount = 1; qi.pQueuePriorities = &priority;
        VkDeviceCreateInfo di{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO}; di.queueCreateInfoCount = 1; di.pQueueCreateInfos = &qi;
        Ok(vkCreateDevice(physical, &di, nullptr, &device)); vkGetDeviceQueue(device, family, 0, &queue);
        VkCommandPoolCreateInfo pi{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO}; pi.queueFamilyIndex = family; pi.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        Ok(vkCreateCommandPool(device, &pi, nullptr, &command_pool));
        VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO}; ai.commandPool = command_pool; ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; ai.commandBufferCount = 1;
        Ok(vkAllocateCommandBuffers(device, &ai, &command));
        VkDescriptorSetLayoutBinding binding{0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
        VkDescriptorSetLayoutCreateInfo si{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO}; si.bindingCount = 1; si.pBindings = &binding;
        Ok(vkCreateDescriptorSetLayout(device, &si, nullptr, &set_layout));
        const VkPushConstantRange range{VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(Eden::HudSnapshot)};
        VkPipelineLayoutCreateInfo li{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO}; li.setLayoutCount = 1; li.pSetLayouts = &set_layout; li.pushConstantRangeCount = 1; li.pPushConstantRanges = &range;
        Ok(vkCreatePipelineLayout(device, &li, nullptr, &pipeline_layout));
        VkDescriptorPoolSize size{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1};
        VkDescriptorPoolCreateInfo pool_info{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO}; pool_info.maxSets = pool_info.poolSizeCount = 1; pool_info.pPoolSizes = &size;
        Ok(vkCreateDescriptorPool(device, &pool_info, nullptr, &pool));
        VkDescriptorSetAllocateInfo allocation{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO}; allocation.descriptorPool = pool; allocation.descriptorSetCount = 1; allocation.pSetLayouts = &set_layout;
        Ok(vkAllocateDescriptorSets(device, &allocation, &set));
        VkSamplerCreateInfo sampler_info{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        sampler_info.magFilter = sampler_info.minFilter = VK_FILTER_LINEAR;
        sampler_info.addressModeU = sampler_info.addressModeV = sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        Ok(vkCreateSampler(device, &sampler_info, nullptr, &sampler));
        Image(256, 256, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, photo, photo_memory, photo_view);
        Buffer(sizeof(Eden::QuickPicture::rgba), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, upload, upload_memory);
        VkDescriptorImageInfo image{sampler, photo_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}; write.dstSet = set; write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; write.pImageInfo = &image;
        vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
        VkAttachmentDescription attachment{}; attachment.format = VK_FORMAT_R8G8B8A8_UNORM; attachment.samples = VK_SAMPLE_COUNT_1_BIT;
        attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE; attachment.finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        const VkAttachmentReference reference{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        VkSubpassDescription subpass{}; subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; subpass.colorAttachmentCount = 1; subpass.pColorAttachments = &reference;
        VkSubpassDependency dependency{}; dependency.srcSubpass = 0; dependency.dstSubpass = VK_SUBPASS_EXTERNAL;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; dependency.dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT; dependency.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        VkRenderPassCreateInfo ri{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO}; ri.attachmentCount = ri.subpassCount = ri.dependencyCount = 1;
        ri.pAttachments = &attachment; ri.pSubpasses = &subpass; ri.pDependencies = &dependency;
        Ok(vkCreateRenderPass(device, &ri, nullptr, &pass));
        const auto vertex = Shader(vertex_path), fragment = Shader(fragment_path);
        VkPipelineShaderStageCreateInfo stages[2]{};
        for (auto& stage : stages) { stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO; stage.pName = "main"; }
        stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT; stages[0].module = vertex;
        stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT; stages[1].module = fragment;
        VkPipelineVertexInputStateCreateInfo input{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        VkPipelineInputAssemblyStateCreateInfo assembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO}; assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        VkPipelineViewportStateCreateInfo viewport{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO}; viewport.viewportCount = viewport.scissorCount = 1;
        VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO}; raster.lineWidth = 1;
        VkPipelineMultisampleStateCreateInfo samples{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO}; samples.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        VkPipelineColorBlendAttachmentState blend{}; blend.blendEnable = VK_TRUE;
        blend.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA; blend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        blend.colorWriteMask = 15;
        VkPipelineColorBlendStateCreateInfo blends{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO}; blends.attachmentCount = 1; blends.pAttachments = &blend;
        const VkDynamicState states[]{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamic{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO}; dynamic.dynamicStateCount = 2; dynamic.pDynamicStates = states;
        VkGraphicsPipelineCreateInfo graphics{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        graphics.stageCount = 2; graphics.pStages = stages; graphics.pVertexInputState = &input; graphics.pInputAssemblyState = &assembly;
        graphics.pViewportState = &viewport; graphics.pRasterizationState = &raster; graphics.pMultisampleState = &samples;
        graphics.pColorBlendState = &blends; graphics.pDynamicState = &dynamic; graphics.layout = pipeline_layout; graphics.renderPass = pass;
        Ok(vkCreateGraphicsPipelines(device, {}, 1, &graphics, nullptr, &pipeline));
        vkDestroyShaderModule(device, fragment, nullptr); vkDestroyShaderModule(device, vertex, nullptr);
    }
    ~Test() {
        Ok(vkDeviceWaitIdle(device));
        vkDestroyPipeline(device, pipeline, nullptr); vkDestroyRenderPass(device, pass, nullptr);
        vkDestroyDescriptorPool(device, pool, nullptr); vkDestroySampler(device, sampler, nullptr);
        vkDestroyImageView(device, photo_view, nullptr); vkDestroyImage(device, photo, nullptr); vkFreeMemory(device, photo_memory, nullptr);
        vkDestroyBuffer(device, upload, nullptr); vkFreeMemory(device, upload_memory, nullptr);
        vkDestroyPipelineLayout(device, pipeline_layout, nullptr); vkDestroyDescriptorSetLayout(device, set_layout, nullptr);
        vkDestroyCommandPool(device, command_pool, nullptr); vkDestroyDevice(device, nullptr);
        reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"))(instance, messenger, nullptr);
        vkDestroyInstance(instance, nullptr);
    }
    void Draw(unsigned width, unsigned height, unsigned selection, const std::filesystem::path& folder) {
        Eden::QuickPicture image;
        for (unsigned y = 0; y < 256; ++y) for (unsigned x = 64; x < 192; ++x) {
            auto* p = image.rgba.data() + (y * 256 + x) * 4; p[selection == 1 || y >= 128 ? 2 : 0] = 255; p[3] = 255;
        }
        void* mapped{}; Ok(vkMapMemory(device, upload_memory, 0, sizeof(image.rgba), 0, &mapped));
        std::memcpy(mapped, image.rgba.data(), image.rgba.size()); vkUnmapMemory(device, upload_memory);
        VkImage output{}; VkDeviceMemory output_memory{}; VkImageView output_view{};
        Image(width, height, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, output, output_memory, output_view);
        VkBuffer readback{}; VkDeviceMemory readback_memory{}; const size_t bytes = width * height * 4;
        Buffer(bytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT, readback, readback_memory);
        VkFramebufferCreateInfo fi{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO}; fi.renderPass = pass; fi.attachmentCount = 1; fi.pAttachments = &output_view;
        fi.width = width; fi.height = height; fi.layers = 1; VkFramebuffer framebuffer{}; Ok(vkCreateFramebuffer(device, &fi, nullptr, &framebuffer));
        Ok(vkResetCommandBuffer(command, 0)); VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO}; Ok(vkBeginCommandBuffer(command, &bi));
        VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER}; barrier.image = photo;
        barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}; barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.oldLayout = uploaded ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED; barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcAccessMask = uploaded ? VK_ACCESS_SHADER_READ_BIT : 0; barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        vkCmdPipelineBarrier(command, uploaded ? VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
        VkBufferImageCopy copy{}; copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1}; copy.imageExtent = {256, 256, 1};
        vkCmdCopyBufferToImage(command, upload, photo, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL; barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier); uploaded = true;
        const VkExtent2D render_area{width, height};
        const VkClearValue clear{{{0.08f, 0.12f, 0.18f, 1}}};
        VkRenderPassBeginInfo begin{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO}; begin.renderPass = pass; begin.framebuffer = framebuffer;
        begin.renderArea = {{0, 0}, render_area}; begin.clearValueCount = 1; begin.pClearValues = &clear;
        vkCmdBeginRenderPass(command, &begin, VK_SUBPASS_CONTENTS_INLINE);
        Eden::QuickOverlay::Show("AMIIBO 1/2 Mario - Smash - L/R - CROSS Scan - SQUARE Remove - CIRCLE Close - UP/DOWN Cheats");
        const auto text = Eden::QuickOverlay::Text();
        const auto hud = Eden::MakeQuickHudSnapshot(text.data());
        const auto hud_graphics_pipeline = pipeline;
        const auto hud_pipeline_layout = pipeline_layout;
        const auto hud_descriptor_set = set;
        const auto hud_picture_set = selection < 2 ? set : VkDescriptorSet{};
        Commands cmdbuf{command};
#include "vulkan_hud_draw.inc"
        vkCmdEndRenderPass(command);
        copy.imageExtent = {width, height, 1};
        vkCmdCopyImageToBuffer(command, output, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, readback, 1, &copy);
        VkMemoryBarrier host{VK_STRUCTURE_TYPE_MEMORY_BARRIER}; host.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; host.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
        vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &host, 0, nullptr, 0, nullptr);
        Ok(vkEndCommandBuffer(command)); VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO}; submit.commandBufferCount = 1; submit.pCommandBuffers = &command;
        Ok(vkQueueSubmit(queue, 1, &submit, {})); Ok(vkQueueWaitIdle(queue));
        Ok(vkMapMemory(device, readback_memory, 0, bytes, 0, &mapped));
        const auto* pixels = static_cast<const unsigned char*>(mapped);
        const auto units = [height](unsigned n) { return n * height / 1080u; };
        const auto box = static_cast<unsigned>(Eden::kHudBoxUnits) * ((hud.loading & Eden::kHudSizeMask) >> Eden::kHudSizeShift) / 2u;
        const unsigned top = units(hud.y + box + Eden::QuickPicture::kGap);
        const auto sample = [&](unsigned y) { return pixels + ((top + units(y)) * width + units(hud.x + Eden::QuickPicture::kBox / 2)) * 4; };
        const auto* upper = sample(40), *lower = sample(184);
        if (selection == 0) assert(upper[0] > 240 && upper[2] < 10 && lower[2] > 240 && lower[0] < 10);
        if (selection == 1) assert(upper[2] > 240 && upper[0] < 10 && lower[2] > 240);
        if (selection == 2) assert(upper[0] < 25 && upper[1] < 35 && upper[2] < 50);
        unsigned ink{}; for (size_t n = 0; n < bytes; n += 4) ink += pixels[n] > 150 && pixels[n + 1] > 150 && pixels[n + 2] > 150;
        assert(ink > 100);
        const auto path = folder / (std::to_string(width) + "-vulkan-" + std::to_string(selection) + ".png");
        assert(stbi_write_png(path.c_str(), width, height, 4, pixels, width * 4));
        vkUnmapMemory(device, readback_memory);
        vkDestroyFramebuffer(device, framebuffer, nullptr); vkDestroyImageView(device, output_view, nullptr);
        vkDestroyImage(device, output, nullptr); vkFreeMemory(device, output_memory, nullptr);
        vkDestroyBuffer(device, readback, nullptr); vkFreeMemory(device, readback_memory, nullptr);
    }
};
} // namespace
int main(int argc, char** argv) {
    assert(argc == 4); std::filesystem::create_directories(argv[3]);
    {
        Test test(argv[1], argv[2]);
        for (const auto width : {1280u, 1920u}) for (unsigned n = 0; n < 3; ++n) test.Draw(width, width * 9 / 16, n, argv[3]);
    }
    assert(validation_errors == 0);
    std::puts("Quick Vulkan PASS: actual draw include and SPIR-V on Mesa, photo orientation/alpha, replacement, missing-photo fallback and glyphs at 720p/1080p; zero validation errors");
}
