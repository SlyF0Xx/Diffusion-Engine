// Engine.cpp : Defines the exported functions for the DLL.
//

#include "framework.h"
#include "Engine.h"

#define VK_USE_PLATFORM_WIN32_KHR

#include <vulkan/vulkan.hpp>

#include <array>
#include <fstream>
#include <iostream>
#include <memory>
#include <vector>

struct PrimitiveVertex
{
    float x, y, z;
};

class IGameComponent
{
public:
    virtual vk::Semaphore Draw(int swapchain_image_index, vk::Semaphore wait_sema) = 0;
    virtual void Initialize(const std::vector<vk::Image>&) = 0;
    virtual void DestroyResources() = 0;
};


class TriangleComponent : public IGameComponent
{
private:
    struct PerSwapchainImageInfo
    {
        vk::Image m_depth_image;
        vk::ImageView m_color_image_view;
        vk::ImageView m_depth_image_view;
        vk::Image m_color_image;

        vk::Framebuffer m_framebuffer;
        vk::DeviceMemory m_depth_memory; // maybe remove?
        vk::CommandBuffer m_command_buffer;
        
        vk::Fence m_fence;
        vk::Semaphore m_sema;
    };

    vk::RenderPass m_render_pass;

    vk::PipelineLayout m_layout;
    vk::ShaderModule m_vertex_shader;
    vk::ShaderModule m_fragment_shader;
    vk::PipelineCache m_cache;
    vk::Pipeline m_pipeline;

    // std::vector<vk::DescriptorSet> m_descriptor_sets;
    std::vector<PrimitiveVertex> m_verticies;
    vk::Buffer m_vertex_buffer;
    vk::DeviceMemory m_vertex_memory;

    std::vector<vk::CommandBuffer> m_command_buffers;
    std::vector<PerSwapchainImageInfo> m_infos;

    GameImpl& m_game;
    vk::Format m_color_format;
    vk::Format m_depth_format;
    int m_width;
    int m_height;
    const vk::PhysicalDeviceMemoryProperties& m_memory_props;

public:
    TriangleComponent(GameImpl& game,
                      vk::Format color_format,
                      vk::Format depth_format,
                      int width,
                      int height,
                      const vk::PhysicalDeviceMemoryProperties& memory_props);

    vk::Semaphore Draw(int swapchain_image_index, vk::Semaphore wait_sema) override;
    void Initialize(const std::vector<vk::Image> &) override;
    void DestroyResources() override;
};

struct GameImpl
{
    int m_width;
    int m_height;

    vk::Instance m_instance;
    vk::Device m_device;
    vk::Queue m_queue;
    vk::CommandPool m_command_pool;
    vk::SurfaceKHR m_surface;
    vk::SwapchainKHR m_swapchain;
    std::vector<std::unique_ptr<IGameComponent>> m_game_components;

    std::vector<vk::Image> m_images;

    vk::Semaphore m_sema;

    vk::ShaderModule  loadSPIRVShader(std::string filename)
    {
        size_t shaderSize;
        char* shaderCode = nullptr;

        std::ifstream is(filename, std::ios::binary | std::ios::in | std::ios::ate);

        if (is.is_open())
        {
            shaderSize = is.tellg();
            is.seekg(0, std::ios::beg);
            // Copy file contents into a buffer
            shaderCode = new char[shaderSize];
            is.read(shaderCode, shaderSize);
            is.close();
            assert(shaderSize > 0);
        }
        if (shaderCode)
        {
            auto shader = m_device.createShaderModule(vk::ShaderModuleCreateInfo({}, shaderSize, reinterpret_cast<const uint32_t*>(shaderCode)));

            delete[] shaderCode;

            return shader;
        }
        else
        {
            throw std::logic_error("Error: Could not open shader file \"" + filename + "\"");
        }
    }
    uint32_t find_appropriate_memory_type(vk::MemoryRequirements& mem_req, const vk::PhysicalDeviceMemoryProperties& memory_props, vk::MemoryPropertyFlags memory_flags);

    void AddGameComponent(std::unique_ptr<IGameComponent> component);
};



// This is the constructor of a class that has been exported.
Game::Game()
{
    m_game_impl = new GameImpl();
}

Game::~Game()
{
    delete m_game_impl;
}

