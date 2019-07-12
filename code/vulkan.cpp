#include <iostream>
#define GLFW_INCLUDE_VULKAN
#include <cstring>

#include "vulkan.hpp"
#include <limits.h>
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

internal uint32_t
find_memory_type_according_to_requirements(gpu_t *gpu
                                           , VkMemoryPropertyFlags properties
                                           , VkMemoryRequirements memory_requirements)
{
    VkPhysicalDeviceMemoryProperties mem_properties;
    vkGetPhysicalDeviceMemoryProperties(gpu->hardware
                                        , &mem_properties);

    for (uint32_t i = 0
             ; i < mem_properties.memoryTypeCount
             ; ++i)
    {
        if (memory_requirements.memoryTypeBits & (1 << i)
            && (mem_properties.memoryTypes[i].propertyFlags & properties) == properties)
        {
            return(i);
        }
    }
	    
    OUTPUT_DEBUG_LOG("%s\n", "failed to find suitable memory type");
    assert(false);
    return(0);
}
	
void
allocate_gpu_memory(VkMemoryPropertyFlags properties
                    , VkMemoryRequirements memory_requirements
                    , gpu_t *gpu
                    , VkDeviceMemory *dest_memory)
{
    VkMemoryAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = memory_requirements.size;
    alloc_info.memoryTypeIndex = find_memory_type_according_to_requirements(gpu
                                                                            , properties
                                                                            , memory_requirements);

    VK_CHECK(vkAllocateMemory(gpu->logical_device
                              , &alloc_info
                              , nullptr
                              , dest_memory));
}

void
gpu_t::find_queue_families(VkSurfaceKHR *surface)
{
    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(hardware
                                             , &queue_family_count
                                             , nullptr);

    VkQueueFamilyProperties *queue_properties = (VkQueueFamilyProperties *)allocate_stack(sizeof(VkQueueFamilyProperties) * queue_family_count
                                                                                          , alignment_t(1)
                                                                                          , "queue_family_list_allocation");
    vkGetPhysicalDeviceQueueFamilyProperties(hardware
                                             , &queue_family_count
                                             , queue_properties);

    for (uint32_t i = 0
             ; i < queue_family_count
             ; ++i)
    {
        if (queue_properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT && queue_properties[i].queueCount > 0)
        {
            queue_families.graphics_family = i;
        }

        VkBool32 present_support = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(hardware
                                             , i
                                             , *surface
                                             , &present_support);
	
        if (queue_properties[i].queueCount > 0 && present_support)
        {
            queue_families.present_family = i;
        }
	
        if (queue_families.complete())
        {
            break;
        }
    }

    pop_stack();
}

struct instance_create_validation_layer_params_t
{
    bool r_enable;
    uint32_t o_layer_count;
    const char **o_layer_names;
};

struct instance_create_extension_params_t
{
    uint32_t r_extension_count;
    const char **r_extension_names;
};

// TODO(luc) : make validation layers truly optional, enable / disable when requested
internal void
init_instance(VkInstance *instance
              , VkApplicationInfo *app_info
              , instance_create_validation_layer_params_t *validation_params
              , instance_create_extension_params_t *extension_params)
{
    VkInstanceCreateInfo instance_info = {};
    instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_info.pApplicationInfo = app_info;

    uint32_t layer_count;
    vkEnumerateInstanceLayerProperties(&layer_count
                                       , nullptr);

    VkLayerProperties *properties = (VkLayerProperties *)allocate_stack(sizeof(VkLayerProperties) * layer_count
                                                                        , alignment_t(1)
                                                                        , "validation_layer_list_allocation");
    vkEnumerateInstanceLayerProperties(&layer_count
                                       , properties);

    for (uint32_t r = 0; r < validation_params->o_layer_count; ++r)
    {
        bool found_layer = false;
        for (uint32_t l = 0; l < layer_count; ++l)
        {
            if (!strcmp(properties[l].layerName, validation_params->o_layer_names[r])) found_layer = true;
        }

        if (!found_layer) assert(false);
    }

    // if found then add to the instance information
    instance_info.enabledLayerCount = validation_params->o_layer_count;
    instance_info.ppEnabledLayerNames = validation_params->o_layer_names;

    // get extensions needed

    instance_info.enabledExtensionCount = extension_params->r_extension_count;
    instance_info.ppEnabledExtensionNames = extension_params->r_extension_names;

    VK_CHECK(vkCreateInstance(&instance_info
                              , nullptr
                              , instance)
             , "failed to create instance\n");

    pop_stack();
}

internal VKAPI_ATTR VkBool32 VKAPI_CALL
vulkan_debug_proc(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity
                  , VkDebugUtilsMessageTypeFlagsEXT message_type
                  , const VkDebugUtilsMessengerCallbackDataEXT *message_data
                  , void *user_data)
{
    //	OUTPUT_DEBUG_LOG("validation layer - %s\n", message_data->pMessage);
    std::cout << "validation layer - " << message_data->pMessage << std::endl;

    //	OUTPUT_DEBUG_LOG_VALIDATION("validation_t layer_t > %s\n", message_data->pMessage);

    return(VK_FALSE);
}

internal void
init_debug_messenger(VkInstance *instance
                     , VkDebugUtilsMessengerEXT *messenger)
{
    // setup debugger
    VkDebugUtilsMessengerCreateInfoEXT debug_info = {};
    debug_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    debug_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
        | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
        | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    debug_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
        | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
        | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    debug_info.pfnUserCallback = vulkan_debug_proc;
    debug_info.pUserData = nullptr;

    PFN_vkCreateDebugUtilsMessengerEXT vk_create_debug_utils_messenger = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(*instance, "vkCreateDebugUtilsMessengerEXT");
    assert(vk_create_debug_utils_messenger != nullptr);
    VK_CHECK(vk_create_debug_utils_messenger(*instance, &debug_info, nullptr, messenger));
}

