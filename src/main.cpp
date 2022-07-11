#include <stdexcept>
#include <vector>
#include <algorithm>
#include <optional>
#include <set>
#include <fstream>

#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#include <spdlog/spdlog.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "HelloVulkan_config.h"

#ifdef NDEBUG
const bool enable_validation_layers = false;
#else
const bool enable_validation_layers = true;
#endif

VkResult proxy_vkCreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT *create_info, const VkAllocationCallbacks *allocator, VkDebugUtilsMessengerEXT *debug_messenger)
{
    auto function = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");

    if (function != nullptr)
        return function(instance, create_info, allocator, debug_messenger);
    else
        return VK_ERROR_EXTENSION_NOT_PRESENT;
}

void proxy_vkDestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debug_messenger, const VkAllocationCallbacks *allocator)
{
    auto function = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");

    if (function != nullptr)
        function(instance, debug_messenger, allocator);
}

class HelloTriangleApplication
{
public:
    void run()
    {
        init_window();
        init_vulkan();
        main_loop();
        cleanup();
    }

private:
    void init_vulkan()
    {
        create_instance();
        setup_debug_messenger();
        create_surface();
        pick_physical_device();
        create_logical_device();
        create_swapchain();
        create_image_views();
        create_render_pass();
        create_graphics_pipeline();
        create_framebuffers();
        create_command_pool();
        create_command_buffers();
        create_sync_objects();
    }

    void init_window()
    {
        glfwInit();

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

        m_window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, PROJECT_NAME, nullptr, nullptr);
        glfwSetWindowUserPointer(m_window, this);
        glfwSetFramebufferSizeCallback(m_window, framebuffer_resized_callback);
    }

    void main_loop()
    {
        while (!glfwWindowShouldClose(m_window))
        {
            glfwPollEvents();
            draw();
        }

        vkDeviceWaitIdle(m_device);
    }

    void cleanup()
    {
        cleanup_swapchain();

        for (auto i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            vkDestroySemaphore(m_device, m_render_finished_semaphores[i], nullptr);
            vkDestroySemaphore(m_device, m_image_available_semaphores[i], nullptr);
            vkDestroyFence(m_device, m_in_flight_fences[i], nullptr);
        }

        vkDestroyCommandPool(m_device, m_command_pool, nullptr);
        vkDestroyDevice(m_device, nullptr);

        if (enable_validation_layers)
            proxy_vkDestroyDebugUtilsMessengerEXT(m_instance, m_debug_messenger, nullptr);

        vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
        vkDestroyInstance(m_instance, nullptr);

        glfwDestroyWindow(m_window);
        glfwTerminate();
    }

    void create_instance()
    {
        VkApplicationInfo app_info{};
        app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        app_info.pApplicationName = PROJECT_NAME;
        app_info.applicationVersion = VK_MAKE_VERSION(PROJECT_VERSION_MAJOR, PROJECT_VERSION_MINOR, PROJECT_VERSION_PATCH);
        app_info.pEngineName = "No Engine";
        app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        app_info.apiVersion = VK_API_VERSION_1_0;

        uint32_t glfw_extension_count = 0;
        const char **glfw_extensions;

        glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);
        std::vector<const char *> extensions(glfw_extensions, glfw_extensions + glfw_extension_count);

        VkInstanceCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        create_info.pApplicationInfo = &app_info;

        VkDebugUtilsMessengerCreateInfoEXT debug_messenger_create_info{};

        if (enable_validation_layers)
        {
            check_validation_layer_support();

            create_info.enabledLayerCount = static_cast<uint32_t>(m_validation_layers.size());
            create_info.ppEnabledLayerNames = m_validation_layers.data();

            populate_debug_messager_create_info(debug_messenger_create_info);
            create_info.pNext = (VkDebugUtilsMessengerCreateInfoEXT *)&debug_messenger_create_info;

            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }
        else
            create_info.enabledLayerCount = 0;

#if 0
        create_info.flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
        extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
        extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
