// Engine.cpp : Defines the exported functions for the DLL.
//

#include "Engine.h"
#include "DeferredRender.h"
#include "ForwardRender.h"

#include "util.h"

#include <array>
#include <fstream>
#include <iostream>
#include <memory>
#include <vector>

// This is the constructor of a class that has been exported.
Game::Game()
{
}

Game::~Game()
{
}

void Game::Initialize(HINSTANCE hinstance, HWND hwnd, int width, int height, const glm::mat4& CameraMatrix, const glm::mat4& ProjectionMatrix)
{
    m_width = width;
    m_height = height;

    vk::ApplicationInfo application_info("Lab1", 1, "Engine", 1, VK_API_VERSION_1_2);


    std::vector<vk::ExtensionProperties> ext = vk::enumerateInstanceExtensionProperties();


    //std::array layers = {
        //"VK_LAYER_KHRONOS_validation",
        //"VK_LAYER_LUNARG_api_dump",
      //  "VK_LAYER_LUNARG_monitor",

        // "VK_LAYER_RENDERDOC_Capture",
#ifdef VK_TRACE
        "VK_LAYER_LUNARG_vktrace",
#endif
    //};
            std::array<const char* const, 0> layers = {};

    std::array extensions = {
        "VK_EXT_debug_report",
        "VK_EXT_debug_utils",
        //"VK_KHR_external_memory_capabilities",
        //"VK_NV_external_memory_capabilities",
        "VK_EXT_swapchain_colorspace",
        "VK_KHR_surface",
        "VK_KHR_win32_surface",

        //"VK_KHR_get_physical_device_properties2"
    };

    //std::array<const char* const, 0> extensions = {};

    m_instance = vk::createInstance(vk::InstanceCreateInfo({}, &application_info, layers, extensions));

    auto devices = m_instance.enumeratePhysicalDevices();

    std::array queue_priorities{ 1.0f };
    std::array queue_create_infos{ vk::DeviceQueueCreateInfo{{}, 0, queue_priorities} };
    std::array device_extensions{ "VK_KHR_swapchain" };
    m_device = devices[0].createDevice(vk::DeviceCreateInfo({}, queue_create_infos, {}, device_extensions));
    m_queue = m_device.getQueue(0, 0);
    m_command_pool = m_device.createCommandPool(vk::CommandPoolCreateInfo(vk::CommandPoolCreateFlagBits::eResetCommandBuffer, 0));
    m_surface = m_instance.createWin32SurfaceKHR(vk::Win32SurfaceCreateInfoKHR({}, hinstance, hwnd));

    //safe check
    if (!devices[0].getSurfaceSupportKHR(0, m_surface)) {
        throw std::exception("device doesn't support surface");
    }

    auto formats = devices[0].getSurfaceFormatsKHR(m_surface);
    m_memory_props = devices[0].getMemoryProperties();

    std::array<uint32_t, 1> queues{ 0 };
    m_swapchain = m_device.createSwapchainKHR(vk::SwapchainCreateInfoKHR({}, m_surface, 2, m_color_format, vk::ColorSpaceKHR::eSrgbNonlinear, vk::Extent2D(width, height), 1, vk::ImageUsageFlagBits::eColorAttachment, vk::SharingMode::eExclusive, queues, vk::SurfaceTransformFlagBitsKHR::eIdentity, vk::CompositeAlphaFlagBitsKHR::eOpaque, vk::PresentModeKHR::eImmediate/*TODO*/, VK_TRUE));

    InitializePipelineLayout();







    std::array pool_size{ vk::DescriptorPoolSize(vk::DescriptorType::eUniformBufferDynamic, 2) };
    m_descriptor_pool = m_device.createDescriptorPool(vk::DescriptorPoolCreateInfo({}, 2, pool_size));

    m_descriptor_set = m_device.allocateDescriptorSets(vk::DescriptorSetAllocateInfo(m_descriptor_pool, m_descriptor_set_layouts[0]))[0];

    m_view_projection_matrix = ProjectionMatrix * CameraMatrix;

    std::vector matrixes{ m_view_projection_matrix };
    auto out2 = create_buffer(*this, matrixes, vk::BufferUsageFlagBits::eUniformBuffer, 0, false);
    m_world_view_projection_matrix_buffer = out2.m_buffer;
    m_world_view_projection_matrix_memory = out2.m_memory;
    m_world_view_projection_mapped_memory = out2.m_mapped_memory;

    std::array descriptor_buffer_infos{ vk::DescriptorBufferInfo(m_world_view_projection_matrix_buffer, {}, VK_WHOLE_SIZE) };

    std::array write_descriptors{ vk::WriteDescriptorSet(m_descriptor_set, 0, 0, vk::DescriptorType::eUniformBufferDynamic, {}, descriptor_buffer_infos, {}) };
    m_device.updateDescriptorSets(write_descriptors, {});




    auto sets = m_device.allocateDescriptorSets(vk::DescriptorSetAllocateInfo(m_descriptor_pool, m_descriptor_set_layouts[3]));
    m_lights_descriptor_set = sets[0];

    auto out3 = sync_create_empty_host_invisible_buffer(*this, 10 * sizeof(LightShaderInfo), vk::BufferUsageFlagBits::eUniformBuffer, 0);
    m_lights_buffer = out3.m_buffer;
    m_lights_memory = out3.m_memory;

    std::array lights_buffer_infos{ vk::DescriptorBufferInfo(m_lights_buffer, {}, VK_WHOLE_SIZE) };

    auto out4 = sync_create_empty_host_invisible_buffer(*this, sizeof(uint32_t), vk::BufferUsageFlagBits::eUniformBuffer, 0);
    m_lights_count_buffer = out4.m_buffer;
    m_lights_count_memory = out4.m_memory;

    std::array lights_count_buffer_infos{ vk::DescriptorBufferInfo(m_lights_count_buffer, {}, VK_WHOLE_SIZE) };

    std::array write_lights_descriptors{ vk::WriteDescriptorSet(m_lights_descriptor_set, 0, 0, vk::DescriptorType::eUniformBuffer, {}, lights_buffer_infos, {}),
                                         vk::WriteDescriptorSet(m_lights_descriptor_set, 1, 0, vk::DescriptorType::eUniformBuffer, {}, lights_count_buffer_infos, {}) };
    m_device.updateDescriptorSets(write_lights_descriptors, {});

    images = m_device.getSwapchainImagesKHR(m_swapchain);
    m_shadpwed_lights = std::make_unique<Lights>(*this, ProjectionMatrix, images);


    //render = new DeferredRender(*this, images);
    render = new ForwardRender(*this, images);
}