internal void
get_swapchain_support(VkSurfaceKHR *surface
                      , gpu_t *gpu)
{
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu->hardware, *surface, &gpu->swapchain_support.capabilities);
    vkGetPhysicalDeviceSurfaceFormatsKHR(gpu->hardware, *surface, &gpu->swapchain_support.available_formats_count, nullptr);

    if (gpu->swapchain_support.available_formats_count != 0)
    {
        gpu->swapchain_support.available_formats = (VkSurfaceFormatKHR *)allocate_stack(sizeof(VkSurfaceFormatKHR) * gpu->swapchain_support.available_formats_count
                                                                                        , alignment_t(1)
                                                                                        , "surface_format_list_allocation");
        vkGetPhysicalDeviceSurfaceFormatsKHR(gpu->hardware
                                             , *surface
                                             , &gpu->swapchain_support.available_formats_count
                                             , gpu->swapchain_support.available_formats);
    }

    vkGetPhysicalDeviceSurfacePresentModesKHR(gpu->hardware, *surface, &gpu->swapchain_support.available_present_modes_count, nullptr);
    if (gpu->swapchain_support.available_present_modes_count != 0)
    {
        gpu->swapchain_support.available_present_modes = (VkPresentModeKHR *)allocate_stack(sizeof(VkPresentModeKHR) * gpu->swapchain_support.available_present_modes_count
                                                                                            , alignment_t(1)
                                                                                            , "surface_present_mode_list_allocation");
        vkGetPhysicalDeviceSurfacePresentModesKHR(gpu->hardware
                                                  , *surface
                                                  , &gpu->swapchain_support.available_present_modes_count
                                                  , gpu->swapchain_support.available_present_modes);
    }
}
    
struct physical_device_extensions_params_t
{
    uint32_t r_extension_count;
    const char **r_extension_names;
};

internal bool
check_if_physical_device_supports_extensions(physical_device_extensions_params_t *extension_params
                                             , VkPhysicalDevice gpu)
{
    uint32_t extension_count;
    vkEnumerateDeviceExtensionProperties(gpu
                                         , nullptr
                                         , &extension_count
                                         , nullptr);

    VkExtensionProperties *extension_properties = (VkExtensionProperties *)allocate_stack(sizeof(VkExtensionProperties) * extension_count
                                                                                          , alignment_t(1)
                                                                                          , "gpu_extension_properties_list_allocation");
    vkEnumerateDeviceExtensionProperties(gpu
                                         , nullptr
                                         , &extension_count
                                         , extension_properties);
    
    uint32_t required_extensions_left = extension_params->r_extension_count;
    for (uint32_t i = 0
             ; i < extension_count && required_extensions_left > 0
             ; ++i)
    {
        for (uint32_t j = 0
                 ; j < extension_params->r_extension_count
                 ; ++j)
        {
            if (!strcmp(extension_properties[i].extensionName, extension_params->r_extension_names[j]))
            {
                --required_extensions_left;
            }
        }
    }
    pop_stack();

    return(!required_extensions_left);
}
    