void Game::Initialize(HINSTANCE hinstance, HWND hwnd, int width, int height)
{
    m_game_impl->m_width = width;
    m_game_impl->m_height = height;

    vk::ApplicationInfo application_info("Lab1", 1, "Engine", 1, VK_API_VERSION_1_2);

    std::array layers = {
        "VK_LAYER_KHRONOS_validation",
        "VK_LAYER_LUNARG_api_dump",
        "VK_LAYER_LUNARG_monitor",

        // "VK_LAYER_RENDERDOC_Capture",
#ifdef VK_TRACE
        "VK_LAYER_LUNARG_vktrace",
#endif
    };

    std::array extensions = {
        "VK_EXT_debug_report",
        "VK_EXT_debug_utils",
        "VK_KHR_external_memory_capabilities",
        "VK_NV_external_memory_capabilities",
        "VK_EXT_swapchain_colorspace",
        "VK_KHR_surface",
        "VK_KHR_win32_surface",

        "VK_KHR_get_physical_device_properties2"
    };

    m_game_impl->m_instance = vk::createInstance(vk::InstanceCreateInfo({}, &application_info, layers, extensions));

    auto devices = m_game_impl->m_instance.enumeratePhysicalDevices();

    std::array queue_priorities{ 1.0f };
    std::array queue_create_infos{ vk::DeviceQueueCreateInfo{{}, 0, queue_priorities} };
    std::array device_extensions{ "VK_KHR_swapchain" };
    m_game_impl->m_device = devices[0].createDevice(vk::DeviceCreateInfo({}, queue_create_infos, {}, device_extensions));
    m_game_impl->m_queue = m_game_impl->m_device.getQueue(0, 0);
    m_game_impl->m_command_pool = m_game_impl->m_device.createCommandPool(vk::CommandPoolCreateInfo({}, 0));
    m_game_impl->m_surface = m_game_impl->m_instance.createWin32SurfaceKHR(vk::Win32SurfaceCreateInfoKHR({}, hinstance, hwnd));


    // material
    vk::Format color_format = vk::Format::eB8G8R8A8Unorm /* vk::Format::eR16G16B16A16Sfloat */ ;
    vk::Format depth_format = vk::Format::eD32SfloatS8Uint;


    //safe check
    if (!devices[0].getSurfaceSupportKHR(0, m_game_impl->m_surface)) {
        throw std::exception("device doesn't support surface");
    }

    auto formats = devices[0].getSurfaceFormatsKHR(m_game_impl->m_surface);
    vk::PhysicalDeviceMemoryProperties memory_props = devices[0].getMemoryProperties();

    std::array<uint32_t, 1> queues{ 0 };
    m_game_impl->m_swapchain = m_game_impl->m_device.createSwapchainKHR(vk::SwapchainCreateInfoKHR({}, m_game_impl->m_surface, 2, color_format, vk::ColorSpaceKHR::eSrgbNonlinear, vk::Extent2D(width, height), 1, vk::ImageUsageFlagBits::eColorAttachment, vk::SharingMode::eExclusive, queues, vk::SurfaceTransformFlagBitsKHR::eIdentity, vk::CompositeAlphaFlagBitsKHR::eOpaque, vk::PresentModeKHR::eImmediate/*TODO*/, VK_TRUE));
    m_game_impl->m_images = m_game_impl->m_device.getSwapchainImagesKHR(m_game_impl->m_swapchain);

    m_game_impl->m_sema = m_game_impl->m_device.createSemaphore(vk::SemaphoreCreateInfo());

    m_game_impl->AddGameComponent(std::make_unique<TriangleComponent>(*m_game_impl, color_format, depth_format, width, height, memory_props));
}

void GameImpl::AddGameComponent(std::unique_ptr<IGameComponent> component)
{
    m_game_components.push_back(std::move(component));
    m_game_components.back()->Initialize(m_images);
}

TriangleComponent::TriangleComponent(
    GameImpl& game,
    vk::Format color_format,
    vk::Format depth_format,
    int width,
    int height,
    const vk::PhysicalDeviceMemoryProperties& memory_props)
    :
    m_game(game),
    m_color_format(color_format),
    m_depth_format(depth_format),
    m_width(width),
    m_height(height),
    m_memory_props(memory_props)
{}

