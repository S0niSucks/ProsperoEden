    Eden::QuickOverlay::Snapshot hud_commands;
    const auto hud = hud_memory ? Eden::GetVulkanHud(&hud_commands) : Eden::HudSnapshot{};
    if (hud.width && !hud_pipeline) {
        // The loading screen's picture (loading_art.h): the game's cover and its blurred copy, or
        // one clear texel for a game without a cover. The overlay shares the pipeline and does
        // not read it.
        const Eden::Loading::Art art = Eden::Loading::GetArt();
        const VkExtent2D art_extent{static_cast<u32>(art.width), static_cast<u32>(art.height)};
        hud_art = CreateWrappedImage(*hud_memory, art_extent, VK_FORMAT_R8G8B8A8_UNORM);
        UploadImage(device, *hud_memory, scheduler, hud_art, art_extent, VK_FORMAT_R8G8B8A8_UNORM, art.rgba);
        hud_art_view = CreateWrappedImageView(device, hud_art, VK_FORMAT_R8G8B8A8_UNORM);
        hud_sampler = CreateBilinearSampler(device);
        hud_set_layout = CreateWrappedDescriptorSetLayout(
            device, {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER}, VK_SHADER_STAGE_FRAGMENT_BIT);
        hud_pool = CreateWrappedDescriptorPool(device, 2, 2);
        const VkDescriptorSetLayout hud_set_layout_handle = *hud_set_layout;
        hud_sets = CreateWrappedDescriptorSets(hud_pool, std::array{hud_set_layout_handle, hud_set_layout_handle});
        std::vector<VkDescriptorImageInfo> art_info;
        art_info.reserve(1);
        device.GetLogical().UpdateDescriptorSets(
            std::array{CreateWriteDescriptorSet(art_info, *hud_sampler, *hud_art_view, hud_sets[0], 0)}, {});
        const VkPushConstantRange range{VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(hud)};
        hud_layout = device.GetLogical().CreatePipelineLayout(VkPipelineLayoutCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = 1,
            .pSetLayouts = hud_set_layout.address(),
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = &range,
        });
        auto vertex = BuildShader(device, EDEN_HUD_VERT_SPV);
        auto fragment = BuildShader(device, EDEN_HUD_FRAG_SPV);
        hud_pipeline = CreateWrappedCoverageBlendingPipeline(
            device, render_pass, hud_layout, std::tie(vertex, fragment));
    }
    if (hud.width && hud_commands.image && hud_quick_picture != hud_commands.image) {
        // Selection changes are rare: finish prior commands before replacing an image/view or
        // changing a descriptor referenced by an in-flight frame. No waits on ordinary frames.
        scheduler.Finish();
        hud_quick_view = {};
        hud_quick_art = {};
        const VkExtent2D extent{Eden::QuickPicture::kSide, Eden::QuickPicture::kSide};
        hud_quick_art = CreateWrappedImage(*hud_memory, extent, VK_FORMAT_R8G8B8A8_UNORM);
        UploadImage(device, *hud_memory, scheduler, hud_quick_art, extent, VK_FORMAT_R8G8B8A8_UNORM,
                    hud_commands.image->rgba);
        hud_quick_view = CreateWrappedImageView(device, hud_quick_art, VK_FORMAT_R8G8B8A8_UNORM);
        std::vector<VkDescriptorImageInfo> image_info;
        image_info.reserve(1);
        device.GetLogical().UpdateDescriptorSets(
            std::array{CreateWriteDescriptorSet(image_info, *hud_sampler, *hud_quick_view, hud_sets[1], 0)}, {});
        hud_quick_picture = hud_commands.image;
    }
    const VkPipeline hud_graphics_pipeline = *hud_pipeline;
    const VkPipelineLayout hud_pipeline_layout = *hud_layout;
    const VkDescriptorSet hud_descriptor_set = hud_pipeline ? hud_sets[0] : VkDescriptorSet{};
    const VkDescriptorSet hud_picture_set = hud_commands.image ? hud_sets[1] : VkDescriptorSet{};
