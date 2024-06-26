/*
    *   Framebuffers: 帧缓冲
    *   Framebuffers are the objects that represent a collection of memory attachments that can be rendered to.
    *   帧缓冲区对象引用所有表示附件的 VkImageView 对象，这些附件将用于渲染操作。
    *   
    *   Framebuffer就是一段存储空间，其可以位于显存，也可以位于内存,用于存储渲染:颜色、深度、模板等数据
    *   
    *   a framebuffer specifies the actual image views to bind to render pass attachments
    * 
    *   FrameBuffer表示可用作附件(渲染目标)的实际图像的链接。
    *   通过指定RenderPass和一组Imageview创建一个Framebuffer对象。
    *   当然，它们的数量和格式必须与RenderPass的规范相匹配。
    * 
    *    帧缓冲区是存储图像渲染结果的一块内存区域。在渲染过程中，所有的绘图操作都是在帧缓冲区中进行的。
    *   一旦所有渲染命令完成，帧缓冲区中的数据可以被送到显示设备上显示出来。
    *   管理帧缓冲区可以让你控制渲染输出的目标，支持多目标渲染，后期处理等高级功能。
    * 
    *   Framebuffer是Image之上的另一层，基本上将这些Imageview组合在一起，以便在渲染特定RenderPass期间绑定为附件。
    *   每当开始渲染RernderPass时，调用函数vkCmdBeginRenderPass并将FrameBuffer传递给它。
    * 
    *   createFramebuffers()
*/
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <limits>
#include <optional>
#include <set>
#include <map>
const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;
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

    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;//物理设备句柄,用于获取GPU信息,在cleanup中自动销毁
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
    VkPipelineLayout pipelineLayout;//管线布局句柄
    VkPipeline graphicsPipeline;//图形管线句柄