void TriangleComponent::Initialize(const std::vector<vk::Image>& swapchain_images)
{
    m_command_buffers = m_game.m_device.allocateCommandBuffers(vk::CommandBufferAllocateInfo(m_game.m_command_pool, vk::CommandBufferLevel::ePrimary, 2));

    std::array<uint32_t, 1> queues{ 0 };
    m_infos.resize(swapchain_images.size());

    std::array attachment_descriptions{ vk::AttachmentDescription{{}, m_color_format, vk::SampleCountFlagBits::e1, vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore, vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore, vk::ImageLayout::eUndefined, vk::ImageLayout::ePresentSrcKHR},
                                        vk::AttachmentDescription{{}, m_depth_format, vk::SampleCountFlagBits::e1, vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore, vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore, vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthStencilAttachmentOptimal} };

    vk::AttachmentReference color_attachment(0, vk::ImageLayout::eColorAttachmentOptimal);
    vk::AttachmentReference depth_attachment(1, vk::ImageLayout::eDepthStencilAttachmentOptimal);
    std::array subpass_description{ vk::SubpassDescription({}, vk::PipelineBindPoint::eGraphics,
                                                           {},
                                                           color_attachment,
                                                           {},
                                                           &depth_attachment) };

    /*
    std::array dependencies{ vk::SubpassDependency(VK_SUBPASS_EXTERNAL, 0, vk::PipelineStageFlagBits::eBottomOfPipe, vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::AccessFlagBits::eMemoryRead, vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite, vk::DependencyFlagBits::eByRegion),
                             vk::SubpassDependency(0, VK_SUBPASS_EXTERNAL, vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eBottomOfPipe, vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite, vk::AccessFlagBits::eMemoryRead, vk::DependencyFlagBits::eByRegion),
                            };
    */


    std::array dependencies{ vk::SubpassDependency(VK_SUBPASS_EXTERNAL, 0,
                                                   vk::PipelineStageFlagBits::eEarlyFragmentTests | vk::PipelineStageFlagBits::eLateFragmentTests,
                                                   vk::PipelineStageFlagBits::eEarlyFragmentTests | vk::PipelineStageFlagBits::eLateFragmentTests,
                                                   vk::AccessFlagBits::eDepthStencilAttachmentWrite,
                                                   vk::AccessFlagBits::eDepthStencilAttachmentRead | vk::AccessFlagBits::eDepthStencilAttachmentWrite),
                             vk::SubpassDependency(VK_SUBPASS_EXTERNAL, 0,
                                                   vk::PipelineStageFlagBits::eColorAttachmentOutput,
                                                   vk::PipelineStageFlagBits::eColorAttachmentOutput,
                                                   {},
                                                   vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite)
    };

    m_render_pass = m_game.m_device.createRenderPass(vk::RenderPassCreateInfo({}, attachment_descriptions, subpass_description, dependencies));

    m_layout = m_game.m_device.createPipelineLayout(vk::PipelineLayoutCreateInfo({}, {}, {}));

    m_vertex_shader = m_game.loadSPIRVShader("E:\\programming\\Graphics\\Game\\Engine\\Engine\\Prim.vert.spv");
    m_fragment_shader = m_game.loadSPIRVShader("E:\\programming\\Graphics\\Game\\Engine\\Engine\\Prim.frag.spv");

    std::array stages{ vk::PipelineShaderStageCreateInfo({}, vk::ShaderStageFlagBits::eVertex, m_vertex_shader, "main"),
                       vk::PipelineShaderStageCreateInfo({}, vk::ShaderStageFlagBits::eFragment, m_fragment_shader, "main")
    };

    std::array vertex_input_bindings{ vk::VertexInputBindingDescription(0, sizeof(PrimitiveVertex), vk::VertexInputRate::eVertex) };
    std::array vertex_input_attributes{ vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat) };

    vk::PipelineVertexInputStateCreateInfo vertex_input_info({}, vertex_input_bindings, vertex_input_attributes);
    vk::PipelineInputAssemblyStateCreateInfo input_assemply({}, vk::PrimitiveTopology::eTriangleList, VK_FALSE);

    std::array viewports{ vk::Viewport(0, 0, m_width, m_height, 0.0f, 1.0f) };
    std::array scissors{ vk::Rect2D(vk::Offset2D(), vk::Extent2D(m_width, m_height)) };
    vk::PipelineViewportStateCreateInfo viewport_state({}, viewports, scissors);
    // TODO dynamic viewport and scissors

    vk::PipelineRasterizationStateCreateInfo rasterization_info({}, VK_FALSE, VK_FALSE, vk::PolygonMode::eFill, vk::CullModeFlagBits::eNone /*eFront*/, vk::FrontFace::eClockwise, VK_FALSE, 0.0f, 0.0f, 0.0f, 1.0f);
    vk::PipelineDepthStencilStateCreateInfo depth_stensil_info({}, VK_FALSE /*Depth test*/);

    vk::PipelineMultisampleStateCreateInfo multisample/*({}, vk::SampleCountFlagBits::e1)*/;

    std::array blend{ vk::PipelineColorBlendAttachmentState(VK_FALSE) };
    blend[0].colorWriteMask = vk::ColorComponentFlagBits::eA | vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB;
    vk::PipelineColorBlendStateCreateInfo blend_state({}, VK_FALSE, vk::LogicOp::eClear, blend);

    m_cache = m_game.m_device.createPipelineCache(vk::PipelineCacheCreateInfo());
    auto pipeline_result = m_game.m_device.createGraphicsPipeline(m_cache, vk::GraphicsPipelineCreateInfo({}, stages, &vertex_input_info, &input_assemply, {}, &viewport_state, &rasterization_info, &multisample, &depth_stensil_info, &blend_state, {}, m_layout, m_render_pass));
    m_pipeline = pipeline_result.value;





    m_verticies = { PrimitiveVertex{0.25f, 0.75f, 0.5f},
                    PrimitiveVertex{0.5f,  0.25f, 0.5f},
                    PrimitiveVertex{0.75f, 0.75f, 0.5f}
    };

    std::size_t buffer_size = m_verticies.size() * sizeof(PrimitiveVertex);
    m_vertex_buffer = m_game.m_device.createBuffer(vk::BufferCreateInfo({}, buffer_size, vk::BufferUsageFlagBits::eVertexBuffer, vk::SharingMode::eExclusive, queues));
    auto memory_buffer_req = m_game.m_device.getBufferMemoryRequirements(m_vertex_buffer);

    uint32_t buffer_index = m_game.find_appropriate_memory_type(memory_buffer_req, m_memory_props, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

    m_vertex_memory = m_game.m_device.allocateMemory(vk::MemoryAllocateInfo(memory_buffer_req.size, buffer_index));

    void* mapped_data = nullptr/*= malloc(buffer_size)*/;
    m_game.m_device.mapMemory(m_vertex_memory, {}, buffer_size, {}, &mapped_data);
    std::memcpy(mapped_data, m_verticies.data(), buffer_size);

    m_game.m_device.bindBufferMemory(m_vertex_buffer, m_vertex_memory, {});
    m_game.m_device.flushMappedMemoryRanges(vk::MappedMemoryRange(m_vertex_memory, {}, buffer_size));



    /*
    vk::DescriptorPool m_descriptor_pool = game.m_device.createDescriptorPool();

    std::array bindings{
        vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eInputAttachment)
    };

    std::array set_layouts{ 
        game.m_device.createDescriptorSetLayout(vk::DescriptorSetLayoutCreateInfo({}, bindings))
    };
    m_descriptor_sets = game.m_device.allocateDescriptorSets(vk::DescriptorSetAllocateInfo(m_descriptor_pool, set_layouts));
    */

    std::array colors{ vk::ClearValue(vk::ClearColorValue(std::array<float, 4>{ 0.0f, 0.0f, 1.0f, 1.0f })),
                       vk::ClearValue(vk::ClearDepthStencilValue(1.0f,0))
    };

    for (int mat_i = 0; mat_i < swapchain_images.size(); ++mat_i) {
        m_infos[mat_i].m_fence = m_game.m_device.createFence(vk::FenceCreateInfo(vk::FenceCreateFlagBits::eSignaled));
        m_infos[mat_i].m_sema = m_game.m_device.createSemaphore(vk::SemaphoreCreateInfo());

        m_infos[mat_i].m_depth_image = m_game.m_device.createImage(vk::ImageCreateInfo({}, vk::ImageType::e2D, m_depth_format, vk::Extent3D(m_width, m_height, 1), 1, 1, vk::SampleCountFlagBits::e1, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eDepthStencilAttachment, vk::SharingMode::eExclusive, queues, vk::ImageLayout::eUndefined /*ePreinitialized*/));
        auto memory_req = m_game.m_device.getImageMemoryRequirements(m_infos[mat_i].m_depth_image);

        uint32_t image_index = m_game.find_appropriate_memory_type(memory_req, m_memory_props, vk::MemoryPropertyFlagBits::eDeviceLocal);

        m_infos[mat_i].m_depth_memory = m_game.m_device.allocateMemory(vk::MemoryAllocateInfo(memory_req.size, image_index));
        m_game.m_device.bindImageMemory(m_infos[mat_i].m_depth_image, m_infos[mat_i].m_depth_memory, {});

        m_infos[mat_i].m_color_image = swapchain_images[mat_i];
        m_infos[mat_i].m_color_image_view = m_game.m_device.createImageView(vk::ImageViewCreateInfo({}, m_infos[mat_i].m_color_image, vk::ImageViewType::e2D, m_color_format, vk::ComponentMapping(), vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)));
        m_infos[mat_i].m_depth_image_view = m_game.m_device.createImageView(vk::ImageViewCreateInfo({}, m_infos[mat_i].m_depth_image, vk::ImageViewType::e2D, m_depth_format, vk::ComponentMapping(), vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil, 0, 1, 0, 1)));

        std::array image_views{ m_infos[mat_i].m_color_image_view, m_infos[mat_i].m_depth_image_view };
        m_infos[mat_i].m_framebuffer = m_game.m_device.createFramebuffer(vk::FramebufferCreateInfo({}, m_render_pass, image_views, m_width, m_height, 1));


        m_infos[mat_i].m_command_buffer = m_command_buffers[mat_i];

        m_infos[mat_i].m_command_buffer.begin(vk::CommandBufferBeginInfo());

        m_infos[mat_i].m_command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline);
        m_infos[mat_i].m_command_buffer.beginRenderPass(vk::RenderPassBeginInfo(m_render_pass, m_infos[mat_i].m_framebuffer, vk::Rect2D({}, vk::Extent2D(m_width, m_height)), colors), vk::SubpassContents::eInline);

        // m_command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_game_impl->m_materials[mat_i].m_layout, 0, m_game_impl->m_materials[mat_i].m_descriptor_sets, {});

        m_infos[mat_i].m_command_buffer.bindVertexBuffers(0, m_vertex_buffer, { {0} });
        m_infos[mat_i].m_command_buffer.draw(3, 1, 0, 0);

        m_infos[mat_i].m_command_buffer.endRenderPass();

        m_infos[mat_i].m_command_buffer.end();
    }
}