#endif

        check_instance_extensions(extensions);
        create_info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        create_info.ppEnabledExtensionNames = extensions.data();

        if (vkCreateInstance(&create_info, nullptr, &m_instance) != VK_SUCCESS)
            throw std::runtime_error("VULKAN_CREATE_INSTANCE_FAILURE");
    }

    void check_instance_extensions(const std::vector<const char *> &extensions)
    {
        uint32_t extension_count = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr);

        std::vector<VkExtensionProperties> available_extensions(extension_count);
        vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, available_extensions.data());

        for (const auto &extension : extensions)
        {
            auto ext_it = std::find_if(available_extensions.cbegin(), available_extensions.cend(), [&extension](const VkExtensionProperties &extension_prop)
                                       { return !std::strcmp(extension, extension_prop.extensionName); });

            if (ext_it == available_extensions.end())
            {
                SPDLOG_ERROR("Missing requested instance extension: {}", extension);
                throw std::runtime_error("VULKAN_INSTANCE_EXTENSION_NOT_FOUND");
            }
        }

        SPDLOG_TRACE("Instance extensions enabled:");
        for (const auto &extension : extensions)
            SPDLOG_TRACE("\t{}", extension);
    }

    void check_validation_layer_support()
    {
        uint32_t layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

        std::vector<VkLayerProperties> available_layers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, available_layers.data());

        for (const auto &layer : m_validation_layers)
        {
            auto layer_it = std::find_if(available_layers.cbegin(), available_layers.cend(), [&layer](const VkLayerProperties &layer_prop)
                                         { return !std::strcmp(layer, layer_prop.layerName); });

            if (layer_it == available_layers.end())
            {
                SPDLOG_ERROR("Missing requested validation layer: {}", layer);
                throw std::runtime_error("VULKAN_VALIDATION_LAYER_NOT_FOUND");
            }
        }

        SPDLOG_TRACE("ValidationLayers enabled:");
        for (const auto &layer : m_validation_layers)
            SPDLOG_TRACE("\t{}", layer);
    }

    void setup_debug_messenger()
    {
        if (!enable_validation_layers)
            return;

        VkDebugUtilsMessengerCreateInfoEXT create_info{};
        populate_debug_messager_create_info(create_info);

        if (proxy_vkCreateDebugUtilsMessengerEXT(m_instance, &create_info, nullptr, &m_debug_messenger) != VK_SUCCESS)
            throw std::runtime_error("VULKAN_SETUP_DEBUG_MESSAGER_FAILURE");
    }

    void populate_debug_messager_create_info(VkDebugUtilsMessengerCreateInfoEXT &create_info)
    {
        create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        create_info.pfnUserCallback = debug_callback;
        create_info.pUserData = nullptr;
    }

    void pick_physical_device()
    {
        uint32_t device_count = 0;
        vkEnumeratePhysicalDevices(m_instance, &device_count, nullptr);

        if (device_count == 0)
            throw std::runtime_error("VULKAN_PHYSICAL_DEVICE_NOT_FOUND");

        std::vector<VkPhysicalDevice> devices(device_count);
        vkEnumeratePhysicalDevices(m_instance, &device_count, devices.data());

        for (const auto &device : devices)
            if (is_device_suitable(device))
            {
                m_physical_device = device;
                break;
            }

        SPDLOG_INFO("Devices found:");
        for (const auto &device : devices)
        {
            VkPhysicalDeviceProperties device_properties;
            vkGetPhysicalDeviceProperties(device, &device_properties);
            SPDLOG_INFO("\t{} {}", device_properties.deviceName, (device == m_physical_device ? "(Selected)" : ""));
        }

        if (m_physical_device == VK_NULL_HANDLE)
            throw std::runtime_error("VULKAN_PHYSICAL_DEVICE_NOT_SUITABLE");
    }

    bool is_device_suitable(VkPhysicalDevice device)
    {
        QueueFamilyIndices indices = find_queue_families(device);

        bool extensions_supported = check_device_extensions(device);

        bool swapchain_adequate = false;
        if (extensions_supported)
        {
            SwapChainSupportDetails swapchain_support = query_Swap_chain_support(device);
            swapchain_adequate = !swapchain_support.formats.empty() && !swapchain_support.present_modes.empty();
        }

        return indices.is_complete() && extensions_supported && swapchain_adequate;
    }

    struct QueueFamilyIndices
    {
        std::optional<uint32_t> graphics_family;
        std::optional<uint32_t> present_family;

        bool is_complete()
        {
            return graphics_family.has_value() && present_family.has_value();
        }
    };

    QueueFamilyIndices find_queue_families(VkPhysicalDevice device)
    {
        uint32_t queue_family_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, nullptr);

        std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families.data());

        QueueFamilyIndices indices;

        for (auto i = 0; i < queue_family_count; i++)
        {
            if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
                indices.graphics_family = i;

            VkBool32 present_support = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_surface, &present_support);

            if (present_support)
                indices.present_family = i;

            if (indices.is_complete())
                break;
        }

        return indices;
    }

    void create_logical_device()
    {
        QueueFamilyIndices indices = find_queue_families(m_physical_device);

        std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
        std::set<uint32_t> unique_queue_families = {
            indices.graphics_family.value(),
            indices.present_family.value(),
        };

        float queue_priority = 1.0f;
        for (const auto &queue_family : unique_queue_families)
        {
            VkDeviceQueueCreateInfo queue_create_info{};
            queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queue_create_info.queueFamilyIndex = queue_family;
            queue_create_info.queueCount = 1;
            queue_create_info.pQueuePriorities = &queue_priority;
            queue_create_infos.push_back(queue_create_info);
        }

        VkPhysicalDeviceFeatures device_features{};

        VkDeviceCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        create_info.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size());
        create_info.pQueueCreateInfos = queue_create_infos.data();
        create_info.pEnabledFeatures = &device_features;

        create_info.enabledExtensionCount = static_cast<uint32_t>(m_device_extensions.size());
        create_info.ppEnabledExtensionNames = m_device_extensions.data();

        if (enable_validation_layers)
        {
            create_info.enabledLayerCount = static_cast<uint32_t>(m_validation_layers.size());
            create_info.ppEnabledLayerNames = m_validation_layers.data();
        }
        else
            create_info.enabledLayerCount = 0;

        if (vkCreateDevice(m_physical_device, &create_info, nullptr, &m_device) != VK_SUCCESS)
            throw std::runtime_error("VULKAN_CREATE_LOGICAL_DEVICE_FAILURE");

        SPDLOG_TRACE("Device extensions enabled:");
        for (const auto &extension : m_device_extensions)
            SPDLOG_TRACE("\t{}", extension);

        vkGetDeviceQueue(m_device, indices.graphics_family.value(), 0, &m_graphics_queue);
        vkGetDeviceQueue(m_device, indices.present_family.value(), 0, &m_present_queue);
    }

    void create_surface()
    {
        if (glfwCreateWindowSurface(m_instance, m_window, nullptr, &m_surface) != VK_SUCCESS)
            throw std::runtime_error("VULKAN_CREATE_SURFACE_FAILURE");
    }

    bool check_device_extensions(VkPhysicalDevice device)
    {
        uint32_t extension_count;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, nullptr);

        std::vector<VkExtensionProperties> available_extensions(extension_count);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, available_extensions.data());

        for (const auto &extension : m_device_extensions)
        {
            auto ext_it = std::find_if(available_extensions.cbegin(), available_extensions.cend(), [&extension](const VkExtensionProperties &extension_prop)
                                       { return !std::strcmp(extension, extension_prop.extensionName); });

            if (ext_it == available_extensions.end())
                return false;
        }

        return true;
    }

    struct SwapChainSupportDetails
    {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> present_modes;
    };

    void create_swapchain()
    {
        SwapChainSupportDetails swapchain_support = query_Swap_chain_support(m_physical_device);

        VkExtent2D extent = choose_swapchain_extent(swapchain_support.capabilities);
        VkSurfaceFormatKHR surface_format = choose_swapchain_surface_format(swapchain_support.formats);
        VkPresentModeKHR present_mode = choose_swapchain_present_mode(swapchain_support.present_modes);

        uint32_t image_count = swapchain_support.capabilities.minImageCount + 1;

        if (swapchain_support.capabilities.maxImageCount > 0 && image_count > swapchain_support.capabilities.maxImageCount)
            image_count = swapchain_support.capabilities.maxImageCount;

        VkSwapchainCreateInfoKHR create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        create_info.surface = m_surface;
        create_info.minImageCount = image_count;
        create_info.imageFormat = surface_format.format;
        create_info.imageColorSpace = surface_format.colorSpace;
        create_info.imageExtent = extent;
        create_info.imageArrayLayers = 1;
        create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        QueueFamilyIndices indices = find_queue_families(m_physical_device);
        uint32_t queue_family_indices[] = {
            indices.graphics_family.value(),
            indices.present_family.value(),
        };

        if (indices.graphics_family != indices.present_family)
        {
            create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            create_info.queueFamilyIndexCount = 2;
            create_info.pQueueFamilyIndices = queue_family_indices;
        }
        else
        {
            create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            create_info.queueFamilyIndexCount = 0;
            create_info.pQueueFamilyIndices = nullptr;
        }

        create_info.preTransform = swapchain_support.capabilities.currentTransform;
        create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        create_info.presentMode = present_mode;
        create_info.clipped = VK_TRUE;
        create_info.oldSwapchain = VK_NULL_HANDLE;

        if (vkCreateSwapchainKHR(m_device, &create_info, nullptr, &m_swapchain) != VK_SUCCESS)
            throw std::runtime_error("VULKAN_CREATE_SWAPCHAIN_FAILURE");

        m_swapchain_extent = extent;
        m_swapchain_image_format = surface_format.format;

        vkGetSwapchainImagesKHR(m_device, m_swapchain, &image_count, nullptr);

        m_swapchain_images.resize(image_count);
        vkGetSwapchainImagesKHR(m_device, m_swapchain, &image_count, m_swapchain_images.data());
    }

    SwapChainSupportDetails query_Swap_chain_support(VkPhysicalDevice device)
    {
        SwapChainSupportDetails details;

        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_surface, &details.capabilities);

        uint32_t format_count;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &format_count, nullptr);

        if (format_count != 0)
        {
            details.formats.resize(format_count);
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &format_count, details.formats.data());
        }

        uint32_t present_mode_count;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &present_mode_count, nullptr);

        if (present_mode_count != 0)
        {
            details.present_modes.resize(present_mode_count);
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &present_mode_count, details.present_modes.data());
        }

        return details;
    }

    VkExtent2D choose_swapchain_extent(const VkSurfaceCapabilitiesKHR &capabilities)
    {
        if (capabilities.currentExtent.width != UINT32_MAX)
            return capabilities.currentExtent;
        else
        {
            int width, height;
            glfwGetFramebufferSize(m_window, &width, &height);

            VkExtent2D actualExtent = {
                static_cast<uint32_t>(width),
                static_cast<uint32_t>(height),
            };

            actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
            actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

            return actualExtent;
        }
    }

    VkSurfaceFormatKHR choose_swapchain_surface_format(const std::vector<VkSurfaceFormatKHR> &available_formats)
    {
        for (const auto &available_format : available_formats)
            if (available_format.format == VK_FORMAT_B8G8R8A8_SRGB && available_format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
                return available_format;

        return available_formats[0];
    }

    VkPresentModeKHR choose_swapchain_present_mode(const std::vector<VkPresentModeKHR> &available_present_modes)
    {
        // for (const auto &available_present_mode : available_present_modes)
        //     if (available_present_mode == VK_PRESENT_MODE_MAILBOX_KHR)
        //         return available_present_mode;

        return VK_PRESENT_MODE_FIFO_KHR;
    }

    void create_image_views()
    {
        m_swapchain_image_views.resize(m_swapchain_images.size());

        for (auto i = 0; i < m_swapchain_images.size(); i++)
        {
            VkImageViewCreateInfo create_info{};
            create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            create_info.image = m_swapchain_images[i];
            create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
            create_info.format = m_swapchain_image_format;
            create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            create_info.subresourceRange.baseMipLevel = 0;
            create_info.subresourceRange.levelCount = 1;
            create_info.subresourceRange.baseArrayLayer = 0;
            create_info.subresourceRange.layerCount = 1;

            if (vkCreateImageView(m_device, &create_info, nullptr, &m_swapchain_image_views[i]) != VK_SUCCESS)
                throw std::runtime_error("VULKAN_CREATE_IMAGE_VIEW_FAILURE");
        }
    }

    void create_render_pass()
    {
        VkAttachmentDescription color_attachment{};
        color_attachment.format = m_swapchain_image_format;
        color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
        color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference color_attachment_ref{};
        color_attachment_ref.attachment = 0;
        color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &color_attachment_ref;

        VkSubpassDependency subpass_dependency{};
        subpass_dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        subpass_dependency.dstSubpass = 0;
        subpass_dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        subpass_dependency.srcAccessMask = 0;
        subpass_dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        subpass_dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo render_pass_create_info{};
        render_pass_create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        render_pass_create_info.attachmentCount = 1;
        render_pass_create_info.pAttachments = &color_attachment;
        render_pass_create_info.subpassCount = 1;
        render_pass_create_info.pSubpasses = &subpass;
        render_pass_create_info.dependencyCount = 1;
        render_pass_create_info.pDependencies = &subpass_dependency;

        if (vkCreateRenderPass(m_device, &render_pass_create_info, nullptr, &m_render_pass) != VK_SUCCESS)
            throw std::runtime_error("VULKAN_CREATE_RENDER_PASS_FAILURE");
    }

    void create_graphics_pipeline()
    {
        auto vertex_shader_code = read_file(SHADER_BINARY_DIRECTORY "/shader.vert.spv");
        auto fragment_shader_code = read_file(SHADER_BINARY_DIRECTORY "/shader.frag.spv");

        VkShaderModule vertex_shader_module = create_shader_module(vertex_shader_code);
        VkShaderModule fragment_shader_module = create_shader_module(fragment_shader_code);

        VkPipelineShaderStageCreateInfo vertex_stage_create_info{};
        vertex_stage_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertex_stage_create_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertex_stage_create_info.module = vertex_shader_module;
        vertex_stage_create_info.pName = "main";

        VkPipelineShaderStageCreateInfo fragment_shader_create_info{};
        fragment_shader_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragment_shader_create_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragment_shader_create_info.module = fragment_shader_module;
        fragment_shader_create_info.pName = "main";

        VkPipelineShaderStageCreateInfo shader_stages[] = {
            vertex_stage_create_info,
            fragment_shader_create_info,
        };

        VkPipelineVertexInputStateCreateInfo vertex_input_create_info{};
        vertex_input_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertex_input_create_info.vertexBindingDescriptionCount = 0;
        vertex_input_create_info.pVertexBindingDescriptions = nullptr;
        vertex_input_create_info.vertexAttributeDescriptionCount = 0;
        vertex_input_create_info.pVertexAttributeDescriptions = nullptr;

        VkPipelineInputAssemblyStateCreateInfo input_assembly_create_info{};
        input_assembly_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        input_assembly_create_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        input_assembly_create_info.primitiveRestartEnable = VK_FALSE;

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float)m_swapchain_extent.width;
        viewport.height = (float)m_swapchain_extent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = m_swapchain_extent;

        VkPipelineViewportStateCreateInfo viewport_create_info{};
        viewport_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewport_create_info.viewportCount = 1;
        viewport_create_info.pViewports = &viewport;
        viewport_create_info.scissorCount = 1;
        viewport_create_info.pScissors = &scissor;

        VkPipelineRasterizationStateCreateInfo rasterization_create_info{};
        rasterization_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterization_create_info.depthClampEnable = VK_FALSE;
        rasterization_create_info.rasterizerDiscardEnable = VK_FALSE;
        rasterization_create_info.polygonMode = VK_POLYGON_MODE_FILL;
        rasterization_create_info.lineWidth = 1.0f;
        rasterization_create_info.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterization_create_info.frontFace = VK_FRONT_FACE_CLOCKWISE;
        rasterization_create_info.depthBiasEnable = VK_FALSE;
        rasterization_create_info.depthBiasConstantFactor = 0.0f;
        rasterization_create_info.depthBiasClamp = 0.0f;
        rasterization_create_info.depthBiasSlopeFactor = 0.0f;

        VkPipelineMultisampleStateCreateInfo multisample_create_info{};
        multisample_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisample_create_info.sampleShadingEnable = VK_FALSE;
        multisample_create_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        multisample_create_info.minSampleShading = 1.0f;
        multisample_create_info.pSampleMask = nullptr;
        multisample_create_info.alphaToCoverageEnable = VK_FALSE;
        multisample_create_info.alphaToOneEnable = VK_FALSE;

        VkPipelineColorBlendAttachmentState color_blend_attachment{};
        color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        color_blend_attachment.blendEnable = VK_FALSE;

        VkPipelineColorBlendStateCreateInfo color_blending_create_info{};
        color_blending_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        color_blending_create_info.logicOpEnable = VK_FALSE;
        color_blending_create_info.logicOp = VK_LOGIC_OP_COPY;
        color_blending_create_info.attachmentCount = 1;
        color_blending_create_info.pAttachments = &color_blend_attachment;
        color_blending_create_info.blendConstants[0] = 0.0f;
        color_blending_create_info.blendConstants[1] = 0.0f;
        color_blending_create_info.blendConstants[2] = 0.0f;
        color_blending_create_info.blendConstants[3] = 0.0f;

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 0;
        pipelineLayoutInfo.pSetLayouts = nullptr;
        pipelineLayoutInfo.pushConstantRangeCount = 0;
        pipelineLayoutInfo.pPushConstantRanges = nullptr;

        if (vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &m_pipeline_layout) != VK_SUCCESS)
            throw std::runtime_error("VULKAN_CREATE_PIPELINE_LAYOUT_FAILURE");

        VkGraphicsPipelineCreateInfo pipeline_create_info{};
        pipeline_create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipeline_create_info.stageCount = 2;
        pipeline_create_info.pStages = shader_stages;
        pipeline_create_info.pVertexInputState = &vertex_input_create_info;
        pipeline_create_info.pInputAssemblyState = &input_assembly_create_info;
        pipeline_create_info.pViewportState = &viewport_create_info;
        pipeline_create_info.pRasterizationState = &rasterization_create_info;
        pipeline_create_info.pMultisampleState = &multisample_create_info;
        pipeline_create_info.pDepthStencilState = nullptr;
        pipeline_create_info.pColorBlendState = &color_blending_create_info;
        pipeline_create_info.pDynamicState = nullptr;
        pipeline_create_info.layout = m_pipeline_layout;
        pipeline_create_info.renderPass = m_render_pass;
        pipeline_create_info.subpass = 0;
        pipeline_create_info.basePipelineHandle = VK_NULL_HANDLE;
        pipeline_create_info.basePipelineIndex = -1;

        if (vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipeline_create_info, nullptr, &m_graphics_pipeline) != VK_SUCCESS)
            throw std::runtime_error("VULKAN_CREATE_GRAPHICS_PIPELINE_FAILURE");

        vkDestroyShaderModule(m_device, vertex_shader_module, nullptr);
        vkDestroyShaderModule(m_device, fragment_shader_module, nullptr);
    }

    VkShaderModule create_shader_module(const std::vector<char> &shader_code)
    {
        VkShaderModuleCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        create_info.codeSize = shader_code.size();
        create_info.pCode = reinterpret_cast<const uint32_t *>(shader_code.data());

        VkShaderModule shader_module;
        if (vkCreateShaderModule(m_device, &create_info, nullptr, &shader_module) != VK_SUCCESS)
            throw std::runtime_error("VULKAN_CREATE_SHADER_MODULE_FAILURE");

        return shader_module;
    }

    void create_framebuffers()
    {
        m_swapchain_framebuffers.resize(m_swapchain_image_views.size());

        for (auto i = 0; i < m_swapchain_image_views.size(); i++)
        {
            VkImageView attachments[] = {
                m_swapchain_image_views[i],
            };

            VkFramebufferCreateInfo framebuffer_create_info{};
            framebuffer_create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebuffer_create_info.renderPass = m_render_pass;
            framebuffer_create_info.attachmentCount = 1;
            framebuffer_create_info.pAttachments = attachments;
            framebuffer_create_info.width = m_swapchain_extent.width;
            framebuffer_create_info.height = m_swapchain_extent.height;
            framebuffer_create_info.layers = 1;

            if (vkCreateFramebuffer(m_device, &framebuffer_create_info, nullptr, &m_swapchain_framebuffers[i]) != VK_SUCCESS)
                throw std::runtime_error("VULKAN_CREATE_FRAMEBUFFER_FAILURE");
        }
    }

    void create_command_pool()
    {
        QueueFamilyIndices queue_family_indices = find_queue_families(m_physical_device);

        VkCommandPoolCreateInfo pool_create_info{};
        pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        pool_create_info.queueFamilyIndex = queue_family_indices.graphics_family.value();
        pool_create_info.flags = 0;

        if (vkCreateCommandPool(m_device, &pool_create_info, nullptr, &m_command_pool) != VK_SUCCESS)
            throw std::runtime_error("VULKAN_CREATE_COMMAND_POOL_FAILURE");
    }

    void create_command_buffers()
    {
        m_command_buffers.resize(m_swapchain_framebuffers.size());

        VkCommandBufferAllocateInfo command_buffer_allocate_info{};
        command_buffer_allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        command_buffer_allocate_info.commandPool = m_command_pool;
        command_buffer_allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        command_buffer_allocate_info.commandBufferCount = (uint32_t)m_command_buffers.size();

        if (vkAllocateCommandBuffers(m_device, &command_buffer_allocate_info, m_command_buffers.data()) != VK_SUCCESS)
            throw std::runtime_error("VULKAN_ALLOCATE_COMMAND_BUFFERS_FAILURE");

        for (auto i = 0; i < m_command_buffers.size(); i++)
        {
            VkCommandBufferBeginInfo command_buffer_begin_info{};
            command_buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

            if (vkBeginCommandBuffer(m_command_buffers[i], &command_buffer_begin_info) != VK_SUCCESS)
                throw std::runtime_error("VULKAN_BEGIN_COMMAND_BUFFER_FAILURE");

            VkRenderPassBeginInfo render_pass_begin_info{};
            render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            render_pass_begin_info.renderPass = m_render_pass;
            render_pass_begin_info.framebuffer = m_swapchain_framebuffers[i];
            render_pass_begin_info.renderArea.offset = {0, 0};
            render_pass_begin_info.renderArea.extent = m_swapchain_extent;

            VkClearValue clear_color = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
            render_pass_begin_info.clearValueCount = 1;
            render_pass_begin_info.pClearValues = &clear_color;

            vkCmdBeginRenderPass(m_command_buffers[i], &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);
            vkCmdBindPipeline(m_command_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphics_pipeline);
            vkCmdDraw(m_command_buffers[i], 3, 1, 0, 0);
            vkCmdEndRenderPass(m_command_buffers[i]);

            if (vkEndCommandBuffer(m_command_buffers[i]) != VK_SUCCESS)
                throw std::runtime_error("VULKAN_END_COMMAND_BUFFER_FAILURE");
        }
    }

    void create_sync_objects()
    {
        m_image_available_semaphores.resize(MAX_FRAMES_IN_FLIGHT);
        m_render_finished_semaphores.resize(MAX_FRAMES_IN_FLIGHT);
        m_in_flight_fences.resize(MAX_FRAMES_IN_FLIGHT);
        m_images_in_flight.resize(m_swapchain_images.size(), VK_NULL_HANDLE);

        VkSemaphoreCreateInfo semaphore_create_info{};
        semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fence_create_info{};
        fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (auto i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
            if (vkCreateSemaphore(m_device, &semaphore_create_info, nullptr, &m_image_available_semaphores[i]) != VK_SUCCESS ||
                vkCreateSemaphore(m_device, &semaphore_create_info, nullptr, &m_render_finished_semaphores[i]) != VK_SUCCESS ||
                vkCreateFence(m_device, &fence_create_info, nullptr, &m_in_flight_fences[i]) != VK_SUCCESS)
                throw std::runtime_error("VULKAN_CREATE_SYNCHRONIZATION_OBJECTS_FAILURE");
    }

    void draw()
    {
        vkWaitForFences(m_device, 1, &m_in_flight_fences[m_current_frame], VK_TRUE, UINT64_MAX);

        uint32_t image_index;
        VkResult result = vkAcquireNextImageKHR(m_device, m_swapchain, UINT64_MAX, m_image_available_semaphores[m_current_frame], VK_NULL_HANDLE, &image_index);

        if (result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            recreate_swapchain();
            return;
        }
        else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
            throw std::runtime_error("VULKAN_ACQUIRE_IMAGE_FAILURE");

        if (m_images_in_flight[image_index] != VK_NULL_HANDLE)
            vkWaitForFences(m_device, 1, &m_images_in_flight[image_index], VK_TRUE, UINT64_MAX);

        m_images_in_flight[image_index] = m_in_flight_fences[m_current_frame];

        VkSubmitInfo submit_info{};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        VkSemaphore wait_semaphores[] = {m_image_available_semaphores[m_current_frame]};
        VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        submit_info.waitSemaphoreCount = 1;
        submit_info.pWaitSemaphores = wait_semaphores;
        submit_info.pWaitDstStageMask = wait_stages;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &m_command_buffers[image_index];

        VkSemaphore signal_semaphores[] = {m_render_finished_semaphores[m_current_frame]};
        submit_info.signalSemaphoreCount = 1;
        submit_info.pSignalSemaphores = signal_semaphores;

        vkResetFences(m_device, 1, &m_in_flight_fences[m_current_frame]);

        if (vkQueueSubmit(m_graphics_queue, 1, &submit_info, m_in_flight_fences[m_current_frame]) != VK_SUCCESS)
            throw std::runtime_error("VULKAN_QUEUE_SUBMIT_FAILURE");

        VkPresentInfoKHR present_info{};
        present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

        present_info.waitSemaphoreCount = 1;
        present_info.pWaitSemaphores = signal_semaphores;

        VkSwapchainKHR swap_chains[] = {m_swapchain};
        present_info.swapchainCount = 1;
        present_info.pSwapchains = swap_chains;
        present_info.pImageIndices = &image_index;
        present_info.pResults = nullptr;

        result = vkQueuePresentKHR(m_present_queue, &present_info);

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || m_framebuffer_resized)
        {
            m_framebuffer_resized = false;
            recreate_swapchain();
        }
        else if (result != VK_SUCCESS)
            throw std::runtime_error("VULKAN_QUEUE_PRESENT_FAILURE");

        m_current_frame = (m_current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    void recreate_swapchain()
    {
        int width = 0, height = 0;
        glfwGetFramebufferSize(m_window, &width, &height);

        while (width == 0 || height == 0)
        {
            glfwGetFramebufferSize(m_window, &width, &height);
            glfwWaitEvents();
        }

        vkDeviceWaitIdle(m_device);

        cleanup_swapchain();

        create_swapchain();
        create_image_views();
        create_render_pass();
        create_graphics_pipeline();
        create_framebuffers();
        create_command_buffers();
    }

    void cleanup_swapchain()
    {
        for (auto framebuffer : m_swapchain_framebuffers)
            vkDestroyFramebuffer(m_device, framebuffer, nullptr);

        vkFreeCommandBuffers(m_device, m_command_pool, static_cast<uint32_t>(m_command_buffers.size()), m_command_buffers.data());

        vkDestroyPipeline(m_device, m_graphics_pipeline, nullptr);
        vkDestroyPipelineLayout(m_device, m_pipeline_layout, nullptr);
        vkDestroyRenderPass(m_device, m_render_pass, nullptr);

        for (auto image_view : m_swapchain_image_views)
            vkDestroyImageView(m_device, image_view, nullptr);

        vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
    }

    const std::vector<const char *> m_validation_layers{
        "VK_LAYER_KHRONOS_validation",
    };

    const std::vector<const char *> m_device_extensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
#if 0
        "VK_KHR_portability_subset",
#endif
    };

    static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
        VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
        VkDebugUtilsMessageTypeFlagsEXT message_type,
        const VkDebugUtilsMessengerCallbackDataEXT *callback_data,
        void *user_data)
    {
        static const char *message_type_strs[] = {"General", "Validation", "Performance"};
        const char *message_type_str = nullptr;

        switch (message_type)
        {
        case VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT:
            message_type_str = message_type_strs[0];
            break;
        case VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT:
            message_type_str = message_type_strs[1];
            break;
        case VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT:
            message_type_str = message_type_strs[2];
            break;
        default:
            break;
        }

        if (message_severity < VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
            return VK_FALSE;

        switch (message_severity)
        {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
            SPDLOG_TRACE("[{}] {}", message_type_str, callback_data->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
            SPDLOG_INFO("[{}] {}", message_type_str, callback_data->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
            SPDLOG_WARN("[{}] {}", message_type_str, callback_data->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
            SPDLOG_ERROR("[{}] {}", message_type_str, callback_data->pMessage);
            break;
        default:
            break;
        }

        return VK_FALSE;
    }

    static std::vector<char> read_file(const std::string &path)
    {
        std::ifstream file(path, std::ios::ate | std::ios::binary);

        if (!file.is_open())
        {
            SPDLOG_ERROR("cloudn't open file: {}", path);
            throw std::runtime_error("FILE_NOT_FOUND");
        }

        size_t file_size = (size_t)file.tellg();
        std::vector<char> buffer(file_size);

        file.seekg(0);
        file.read(buffer.data(), file_size);
        file.close();

        return buffer;
    }

    static void framebuffer_resized_callback(GLFWwindow *window, int width, int height)
    {
        auto app = reinterpret_cast<HelloTriangleApplication *>(glfwGetWindowUserPointer(window));
        app->m_framebuffer_resized = true;
    }

    GLFWwindow *m_window = nullptr;
    VkDebugUtilsMessengerEXT m_debug_messenger;
    VkInstance m_instance;
    VkPhysicalDevice m_physical_device = VK_NULL_HANDLE;
    VkDevice m_device;
    VkQueue m_graphics_queue;
    VkSurfaceKHR m_surface;
    VkQueue m_present_queue;
    VkSwapchainKHR m_swapchain;
    std::vector<VkImage> m_swapchain_images;
    VkFormat m_swapchain_image_format;
    VkExtent2D m_swapchain_extent;
    std::vector<VkImageView> m_swapchain_image_views;
    VkRenderPass m_render_pass;
    VkPipelineLayout m_pipeline_layout;
    VkPipeline m_graphics_pipeline;
    std::vector<VkFramebuffer> m_swapchain_framebuffers;
    VkCommandPool m_command_pool;
    std::vector<VkCommandBuffer> m_command_buffers;
    std::vector<VkSemaphore> m_image_available_semaphores;
    std::vector<VkSemaphore> m_render_finished_semaphores;
    std::vector<VkFence> m_in_flight_fences;
    std::vector<VkFence> m_images_in_flight;
    size_t m_current_frame = 0;
    bool m_framebuffer_resized = false;
};

int main()
{
    spdlog::set_level(spdlog::level::trace);
    spdlog::set_pattern("%^[%T] %v%$");

    SPDLOG_INFO("HelloVukan v{}.{}.{}", PROJECT_VERSION_MAJOR, PROJECT_VERSION_MINOR, PROJECT_VERSION_PATCH);

    HelloTriangleApplication app;

    try
    {
        app.run();
    }
    catch (const std::exception &e)
    {
        SPDLOG_CRITICAL(e.what());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}