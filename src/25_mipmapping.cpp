/*
    * Mipmap: 一种纹理映射技术，用于提高纹理质量
    * VkImage holds the mipmap data, 
    * VkSampler controls how that data is read while rendering. 
    * 
    * add: 
    *   generateMipmaps()
    * 
    * modify:
    *   createImage()
    *   createImageView()
    *   createTextureImage()
    *   transitionImageLayout()
    * 
*/
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include <iostream>
#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <chrono>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <limits>
#include <array>
#include <optional>
#include <set>
#include <unordered_map>

const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

const std::string MODEL_PATH = "../models/viking_room.obj";
const std::string TEXTURE_PATH = "../textures/viking_room.png";

//多个帧缓冲
const int MAX_FRAMES_IN_FLIGHT = 2;


const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

const std::vector<const char*> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

//C++中的宏定义
#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

//创建Debug instance
VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
    //加载VkDebugUtilsMessengerEXT函数
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    } else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

//销毁Debug instance
void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) {
        func(instance, debugMessenger, pAllocator);
    }
}

//队列族索引
struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;//图形队列族索引
    std::optional<uint32_t> presentFamily;//呈现队列族索引

    //检查队列族是否支持VK_QUEUE_GRAPHICS_BIT
    bool isComplete() {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

//交换链支持信息
struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;//表面能力
    std::vector<VkSurfaceFormatKHR> formats;//表面格式
    std::vector<VkPresentModeKHR> presentModes;//呈现模式
};

struct Vertex {
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec2 texCoord;

    //用来描述组成array的vertices
    //A vertex binding describes at which rate to load data from memory throughout the vertices array.
    static VkVertexInputBindingDescription getBindingDescription() {
        VkVertexInputBindingDescription bindingDescription{};
        // The binding parameter specifies the index of the binding in the array of bindings. 
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        //inputRate
        //option 1: VK_VERTEX_INPUT_RATE_VERTEX: move to the next data entry after each vertex
        //option 2: VK_VERTEX_INPUT_RATE_INSTANCE: move to the next data entry after each instance
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        return bindingDescription;
    }
    //用来描述如何处理每个顶点数据
    static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions() {
        std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};
        //The binding parameter tells Vulkan from which binding the per-vertex data comes.
        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        //format options
        // VK_FORMAT_R32G32_SFLOAT: vec2
        // VK_FORMAT_R64_SFLOAT: double
        // VK_FORMAT_R32G32_SINT: ivec2
        // VK_FORMAT_R8G8B8A8_UINT: uvec4
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(Vertex, pos);

        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(Vertex, color);

        attributeDescriptions[2].binding = 0;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[2].offset = offsetof(Vertex, texCoord);

        return attributeDescriptions;
    }
    
    bool operator==(const Vertex& other) const {
        return pos == other.pos && color == other.color && texCoord == other.texCoord;
    }
};

namespace std {
    template<> struct hash<Vertex> {
        size_t operator()(Vertex const& vertex) const {
            return ((hash<glm::vec3>()(vertex.pos) ^ (hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^ (hash<glm::vec2>()(vertex.texCoord) << 1);
        }
    };
}

struct UniformBufferObject {
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
};

class HelloTriangleApplication {
public:
    void run() {
        initWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }

private:
    GLFWwindow* window;//窗口句柄   
    
    VkInstance instance;//实例句柄
    VkDebugUtilsMessengerEXT debugMessenger;//调试信息句柄
    VkSurfaceKHR surface;//表面句柄

    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;//物理设备句柄,用于获取GPU信息
    VkDevice device;//逻辑设备句柄,用于和物理设备交互

    VkQueue graphicsQueue;//图形队列句柄
    VkQueue presentQueue;//呈现队列句柄

    VkSwapchainKHR swapChain;//交换链句柄
    std::vector<VkImage> swapChainImages;//交换链图像
    VkFormat swapChainImageFormat;//交换链图像格式
    VkExtent2D swapChainExtent;//交换链图像分辨率
    std::vector<VkImageView> swapChainImageViews;//交换链图像视图
    std::vector<VkFramebuffer> swapChainFramebuffers;//交换链帧缓冲

    VkRenderPass renderPass;//渲染流程句柄
    VkDescriptorSetLayout descriptorSetLayout;//描述符布局句柄
    VkPipelineLayout pipelineLayout;//管线布局句柄
    VkPipeline graphicsPipeline;//图形管线句柄

    VkCommandPool commandPool;//命令池句柄
    std::vector<VkCommandBuffer> commandBuffers;

    VkImage depthImage;//深度图像句柄
    VkDeviceMemory depthImageMemory;
    VkImageView depthImageView;//深度图像视图

    uint32_t mipLevels;//纹理图像mipmap级别
    VkImage textureImage;//纹理图像句柄
    
    VkDeviceMemory textureImageMemory;
    VkImageView textureImageView;//纹理图像视图
    VkSampler textureSampler;

    std::vector<Vertex> vertices;//顶点
    std::vector<uint32_t> indices;//索引

    VkBuffer vertexBuffer;//顶点缓冲区句柄
    VkDeviceMemory vertexBufferMemory;
    VkBuffer indexBuffer;  //索引缓冲区句柄
    VkDeviceMemory indexBufferMemory;

    std::vector<VkBuffer> uniformBuffers;
    std::vector<VkDeviceMemory> uniformBuffersMemory;
    std::vector<void*> uniformBuffersMapped;

    VkDescriptorPool descriptorPool;
    std::vector<VkDescriptorSet> descriptorSets;

    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
    VkFence inFlightFence;//栅栏句柄

    uint32_t currentFrame = 0;

    bool framebufferResized = false;

private:
    void initWindow() {
        glfwInit();//初始化GLFW

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);//不使用OpenGL
        // glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);//不可调整窗口大小

        window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);//创建窗口
        glfwSetWindowUserPointer(window, this);//将this指针存入窗口
        glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);//设置窗口回调函数

    }

    // 该回调函数在窗口大小改变时被调用，设置framebufferResized标志位，表示需要重新创建交换链
    static void framebufferResizeCallback(GLFWwindow* window, int width, int height) {
        auto app = reinterpret_cast<HelloTriangleApplication*>(glfwGetWindowUserPointer(window));
        app->framebufferResized = true;
    }

    void initVulkan() {
        // The very first thing you need to do is
        // initialize the Vulkan library by creating an instance
        createInstance();

        // 配置调试信息
        setupDebugMessenger();

        // 创建窗口表面
        createSurface();

        // 选择物理设备
        pickPhysicalDevice();

        // 逻辑设备
        createLogicalDevice();

        // 创建交换链与选择交换链图像格式
        createSwapChain();

        // 创建交换链图像视图
        createImageViews();

        // 创建渲染流程
        createRenderPass();

        // 创建描述符布局
        createDescriptorSetLayout();

        // 创建图形管线
        createGraphicsPipeline();

        // 创建命令池
        createCommandPool() ;

        // 创建深度缓冲
        createDepthResources();

        // 创建帧缓冲
        createFramebuffers();

        // 创建纹理图像
        createTextureImage();

        // 创建纹理图像访问方式
        createTextureImageView();

        // 创建纹理图像采样器
        createTextureSampler();

        // 加载模型
        loadModel();

        // 创建顶点缓冲区
        createVertexBuffer();

        // 创建索引缓冲区
        createIndexBuffer();

        // 创建全局缓冲区
        createUniformBuffers();

        // 创建描述符池
        createDescriptorPool();

        // 创建描述符集
        createDescriptorSets();

        // 创建命令缓冲
        // createCommandBuffer() ;
        createCommandBuffers();

        // 创建信号量
        createSyncObjects();

    }

    void mainLoop() {
        while (!glfwWindowShouldClose(window)) {//循环直到窗口关闭
            glfwPollEvents();//处理事件:如果有事件触发，调用对应的回调函数
            drawFrame();
        }

        vkDeviceWaitIdle(device);//等待设备空闲
    }