internal bool
check_if_physical_device_is_suitable(physical_device_extensions_params_t *extension_params
                                     , VkSurfaceKHR *surface
                                     , gpu_t *gpu)
{
    gpu->find_queue_families(surface);

    VkPhysicalDeviceProperties device_properties;
    vkGetPhysicalDeviceProperties(gpu->hardware
                                  , &device_properties);
    
    VkPhysicalDeviceFeatures device_features;
    vkGetPhysicalDeviceFeatures(gpu->hardware
                                , &device_features);

    bool swapchain_supported = check_if_physical_device_supports_extensions(extension_params
                                                                            , gpu->hardware);

    bool swapchain_usable = false;
    if (swapchain_supported)
    {
        get_swapchain_support(surface, gpu);
        swapchain_usable = gpu->swapchain_support.available_formats_count && gpu->swapchain_support.available_present_modes_count;
    }

    return(swapchain_supported && swapchain_usable
           && (device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
           && gpu->queue_families.complete()
           && device_features.geometryShader
           && device_features.wideLines
           && device_features.textureCompressionBC
           && device_features.samplerAnisotropy
           && device_features.fillModeNonSolid);
}

internal void
choose_gpu(physical_device_extensions_params_t *extension_params
           , VkSurfaceKHR *surface
           , VkInstance *instance
           , gpu_t *gpu_result)
{
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(*instance
                               , &device_count
                               , nullptr);
    
    VkPhysicalDevice *devices = (VkPhysicalDevice *)allocate_stack(sizeof(VkPhysicalDevice) * device_count
                                                                   , alignment_t(1)
                                                                   , "physical_device_list_allocation");
    vkEnumeratePhysicalDevices(*instance
                               , &device_count
                               , devices);

    OUTPUT_DEBUG_LOG("available physical hardware devices count : %d\n", device_count);

    for (uint32_t i = 0
             ; i < device_count
             ; ++i)
    {
        gpu_t gpu;
        gpu.hardware = devices[i];
	
        // check if device is suitable
        if (check_if_physical_device_is_suitable(extension_params
                                                 , surface
                                                 , &gpu))
        {
            *gpu_result = gpu;

            vkGetPhysicalDeviceProperties(gpu.hardware, &gpu_result->properties);
		
            break;
        }
    }

    assert(gpu_result->hardware != VK_NULL_HANDLE);
    OUTPUT_DEBUG_LOG("%s\n", "found gpu compatible with application");
}

internal void
init_device(physical_device_extensions_params_t *gpu_extensions
            , instance_create_validation_layer_params_t *validation_layers
            , gpu_t *gpu)
{
    // create the logical device
    queue_families_t *indices = &gpu->queue_families;

    bitset32_t bitset;
    bitset.set1(indices->graphics_family);
    bitset.set1(indices->present_family);

    uint32_t unique_sets = bitset.pop_count();

    uint32_t *unique_family_indices = (uint32_t *)allocate_stack(sizeof(uint32_t) * unique_sets
                                                       , alignment_t(1)
                                                       , "unique_queue_family_indices_allocation");
    VkDeviceQueueCreateInfo *unique_queue_infos = (VkDeviceQueueCreateInfo *)allocate_stack(sizeof(VkDeviceCreateInfo) * unique_sets
                                                                                            , alignment_t(1)
                                                                                            , "unique_queue_list_allocation");

    // fill the unique_family_indices with the indices
    for (uint32_t b = 0, set_bit = 0
             ; b < 32 && set_bit < unique_sets
             ; ++b)
    {
        if (bitset.get(b))
        {
            unique_family_indices[set_bit++] = b;
        }
    }
    
    float32_t priority1 = 1.0f;
    for (uint32_t i = 0
             ; i < unique_sets
             ; ++i)
    {
        VkDeviceQueueCreateInfo queue_info = {};
        queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_info.queueFamilyIndex = unique_family_indices[i];
        queue_info.queueCount = 1;
        queue_info.pQueuePriorities = &priority1;
        unique_queue_infos[i] = queue_info;
    }

    VkPhysicalDeviceFeatures device_features = {};
    device_features.samplerAnisotropy = VK_TRUE;
    device_features.wideLines = VK_TRUE;
    device_features.geometryShader = VK_TRUE;
    device_features.fillModeNonSolid = VK_TRUE;
	
    VkDeviceCreateInfo device_info = {};
    device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_info.pQueueCreateInfos = unique_queue_infos;
    device_info.queueCreateInfoCount = unique_sets;
    device_info.pEnabledFeatures = &device_features;
    device_info.enabledExtensionCount = gpu_extensions->r_extension_count;
    device_info.ppEnabledExtensionNames = gpu_extensions->r_extension_names;
    device_info.ppEnabledLayerNames = validation_layers->o_layer_names;
    device_info.enabledLayerCount = validation_layers->o_layer_count;

    VK_CHECK(vkCreateDevice(gpu->hardware
                            , &device_info
                            , nullptr
                            , &gpu->logical_device));
    pop_stack();
    pop_stack();

    vkGetDeviceQueue(gpu->logical_device, gpu->queue_families.graphics_family, 0, &gpu->graphics_queue);
    vkGetDeviceQueue(gpu->logical_device, gpu->queue_families.present_family, 0, &gpu->present_queue);
}

internal VkSurfaceFormatKHR
choose_surface_format(VkSurfaceFormatKHR *available_formats
                      , uint32_t format_count)
{
    if (format_count == 1 && available_formats[0].format == VK_FORMAT_UNDEFINED)
    {
        VkSurfaceFormatKHR format;
        format.format		= VK_FORMAT_B8G8R8A8_UNORM;
        format.colorSpace	= VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    }
    for (uint32_t i = 0
             ; i < format_count
             ; ++i)
    {
        if (available_formats[i].format == VK_FORMAT_B8G8R8A8_UNORM && available_formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            return(available_formats[i]);
        }
    }

    return(available_formats[0]);
}

internal VkPresentModeKHR
choose_surface_present_mode(const VkPresentModeKHR *available_present_modes
                            , uint32_t present_modes_count)
{
    // supported by most hardware
    VkPresentModeKHR best_mode = VK_PRESENT_MODE_FIFO_KHR;
    for (uint32_t i = 0
             ; i < present_modes_count
             ; ++i)
    {
        if (available_present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
        {
            return(available_present_modes[i]);
        }
        else if (available_present_modes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR)
        {
            best_mode = available_present_modes[i];
        }
    }
    return(best_mode);
}

internal VkExtent2D
choose_swapchain_extent(GLFWwindow *window
                        , const VkSurfaceCapabilitiesKHR *capabilities)
{
    if (capabilities->currentExtent.width != std::numeric_limits<uint64_t>::max())
    {
        return(capabilities->currentExtent);
    }
    else
    {
        int32_t width, height;
        glfwGetFramebufferSize(window, &width, &height);

        VkExtent2D actual_extent	= { (uint32_t)width, (uint32_t)height };
        actual_extent.width		= MAX(capabilities->minImageExtent.width, MIN(capabilities->maxImageExtent.width, actual_extent.width));
        actual_extent.height	= MAX(capabilities->minImageExtent.height, MIN(capabilities->maxImageExtent.height, actual_extent.height));

        return(actual_extent);
    }
}

void
init_image(uint32_t width
           , uint32_t height
           , VkFormat format
           , VkImageTiling tiling
           , VkImageUsageFlags usage
           , VkMemoryPropertyFlags properties
           , uint32_t layers
           , gpu_t *gpu
           , image2d_t *dest_image
           , VkImageCreateFlags flags)
{
    VkImageCreateInfo image_info = {};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.extent.width = width;
    image_info.extent.height = height;
    image_info.extent.depth = 1;
    image_info.mipLevels = 1;
    image_info.arrayLayers = layers;
    image_info.format = format;
    image_info.tiling = tiling;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_info.usage = usage;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_info.flags = flags;

    VK_CHECK(vkCreateImage(gpu->logical_device, &image_info, nullptr, &dest_image->image));

    VkMemoryRequirements mem_requirements = {};
    vkGetImageMemoryRequirements(gpu->logical_device
                                 , dest_image->image
                                 , &mem_requirements);

    allocate_gpu_memory(properties, mem_requirements, gpu, &dest_image->device_memory);

    vkBindImageMemory(gpu->logical_device, dest_image->image, dest_image->device_memory, 0);

    dest_image->format;
}
    
void
init_image_view(VkImage *image
                , VkFormat format
                , VkImageAspectFlags aspect_flags
                , gpu_t *gpu
                , VkImageView *dest_image_view
                , VkImageViewType type
                , uint32_t layers)
{
    VkImageViewCreateInfo view_info			= {};
    view_info.sType					= VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image					= *image;
    view_info.viewType				= type;
    view_info.format				= format;
    view_info.subresourceRange.aspectMask		= aspect_flags;
    view_info.subresourceRange.baseMipLevel		= 0;
    view_info.subresourceRange.levelCount		= 1;
    view_info.subresourceRange.baseArrayLayer	= 0;
    view_info.subresourceRange.layerCount		= layers;

    VK_CHECK(vkCreateImageView(gpu->logical_device, &view_info, nullptr, dest_image_view));
}

void
init_image_sampler(VkFilter mag_filter
                   , VkFilter min_filter
                   , VkSamplerAddressMode u_sampler_address_mode
                   , VkSamplerAddressMode v_sampler_address_mode
                   , VkSamplerAddressMode w_sampler_address_mode
                   , VkBool32 anisotropy_enable
                   , uint32_t max_anisotropy
                   , VkBorderColor clamp_border_color
                   , VkBool32 compare_enable
                   , VkCompareOp compare_op
                   , VkSamplerMipmapMode mipmap_mode
                   , float32_t mip_lod_bias
                   , float32_t min_lod
                   , float32_t max_lod
                   , gpu_t *gpu
                   , VkSampler *dest_sampler)
{
    VkSamplerCreateInfo sampler_info = {};
    sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_info.magFilter = mag_filter;
    sampler_info.minFilter = min_filter;
    sampler_info.addressModeU = u_sampler_address_mode;
    sampler_info.addressModeV = v_sampler_address_mode;
    sampler_info.addressModeW = w_sampler_address_mode;
    sampler_info.anisotropyEnable = anisotropy_enable;
    sampler_info.maxAnisotropy = max_anisotropy;
    sampler_info.borderColor = clamp_border_color; // when clamping
    sampler_info.compareEnable = compare_enable;
    sampler_info.compareOp = compare_op;
    sampler_info.mipmapMode = mipmap_mode;
    sampler_info.mipLodBias = mip_lod_bias;
    sampler_info.minLod = min_lod;
    sampler_info.maxLod = max_lod;

    VK_CHECK(vkCreateSampler(gpu->logical_device, &sampler_info, nullptr, dest_sampler));
}

void
begin_command_buffer(VkCommandBuffer *command_buffer
                     , VkCommandBufferUsageFlags usage_flags
                     , VkCommandBufferInheritanceInfo *inheritance)
{
    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = usage_flags;
    begin_info.pInheritanceInfo = inheritance;

    vkBeginCommandBuffer(*command_buffer
                         , &begin_info);
}

void
allocate_command_buffers(VkCommandPool *command_pool_source
                         , VkCommandBufferLevel level
                         , gpu_t *gpu
                         , const memory_buffer_view_t<VkCommandBuffer> &command_buffers)
{
    VkCommandBufferAllocateInfo allocate_info = {};
    allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocate_info.level = level;
    allocate_info.commandPool = *command_pool_source;
    allocate_info.commandBufferCount = command_buffers.count;

    vkAllocateCommandBuffers(gpu->logical_device
                             , &allocate_info
                             , command_buffers.buffer);
}
    
void
submit(const memory_buffer_view_t<VkCommandBuffer> &command_buffers
       , const memory_buffer_view_t<VkSemaphore> &wait_semaphores
       , const memory_buffer_view_t<VkSemaphore> &signal_semaphores
       , const memory_buffer_view_t<VkPipelineStageFlags> &wait_stages
       , VkFence *fence
       , VkQueue *queue)
{
    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = command_buffers.count;
    submit_info.pCommandBuffers = command_buffers.buffer;

    submit_info.waitSemaphoreCount = wait_semaphores.count;
    submit_info.pWaitSemaphores = wait_semaphores.buffer;
    submit_info.pWaitDstStageMask = wait_stages.buffer;

    submit_info.signalSemaphoreCount = signal_semaphores.count;
    submit_info.pSignalSemaphores = signal_semaphores.buffer;

    vkQueueSubmit(*queue, 1, &submit_info, *fence);
}

internal bool
has_stencil_component(VkFormat format)
{
    return(format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT);
}

void
init_single_use_command_buffer(VkCommandPool *command_pool
                               , gpu_t *gpu
                               , VkCommandBuffer *dst)
{
    allocate_command_buffers(command_pool
                             , VK_COMMAND_BUFFER_LEVEL_PRIMARY
                             , gpu
                             , memory_buffer_view_t<VkCommandBuffer>{1, dst});

    begin_command_buffer(dst
                         , VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
                         , nullptr);
}

void
destroy_single_use_command_buffer(VkCommandBuffer *command_buffer
                                  , VkCommandPool *command_pool
                                  , gpu_t *gpu)
{
    end_command_buffer(command_buffer);

    VkFence null_fence = VK_NULL_HANDLE;
    submit(memory_buffer_view_t<VkCommandBuffer>{1, command_buffer}
               , null_buffer<VkSemaphore>()
               , null_buffer<VkSemaphore>()
               , null_buffer<VkPipelineStageFlags>()
               , &null_fence
               , &gpu->graphics_queue);

    vkQueueWaitIdle(gpu->graphics_queue);

    free_command_buffer(memory_buffer_view_t<VkCommandBuffer>{1, command_buffer}, command_pool, gpu);
}
    
void
transition_image_layout(VkImage *image
                        , VkFormat format
                        , VkImageLayout old_layout
                        , VkImageLayout new_layout
                        , VkCommandPool *graphics_command_pool
                        , gpu_t *gpu)
{
    VkCommandBuffer single_use;
    init_single_use_command_buffer(graphics_command_pool, gpu, &single_use);
	
    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = old_layout;
    barrier.newLayout = new_layout;

    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
   
    barrier.image				= *image;
    barrier.subresourceRange.aspectMask	= VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel	= 0;
    barrier.subresourceRange.levelCount	= 1;
    barrier.subresourceRange.baseArrayLayer	= 0;
    barrier.subresourceRange.layerCount	= 1;

    VkPipelineStageFlags source_stage;
    VkPipelineStageFlags destination_stage;

    if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
    {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destination_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destination_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
    {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT
            | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        source_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destination_stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    }
    else
    {
        OUTPUT_DEBUG_LOG("%s\n", "unsupported layout transition");
        assert(false);
    }

    if (new_layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
    {
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        if (has_stencil_component(format))
        {
            barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }
    }
    else
    {
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    }

    vkCmdPipelineBarrier(single_use
                         , source_stage
                         , destination_stage
                         , 0
                         , 0
                         , nullptr
                         , 0
                         , nullptr
                         , 1
                         , &barrier);

    destroy_single_use_command_buffer(&single_use
                                      , graphics_command_pool
                                      , gpu);
}

void
copy_buffer_into_image(gpu_buffer_t *src_buffer
                       , image2d_t *dst_image
                       , uint32_t width
                       , uint32_t height
                       , VkCommandPool *command_pool
                       , gpu_t *gpu)
{
    VkCommandBuffer command_buffer;
    init_single_use_command_buffer(command_pool
                                   , gpu
                                   , &command_buffer);

    VkBufferImageCopy region	= {};
    region.bufferOffset		= 0;
    region.bufferRowLength		= 0;
    region.bufferImageHeight	= 0;

    region.imageSubresource.aspectMask	= VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel	= 0;
    region.imageSubresource.baseArrayLayer	= 0;
    region.imageSubresource.layerCount	= 1;

    region.imageOffset = {0, 0, 0};
    region.imageExtent = { width, height, 1 };

    vkCmdCopyBufferToImage(command_buffer
                           , src_buffer->buffer
                           , dst_image->image
                           , VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
                           , 1
                           , &region);

    destroy_single_use_command_buffer(&command_buffer
                                      , command_pool
                                      , gpu);
}

void
copy_buffer(gpu_buffer_t *src_buffer
            , gpu_buffer_t *dst_buffer
            , VkCommandPool *command_pool
            , gpu_t *gpu)
{
    VkCommandBuffer command_buffer;
    init_single_use_command_buffer(command_pool
                                   , gpu
                                   , &command_buffer);

    VkBufferCopy region = {};
    region.size = src_buffer->size;
    vkCmdCopyBuffer(command_buffer
                    , src_buffer->buffer
                    , dst_buffer->buffer
                    , 1
                    , &region);

    destroy_single_use_command_buffer(&command_buffer
                                      , command_pool
                                      , gpu);
}

void
invoke_staging_buffer_for_device_local_buffer(memory_byte_buffer_t items
                                              , VkBufferUsageFlags usage
                                              , VkCommandPool *transfer_command_pool
                                              , gpu_buffer_t *dst_buffer
                                              , gpu_t *gpu)
{
    VkDeviceSize buffer_size = items.size;
	
    gpu_buffer_t staging_buffer;
    staging_buffer.size = buffer_size;

    init_buffer(buffer_size
                , VK_BUFFER_USAGE_TRANSFER_SRC_BIT
                , VK_SHARING_MODE_EXCLUSIVE
                , VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
                , gpu
                , &staging_buffer);

    mapped_gpu_memory_t mapped_memory = staging_buffer.construct_map();
    mapped_memory.begin(gpu);
    mapped_memory.fill(items);
    mapped_memory.end(gpu);

    init_buffer(buffer_size
                , VK_BUFFER_USAGE_TRANSFER_SRC_BIT | usage
                , VK_SHARING_MODE_EXCLUSIVE
                , VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
                , gpu
                , dst_buffer);

    copy_buffer(&staging_buffer
                , dst_buffer
                , transfer_command_pool
                , gpu);

    vkDestroyBuffer(gpu->logical_device, staging_buffer.buffer, nullptr);
    vkFreeMemory(gpu->logical_device, staging_buffer.memory, nullptr);
}
    
internal void
init_swapchain(GLFWwindow *window
               , VkSurfaceKHR *surface
               , gpu_t *gpu
               , swapchain_t *swapchain)
{
    swapchain_details_t *swapchain_details = &gpu->swapchain_support;
    VkSurfaceFormatKHR surface_format = choose_surface_format(swapchain_details->available_formats, swapchain_details->available_formats_count);
    VkExtent2D surface_extent = choose_swapchain_extent(window, &swapchain_details->capabilities);
    VkPresentModeKHR present_mode = choose_surface_present_mode(swapchain_details->available_present_modes, swapchain_details->available_present_modes_count);

    // add 1 to the minimum images supported in the swapchain
    uint32_t image_count = swapchain_details->capabilities.minImageCount + 1;
    if (image_count > swapchain_details->capabilities.maxImageCount)
    {
        image_count = swapchain_details->capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR swapchain_info;
    swapchain_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchain_info.surface = *surface;
    swapchain_info.minImageCount = image_count;
    swapchain_info.imageFormat = surface_format.format;
    swapchain_info.imageColorSpace = surface_format.colorSpace;
    swapchain_info.imageExtent = surface_extent;
    swapchain_info.imageArrayLayers = 1;
    swapchain_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    uint32_t queue_family_indices[] = { (uint32_t)gpu->queue_families.graphics_family, (uint32_t)gpu->queue_families.present_family };

    if (gpu->queue_families.graphics_family != gpu->queue_families.present_family)
    {
        swapchain_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        swapchain_info.queueFamilyIndexCount = 2;
        swapchain_info.pQueueFamilyIndices = queue_family_indices;
    }
    else
    {
        swapchain_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        swapchain_info.queueFamilyIndexCount = 0;
        swapchain_info.pQueueFamilyIndices = nullptr;
    }

    swapchain_info.preTransform = swapchain_details->capabilities.currentTransform;
    swapchain_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchain_info.presentMode = present_mode;
    swapchain_info.clipped = VK_TRUE;
    swapchain_info.oldSwapchain = VK_NULL_HANDLE;

    VK_CHECK(vkCreateSwapchainKHR(gpu->logical_device, &swapchain_info, nullptr, &swapchain->swapchain));

    vkGetSwapchainImagesKHR(gpu->logical_device, swapchain->swapchain, &image_count, nullptr);

    allocate_memory_buffer(swapchain->imgs, image_count);
	
    vkGetSwapchainImagesKHR(gpu->logical_device, swapchain->swapchain, &image_count, swapchain->imgs.buffer);
	
    swapchain->extent = surface_extent;
    swapchain->format = surface_format.format;
    swapchain->present_mode = present_mode;

    allocate_memory_buffer(swapchain->views, image_count);
	
    for (uint32_t i = 0
             ; i < image_count
             ; ++i)
    {
        VkImage *image = &swapchain->imgs[i];

        init_image_view(image
                        , swapchain->format
                        , VK_IMAGE_ASPECT_COLOR_BIT
                        , gpu
                        , &swapchain->views[i]
                        , VK_IMAGE_VIEW_TYPE_2D
                        , 1);
    }
}
    
void
init_render_pass(const memory_buffer_view_t<VkAttachmentDescription> &attachment_descriptions
                 , const memory_buffer_view_t<VkSubpassDescription> &subpass_descriptions
                 , const memory_buffer_view_t<VkSubpassDependency> &subpass_dependencies
                 , gpu_t *gpu
                 , render_pass_t *dest_render_pass)
{
    VkRenderPassCreateInfo render_pass_info	= {};
    render_pass_info.sType			= VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_info.attachmentCount	= attachment_descriptions.count;
    render_pass_info.pAttachments		= attachment_descriptions.buffer;
    render_pass_info.subpassCount		= subpass_descriptions.count;
    render_pass_info.pSubpasses		= subpass_descriptions.buffer;
    render_pass_info.dependencyCount	= subpass_dependencies.count;
    render_pass_info.pDependencies		= subpass_dependencies.buffer;

    VK_CHECK(vkCreateRenderPass(gpu->logical_device, &render_pass_info, nullptr, &dest_render_pass->render_pass));
    dest_render_pass->subpass_count = subpass_descriptions.count;
}

// find gpu supported depth format
internal VkFormat
find_supported_format(const VkFormat *candidates
                      , uint32_t candidate_size
                      , VkImageTiling tiling
                      , VkFormatFeatureFlags features
                      , gpu_t *gpu)
{
    for (uint32_t i = 0
             ; i < candidate_size
             ; ++i)
    {
        VkFormatProperties properties;
        vkGetPhysicalDeviceFormatProperties(gpu->hardware, candidates[i], &properties);
        if (tiling == VK_IMAGE_TILING_LINEAR && (properties.linearTilingFeatures & features) == features)
        {
            return(candidates[i]);
        }
        else if (tiling == VK_IMAGE_TILING_OPTIMAL && (properties.optimalTilingFeatures & features) == features)
        {
            return(candidates[i]);
        }
    }
    OUTPUT_DEBUG_LOG("%s\n", "failed to find supported format");
    assert(false);

    return VkFormat{};
}

internal void
find_depth_format(gpu_t *gpu)
{
    VkFormat formats[] = 
        {
            VK_FORMAT_D32_SFLOAT
            , VK_FORMAT_D32_SFLOAT_S8_UINT
            , VK_FORMAT_D24_UNORM_S8_UINT
        };
    
    gpu->supported_depth_format	= find_supported_format(formats
                                                        , 3
                                                        , VK_IMAGE_TILING_OPTIMAL
                                                        , VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
                                                        , gpu);
}
    
void
init_shader(VkShaderStageFlagBits stage_bits
            , uint32_t content_size
            , byte_t *file_contents
            , gpu_t *gpu
            , VkShaderModule *dest_shader_module)
{
    VkShaderModuleCreateInfo shader_info = {};
    shader_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shader_info.codeSize = content_size;
    shader_info.pCode = reinterpret_cast<const uint32_t *>(file_contents);

    VK_CHECK(vkCreateShaderModule(gpu->logical_device
                                  , &shader_info
                                  , nullptr
                                  , dest_shader_module));
}

void
init_pipeline_layout(const memory_buffer_view_t<VkDescriptorSetLayout> &layouts
                     , const memory_buffer_view_t<VkPushConstantRange> &ranges
                     , gpu_t *gpu
                     , VkPipelineLayout *pipeline_layout)
{
    VkPipelineLayoutCreateInfo layout_info = {};
    layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout_info.setLayoutCount = layouts.count;
    layout_info.pSetLayouts = layouts.buffer;
    layout_info.pushConstantRangeCount = ranges.count;
    layout_info.pPushConstantRanges = ranges.buffer;

    VK_CHECK(vkCreatePipelineLayout(gpu->logical_device
                                    , &layout_info
                                    , nullptr
                                    , pipeline_layout));        
}
    
void
init_pipeline_layout(memory_buffer_view_t<VkDescriptorSetLayout> *layouts
                     , memory_buffer_view_t<VkPushConstantRange> *ranges
                     , gpu_t *gpu
                     , VkPipelineLayout *pipeline_layout)
{
    VkPipelineLayoutCreateInfo layout_info = {};
    layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout_info.setLayoutCount = layouts->count;
    layout_info.pSetLayouts = layouts->buffer;
    layout_info.pushConstantRangeCount = ranges->count;
    layout_info.pPushConstantRanges = ranges->buffer;

    VK_CHECK(vkCreatePipelineLayout(gpu->logical_device
                                    , &layout_info
                                    , nullptr
                                    , pipeline_layout));
}

void
init_graphics_pipeline(memory_buffer_view_t<VkPipelineShaderStageCreateInfo> *shaders
                       , VkPipelineVertexInputStateCreateInfo *vertex_input_info
                       , VkPipelineInputAssemblyStateCreateInfo *input_assembly_info
                       , VkPipelineViewportStateCreateInfo *viewport_info
                       , VkPipelineRasterizationStateCreateInfo *rasterization_info
                       , VkPipelineMultisampleStateCreateInfo *multisample_info
                       , VkPipelineColorBlendStateCreateInfo *blend_info
                       , VkPipelineDynamicStateCreateInfo *dynamic_state_info
                       , VkPipelineDepthStencilStateCreateInfo *depth_stencil_info
                       , VkPipelineLayout *pipeline_layout
                       , render_pass_t *render_pass
                       , uint32_t subpass
                       , gpu_t *gpu
                       , VkPipeline *pipeline)
{
    VkGraphicsPipelineCreateInfo pipeline_info = {};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = shaders->count;
    pipeline_info.pStages = shaders->buffer;
    pipeline_info.pVertexInputState = vertex_input_info;
    pipeline_info.pInputAssemblyState = input_assembly_info;
    pipeline_info.pViewportState = viewport_info;
    pipeline_info.pRasterizationState = rasterization_info;
    pipeline_info.pMultisampleState = multisample_info;
    pipeline_info.pDepthStencilState = depth_stencil_info;
    pipeline_info.pColorBlendState = blend_info;
    pipeline_info.pDynamicState = dynamic_state_info;

    pipeline_info.layout = *pipeline_layout;
    pipeline_info.renderPass = render_pass->render_pass;
    pipeline_info.subpass = subpass;

    pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
    pipeline_info.basePipelineIndex = -1;

    VK_CHECK (vkCreateGraphicsPipelines(gpu->logical_device
                                        , VK_NULL_HANDLE
                                        , 1
                                        , &pipeline_info
                                        , nullptr
                                        , pipeline) != VK_SUCCESS);
}

void
allocate_command_pool(uint32_t queue_family_index
                      , gpu_t *gpu
                      , VkCommandPool *command_pool)
{
    VkCommandPoolCreateInfo pool_info = {};
    pool_info.sType			= VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.queueFamilyIndex	= queue_family_index;
    pool_info.flags			= VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    VK_CHECK(vkCreateCommandPool(gpu->logical_device, &pool_info, nullptr, command_pool));
}

void
init_framebuffer_attachment(uint32_t width
                            , uint32_t height
                            , VkFormat format
                            , VkImageUsageFlags usage
                            , gpu_t *gpu
                            , image2d_t *attachment
                            // for_t cubemap targets
                            , uint32_t layers
                            , VkImageCreateFlags create_flags
                            , VkImageViewType image_view_type)
{
    init_image(width
               , height
               , format
               , VK_IMAGE_TILING_OPTIMAL
               , usage
               , VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
               , layers
               , gpu
               , attachment
               , create_flags);

    VkImageAspectFlags aspect_flags;
    if (usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) aspect_flags = VK_IMAGE_ASPECT_COLOR_BIT;
    if (usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) aspect_flags = VK_IMAGE_ASPECT_DEPTH_BIT;
	
    init_image_view(&attachment->image
                    , format
                    , aspect_flags
                    , gpu
                    , &attachment->image_view
                    , image_view_type
                    , layers);
}
    
void
init_framebuffer(render_pass_t *compatible_render_pass
                 , uint32_t width
                 , uint32_t height
                 , uint32_t layer_count
                 , gpu_t *gpu
                 , framebuffer_t *framebuffer)
{
    memory_buffer_view_t<VkImageView> image_view_attachments;
	
    image_view_attachments.count = framebuffer->color_attachments.count;
    image_view_attachments.buffer = (VkImageView *)allocate_stack(sizeof(VkImageView) * image_view_attachments.count);
	
    for (uint32_t i = 0
             ; i < image_view_attachments.count
             ; ++i)
    {
        VkImageView *image = &framebuffer->color_attachments.buffer[i];
        image_view_attachments.buffer[i] = *image;
    }

    if (framebuffer->depth_attachment != VK_NULL_HANDLE)
    {
        VkImageView *depth_image = &framebuffer->depth_attachment;
        extend_stack_top(sizeof(VkImageView));
        image_view_attachments.buffer[image_view_attachments.count++] = *depth_image;
    }
	
    VkFramebufferCreateInfo fbo_info	= {};
    fbo_info.sType				= VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fbo_info.renderPass			= compatible_render_pass->render_pass;
    fbo_info.attachmentCount		= image_view_attachments.count;
    fbo_info.pAttachments			= image_view_attachments.buffer;
    fbo_info.width				= width;
    fbo_info.height				= height;
    fbo_info.layers				= layer_count;

    VK_CHECK(vkCreateFramebuffer(gpu->logical_device, &fbo_info, nullptr, &framebuffer->framebuffer));
}

VkDescriptorSet
allocate_descriptor_set(VkDescriptorSetLayout *layout
                        , gpu_t *gpu
                        , VkDescriptorPool *descriptor_pool)
{
    VkDescriptorSet result;
	
    VkDescriptorSetAllocateInfo alloc_info	= {};
    alloc_info.sType		= VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool	= *descriptor_pool;
    alloc_info.descriptorSetCount	= 1;
    alloc_info.pSetLayouts		= layout;

    VK_CHECK(vkAllocateDescriptorSets(gpu->logical_device, &alloc_info, &result));

    return(result);
}
    
void
allocate_descriptor_sets(memory_buffer_view_t<VkDescriptorSet> &descriptor_sets
                         , const memory_buffer_view_t<VkDescriptorSetLayout> &layouts
                         , gpu_t *gpu
                         , VkDescriptorPool *descriptor_pool)
{
    VkDescriptorSetAllocateInfo alloc_info	= {};
    alloc_info.sType		= VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool	= *descriptor_pool;
    alloc_info.descriptorSetCount	= layouts.count;
    alloc_info.pSetLayouts		= layouts.buffer;

    VK_CHECK(vkAllocateDescriptorSets(gpu->logical_device, &alloc_info, descriptor_sets.buffer));
}

void
init_descriptor_pool(const memory_buffer_view_t<VkDescriptorPoolSize> &sizes
                     , uint32_t max_sets
                     , gpu_t *gpu
                     , VkDescriptorPool *pool)
{
    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.poolSizeCount = sizes.count;
    pool_info.pPoolSizes = sizes.buffer;

    pool_info.maxSets = max_sets;

    VK_CHECK(vkCreateDescriptorPool(gpu->logical_device, &pool_info, nullptr, pool));
}
    
void
init_descriptor_pool(const memory_buffer_view_t<VkDescriptorPoolSize> &sizes
                     , uint32_t max_sets
                     , gpu_t *gpu
                     , descriptor_pool_t *pool)
{
    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.poolSizeCount = sizes.count;
    pool_info.pPoolSizes = sizes.buffer;

    pool_info.maxSets = max_sets;

    VK_CHECK(vkCreateDescriptorPool(gpu->logical_device, &pool_info, nullptr, &pool->pool));
}

void
init_descriptor_set_layout(const memory_buffer_view_t<VkDescriptorSetLayoutBinding> &bindings
                           , gpu_t *gpu
                           , VkDescriptorSetLayout *dst)
{
    VkDescriptorSetLayoutCreateInfo layout_info	= {};
    layout_info.sType				= VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.bindingCount			= bindings.count;
    layout_info.pBindings				= bindings.buffer;
	
    VK_CHECK(vkCreateDescriptorSetLayout(gpu->logical_device, &layout_info, nullptr, dst));
}
    
void
update_descriptor_sets(const memory_buffer_view_t<VkWriteDescriptorSet> &writes
                       , gpu_t *gpu)
{
    vkUpdateDescriptorSets(gpu->logical_device
                           , writes.count
                           , writes.buffer
                           , 0
                           , nullptr);
}
    
void
init_buffer(VkDeviceSize buffer_size
            , VkBufferUsageFlags usage
            , VkSharingMode sharing_mode
            , VkMemoryPropertyFlags memory_properties
            , gpu_t *gpu
            , gpu_buffer_t *dest_buffer)
{
    dest_buffer->size = buffer_size;
	
    VkBufferCreateInfo buffer_info	= {};
    buffer_info.sType	= VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size	= buffer_size;
    buffer_info.usage	= usage;
    buffer_info.sharingMode	= sharing_mode;
    buffer_info.flags	= 0;

    VK_CHECK(vkCreateBuffer(gpu->logical_device, &buffer_info, nullptr, &dest_buffer->buffer));

    VkMemoryRequirements mem_requirements;
    vkGetBufferMemoryRequirements(gpu->logical_device, dest_buffer->buffer, &mem_requirements);

    allocate_gpu_memory(memory_properties, mem_requirements, gpu, &dest_buffer->memory);
	
    vkBindBufferMemory(gpu->logical_device, dest_buffer->buffer, dest_buffer->memory, 0);
}

void
update_gpu_buffer(gpu_buffer_t *dst, void *data, uint32_t size, uint32_t offset, VkPipelineStageFlags stage, VkAccessFlags access, VkCommandBuffer *queue)
{
    VkBufferMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrier.srcAccessMask = access;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.buffer = dst->buffer;
    barrier.offset = offset;
    barrier.size = size;
    vkCmdPipelineBarrier(*queue,
                         stage,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0,
                         0,
                         nullptr,
                         1,
                         &barrier,
                         0,
                         nullptr);

    vkCmdUpdateBuffer(*queue, dst->buffer, offset, size, data);
    
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = access;
    vkCmdPipelineBarrier(*queue,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         stage,
                         0,
                         0,
                         nullptr,
                         1,
                         &barrier,
                         0,
                         nullptr);
}

void
command_buffer_begin_render_pass(render_pass_t *render_pass
                                 , framebuffer_t *fbo
                                 , VkRect2D render_area
                                 , const memory_buffer_view_t<VkClearValue> &clear_colors
                                 , VkSubpassContents subpass_contents
                                 , VkCommandBuffer *command_buffer)
{
    VkRenderPassBeginInfo render_pass_info = {};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_info.pNext = nullptr;
    render_pass_info.renderPass = render_pass->render_pass;

    render_pass_info.framebuffer = fbo->framebuffer;
    render_pass_info.renderArea = render_area;

    render_pass_info.clearValueCount = clear_colors.count;
    render_pass_info.pClearValues = clear_colors.buffer;

    vkCmdBeginRenderPass(*command_buffer
                         , &render_pass_info
                         , subpass_contents);
}

void
command_buffer_next_subpass(VkCommandBuffer *cmdbuf
                            , VkSubpassContents contents)
{
    vkCmdNextSubpass(*cmdbuf
                     , contents);
}

VkResult
present(const memory_buffer_view_t<VkSemaphore> &signal_semaphores
        , const memory_buffer_view_t<VkSwapchainKHR> &swapchains
        , uint32_t *image_index
        , VkQueue *present_queue)
{
    VkPresentInfoKHR present_info = {};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = signal_semaphores.count;
    present_info.pWaitSemaphores = signal_semaphores.buffer;

    present_info.swapchainCount = swapchains.count;
    present_info.pSwapchains = swapchains.buffer;
    present_info.pImageIndices = image_index;

    return(vkQueuePresentKHR(*present_queue
                             , &present_info));
}
    
void
init_vulkan_state(vulkan_state_t *state
                  , GLFWwindow *window)
{
    init_manager();
	
    // initialize instance
    persist constexpr uint32_t layer_count = 1;
    const char *layer_names[layer_count] = { "VK_LAYER_LUNARG_standard_validation" };

    instance_create_validation_layer_params_t validation_params = {};
    validation_params.r_enable = true;
    validation_params.o_layer_count = layer_count;
    validation_params.o_layer_names = layer_names;

    uint32_t extension_count;
    const char **extension_names = glfwGetRequiredInstanceExtensions(&extension_count);
    const char **total_extension_buffer = (const char **)allocate_stack(sizeof(const char *) * (extension_count + 4)
                                                                        , alignment_t(1)
                                                                        , "vulkan_instanc_extension_names_list_allocation");
    memcpy(total_extension_buffer, extension_names, sizeof(const char *) * extension_count);
    total_extension_buffer[extension_count++] = "VK_EXT_debug_utils";
    total_extension_buffer[extension_count++] = "VK_EXT_debug_report";
	
    instance_create_extension_params_t extension_params = {};
    extension_params.r_extension_count = extension_count;
    extension_params.r_extension_names = total_extension_buffer;

    VkApplicationInfo app_info = {};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "vulkan engine";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "No Engine";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_0;
	
    init_instance(&state->instance
                  , &app_info
                  , &validation_params
                  , &extension_params);
	
    pop_stack();

    init_debug_messenger(&state->instance
                         , &state->debug_messenger);

    // create the surface
    VK_CHECK(glfwCreateWindowSurface(state->instance
                                     , window
                                     , nullptr
                                     , &state->surface));

    // choose hardware and create device
    persist constexpr uint32_t gpu_extension_count = 1;
    const char *gpu_extension_names[gpu_extension_count] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    physical_device_extensions_params_t gpu_extensions = {};
    gpu_extensions.r_extension_count = gpu_extension_count;
    gpu_extensions.r_extension_names = gpu_extension_names;
    choose_gpu(&gpu_extensions	// function initializes the queue families in the GPU struct
               , &state->surface
               , &state->instance
               , &state->gpu);
    vkGetPhysicalDeviceMemoryProperties(state->gpu.hardware, &state->gpu.memory_properties);
    find_depth_format(&state->gpu);
    init_device(&gpu_extensions
                , &validation_params
                , &state->gpu);

    // create swapchain
    init_swapchain(window
                   , &state->surface
                   , &state->gpu
                   , &state->swapchain);
}

void
destroy_debug_utils_messenger_ext(VkInstance instance
                                  , VkDebugUtilsMessengerEXT messenger
                                  , const VkAllocationCallbacks *allocator)
{
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr)
    {
        func(instance, messenger, allocator);
    }
}
    
void
destroy_vulkan_state(vulkan_state_t *state)
{	
    vkDestroyDevice(state->gpu.logical_device, nullptr);
	
    vkDestroySurfaceKHR(state->instance, state->surface, nullptr);
	
    destroy_debug_utils_messenger_ext(state->instance, state->debug_messenger, nullptr);
    vkDestroyInstance(state->instance, nullptr);
}

void
destroy_swapchain(vulkan_state_t *state)
{
    for (uint32_t i = 0; i < state->swapchain.views.count; ++i)
    {
        vkDestroyImageView(state->gpu.logical_device, state->swapchain.views[i], nullptr);
    }
	
    vkDestroySwapchainKHR(state->gpu.logical_device, state->swapchain.swapchain, nullptr);
}