void Game::SecondInitialize()
{
    render->Initialize();

    m_initialized = true;
}

void Game::InitializePipelineLayout()
{
    std::array view_proj_binding{
        vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBufferDynamic, 1, vk::ShaderStageFlagBits::eVertex, nullptr) /*view_proj*/
    };

    std::array world_binding{
        vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBufferDynamic, 1, vk::ShaderStageFlagBits::eVertex, nullptr)
    };

    std::array material_bindings{
        vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment, nullptr), /*albedo*/
        vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment, nullptr) /*normal*/
    };

    std::array light_bindings{
        vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eVertex, nullptr), /*light*/
        vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eVertex, nullptr)  /*light count*/
    };

    m_descriptor_set_layouts = {
        m_device.createDescriptorSetLayout(vk::DescriptorSetLayoutCreateInfo({}, view_proj_binding)),
        m_device.createDescriptorSetLayout(vk::DescriptorSetLayoutCreateInfo({}, world_binding)),
        m_device.createDescriptorSetLayout(vk::DescriptorSetLayoutCreateInfo({}, material_bindings)),
        m_device.createDescriptorSetLayout(vk::DescriptorSetLayoutCreateInfo({}, light_bindings)),
    };

    /*
    std::array push_constants = {
        vk::PushConstantRange(vk::ShaderStageFlagBits::eFragment, 0, 4)
    };

    m_layout = m_device.createPipelineLayout(vk::PipelineLayoutCreateInfo({}, m_descriptor_set_layouts, push_constants));
    */
}

void Game::create_memory_for_image(const vk::Image & image, vk::DeviceMemory & memory, vk::MemoryPropertyFlags flags)
{
    auto memory_req = m_device.getImageMemoryRequirements(image);

    uint32_t image_index = find_appropriate_memory_type(memory_req, m_memory_props, flags);

    memory = m_device.allocateMemory(vk::MemoryAllocateInfo(memory_req.size, image_index));
    m_device.bindImageMemory(image, memory, {});
}

void Game::Update(int width, int height)
{
    // Don't react to resize until after first initialization.
    if (!m_initialized) {
        return;
    }

    m_device.waitIdle();

    /*
    for (auto& swapchain_data : m_swapchain_data) {
        m_device.destroyFramebuffer(swapchain_data.m_deffered_framebuffer);
        m_device.destroyFramebuffer(swapchain_data.m_composite_framebuffer);


        m_device.destroyImageView(swapchain_data.m_color_image_view);
        m_device.destroyImageView(swapchain_data.m_depth_image_view);

        m_device.freeMemory(swapchain_data.m_depth_memory);

        m_device.destroyImage(swapchain_data.m_depth_image);
        // m_device.destroyImage(swapchain_data.m_color_image);
    }
    */
    m_width = width;
    m_height = height;

    std::array<uint32_t, 1> queues{ 0 };
    m_swapchain = m_device.createSwapchainKHR(vk::SwapchainCreateInfoKHR({}, m_surface, 2, m_color_format, vk::ColorSpaceKHR::eSrgbNonlinear, vk::Extent2D(width, height), 1, vk::ImageUsageFlagBits::eColorAttachment, vk::SharingMode::eExclusive, queues, vk::SurfaceTransformFlagBitsKHR::eIdentity, vk::CompositeAlphaFlagBitsKHR::eOpaque, vk::PresentModeKHR::eImmediate, VK_TRUE, m_swapchain));

    images = m_device.getSwapchainImagesKHR(m_swapchain);
    render->Update(images);
}