vk::Semaphore TriangleComponent::Draw(int swapchain_image_index, vk::Semaphore wait_sema)
{
    m_game.m_device.waitForFences(m_infos[swapchain_image_index].m_fence, VK_TRUE, -1);
    m_game.m_device.resetFences(m_infos[swapchain_image_index].m_fence);

    vk::PipelineStageFlags stage_flags = { vk::PipelineStageFlagBits::eBottomOfPipe };
    std::array command_buffers{ m_infos[swapchain_image_index].m_command_buffer };
    std::array queue_submits{ vk::SubmitInfo(wait_sema, stage_flags, command_buffers, m_infos[swapchain_image_index].m_sema) };
    m_game.m_queue.submit(queue_submits, m_infos[swapchain_image_index].m_fence);

    return m_infos[swapchain_image_index].m_sema;
}

void TriangleComponent::DestroyResources()
{
    for (auto& info : m_infos) {
        m_game.m_device.destroyFramebuffer(info.m_framebuffer);

        m_game.m_device.destroyImageView(info.m_color_image_view);
        m_game.m_device.destroyImageView(info.m_depth_image_view);

        m_game.m_device.destroyImage(info.m_depth_image);
        // m_game_impl->m_device.destroyImage(m_game_impl->m_color_image);

        m_game.m_device.destroyRenderPass(m_render_pass);
    }

    m_game.m_device.freeCommandBuffers(m_game.m_command_pool, m_command_buffers);
}