    // 清除关于交换链的所有资源
    void cleanupSwapChain() {
        vkDestroyImageView(device, depthImageView, nullptr);
        vkDestroyImage(device, depthImage, nullptr);
        vkFreeMemory(device, depthImageMemory, nullptr);
        for (size_t i = 0; i < swapChainFramebuffers.size(); i++) {
            vkDestroyFramebuffer(device, swapChainFramebuffers[i], nullptr);
        }

        for (size_t i = 0; i < swapChainImageViews.size(); i++) {
            vkDestroyImageView(device, swapChainImageViews[i], nullptr);
        }

        vkDestroySwapchainKHR(device, swapChain, nullptr);
    }
    void cleanup() {
        cleanupSwapChain();

        vkDestroyPipeline(device, graphicsPipeline, nullptr);
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        vkDestroyRenderPass(device, renderPass, nullptr);

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            vkDestroyBuffer(device, uniformBuffers[i], nullptr);
            vkFreeMemory(device, uniformBuffersMemory[i], nullptr);
        }

        //Set被自动销毁
        vkDestroyDescriptorPool(device, descriptorPool, nullptr);

        
        vkDestroySampler(device, textureSampler, nullptr);
        vkDestroyImageView(device, textureImageView, nullptr);

        vkDestroyImage(device, textureImage, nullptr);
        vkFreeMemory(device, textureImageMemory, nullptr);

        vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);

        vkDestroyBuffer(device, indexBuffer, nullptr);
        vkFreeMemory(device, indexBufferMemory, nullptr);

        vkDestroyBuffer(device, vertexBuffer, nullptr);
        vkFreeMemory(device, vertexBufferMemory, nullptr);

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
            vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
            vkDestroyFence(device, inFlightFences[i], nullptr);
        }

        vkDestroyCommandPool(device, commandPool, nullptr);

        vkDestroyDevice(device, nullptr);//销毁逻辑设备

        if (enableValidationLayers) {
            DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);//销毁调试信息
        }
        vkDestroySurfaceKHR(instance, surface, nullptr);//销毁表面
        vkDestroyInstance(instance, nullptr);//销毁vulkan实例

        glfwDestroyWindow(window);//销毁窗口
        
        glfwTerminate();//终止GLFW

    }

    void recreateSwapChain() {
        int width = 0, height = 0;
        glfwGetFramebufferSize(window, &width, &height);
        //处理窗口最小化
        while (width == 0 || height == 0) {
            glfwGetFramebufferSize(window, &width, &height);
            glfwWaitEvents();
        }

        vkDeviceWaitIdle(device);

        cleanupSwapChain();

        createSwapChain();
        createImageViews();
        createDepthResources();
        createFramebuffers();
    }

    // 创建实例
    void createInstance(){

        //  check validation layer support by LunarG
        if (enableValidationLayers && !checkValidationLayerSupport())
        {
            throw std::runtime_error("validation layers requested, but not available!");
        }

        // to create an instance we'll first have to
        // fill in a struct with some information about our application.
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Hello Triangle";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "No Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_0;

        // A lot of information in Vulkan is passed
        // through structs instead of function parameters
        // 它告诉Vulkan驱动程序需要使用的全局扩展和验证层
        // 全局是说它对整个程序有效，而不是特定于设备
        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;

        // glfw extensions
        auto extensions = getRequiredExtensions();
        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();

        // validation layers
        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
        if (enableValidationLayers) {
            // 在校验层启用的情况下，使用校验层
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size()); // 启用的校验层数量
            createInfo.ppEnabledLayerNames = validationLayers.data();                      // 启用的校验层名称
            // 配置调试信息
            populateDebugMessengerCreateInfo(debugCreateInfo);
            createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*) &debugCreateInfo;
        } else {
            createInfo.enabledLayerCount = 0; // 启用的校验层数量
            createInfo.pNext = nullptr;
        }

        //  create instance
        //  param1: Pointer to struct with creation info
        //  param2: Pointer to custom allocator callbacks
        //  param3: Pointer to the variable that stores the handle to the new object
        if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create instance!");
        }
    }

    // 配置调试信息
    void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
        createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        //指定回调函数处理的消息级别
        createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        //指定回调函数处理的消息类型
        createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        //指定回调函数的指针
        createInfo.pfnUserCallback = debugCallback;
    }

    void setupDebugMessenger() {
        if (!enableValidationLayers) return;

        VkDebugUtilsMessengerCreateInfoEXT createInfo;
        //配置调试信息
        populateDebugMessengerCreateInfo(createInfo);
        //创建调试对象
        if (CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS) {
            throw std::runtime_error("failed to set up debug messenger!");
        }
    }

    // 创建窗口表面
    void createSurface() {
        //surface是一个抽象的概念，它可以是windows系统的窗口，也可以是全屏窗口，甚至可以是一个X Window系统的窗口
        //surface是一个与平台相关的对象，所以需要GLFW来创建它
        //surface是一个Vulkan对象，它与窗口系统的表面相关联，以便Vulkan可以将图像输出到窗口
        //创建表面需要使用扩展VK_KHR_surface，它已经被集成到了Vulkan库中
        //但是它是一个实验性的扩展，所以需要在创建实例的时候显式地请求它
        //GLFW可以为我们处理这些细节，它提供了一个glfwCreateWindowSurface函数
        //它接受实例和窗口作为参数，并在Vulkan中创建一个表面对象
        //如果使用其他窗口系统，那么需要使用其他的扩展来创建表面对象
        if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
            throw std::runtime_error("failed to create window surface!");
        }
    }

    // 选择物理设备
    void pickPhysicalDevice(){
        // 1. 列出所有可用的物理设备
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);//获取可用的物理设备数量
        if (deviceCount == 0) {
            throw std::runtime_error("failed to find GPUs with Vulkan support!");
        }

        // 2. 获取所有可用的物理设备
        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());//获取所有可用的物理设备

        // 3. 按照是否符合要求的特性，选出第一个可用的物理设备
        for (const auto& device : devices) {
            if (isDeviceSuitable(device)) {
                physicalDevice = device;
                break;
            }
        }

        // 3. 按照优先级，选择最佳的物理设备
        // std::multimap<int, VkPhysicalDevice> candidates;
        // for(const auto& device : devices) {
        //     int score = rateDeviceSuitability(device);
        //     candidates.insert(std::make_pair(score, device));
        // }

        // if(candidates.rbegin()->first > 0) {
        //     physicalDevice = candidates.rbegin()->second;
        // } else {
        //     throw std::runtime_error("failed to find a suitable GPU!");
        // }

        // 4. 检查物理设备是否可用
        if (physicalDevice == VK_NULL_HANDLE) {
            throw std::runtime_error("failed to find a suitable GPU!");
        }
    }
    
    // 创建逻辑设备
    void createLogicalDevice(){
        // 1. 配置队列族信息
        QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

        // 2. 配置多个队列信息
        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        //有的队列族虽然具备的功能不同，但是属于同一个队列族，所以需要去重
        std::set<uint32_t> uniqueQueueFamilies = {
            indices.graphicsFamily.value(),
            indices.presentFamily.value()
        };//去重

        float queuePriority = 1.0f;//队列优先级
        for (uint32_t queueFamily : uniqueQueueFamilies) {
            VkDeviceQueueCreateInfo queueCreateInfo{};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;//队列族信息
            queueCreateInfo.queueFamilyIndex = queueFamily;//队列族索引
            queueCreateInfo.queueCount = 1;//队列数量
            queueCreateInfo.pQueuePriorities = &queuePriority;//队列优先级
            queueCreateInfos.push_back(queueCreateInfo);//队列信息
        }

        VkPhysicalDeviceFeatures deviceFeatures{};//物理设备特性
        deviceFeatures.samplerAnisotropy = VK_TRUE;//使用采样各项异性

        // 3. 配置逻辑设备信息
        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());//队列数量
        createInfo.pQueueCreateInfos = queueCreateInfos.data();//队列信息

        createInfo.pEnabledFeatures = &deviceFeatures;

        createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());//启用的扩展数量
        createInfo.ppEnabledExtensionNames = deviceExtensions.data();

        if (enableValidationLayers) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());//启用的校验层数量
            createInfo.ppEnabledLayerNames = validationLayers.data();//启用的校验层名称
        } else {
            createInfo.enabledLayerCount = 0;
        }

        // 4. 创建逻辑设备
        if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
            throw std::runtime_error("failed to create logical device!");
        }

        // 根据逻辑设备和队列族索引，获取队列句柄 graphicsQueue，presentQueue
        vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
        vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);

    }

    // 创建交换链与选择交换链图像格式
    // 表面格式：像素格式、颜色空间、深度
    // 呈现模式：显示图像到屏幕的条件
    // 交换范围：交换链图像的分辨率
    void createSwapChain() {
        // 1. 获取交换链支持信息
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);

        // 2. 选择交换链图像格式
        VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
        // 3. 选择交换链呈现模式
        VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
        // 4. 选择交换链图像分辨率
        VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

        // 5. 交换链可容纳图像数量
        uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
        // 交换链图像数量不能超过最大值，0表示没有限制
        if (swapChainSupport.capabilities.maxImageCount > 0 
            && imageCount > swapChainSupport.capabilities.maxImageCount)
        {
            imageCount = swapChainSupport.capabilities.maxImageCount;//交换链图像数量
        }

        // 6. 配置交换链信息
        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;//交换链信息
        createInfo.surface = surface;//表面句柄

        createInfo.minImageCount = imageCount;//交换链图像数量
        createInfo.imageFormat = surfaceFormat.format;//交换链图像格式
        createInfo.imageColorSpace = surfaceFormat.colorSpace;//交换链图像颜色空间
        createInfo.imageExtent = extent;//交换链图像分辨率
        createInfo.imageArrayLayers = 1;//交换链图像数组层数
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;//交换链图像使用方式

        // 7. 配置队列族索引
        QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
        uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(),
                                         indices.presentFamily.value()};//队列族索引

        // 8. 如果图形队列族和呈现队列族不同，需要进行图像布局变换
        // 我们通过图形队列在交换链图像上进行绘制操作，然后将图像提交给呈现队列来显示
        if (indices.graphicsFamily != indices.presentFamily) {
            // 可选模式：
            // VK_SHARING_MODE_EXCLUSIVE：图像只能在一个队列族中使用，必须显示地改变所有权,这种模式性能更好
            // VK_SHARING_MODE_CONCURRENT：图像可以在多个队列族中使用，不需要进行所有权转移
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;//图像协同模式
            createInfo.queueFamilyIndexCount = 2;//队列族索引数量
            createInfo.pQueueFamilyIndices = queueFamilyIndices;//队列族索引
        } else {
            // 图像布局不变
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;//图像单独模式
            createInfo.queueFamilyIndexCount = 0;//队列族索引数量
            createInfo.pQueueFamilyIndices = nullptr;//队列族索引
        }

        createInfo.preTransform = swapChainSupport.capabilities.currentTransform;//图像变换
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;//alpha通道不开启
        createInfo.presentMode = presentMode;//呈现模式
        createInfo.clipped = VK_TRUE;//裁剪

        //因为应用程序在运行过程中交换链可能会失效。
        //比如，改变窗口大小后，交换链需要重建，重建时需要之前的交换链
        createInfo.oldSwapchain = VK_NULL_HANDLE;//旧的交换链为空
        // 9. 创建交换链
        if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) != VK_SUCCESS) {
            throw std::runtime_error("failed to create swap chain!");
        }

        // 10. 获取交换链图像句柄
        vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);//获取交换链图像数量
        swapChainImages.resize(imageCount);//交换链图像
        vkGetSwapchainImagesKHR(device, swapChain, &imageCount, 
                                swapChainImages.data());//获取交换链图像

        // 11. 保存交换链图像格式和分辨率
        swapChainImageFormat = surfaceFormat.format;//交换链图像格式
        swapChainExtent = extent;//交换链图像分辨率
    }
    

    //创建交换链内图像视图
    void createImageViews() {
        //配置交换链图像视图信息
        swapChainImageViews.resize(swapChainImages.size());
        for (uint32_t i = 0; i < swapChainImages.size(); i++) {
            swapChainImageViews[i] = createImageView(swapChainImages[i], swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT,1);
        }
    }
    // 创建RenderPass
    void createRenderPass() {
        // 颜色附着
        VkAttachmentDescription colorAttachment{};
        colorAttachment.format = swapChainImageFormat;//format成员变量用于指定颜色缓冲附着的格式
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;//指定采样数量
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;//指定渲染前对颜色缓冲的操作：清除
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;//指定渲染后对颜色缓冲的操作：存储
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;//指定渲染后图像的布局：呈现

        // 深度附着
        VkAttachmentDescription depthAttachment{};
        depthAttachment.format = findDepthFormat();
        depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        //子流程:用于引用多个/一个attachment，处理attachment的内容
        VkAttachmentReference colorAttachmentRef{};
        colorAttachmentRef.attachment = 0;//指定颜色缓冲附着的索引
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference depthAttachmentRef{};
        depthAttachmentRef.attachment = 1;
        depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;//颜色缓冲附着数量
        subpass.pColorAttachments = &colorAttachmentRef;//对应 layout(location = 0) out vec4 outColor
        subpass.pDepthStencilAttachment = &depthAttachmentRef;//深度缓冲附着

        // Denpendencies:用于同步不同subpass之间的操作
         VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |       
                                VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        dependency.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | 
                                VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | 
                                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        //渲染流程
        std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};
        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        renderPassInfo.pAttachments = attachments.data();
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;

        if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
            throw std::runtime_error("failed to create render pass!");
        }

    }
    
    // 创建描述符布局
    void createDescriptorSetLayout() {
        VkDescriptorSetLayoutBinding uboLayoutBinding{};
        uboLayoutBinding.binding = 0;//绑定索引0
        uboLayoutBinding.descriptorCount = 1;
        //Uniform Buffer Object
        uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uboLayoutBinding.pImmutableSamplers = nullptr;
        //在顶点着色器阶段使用
        uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        VkDescriptorSetLayoutBinding samplerLayoutBinding{};
        samplerLayoutBinding.binding = 1;//绑定索引1
        samplerLayoutBinding.descriptorCount = 1;
        //纹理图像采样器
        samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        samplerLayoutBinding.pImmutableSamplers = nullptr;
        //在片段着色器阶段使用
        samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        std::array<VkDescriptorSetLayoutBinding, 2> bindings = {uboLayoutBinding, samplerLayoutBinding};
        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();

        if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create descriptor set layout!");
        }
    }
    //  创建图形管线
    void createGraphicsPipeline() {
        auto vertShaderCode = readFile("../shaders/vert.spv");//顶点着色器
        auto fragShaderCode = readFile("../shaders/frag.spv");//片段着色器

        // 1. 创建着色器模块
        VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);//顶点着色器模块
        VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);//片段着色器模块

        // 2. 配置顶点着色器信息
        VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
        vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;//顶点着色器信息
        vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;//在顶点着色器阶段使用
        vertShaderStageInfo.module = vertShaderModule;//顶点着色器模块
        vertShaderStageInfo.pName = "main";//着色器入口函数名称：可以使用不同的入口函数来实现不同的效果
        vertShaderStageInfo.pSpecializationInfo = nullptr;//特殊化常量信息
        // 3. 配置片段着色器信息
        VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
        fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;//片段着色器信息
        fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;//在片段着色器阶段使用
        fragShaderStageInfo.module = fragShaderModule;//片段着色器模块
        fragShaderStageInfo.pName = "main";
        fragShaderStageInfo.pSpecializationInfo = nullptr;

        // 4. 配置着色器阶段信息
        VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};//着色器阶段信息
        
        // 5. 配置顶点输入信息
        // 使用顶点缓冲区对象来存储顶点数据
        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

        auto bindingDescription = Vertex::getBindingDescription();
        auto attributeDescriptions = Vertex::getAttributeDescriptions();

        vertexInputInfo.vertexBindingDescriptionCount = 1;
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
        vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;//顶点输入绑定信息
        vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();//顶点输入属性信息

        // 6. 配置几何图元信息
        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;//输入汇总信息
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;//图元拓扑：三角形列表
        inputAssembly.primitiveRestartEnable = VK_FALSE;//启用图元重启

        // 7. 配置视口信息
        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;//视口信息
        viewportState.viewportCount = 1;//视口数量
        viewportState.scissorCount = 1;//裁剪数量

        // 8. 配置光栅化信息:将来自顶点着色器的顶点构成的几何图元转换为片段交由片段着色器着色
        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;//光栅化信息
        rasterizer.depthClampEnable = VK_FALSE;//启用深度截取:TRUE，超出深度范围的片段会被截取，而不是丢弃
        rasterizer.rasterizerDiscardEnable = VK_FALSE;//启用光栅化丢弃:TRUE，所有几何图元都会丢弃，生成的片段不会传递给下一阶段、
        //多边形模式：
        //VK_POLYGON_MODE_FILL 填充模式，多边形内部都产生片段
        //VK_POLYGON_MODE_LINE 线框模式，多边形边缘产生片段
        //VK_POLYGON_MODE_POINT 点模式，多边形顶点产生片段
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;//片段线宽
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;//剔除模式：背面剔除
        rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;//逆时针为正面
        rasterizer.depthBiasEnable = VK_FALSE;//启用深度偏移

        // 9. 配置多重采样信息
        //多重采样是一种组合多个不同多边形产生的片段的颜色来决定最终的像素颜色的技术
        //它可以一定程度上减少多边形边缘的走样现象。
        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;//多重采样信息
        multisampling.sampleShadingEnable = VK_FALSE;//启用采样阴影
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;//采样数量
        multisampling.minSampleShading = 1.0f;//最小采样阴影
        multisampling.pSampleMask = nullptr;//采样掩码
        multisampling.alphaToCoverageEnable = VK_FALSE;
        multisampling.alphaToOneEnable = VK_FALSE;

        // 10. 配置深度模板信息
        VkPipelineDepthStencilStateCreateInfo depthStencil{};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable = VK_TRUE;
        depthStencil.depthWriteEnable = VK_TRUE;
        depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;//深度比较操作模式
        depthStencil.depthBoundsTestEnable = VK_FALSE;
        depthStencil.stencilTestEnable = VK_FALSE;

        // 11. 配置颜色混合信息
        //片段着色器返回的片段颜色需要和原来帧缓冲中对应像素的颜色进行混合。
        //混合的方式有两种：
        // - 混合旧值和新值产生最终的颜色
        // - 使用逻辑位运算组合旧值和新值
        //下面的代码：片段颜色会直接覆盖原来帧缓冲中存储的颜色值。
        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | 
                                              VK_COLOR_COMPONENT_G_BIT | 
                                              VK_COLOR_COMPONENT_B_BIT | 
                                              VK_COLOR_COMPONENT_A_BIT;//颜色写入掩码
        colorBlendAttachment.blendEnable = VK_FALSE;//不启用混合

        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;//颜色混合信息
        colorBlending.logicOpEnable = VK_FALSE;//启用逻辑运算
        colorBlending.logicOp = VK_LOGIC_OP_COPY;//逻辑运算
        colorBlending.attachmentCount = 1;//颜色混合附件数量
        colorBlending.pAttachments = &colorBlendAttachment;//颜色混合附件
        colorBlending.blendConstants[0] = 0.0f;//混合常量
        colorBlending.blendConstants[1] = 0.0f;//混合常量
        colorBlending.blendConstants[2] = 0.0f;//混合常量
        colorBlending.blendConstants[3] = 0.0f;//混合常量

        // 11. 配置动态状态信息
        //动态状态是指在绘制过程中可以改变的状态
        //下面配置了两个动态状态：视口和裁剪矩形
        std::vector<VkDynamicState> dynamicStates = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        };
        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;//动态状态信息
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();

        // 12. 配置管线布局信息
        // 配置一些uniform值，它们可以在着色器中使用
        // uniform值是在绘制过程中可以改变的值,它们可以用来传递变换矩阵和纹理采样器
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;//描述符集布局

        // 13. 创建管线布局
        if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create pipeline layout!");
        }

        // 14. 配置管线信息
        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = pipelineLayout;
        pipelineInfo.renderPass = renderPass;
        pipelineInfo.subpass = 0;
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

        if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS) {
            throw std::runtime_error("failed to create graphics pipeline!");
        }

        // 15. 销毁着色器模块
        vkDestroyShaderModule(device, fragShaderModule, nullptr);//销毁片段着色器模块   
        vkDestroyShaderModule(device, vertShaderModule, nullptr);//销毁顶点着色器模块
    }

    // 创建帧缓冲: 将图像绑定到附件上
    void createFramebuffers() {
        swapChainFramebuffers.resize(swapChainImageViews.size());

        //为每个交换链图像视图创建帧缓冲
        for (size_t i = 0; i < swapChainImageViews.size(); i++) {
            std::array<VkImageView, 2> attachments = {
                swapChainImageViews[i],
                depthImageView
            };

            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = renderPass;
            framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
            framebufferInfo.pAttachments = attachments.data();//实际的图像内容
            framebufferInfo.width = swapChainExtent.width;
            framebufferInfo.height = swapChainExtent.height;
            framebufferInfo.layers = 1;

            if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create framebuffer!");
            }
        }
    }

    // 创建指令池
    void createCommandPool() {
        QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice);
        
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        //每个命令池只能提交给一个特定的队列族，这里我们使用图形队列族
        poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();
        if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create command pool!");
        }
    }

    void createDepthResources() {
        VkFormat depthFormat = findDepthFormat();

        createImage(swapChainExtent.width, 
                    swapChainExtent.height,
                    1, 
                    depthFormat, 
                    VK_IMAGE_TILING_OPTIMAL, 
                    VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, 
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 
                    depthImage, 
                    depthImageMemory);

        depthImageView = createImageView(
                            depthImage, 
                            depthFormat,
                            VK_IMAGE_ASPECT_DEPTH_BIT,1);

        // 转化图像布局
        transitionImageLayout(depthImage, depthFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,1);
    }

    VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {
        for (VkFormat format : candidates) {
            VkFormatProperties props;
            vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);

            if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
                return format;
            } else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
                return format;
            }
        }

        throw std::runtime_error("failed to find supported format!");
    }

    VkFormat findDepthFormat() {
        return findSupportedFormat(
            {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
        );
    }

    bool hasStencilComponent(VkFormat format) {
        return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
    }

    void createTextureImage() {
        int texWidth, texHeight, texChannels;
        stbi_uc* pixels = stbi_load(TEXTURE_PATH.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
        VkDeviceSize imageSize = texWidth * texHeight * 4;
        mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1;


        if (!pixels) {
            throw std::runtime_error("failed to load texture image!");
        }

        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        //VK_BUFFER_USAGE_TRANSFER_SRC_BIT：缓冲区可以用作内存传输操作的源
        //VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT：CPU可见,才可以读取CPU数据
        //VK_MEMORY_PROPERTY_HOST_COHERENT_BIT：CPU一致
        createBuffer(imageSize, 
                    VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                    stagingBuffer,
                    stagingBufferMemory);

        //  Mipmaping:
        //  由于现在有多个 mip 级别，但暂存缓冲区只能用于填充 mip 级别 0。其他级别仍然未定义。
        //  为了填充这些级别，我们需要从我们拥有的单个级别生成数据，反复调用图像布局转换函数，
        //  以便将每个级别转换为传输目标布局，然后将数据复制到该级别，然后将其转换为着色器只读布局。

        //  将图像数据拷贝到缓冲区
        void* data;
        vkMapMemory(device, stagingBufferMemory, 0, imageSize, 0, &data);
        memcpy(data, pixels, static_cast<size_t>(imageSize));
        vkUnmapMemory(device, stagingBufferMemory);

        stbi_image_free(pixels);

        //  申请图像内存，指定图像用途格式
        createImage(texWidth,
                    texHeight,
                    mipLevels, 
                    VK_FORMAT_R8G8B8A8_SRGB, 
                    VK_IMAGE_TILING_OPTIMAL, 
                    VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, 
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 
                    textureImage, 
                    textureImageMemory);

        //  对image执行布局转换:从UNDEFINED转换为TRANSFER_DST_OPTIMAL
        //  只能被用作一个传输命令的一个目标图像
        //  将纹理图像的每个级别保留在 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL 中。 
        //  后续blit 命令读取完成后，每个级别将转换为 VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        transitionImageLayout(textureImage,
                            VK_FORMAT_R8G8B8A8_SRGB,
                            VK_IMAGE_LAYOUT_UNDEFINED, 
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                            mipLevels);
        //  将缓冲区数据拷贝到图像句柄
        copyBufferToImage(stagingBuffer, 
                        textureImage, 
                        static_cast<uint32_t>(texWidth), 
                        static_cast<uint32_t>(texHeight));
        //  清理缓冲区
        vkDestroyBuffer(device, stagingBuffer, nullptr);
        vkFreeMemory(device, stagingBufferMemory, nullptr);

        //  生成mipmaps
        generateMipmaps(textureImage, VK_FORMAT_R8G8B8A8_SRGB, texWidth, texHeight, mipLevels);
    }

    void generateMipmaps(VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels) {
        // Check if image format supports linear blitting
        // 检查图像格式是否支持线性blitting
        VkFormatProperties formatProperties;
        vkGetPhysicalDeviceFormatProperties(physicalDevice, imageFormat, &formatProperties);

        if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
            throw std::runtime_error("texture image format does not support linear blitting!");
        }

        VkCommandBuffer commandBuffer = beginSingleTimeCommands();

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.image = image;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.subresourceRange.levelCount = 1;

        int32_t mipWidth = texWidth;
        int32_t mipHeight = texHeight;

        for (uint32_t i = 1; i < mipLevels; i++) {
            // 设置当前mipmap级别的barrier
            barrier.subresourceRange.baseMipLevel = i - 1;
            // 指定转换前后的图像布局
            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            // 在同步障下执行布局转换
            vkCmdPipelineBarrier(commandBuffer,
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                0, nullptr,
                0, nullptr,
                1, &barrier);

            VkImageBlit blit{};
            // srcOffsets[0]和srcOffsets[1]指定了源图像的起始和结束坐标
            blit.srcOffsets[0] = {0, 0, 0};
            blit.srcOffsets[1] = {mipWidth, mipHeight, 1};
            blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.srcSubresource.mipLevel = i - 1;//源mipmap级别
            blit.srcSubresource.baseArrayLayer = 0;
            blit.srcSubresource.layerCount = 1;
            // dstOffsets[0]和dstOffsets[1]指定了目标图像的起始和结束坐标
            blit.dstOffsets[0] = {0, 0, 0};
            blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
            blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.dstSubresource.mipLevel = i;//目标mipmap级别
            blit.dstSubresource.baseArrayLayer = 0;
            blit.dstSubresource.layerCount = 1;

            // vkCmdBlitImage 命令执行复制、缩放和过滤操作
            // VK_FILTER_LINEAR 指定了在缩放时使用线性过滤
            // 指定了源和目标图像以及过滤器
            // 注意， textureImage 用于 srcImage 和 dstImage 参数。
            // 这是因为我们在同一图像的不同级别之间进行位块传输
            vkCmdBlitImage(commandBuffer,
                image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1, &blit,
                VK_FILTER_LINEAR);

            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            // 将当前mipmap级别转换为着色器只读布局
            vkCmdPipelineBarrier(commandBuffer,
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
                0, nullptr,
                0, nullptr,
                1, &barrier);

            if (mipWidth > 1) mipWidth /= 2;
            if (mipHeight > 1) mipHeight /= 2;
        }

        barrier.subresourceRange.baseMipLevel = mipLevels - 1;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
            0, nullptr,
            0, nullptr,
            1, &barrier);

        endSingleTimeCommands(commandBuffer);
    }


    void createTextureImageView() {
        textureImageView = createImageView(textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT,mipLevels);
    }

    void createTextureSampler() {
        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(physicalDevice, &properties);

        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        //纹理过滤器：当纹理被缩小或者被放大时，纹理需要从多个纹素中进行线性过滤
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        //纹理寻址模式：当纹理坐标超出[0,1]范围时，指定如何采样纹理：
        //repeat, mirrored, clamp to edge, or clamp to border
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.anisotropyEnable = VK_TRUE;
        samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;//最大各向异性采样数目
        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;//边框颜色
        //是否使用归一化坐标
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        //比较过滤操作
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
        //设置读取mipmap级别的方式
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = VK_LOD_CLAMP_NONE;
        samplerInfo.mipLodBias = 0.0f;

        if (vkCreateSampler(device, &samplerInfo, nullptr, &textureSampler) != VK_SUCCESS) {
            throw std::runtime_error("failed to create texture sampler!");
        }
    }
    
    VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags,uint32_t mipLevels=1) {
        //配置图像视图信息
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = format;//图像格式
        //配置图像子资源范围
        //subresourceRange 字段描述图像的用途以及应该访问图像的哪一部分
        viewInfo.subresourceRange.aspectMask = aspectFlags;//子资源范围
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = mipLevels;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        VkImageView imageView;
        if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
            throw std::runtime_error("failed to create image view!");
        }

        return imageView;
    }
    void createImage(uint32_t width, 
                    uint32_t height, 
                    uint32_t mipLevels,
                    VkFormat format, 
                    VkImageTiling tiling, 
                    VkImageUsageFlags usage, 
                    VkMemoryPropertyFlags properties, 
                    VkImage& image, 
                    VkDeviceMemory& imageMemory) {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = width;
        imageInfo.extent.height = height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = mipLevels;
        imageInfo.arrayLayers = 1;
        imageInfo.format = format;//图像格式:VK_FORMAT_R8G8B8A8_SRGB
        //Tiling是关于纹理像素的布局方式，
        //VK_IMAGE_TILING_OPTIMAL,最高效的布局,空间中相邻的像素点的内存位置也尽可能挨在一起
        //VK_IMAGE_TILING_LINEAR,row-major order, be able to directly access texels in the memory of the image
        imageInfo.tiling = tiling;
        //图像初始化布局：
        //VK_IMAGE_LAYOUT_UNDEFINED：Not usable by the GPU and the very first transition will discard the texels.
        //VK_IMAGE_LAYOUT_PREINITIALIZED：Not usable by the GPU, but the first transition will preserve the texels.
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        //图像用途：
        //VK_IMAGE_USAGE_TRANSFER_SRC_BIT：缓冲区可以用作内存传输操作的源
        //VK_IMAGE_USAGE_TRANSFER_DST_BIT：缓冲区可以用作内存传输操作的目标
        //VK_IMAGE_USAGE_SAMPLED_BIT：缓冲区可以用作采样纹理
        //VK_IMAGE_USAGE_STORAGE_BIT：缓冲区可以用作存储图像
        imageInfo.usage = usage;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
            throw std::runtime_error("failed to create image!");
        }

        //申请图像内存
        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(device, image, &memRequirements);
        //分配内存
        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

        if (vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate image memory!");
        }
        //绑定内存
        vkBindImageMemory(device, image, imageMemory, 0);
    }

    void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels=1) {
        VkCommandBuffer commandBuffer = beginSingleTimeCommands();

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = oldLayout;//旧布局:VK_IMAGE_LAYOUT_UNDEFINED,未定义布局
        barrier.newLayout = newLayout;//新布局:VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,传输目标布局
        //VK_QUEUE_FAMILY_IGNORED：不使用队列族
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        //图像句柄
        barrier.image = image;
        if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            if (hasStencilComponent(format)) 
                barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
        } else {
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        }
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = mipLevels;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        //sourceStage：指定哪些阶段在barrier前需要完成
        VkPipelineStageFlags sourceStage;
        //destinationStage：指定哪些阶段在barrier后需要完成
        VkPipelineStageFlags destinationStage;

        if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && 
            newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
            //transfer writes that don't need to wait on anything
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
            newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
            //shader reads should wait on transfer writes
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        } else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && 
            newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        } else {
            throw std::invalid_argument("unsupported layout transition!");
        }

        //执行障碍同步操作，并将图像布局转换为新布局
        //commandBuffer : 命令缓冲区
        //sourceStage：指定哪些阶段在barrier前需要完成
        //destinationStage：指定哪些阶段在barrier后需要完成
        //dependencyFlags：指定依赖关系：0/ VK_DEPENDENCY_BY_REGION_BIT
        //memoryBarrierCount，pMemoryBarrier：内存障碍数量，内存障碍
        //bufferMemoryBarrierCount，pBufferMemoryBarriers：缓冲区内存障碍数量，缓冲区内存障碍
        //imageMemoryBarrierCount，pImageMemoryBarriers：图像障碍数量，图像障碍
        vkCmdPipelineBarrier(
            commandBuffer,
            sourceStage, destinationStage,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier
        );
        endSingleTimeCommands(commandBuffer);
    }

    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
        VkCommandBuffer commandBuffer = beginSingleTimeCommands();

        VkBufferImageCopy region{};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = {0, 0, 0};
        region.imageExtent = {
            width,
            height,
            1
        };
        //在copy前，image的布局要转换成VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
        vkCmdCopyBufferToImage(commandBuffer,
                            buffer, 
                            image, 
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                            1, 
                            &region);

        endSingleTimeCommands(commandBuffer);
    }

    // 读取OBJ文件
    void loadModel() {
        tinyobj::attrib_t attrib;
        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> materials;
        std::string warn, err;

        if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, MODEL_PATH.c_str())) {
            throw std::runtime_error(warn + err);
        }

        std::unordered_map<Vertex, uint32_t> uniqueVertices{};

        for (const auto& shape : shapes) {
            for (const auto& index : shape.mesh.indices) {
                Vertex vertex{};

                vertex.pos = {
                    attrib.vertices[3 * index.vertex_index + 0],
                    attrib.vertices[3 * index.vertex_index + 1],
                    attrib.vertices[3 * index.vertex_index + 2]
                };

                vertex.texCoord = {
                    attrib.texcoords[2 * index.texcoord_index + 0],
                    1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
                };

                vertex.color = {1.0f, 1.0f, 1.0f};

                if (uniqueVertices.count(vertex) == 0) {
                    uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
                    vertices.push_back(vertex);
                }

                indices.push_back(uniqueVertices[vertex]);
            }
        }
    }

    void createVertexBuffer() {
        VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        //VK_BUFFER_USAGE_TRANSFER_SRC_BIT：缓冲区可以用作内存传输操作的源
        //VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT：CPU可访问
        //VK_MEMORY_PROPERTY_HOST_COHERENT_BIT：CPU内存一致
        createBuffer(bufferSize, 
                    VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
                    stagingBuffer, stagingBufferMemory);

        void* data;
        vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
        memcpy(data, vertices.data(), (size_t) bufferSize);
        vkUnmapMemory(device, stagingBufferMemory);

        //VK_BUFFER_USAGE_TRANSFER_DST_BIT：缓冲区可以用作内存传输操作的目标
        //VK_BUFFER_USAGE_VERTEX_BUFFER_BIT：缓冲区可以用作顶点缓冲区
        //VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT：仅GPU可访问
        createBuffer(bufferSize,
                    VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                    vertexBuffer, vertexBufferMemory);

        //From stagingBuffer to vertexBuffer
        copyBuffer(stagingBuffer, vertexBuffer, bufferSize);

        vkDestroyBuffer(device, stagingBuffer, nullptr);
        vkFreeMemory(device, stagingBufferMemory, nullptr);
    }

    void createIndexBuffer() {
        VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        createBuffer(bufferSize, 
                    VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                    stagingBuffer, stagingBufferMemory);

        void* data;
        vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
        memcpy(data, indices.data(), (size_t) bufferSize);
        vkUnmapMemory(device, stagingBufferMemory);


        //VK_BUFFER_USAGE_TRANSFER_DST_BIT：缓冲区可以用作内存传输操作的目标
        //VK_BUFFER_USAGE_INDEX_BUFFER_BIT：缓冲区可以用作索引缓冲区
        //VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT：仅GPU可访问
        createBuffer(bufferSize, 
                    VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                    indexBuffer, indexBufferMemory);

        copyBuffer(stagingBuffer, indexBuffer, bufferSize);

        vkDestroyBuffer(device, stagingBuffer, nullptr);
        vkFreeMemory(device, stagingBufferMemory, nullptr);
    }

    void createUniformBuffers() {
        VkDeviceSize bufferSize = sizeof(UniformBufferObject);

        uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
        uniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
        uniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            createBuffer(bufferSize, 
                        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                        uniformBuffers[i],
                        uniformBuffersMemory[i]);
            // persistent mapped memory
            // uniformBuffersMapped are pointers to the mapped memory
            // mapping is expensive operation, so we map it once and reuse it
            vkMapMemory(device, uniformBuffersMemory[i], 0, bufferSize, 0, &uniformBuffersMapped[i]);
        }
    }

    void createDescriptorPool() {
        //descriptor pool用于存储描述符集
        std::array<VkDescriptorPoolSize, 2> poolSizes{};
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;//uniform缓冲区
        poolSizes[0].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
        poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;//采样器
        poolSizes[1].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

        if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create descriptor pool!");
        }
    }

    void createDescriptorSets() {
        //descriptor layout记录了一种描述符的蓝图，但是他并没有指向任何有效的资源位置
        //descriptor set：描述layout对应的具体资源是什么，每个shader 的 binding绑定的内容是什么。
        std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, descriptorSetLayout);
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = descriptorPool;//descriptor从pool中分配
        allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
        allocInfo.pSetLayouts = layouts.data();
        //we will create one descriptor set for each frame in flight, all with the same layout
        descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
        //allocate descriptor sets
        if (vkAllocateDescriptorSets(device, &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate descriptor sets!");
        }

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            VkDescriptorBufferInfo bufferInfo{};
            bufferInfo.buffer = uniformBuffers[i];
            bufferInfo.offset = 0;
            bufferInfo.range = sizeof(UniformBufferObject);

            VkDescriptorImageInfo imageInfo{};
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfo.imageView = textureImageView;
            imageInfo.sampler = textureSampler;

            std::array<VkWriteDescriptorSet, 2> descriptorWrites{};

            descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[0].dstSet = descriptorSets[i];
            descriptorWrites[0].dstBinding = 0;
            descriptorWrites[0].dstArrayElement = 0;
            descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptorWrites[0].descriptorCount = 1;
            descriptorWrites[0].pBufferInfo = &bufferInfo;

            descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[1].dstSet = descriptorSets[i];
            descriptorWrites[1].dstBinding = 1;
            descriptorWrites[1].dstArrayElement = 0;
            descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorWrites[1].descriptorCount = 1;
            descriptorWrites[1].pImageInfo = &imageInfo;

            vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
        }
    }

    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to create buffer!");
        }

        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

        if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate buffer memory!");
        }
        
        //将内存和缓冲区对象关联
        vkBindBufferMemory(device, buffer, bufferMemory, 0);
    }
    
    //创建临时指令缓冲区,并开始记录指令缓冲区
    VkCommandBuffer beginSingleTimeCommands() {
        
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

    //结束记录指令缓冲区,并执行相关命令,销毁指令缓冲区
    void endSingleTimeCommands(VkCommandBuffer commandBuffer) {
        vkEndCommandBuffer(commandBuffer);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;
        //提交指令缓冲区，执行相关命令
        vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(graphicsQueue);//等待指令完成
        //销毁指令缓冲区
        vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
    }

    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
        VkCommandBuffer commandBuffer = beginSingleTimeCommands();

        //执行缓冲区拷贝操作
        VkBufferCopy copyRegion{};
        copyRegion.size = size;
        vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

        endSingleTimeCommands(commandBuffer);
    }

    // 获取可用的内存类型
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
        VkPhysicalDeviceMemoryProperties memProperties;
        //获取物理设备可用的内存属性
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
        
        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
            //typeFilter：指定内存类型的位域,其每一位代表一种内存类型
            //propertyFlags: 指定内存属性的位域，其每一位代表一种内存属性
            if ((typeFilter & (1 << i)) &&
                (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
            }
        }

        throw std::runtime_error("failed to find suitable memory type!");
    }

    // 多帧并发，创建指令缓冲
    void createCommandBuffers() {
        commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = (uint32_t) commandBuffers.size();

        if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate command buffers!");
        }
    }

    // 记录指令缓冲
    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        // 指定如何使用指令缓冲：
        // VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT：上一帧还未结束渲染时，提交下一帧的渲染指令。
        beginInfo.flags = 0;
        beginInfo.pInheritanceInfo = nullptr;//用于辅助指令缓冲
        //If the command buffer was already recorded once, 
        //then a call to vkBeginCommandBuffer will implicitly reset it
        if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS){
            throw std::runtime_error("failed to begin recording command buffer!");
        }

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = renderPass;
        renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex];//指定帧缓冲
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = swapChainExtent;

        std::array<VkClearValue, 2> clearValues{};
        clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
        clearValues[1].depthStencil = {1.0f, 0};

        renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();

        /*--------------------------------开始渲染流程------------------------------------*/
        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        //绑定管线：指定要使用的管线对象，以获得管线的状态
        // The second parameter specifies if the pipeline object is a graphics or compute pipeline. 
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

        //设置视口（动态）
        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float)swapChainExtent.width;
        viewport.height = (float)swapChainExtent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
        //设置裁剪矩形（动态）
        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = swapChainExtent;
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);


        /*-----------------------------------绑定顶点缓冲区-----------------------------------*/
        VkBuffer vertexBuffers[] = {vertexBuffer};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

        // vkCmdDraw(commandBuffer, static_cast<uint32_t>(vertices.size()), 1, 0, 0);
        vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);

        //绑定描述符集：使用描述符集来更新着色器中的uniform值
        //参数：
        //commandBuffer：指定要记录的指令缓冲
        //pipelineBindPoint：指定管线类型
        //layout：指定管线布局
        //firstSet：the index of the first descriptor set
        //descriptorSetCount：the number of sets to bind
        //pDescriptorSets：the array of sets to bind
        //dynamicOffsetCount：指定动态偏移量数量
        //pDynamicOffsets：指定动态偏移量数组
        vkCmdBindDescriptorSets(commandBuffer, 
                                VK_PIPELINE_BIND_POINT_GRAPHICS, 
                                pipelineLayout, 
                                0, 1, &descriptorSets[currentFrame], 0, 
                                nullptr);

        //vkCmdDrawIndexed 参数：
        //commandBuffer：指定要记录的指令缓冲
        //indexCount：绘制的索引数量
        //instanceCount：实例数量
        //firstIndex：索引缓冲区中的偏移量
        //vertexOffset：顶点缓冲区中的偏移量
        //firstInstance：实例ID的偏移量
        vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);

        vkCmdEndRenderPass(commandBuffer);
        //*----------------------------------结束渲染流程---------------------------------*//

        if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS){
            throw std::runtime_error("failed to record command buffer!");
        }
    }

    // 创建同步对象
    // semaphores：信号量，用于GPU中 swap chain 图像的获取和呈现的同步
    // fences：栅栏，用于CPU和GPU之间的同步，用于指令缓冲的提交
    void createSyncObjects() {
        imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
                vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
                vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create synchronization objects for a frame!");
            }
        }
    }

    void updateUniformBuffer(uint32_t currentImage) {
        static auto startTime = std::chrono::high_resolution_clock::now();

        auto currentTime = std::chrono::high_resolution_clock::now();
        float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();
        time = 0.0f;

        UniformBufferObject ubo{};
        ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        ubo.proj = glm::perspective(glm::radians(45.0f), swapChainExtent.width / (float) swapChainExtent.height, 0.1f, 10.0f);
        ubo.proj[1][1] *= -1;

        //将数据拷贝到uniform缓冲区
        memcpy(uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
    }


    // 绘制一帧!!!!!!!!!
    void drawFrame() {
        // 1. 等待上一帧渲染结束
        vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);
        //2. 获取需要渲染的交换链图像索引
        uint32_t imageIndex;
        VkResult result = vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);
        //交换链已与表面不兼容，无法再用于渲染。通常发生在窗口调整大小之后。
        //需要重新创建交换链,在下一次drawFrame中重新尝试获取图像
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            recreateSwapChain();
            return;
        } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            throw std::runtime_error("failed to acquire swap chain image!");
        }

        // 更新uniform缓冲区
        updateUniformBuffer(currentFrame);

        // 3. 重置上一帧渲染结束的标志位
        // Only reset the fence if we are submitting work
        vkResetFences(device, 1, &inFlightFences[currentFrame]);
        // 4.记录指令缓冲
        vkResetCommandBuffer(commandBuffers[currentFrame],  0);
        recordCommandBuffer(commandBuffers[currentFrame], imageIndex);

        // 5. 提交指令缓冲
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        VkSemaphore waitSemaphores[] = {imageAvailableSemaphores[currentFrame]};
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        // 指定在执行提交操作之前要等待的信号量和相应的管线阶段: 等待颜色写入图像的信号，才开始提交指令
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;//等待图像可用的信号量
        submitInfo.pWaitDstStageMask = waitStages;
        // 指定要提交的指令缓冲数量和指令缓冲数组
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffers[currentFrame];

        
        // 指定在执行提交操作之后要发出的信号量和相应的管线阶段: 发出信号量，表示可以开始进行呈现操作
        VkSemaphore signalSemaphores[] = {renderFinishedSemaphores[currentFrame]};
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        // 提交指令缓冲，在graphicsQueue中执行指令缓冲,进行渲染
        // 当指令缓冲执行完毕后，会发出信号量：renderFinishedSemaphore，inFlightFence会变为signaled状态
        if (vkQueueSubmit(graphicsQueue, 1, 
                        &submitInfo,
                        inFlightFences[currentFrame]) != VK_SUCCESS) {
            throw std::runtime_error("failed to submit draw command buffer!");    
        }

        // 6. 呈现交换链图像:将渲染好的图像提交到交换链进行显示
        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;//等待渲染结束的信号量

        VkSwapchainKHR swapChains[] = {swapChain};
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;

        presentInfo.pImageIndices = &imageIndex;
        // 呈现交换链图像，将渲染好的图像提交到呈现队列中进行显示
        result = vkQueuePresentKHR(presentQueue, &presentInfo);
        
        // 设置framebufferResized是为了避免信号量不同步的问题
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized) {
            framebufferResized = false;
            recreateSwapChain();
        } else if (result != VK_SUCCESS) {
            throw std::runtime_error("failed to present swap chain image!");
        }

        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }


    // 创建着色器模块：对着色器代码的包装
    VkShaderModule createShaderModule(const std::vector<char>& code) {
        // 1. 配置着色器模块信息
        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;//着色器模块信息
        createInfo.codeSize = code.size();//着色器模块大小
        createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());//着色器模块数据

        // 2. 创建着色器模块
        VkShaderModule shaderModule;
        if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {//创建着色器模块
            throw std::runtime_error("failed to create shader module!");
        }

        return shaderModule;
    }

    // 选择交换链图像格式
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(
                    const std::vector<VkSurfaceFormatKHR>& availableFormats){
        // 1. 如果只有一个可用的表面格式，并且格式为VK_FORMAT_UNDEFINED，则表示所有格式都可用
        if (availableFormats.size() == 1 && availableFormats[0].format == VK_FORMAT_UNDEFINED) {
            // 返回默认格式：BGRA8，SRGB_NONLINEAR
            return {VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
        }

        // 2. 如果不是所有格式都可用，需要遍历所有可用的表面格式，选择最佳的格式
        for (const auto& availableFormat : availableFormats) {
            // 选择最佳的格式
            if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && 
                availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                return availableFormat;
            }
        }

        // 3. 如果没有找到最佳的格式，返回第一个可用的格式
        return availableFormats[0];
    }

    // 选择交换链呈现模式
    VkPresentModeKHR chooseSwapPresentMode(
                    const std::vector<VkPresentModeKHR>& availablePresentModes){
        /*
        可选呈现模式：
        VK_PRESENT_MODE_IMMEDIATE_KHR：应用程序提交的图像会立即传输到屏幕上，可能会导致撕裂
        VK_PRESENT_MODE_FIFO_KHR：交换链是一个队列，应用程序提交的图像会排队，直到显示器准备好显示它们为止，
                                如果队列已满，则应用程序必须等待
        VK_PRESENT_MODE_FIFO_RELAXED_KHR：和第二个大致相同，但如果应用程序延迟，
                                导致交换链的队列在上一次垂直回扫时为空，那么就会出现撕裂
        VK_PRESENT_MODE_MAILBOX_KHR：和第二个大致相同，但是如果队列已满，那么不会阻塞应用程序，
                                旧的图像会被替换为新的图像，这一模式可以用来实现三
                                倍缓冲，避免撕裂现象的同时减小了延迟问题                        
        */
        // 1. 遍历所有可用的呈现模式，选择最佳的呈现模式
        for (const auto& availablePresentMode : availablePresentModes) {
            // 选择最佳的呈现模式
            if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
                return availablePresentMode;
            }
        }
        // 2. 如果没有找到最佳的呈现模式，返回VK_PRESENT_MODE_FIFO_KHR
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    // 选择交换范围
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities){
        // 1. 如果当前分辨率不等于最大分辨率，则返回当前分辨率
        if (capabilities.currentExtent.width != UINT32_MAX) {
            return capabilities.currentExtent;
        } else {
            // 2. 如果当前分辨率等于最大分辨率，则返回最大分辨率
            int width, height;
            glfwGetFramebufferSize(window, &width, &height);//获取窗口分辨率

            VkExtent2D actualExtent = {
                static_cast<uint32_t>(width),
                static_cast<uint32_t>(height)
            };

            // 3. 返回最大分辨率
            return actualExtent;
        }
    }


    // 交换链支持信息
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device) {
        SwapChainSupportDetails details;

        // 1. 基础信息:交换链的最小/最大图像数量，最小/最大图像宽度、高度
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);//获取表面能力

        // 2. 表面格式：像素格式、颜色空间
        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);//获取表面格式数量
        if (formatCount != 0) {
            details.formats.resize(formatCount);//表面格式
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());//获取表面格式
        }

        // 3. 呈现模式
        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);//获取呈现模式数量
        if (presentModeCount != 0) {
            details.presentModes.resize(presentModeCount);//呈现模式
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());//获取呈现模式
        }

        return details;
    }


    // 检查物理设备是否可用
     bool isDeviceSuitable(VkPhysicalDevice device) {
        QueueFamilyIndices indices = findQueueFamilies(device);
        
        //检查物理设备是否支持相应的扩展
        bool extensionsSupported = checkDeviceExtensionSupport(device);

        bool swapChainAdequate = false;
        if (extensionsSupported) {
            //检查交换链是否支持
            SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
            swapChainAdequate = !swapChainSupport.formats.empty() && 
                                !swapChainSupport.presentModes.empty();
        }
        VkPhysicalDeviceFeatures supportedFeatures;
        vkGetPhysicalDeviceFeatures(device, &supportedFeatures);
        return indices.isComplete() &&
            extensionsSupported && 
            swapChainAdequate && 
            supportedFeatures.samplerAnisotropy;
    }

    // 检查物理设备是否支持相应的扩展
    bool checkDeviceExtensionSupport(VkPhysicalDevice device) {
        // 1. 获取所有可用的扩展
        uint32_t extensionCount;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);//获取可用的扩展数量
        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());//获取可用的扩展

        // 2. 检查所有需要的扩展是否都被支持
        std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());//需要的扩展
        for (const auto& extension : availableExtensions) {
            requiredExtensions.erase(extension.extensionName);//从需要的扩展中删除已被支持的扩展
        }

        return requiredExtensions.empty();
    }

    // 队列族索引
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device){
        QueueFamilyIndices indices;

        // 1. 获取所有队列族
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);//获取队列族数量
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());//获取队列族属性

        // 2. 找出支持某种功能的队列族
        int i = 0;
        for (const auto& queueFamily : queueFamilies) {
            // 检查队列族是否支持图形指令
            if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                indices.graphicsFamily = i;
            }

            // 检查队列族是否支持呈现指令
            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);//检查物理设备是否支持表面呈现
            if (presentSupport) {
                indices.presentFamily = i;
            }

            // // 检查队列族是否支持VK_QUEUE_COMPUTE_BIT
            // if (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT) {
            //     indices.computeFamily = i;
            // }
            // // 检查队列族是否支持VK_QUEUE_TRANSFER_BIT
            // if (queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT) {
            //     indices.transferFamily = i;
            // }
            // // 检查队列族是否支持VK_QUEUE_SPARSE_BINDING_BIT
            // if (queueFamily.queueFlags & VK_QUEUE_SPARSE_BINDING_BIT) {
            //     indices.sparseBindingFamily = i;
            // }
            // // 检查队列族是否支持VK_QUEUE_PROTECTED_BIT
            // if (queueFamily.queueFlags & VK_QUEUE_PROTECTED_BIT) {
            //     indices.protectedFamily = i;
            // }

            // 要求的属性是否检查完毕
            if (indices.isComplete()) {
                break;
            }

            i++;
        }

        return indices;
    }

    // 对物理设备进行打分
    int rateDeviceSuitability(VkPhysicalDevice device) {
        int score = 0;
        // 1. 基础信息
        VkPhysicalDeviceProperties deviceProperties;
        vkGetPhysicalDeviceProperties(device, &deviceProperties);//获取物理设备属性
        // VkPhysicalDeviceFeatures deviceFeatures;
        // vkGetPhysicalDeviceFeatures(device, &deviceFeatures);//获取物理设备特性

        // 2. 设备类型
        if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            score += 1000;
        }

        score += deviceProperties.limits.maxImageDimension2D;
        if(!deviceProperties.limits.framebufferColorSampleCounts) {
            return 0;
        }
        return score;
    }

    // 获取扩展
    std::vector<const char*> getRequiredExtensions() {
        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions;
        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);//获取GLFW需要的扩展

        std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

        //根据是否启用校验层，返回所需的扩展列表
        if (enableValidationLayers) {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);//获取调试信息，添加调试扩展
        }

        return extensions;
    }

    //请求所有可用的校验层
    bool checkValidationLayerSupport() {
        uint32_t layerCount;
        //获取可用的校验层
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());//获取可用的校验层

        //检查所有的校验层是否都被支持
        for (const char* layerName : validationLayers) {
            bool layerFound = false;

            //遍历所有可用的校验层
            for (const auto& layerProperties : availableLayers) {
                //如果校验层被支持
                if (strcmp(layerName, layerProperties.layerName) == 0) {
                    layerFound = true;
                    break;
                }
            }
            //如果有校验层不被支持
            if (!layerFound) {
                return false;
            }
        }
        return true;
    }

    //读取文件
    static std::vector<char> readFile(const std::string& filename) {
        //ate：从文件末尾开始读取
        //binary：读取二进制文件
        std::ifstream file(filename, std::ios::ate | std::ios::binary);

        if (!file.is_open()) {
            throw std::runtime_error("failed to open file!");
        }

        size_t fileSize = (size_t) file.tellg();
        std::vector<char> buffer(fileSize);

        //将文件指针移动到文件开头处，然后读取文件
        file.seekg(0);
        file.read(buffer.data(), fileSize);

        file.close();

        return buffer;
    }

    
    /*
    debug callback
    param: messageSeverity 消息级别：诊断、资源创建、警告、错误
    param: messageType 消息类型：与性能无关、违反规范的行为或者错误、影响vulkan性能的事件
    param: pCallbackData 指向包含消息的结构体: 包含
           pMessage：包含调试信息的字符串
           pObjects：与消息相关的Vulkan对象的句柄数组
           objectCount：pObjects数组中的对象数量
    param: pUserData 指向回调函数的用户数据
    */
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData) 
    {   
        //example of messageSeverity
        if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
            // Message is important enough to show
        }

        //example of messageType
        if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) {
            // Message is related to validation or descriptive debug info
        }

        //输出调试信息
        std::cerr << "validation layer: " 
                << pCallbackData->pMessage 
                << std::endl;
                
        //返回值表示引发校验层处理的API调用是否被中断
        //如果返回VK_TRUE，表示应用程序应该终止调用引发此回调的Vulkan函数
        //如果返回VK_FALSE，表示应用程序应该继续进行调用引发此回调的Vulkan函数
        return VK_FALSE;
    }


};

int main() {
    HelloTriangleApplication app;

    try {
        app.run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}