private:
    void initWindow() {
        glfwInit();//初始化GLFW
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);//不使用OpenGL
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);//不可调整窗口大小

        window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);//创建窗口
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

        // 创建图形管线
        createGraphicsPipeline();

        // 创建帧缓冲
        createFramebuffers();

    }

    void mainLoop() {
        while (!glfwWindowShouldClose(window)) {//循环直到窗口关闭
            glfwPollEvents();//处理事件:如果有事件触发，调用对应的回调函数
        }
    }

    void cleanup() {
        // 销毁交换链帧缓冲
        for (auto framebuffer : swapChainFramebuffers) {
            vkDestroyFramebuffer(device, framebuffer, nullptr);
        }

        vkDestroyPipeline(device , graphicsPipeline , nullptr ) ;//销毁图形管线
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);//销毁管线布局
        vkDestroyRenderPass(device, renderPass, nullptr);//销毁渲染流程

        for (auto imageView : swapChainImageViews) {
            vkDestroyImageView(device, imageView, nullptr);//销毁交换链图像视图
        }

        vkDestroySwapchainKHR(device, swapChain, nullptr);//销毁交换链
        vkDestroyDevice(device, nullptr);//销毁逻辑设备

        if(enableValidationLayers) {
            DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);//销毁调试信息
        }
        vkDestroySurfaceKHR(instance, surface, nullptr);//销毁表面
        vkDestroyInstance(instance, nullptr);//销毁vulkan实例

        glfwDestroyWindow(window);//销毁窗口
        
        glfwTerminate();//终止GLFW

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
        if (enableValidationLayers)
        {
            // 在校验层启用的情况下，使用校验层
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size()); // 启用的校验层数量
            createInfo.ppEnabledLayerNames = validationLayers.data();                      // 启用的校验层名称
            // 配置调试信息
            populateDebugMessengerCreateInfo(debugCreateInfo);
            createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT *)&debugCreateInfo;
        }
        else
        {
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

        // 3. 配置逻辑设备信息
        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());//队列数量
        createInfo.pQueueCreateInfos = queueCreateInfos.data();//队列信息

        createInfo.pEnabledFeatures = &deviceFeatures;

        createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());//启用的扩展数量
        createInfo.ppEnabledExtensionNames = deviceExtensions.data();

        if(enableValidationLayers) {
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
    
    // 创建交换链图像视图：用于定义交换链图像的内存布局
    void createImageViews() {
        // 1. 配置交换链图像视图信息
        swapChainImageViews.resize(swapChainImages.size());//分配交换链图像视图内存
        for (uint32_t i = 0; i < swapChainImages.size(); i++) {
            VkImageViewCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;//交换链图像视图信息
            createInfo.image = swapChainImages[i];//交换链图像
            createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;//图像视图类型：2D纹理
            createInfo.format = swapChainImageFormat;//图像视图格式

            // 2. 配置图像通道
            createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;//图像通道r
            createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;//图像通道g
            createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;//图像通道b
            createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;//图像通道a

            // 3. 配置图像子资源范围
            createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;//图像子资源范围
            createInfo.subresourceRange.baseMipLevel = 0;//基本mip等级
            createInfo.subresourceRange.levelCount = 1;//mip等级数量
            createInfo.subresourceRange.baseArrayLayer = 0;//基本数组层
            createInfo.subresourceRange.layerCount = 1;//数组层数量

            // 4. 创建交换链图像视图
            if (vkCreateImageView(device, &createInfo, nullptr, &swapChainImageViews[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create image views!");
            }
        }
    }

    // 创建渲染流程
    void createRenderPass() {
        VkAttachmentDescription colorAttachment{};
        colorAttachment.format = swapChainImageFormat;//format成员变量用于指定颜色缓冲附着的格式
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;//指定采样数量
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;//指定渲染前对颜色缓冲的操作：清除
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;//指定渲染后对颜色缓冲的操作：存储
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;//指定渲染后图像的布局：呈现

        //子流程:用于引用多个/一个attachment，处理attachment的内容
        VkAttachmentReference colorAttachmentRef{};
        colorAttachmentRef.attachment = 0;//指定颜色缓冲附着的索引
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;//颜色缓冲附着数量
        subpass.pColorAttachments = &colorAttachmentRef;//对应 layout(location = 0) out vec4 outColor

        //渲染流程
        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = 1;
        renderPassInfo.pAttachments = &colorAttachment;
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;

        if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
            throw std::runtime_error("failed to create render pass!");
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
        // 在硬编码中不载入顶点数据，而是在着色器中使用gl_VertexIndex内置变量来获取顶点数据，所以绑定数据置为0
        // 后面使用顶点缓冲区对象来存储顶点数据是更好的选择
        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;//顶点输入信息
        vertexInputInfo.vertexBindingDescriptionCount = 0;//顶点绑定描述数量
        vertexInputInfo.pVertexBindingDescriptions = nullptr;//顶点绑定描述
        vertexInputInfo.vertexAttributeDescriptionCount = 0;//顶点属性描述数量
        vertexInputInfo.pVertexAttributeDescriptions = nullptr;//顶点属性描述

        // 6. 配置几何图元信息
        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;//输入汇总信息
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;//图元拓扑：三角形列表
        inputAssembly.primitiveRestartEnable = VK_FALSE;//启用图元重启

        // 7. 配置视口信息
        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;//视口信息
        // viewportState.viewportCount = 1;//视口数量
        // viewportState.pViewports = nullptr;//视口
        // viewportState.scissorCount = 1;//裁剪数量
        // viewportState.pScissors = nullptr;//裁剪
        VkViewport viewport{};
        viewport.x = 0.0f;//视口x坐标
        viewport.y = 0.0f;//视口y坐标
        viewport.width = (float) swapChainExtent.width;//视口宽度
        viewport.height = (float) swapChainExtent.height;//视口高度
        viewport.minDepth = 0.0f;//视口最小深度
        viewport.maxDepth = 1.0f;//视口最大深度
        VkRect2D scissor{};
        scissor.offset = {0, 0};//裁剪偏移量
        scissor.extent = swapChainExtent;//裁剪范围
        viewportState.viewportCount = 1;//视口数量
        viewportState.pViewports = &viewport;//视口
        viewportState.scissorCount = 1;//裁剪数量
        viewportState.pScissors = &scissor;//裁剪

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
        rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;//正面朝向：顺时针
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

        // 10. 配置颜色混合信息
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
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;//管线布局信息
        pipelineLayoutInfo.setLayoutCount = 0;//布局数量
        pipelineLayoutInfo.pSetLayouts = nullptr;//布局
        pipelineLayoutInfo.pushConstantRangeCount = 0;//推送常量范围数量
        pipelineLayoutInfo.pPushConstantRanges = nullptr;//推送常量范围

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

    // 创建帧缓冲
    void createFramebuffers() {
        swapChainFramebuffers.resize(swapChainImageViews.size());

        //为每个交换链图像视图创建帧缓冲
        for (size_t i = 0; i < swapChainImageViews.size(); i++) {
            // FrameBuffer 是通过ImageView来引用attachment的
            // 如果还有深度缓冲，也可以在这里引用
            VkImageView attachments[] = {
                swapChainImageViews[i]
            };

            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = renderPass;
            framebufferInfo.attachmentCount = 1;
            framebufferInfo.pAttachments = attachments;
            framebufferInfo.width = swapChainExtent.width;
            framebufferInfo.height = swapChainExtent.height;
            framebufferInfo.layers = 1;

            if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create framebuffer!");
            }
        }
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

        return indices.isComplete() && extensionsSupported && swapChainAdequate;
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