uint32_t GameImpl::find_appropriate_memory_type(vk::MemoryRequirements & mem_req, const vk::PhysicalDeviceMemoryProperties& memory_props, vk::MemoryPropertyFlags memory_flags)
{
    for (uint32_t i = 0; i < VK_MAX_MEMORY_TYPES; ++i) {
        if ((mem_req.memoryTypeBits & 1) == 1) {
            // Type is available, does it match user properties?
            if ((memory_props.memoryTypes[i].propertyFlags & memory_flags) == memory_flags) {
                return i;
            }
        }
        mem_req.memoryTypeBits >>= 1;
    }
    return -1;
}

void Game::Exit()
{
    for (auto& game_component : m_game_impl->m_game_components) {
        game_component->DestroyResources();
    }

    m_game_impl->m_device.destroySwapchainKHR(m_game_impl->m_swapchain);

    m_game_impl->m_instance.destroySurfaceKHR(m_game_impl->m_surface);
    m_game_impl->m_device.destroyCommandPool(m_game_impl->m_command_pool);
    m_game_impl->m_device.destroy();
    m_game_impl->m_instance.destroy();
}

void Game::Draw()
{
    auto next_image = m_game_impl->m_device.acquireNextImageKHR(m_game_impl->m_swapchain, UINT64_MAX, m_game_impl->m_sema);

    std::vector<vk::Semaphore> semaphores;
    for (auto& game_component : m_game_impl->m_game_components) {
        semaphores.push_back(game_component->Draw(next_image.value, m_game_impl->m_sema));
    }

    std::array results{vk::Result()};
    m_game_impl->m_queue.presentKHR(vk::PresentInfoKHR(semaphores, m_game_impl->m_swapchain, next_image.value, results));
}
