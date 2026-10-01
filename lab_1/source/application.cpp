#include "application.hpp"
#include "math.hpp"
#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <imgui.h>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace application {
namespace {
using namespace math;
struct Buffer {
    VkBuffer handle{};
    VmaAllocation allocation{};
    void *mapped{};
};
Buffer vertexBuffer, indexBuffer, edgeBuffer, uniformBuffer;
VkDescriptorPool descriptorPool{};
VkDescriptorSetLayout descriptorLayout{};
VkDescriptorSet descriptorSet{};
VkPipelineLayout pipelineLayout{};
VkPipeline solidPipeline{}, edgePipeline{};
struct alignas(16) Uniform {
    Mat4 model, view, projection;
    std::array<float, 4> tint;
};
static_assert(sizeof(Uniform) == 208);
Vec3 position{}, angles{12, -18, 0}, stretch{1, 1, 1};
std::array<float, 3> tint{1, 1, 1};
Animation animation;
bool animationEnabled = false, drawEdges = true, procedural = true;
int projection = 0;
float cameraYaw = 30, cameraPitch = 20, cameraDistance = 8.5f, fov = 45, orthoHeight = 3.3f;
float panelWidth = 340;
double lastTime = -1;
void check(VkResult r, const char *where) {
    if (r != VK_SUCCESS)
        throw std::runtime_error(std::string(where) + ": " + std::to_string(r));
}
Buffer makeBuffer(VkDeviceSize bytes, VkBufferUsageFlags usage, const void *data = nullptr) {
    Buffer b;
    VkBufferCreateInfo info{.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                            .size = bytes,
                            .usage = usage,
                            .sharingMode = VK_SHARING_MODE_EXCLUSIVE};
    VmaAllocationCreateInfo alloc{};
    alloc.usage = VMA_MEMORY_USAGE_AUTO;
    alloc.flags =
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
    VmaAllocationInfo ai{};
    check(vmaCreateBuffer(graphics::internal::context.allocator, &info, &alloc, &b.handle,
                          &b.allocation, &ai),
          "create buffer");
    b.mapped = ai.pMappedData;
    if (data) {
        std::memcpy(b.mapped, data, bytes);
        vmaFlushAllocation(graphics::internal::context.allocator, b.allocation, 0, bytes);
    }
    return b;
}
VkShaderModule shader(const char *name) {
    std::filesystem::path path = std::filesystem::path(LAB_SHADER_DIR) / name;
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input)
        throw std::runtime_error("Cannot open shader: " + path.string());
    auto size = input.tellg();
    if (size <= 0 || size % 4)
        throw std::runtime_error("Invalid SPIR-V");
    std::vector<uint32_t> code(static_cast<size_t>(size) / 4);
    input.seekg(0);
    input.read(reinterpret_cast<char *>(code.data()), size);
    VkShaderModuleCreateInfo info{.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                                  .codeSize = static_cast<size_t>(size),
                                  .pCode = code.data()};
    VkShaderModule module{};
    check(vkCreateShaderModule(graphics::internal::context.device, &info, nullptr, &module),
          "shader");
    return module;
}
VkPipeline makePipeline(VkShaderModule vert, VkShaderModule frag, bool edgesOnly) {
    const VkPipelineShaderStageCreateInfo stages[] = {
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
         .stage = VK_SHADER_STAGE_VERTEX_BIT,
         .module = vert,
         .pName = "main"},
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
         .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
         .module = frag,
         .pName = "main"}};
    VkVertexInputBindingDescription binding{0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX};
    const VkVertexInputAttributeDescription attributes[] = {
        {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, position)},
        {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, color)}};
    VkPipelineVertexInputStateCreateInfo vi{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &binding,
        .vertexAttributeDescriptionCount = 2,
        .pVertexAttributeDescriptions = attributes};
    VkPipelineInputAssemblyStateCreateInfo ia{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology =
            edgesOnly ? VK_PRIMITIVE_TOPOLOGY_LINE_LIST : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};
    VkPipelineViewportStateCreateInfo viewport{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1};
    VkPipelineRasterizationStateCreateInfo raster{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .depthBiasEnable = edgesOnly ? VK_FALSE : VK_TRUE,
        .depthBiasConstantFactor = 1.f,
        .depthBiasSlopeFactor = 1.f,
        .lineWidth = 1.f};
    VkPipelineMultisampleStateCreateInfo ms{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT};
    VkPipelineDepthStencilStateCreateInfo depth{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = VK_TRUE,
        .depthWriteEnable = edgesOnly ? VK_FALSE : VK_TRUE,
        .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL};
    VkPipelineColorBlendAttachmentState attachment{
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT};
    VkPipelineColorBlendStateCreateInfo blend{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &attachment};
    VkDynamicState dynamics[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dyn{.sType =
                                             VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
                                         .dynamicStateCount = 2,
                                         .pDynamicStates = dynamics};
    VkGraphicsPipelineCreateInfo info{.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                                      .stageCount = 2,
                                      .pStages = stages,
                                      .pVertexInputState = &vi,
                                      .pInputAssemblyState = &ia,
                                      .pViewportState = &viewport,
                                      .pRasterizationState = &raster,
                                      .pMultisampleState = &ms,
                                      .pDepthStencilState = &depth,
                                      .pColorBlendState = &blend,
                                      .pDynamicState = &dyn,
                                      .layout = pipelineLayout,
                                      .renderPass = graphics::internal::context.render_pass};
    VkPipeline p{};
    check(vkCreateGraphicsPipelines(graphics::internal::context.device, VK_NULL_HANDLE, 1, &info,
                                    nullptr, &p),
          "pipeline");
    return p;
}
void reset() {
    position = {};
    angles = {12, -18, 0};
    stretch = {1, 1, 1};
    tint = {1, 1, 1};
    animation = {};
    animationEnabled = false;
    projection = 0;
    cameraYaw = 30;
    cameraPitch = 20;
    cameraDistance = 8.5f;
    fov = 45;
    orthoHeight = 3.3f;
    procedural = true;
    drawEdges = true;
}
} // namespace
bool initialize() {
    try {
        auto &ctx = graphics::internal::context;
        auto vertices = parallelepiped();
        vertexBuffer =
            makeBuffer(sizeof(vertices), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, vertices.data());
        indexBuffer =
            makeBuffer(sizeof(triangles), VK_BUFFER_USAGE_INDEX_BUFFER_BIT, triangles.data());
        edgeBuffer = makeBuffer(sizeof(edges), VK_BUFFER_USAGE_INDEX_BUFFER_BIT, edges.data());
        uniformBuffer = makeBuffer(sizeof(Uniform), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
        VkDescriptorSetLayoutBinding binding{.binding = 0,
                                             .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                             .descriptorCount = 1,
                                             .stageFlags = VK_SHADER_STAGE_VERTEX_BIT};
        VkDescriptorSetLayoutCreateInfo li{.sType =
                                               VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                                           .bindingCount = 1,
                                           .pBindings = &binding};
        check(vkCreateDescriptorSetLayout(ctx.device, &li, nullptr, &descriptorLayout),
              "descriptor layout");
        VkDescriptorPoolSize size{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1};
        VkDescriptorPoolCreateInfo pool{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                                        .maxSets = 1,
                                        .poolSizeCount = 1,
                                        .pPoolSizes = &size};
        check(vkCreateDescriptorPool(ctx.device, &pool, nullptr, &descriptorPool),
              "descriptor pool");
        VkDescriptorSetAllocateInfo da{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                       .descriptorPool = descriptorPool,
                                       .descriptorSetCount = 1,
                                       .pSetLayouts = &descriptorLayout};
        check(vkAllocateDescriptorSets(ctx.device, &da, &descriptorSet), "descriptor set");
        VkDescriptorBufferInfo bi{uniformBuffer.handle, 0, sizeof(Uniform)};
        VkWriteDescriptorSet write{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                   .dstSet = descriptorSet,
                                   .dstBinding = 0,
                                   .descriptorCount = 1,
                                   .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                   .pBufferInfo = &bi};
        vkUpdateDescriptorSets(ctx.device, 1, &write, 0, nullptr);
        VkPushConstantRange pc{VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(int)};
        VkPipelineLayoutCreateInfo pl{.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                      .setLayoutCount = 1,
                                      .pSetLayouts = &descriptorLayout,
                                      .pushConstantRangeCount = 1,
                                      .pPushConstantRanges = &pc};
        check(vkCreatePipelineLayout(ctx.device, &pl, nullptr, &pipelineLayout), "pipeline layout");
        auto vert = shader("figure.vert.spv"), frag = shader("figure.frag.spv");
        try {
            solidPipeline = makePipeline(vert, frag, false);
            edgePipeline = makePipeline(vert, frag, true);
        } catch (...) {
            vkDestroyShaderModule(ctx.device, vert, nullptr);
            vkDestroyShaderModule(ctx.device, frag, nullptr);
            throw;
        }
        vkDestroyShaderModule(ctx.device, vert, nullptr);
        vkDestroyShaderModule(ctx.device, frag, nullptr);
        ImGui::GetIO().Fonts->AddFontDefaultVector();
        ImGui::StyleColorsDark();
        auto &s = ImGui::GetStyle();
        s.WindowRounding = 0;
        s.FrameRounding = 5;
        s.GrabRounding = 5;
        s.WindowPadding = {18, 18};
        s.ItemSpacing = {8, 7};
        s.FramePadding = {7, 5};
        s.Colors[ImGuiCol_WindowBg] = {.065f, .086f, .125f, 1};
        s.Colors[ImGuiCol_Header] = {.13f, .22f, .32f, 1};
        s.Colors[ImGuiCol_Button] = {.14f, .3f, .43f, 1};
        s.Colors[ImGuiCol_FrameBg] = {.10f, .15f, .21f, 1};
        s.Colors[ImGuiCol_CheckMark] = {.28f, .84f, .80f, 1};
        s.Colors[ImGuiCol_SliderGrab] = {.28f, .84f, .80f, 1};
        ImGui::GetIO().IniFilename = nullptr;
        return true;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        shutdown();
        return false;
    }
}
void shutdown() {
    auto &c = graphics::internal::context;
    vkDeviceWaitIdle(c.device);
    vkDestroyPipeline(c.device, solidPipeline, nullptr);
    vkDestroyPipeline(c.device, edgePipeline, nullptr);
    vkDestroyPipelineLayout(c.device, pipelineLayout, nullptr);
    vkDestroyDescriptorPool(c.device, descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(c.device, descriptorLayout, nullptr);
    for (auto *b : {&vertexBuffer, &indexBuffer, &edgeBuffer, &uniformBuffer})
        if (b->handle)
            vmaDestroyBuffer(c.allocator, b->handle, b->allocation);
}
void playAnimation() {
    animationEnabled = true;
    animation.playing = true;
}
void preset(int i) {
    reset();
    if (i == 1) {
        projection = 1;
        position = {.25f, .15f, 0};
        angles = {20, 40, 15};
        stretch = {1.2f, .7f, 1.15f};
        tint = {.55f, .9f, 1};
    }
    if (i == 2) {
        animationEnabled = true;
        animation.phase = 1.2;
        animation.radius = 1.6f;
        animation.height = .9f;
        animation.speed = .8f;
        angles = {5, 15, 0};
        tint = {1, .65f, .8f};
        cameraDistance = 11;
    }
}
void update(double time) {
    double dt = lastTime < 0 ? 0 : time - lastTime;
    lastTime = time;
    auto &io = ImGui::GetIO();
    panelWidth = std::min(340.f, io.DisplaySize.x * .45f);
    if (!io.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Space)) {
        animationEnabled = true;
        animation.playing = !animation.playing;
    }
    if (!io.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_R))
        reset();
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize({panelWidth, io.DisplaySize.y});
    ImGui::Begin("Controls", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoSavedSettings);
    ImGui::TextColored({.3f, .86f, .82f, 1}, "COMPUTER GRAPHICS / LAB 01");
    ImGui::SetWindowFontScale(1.4f);
    ImGui::TextUnformatted("Parallelepiped");
    ImGui::SetWindowFontScale(1);
    ImGui::TextDisabled("Variant 03  |  D. D. Yakovleva");
    ImGui::TextDisabled("M8O-315BV-24  /  tasks 1-5");
    ImGui::Spacing();
    ImGui::PushItemWidth(-1);
    if (ImGui::CollapsingHeader("01  Projection & camera", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::RadioButton("Perspective", &projection, 0);
        ImGui::SameLine();
        ImGui::RadioButton("Orthographic", &projection, 1);
        if (projection == 0) {
            ImGui::TextUnformatted("Field of view");
            ImGui::SliderFloat("##fov", &fov, 20, 80, "%.0f deg");
        } else {
            ImGui::TextUnformatted("View half-height");
            ImGui::SliderFloat("##ortho", &orthoHeight, 1.5f, 8, "%.2f");
        }
        if (ImGui::TreeNode("Camera")) {
            ImGui::SliderFloat("Yaw", &cameraYaw, -180, 180);
            ImGui::SliderFloat("Pitch", &cameraPitch, -80, 80);
            ImGui::SliderFloat("Distance", &cameraDistance, 4, 20);
            ImGui::TreePop();
        }
    }
    if (ImGui::CollapsingHeader("02  Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::TextUnformatted("Position  X / Y / Z");
        ImGui::DragFloat3("##position", &position.x, .02f, -5, 5, "%.2f",
                          ImGuiSliderFlags_AlwaysClamp);
        ImGui::TextUnformatted("Rotation  X / Y / Z (degrees)");
        ImGui::DragFloat3("##rotation", &angles.x, .5f, -180, 180, "%.1f",
                          ImGuiSliderFlags_AlwaysClamp);
        ImGui::TextUnformatted("Scale  X / Y / Z");
        ImGui::DragFloat3("##scale", &stretch.x, .01f, .1f, 3, "%.2f",
                          ImGuiSliderFlags_AlwaysClamp);
    }
    if (ImGui::CollapsingHeader("03  Trajectory & animation", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Enable trajectory", &animationEnabled);
        if (ImGui::Button(animation.playing ? "Pause" : "Play", {90, 0})) {
            animationEnabled = true;
            animation.playing = !animation.playing;
        }
        ImGui::SameLine();
        if (ImGui::Button("Rewind"))
            animation.phase = 0;
        ImGui::SameLine();
        ImGui::TextDisabled("t = %.2f", animation.phase);
        ImGui::TextUnformatted("Speed");
        ImGui::SliderFloat("##speed", &animation.speed, 0, 3, "%.2fx");
        if (ImGui::TreeNode("Trajectory parameters")) {
            ImGui::SliderFloat("Radius", &animation.radius, 0, 3, "%.2f");
            ImGui::SliderFloat("Height", &animation.height, 0, 2, "%.2f");
            ImGui::SliderFloat("Frequency", &animation.frequency, 1, 5, "%.2f");
            ImGui::TextWrapped("x = a cos(t), y = b sin(k t), z = 0.6 a sin(t)");
            ImGui::TreePop();
        }
    }
    if (ImGui::CollapsingHeader("04-05  Color", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::ColorEdit3("##tint", tint.data(), ImGuiColorEditFlags_NoInputs);
        ImGui::SameLine();
        ImGui::TextUnformatted("Color multiplier");
        ImGui::Checkbox("Procedural vertex colors", &procedural);
        ImGui::Checkbox("Show edges", &drawEdges);
    }
    if (ImGui::Button("Reset all", {-1, 0}))
        reset();
    ImGui::TextDisabled("Drag scene: orbit | Wheel: zoom");
    ImGui::TextDisabled("Space: play/pause | R: reset");
    ImGui::PopItemWidth();
    ImGui::End();
    if (animationEnabled)
        animation.advance(dt);
    if (io.MousePos.x > panelWidth && !io.WantCaptureMouse) {
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            cameraYaw -= io.MouseDelta.x * .35f;
            cameraPitch = std::clamp(cameraPitch + io.MouseDelta.y * .35f, -80.f, 80.f);
        }
        if (projection == 0)
            cameraDistance = std::clamp(cameraDistance - io.MouseWheel * .5f, 4.f, 20.f);
        else
            orthoHeight = std::clamp(orthoHeight - io.MouseWheel * .2f, 1.5f, 8.f);
    }
    auto *overlay = ImGui::GetBackgroundDrawList();
    overlay->AddText({panelWidth + 30, 25}, IM_COL32(150, 178, 205, 255),
                     projection == 0 ? "PERSPECTIVE / 3D VIEW" : "ORTHOGRAPHIC / 3D VIEW");
    overlay->AddText({panelWidth + 30, 48}, IM_COL32(90, 119, 147, 255),
                     "8 vertices   /   12 triangles   /   12 edges");
    overlay->AddText({panelWidth + 30, io.DisplaySize.y - 35}, IM_COL32(110, 145, 172, 255),
                     animationEnabled
                         ? (animation.playing ? "TRAJECTORY  /  PLAYING" : "TRAJECTORY  /  PAUSED")
                         : "MANUAL TRANSFORM");
}
void render(const graphics::internal::FrameData &fd) {
    auto &c = graphics::internal::context;
    Vec3 p = position, r = angles;
    if (animationEnabled) {
        p = p + animation.offset();
        r = r + animation.angles();
    }
    float yaw = radians(cameraYaw), pitch = radians(cameraPitch);
    Vec3 eye = {cameraDistance * std::cos(pitch) * std::sin(yaw), cameraDistance * std::sin(pitch),
                cameraDistance * std::cos(pitch) * std::cos(yaw)};
    auto &io = ImGui::GetIO();
    float left = panelWidth * io.DisplayFramebufferScale.x;
    float width = std::max(1.f, float(c.swapchain_extent.width) - left),
          height = float(c.swapchain_extent.height);
    Uniform u{model(p, r, stretch),
              lookAt(eye, {0, 0, 0}),
              projection == 0 ? perspective(radians(fov), width / height, .1f, 100)
                              : ortho(orthoHeight, width / height, .1f, 100),
              {tint[0], tint[1], tint[2], procedural ? 1.f : 0.f}};
    // main calls prepare() before update/render: the previous GPU use has completed.
    std::memcpy(uniformBuffer.mapped, &u, sizeof(u));
    vmaFlushAllocation(c.allocator, uniformBuffer.allocation, 0, sizeof(u));
    check(vkResetCommandBuffer(fd.command_buffer, 0), "reset command buffer");
    VkCommandBufferBeginInfo begin{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                   .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
    check(vkBeginCommandBuffer(fd.command_buffer, &begin), "begin commands");
    VkClearValue clear[2]{};
    clear[0].color = {{.025f, .040f, .067f, 1}};
    clear[1].depthStencil = {1, 0};
    VkRenderPassBeginInfo pass{.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                               .renderPass = c.render_pass,
                               .framebuffer = fd.framebuffer,
                               .renderArea = {.extent = c.swapchain_extent},
                               .clearValueCount = 2,
                               .pClearValues = clear};
    vkCmdBeginRenderPass(fd.command_buffer, &pass, VK_SUBPASS_CONTENTS_INLINE);
    VkViewport viewport{left, 0, width, height, 0, 1};
    VkRect2D scissor{{int32_t(left), 0}, {uint32_t(width), c.swapchain_extent.height}};
    vkCmdSetViewport(fd.command_buffer, 0, 1, &viewport);
    vkCmdSetScissor(fd.command_buffer, 0, 1, &scissor);
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(fd.command_buffer, 0, 1, &vertexBuffer.handle, &offset);
    vkCmdBindDescriptorSets(fd.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0,
                            1, &descriptorSet, 0, nullptr);
    int mode = 0;
    vkCmdPushConstants(fd.command_buffer, pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                       sizeof(mode), &mode);
    vkCmdBindPipeline(fd.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, solidPipeline);
    vkCmdBindIndexBuffer(fd.command_buffer, indexBuffer.handle, 0, VK_INDEX_TYPE_UINT16);
    vkCmdDrawIndexed(fd.command_buffer, uint32_t(triangles.size()), 1, 0, 0, 0);
    if (drawEdges) {
        mode = 1;
        vkCmdPushConstants(fd.command_buffer, pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                           sizeof(mode), &mode);
        vkCmdBindPipeline(fd.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, edgePipeline);
        vkCmdBindIndexBuffer(fd.command_buffer, edgeBuffer.handle, 0, VK_INDEX_TYPE_UINT16);
        vkCmdDrawIndexed(fd.command_buffer, uint32_t(edges.size()), 1, 0, 0, 0);
    }
    vkCmdEndRenderPass(fd.command_buffer);
    check(vkEndCommandBuffer(fd.command_buffer), "end commands");
}
} // namespace application