vk::ShaderModule Game::loadSPIRVShader(std::string filename)
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

uint32_t Game::find_appropriate_memory_type(vk::MemoryRequirements & mem_req, const vk::PhysicalDeviceMemoryProperties& memory_props, vk::MemoryPropertyFlags memory_flags)
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

void Game::update_camera_projection_matrixes(const glm::mat4& CameraMatrix, const glm::mat4& ProjectionMatrix)
{
    m_view_projection_matrix = ProjectionMatrix * CameraMatrix;

    std::vector matrixes{ m_view_projection_matrix };

    auto memory_buffer_req = m_device.getBufferMemoryRequirements(m_world_view_projection_matrix_buffer);

    std::memcpy(m_world_view_projection_mapped_memory, matrixes.data(), sizeof(glm::mat4));
    m_device.flushMappedMemoryRanges(vk::MappedMemoryRange(m_world_view_projection_matrix_memory, {}, memory_buffer_req.size));
}

int Game::add_light(const glm::vec3& position, const glm::vec3& cameraTarget, const glm::vec3& upVector)
{
    m_shadpwed_lights->add_light(position, cameraTarget, upVector);
    render->Update(images);

    //m_lights.push_back(light);
    std::vector<LightShaderInfo> light = m_shadpwed_lights->get_light_shader_info();

    m_lights_memory_to_transfer.clear();
    m_lights_memory_to_transfer.reserve(light.size() * sizeof(LightShaderInfo));
    m_lights_memory_to_transfer.insert(m_lights_memory_to_transfer.end(), reinterpret_cast<std::byte*>(light.begin()._Ptr), reinterpret_cast<std::byte*>(light.end()._Ptr));
    update_buffer(*this, m_lights_memory_to_transfer.size(), m_lights_memory_to_transfer.data(), m_lights_buffer, vk::BufferUsageFlagBits::eUniformBuffer, 0);

    uint32_t size = static_cast<uint32_t>(light.size());
    m_lights_count_memory_to_transfer.clear();
    m_lights_count_memory_to_transfer.reserve(sizeof(size));
    m_lights_count_memory_to_transfer.insert(m_lights_count_memory_to_transfer.end(), reinterpret_cast<std::byte*>(&size), reinterpret_cast<std::byte*>(&size) + sizeof(size));
    update_buffer(*this, m_lights_count_memory_to_transfer.size(), m_lights_count_memory_to_transfer.data(), m_lights_count_buffer, vk::BufferUsageFlagBits::eUniformBuffer, 0);

    return light.size() - 1;
}
/*
void Game::update_light(int index, const LightInfo & light)
{
    m_lights[index] = light;

    m_lights_memory_to_transfer.clear();
    m_lights_memory_to_transfer.reserve(sizeof(uint32_t) + m_lights.size() * sizeof(LightInfo));

    uint32_t size = static_cast<uint32_t>(m_lights.size());
    m_lights_memory_to_transfer.insert(m_lights_memory_to_transfer.end(), reinterpret_cast<std::byte*>(&size), reinterpret_cast<std::byte*>(&size) + sizeof(size));
    m_lights_memory_to_transfer.insert(m_lights_memory_to_transfer.end(), reinterpret_cast<std::byte*>(m_lights.begin()._Ptr), reinterpret_cast<std::byte*>(m_lights.end()._Ptr));
    update_buffer(*this, m_lights_memory_to_transfer.size() * sizeof(std::byte), m_lights_memory_to_transfer.data(), m_lights_buffer, vk::BufferUsageFlagBits::eUniformBuffer, 0);
}
*/
void Game::Exit()
{

    m_device.unmapMemory(m_world_view_projection_matrix_memory);
    /*
    for (auto& swapchain_data : m_swapchain_data) {
        m_device.destroyFramebuffer(swapchain_data.m_deffered_framebuffer);
        m_device.destroyFramebuffer(swapchain_data.m_composite_framebuffer);

        m_device.destroyImageView(swapchain_data.m_color_image_view);
        m_device.destroyImageView(swapchain_data.m_depth_image_view);

        m_device.freeMemory(swapchain_data.m_depth_memory);

        m_device.destroyImage(swapchain_data.m_depth_image);
        // m_device.destroyImage(swapchain_data.m_color_image);
    }

    m_device.destroyRenderPass(m_deffered_render_pass);
    m_device.destroyRenderPass(m_composite_render_pass);
    */
    m_device.destroySwapchainKHR(m_swapchain);

    m_instance.destroySurfaceKHR(m_surface);
    m_device.destroyCommandPool(m_command_pool);
    m_device.destroy();
    m_instance.destroy();
}

void Game::Draw()
{
    render->Draw();
}
