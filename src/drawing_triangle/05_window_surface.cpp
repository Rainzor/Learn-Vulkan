/*
    * Vulkan Tutorial (https://vulkan-tutorial.com/)
    * 
    * 05 - Window surface
    *
    * The window surface is the connection between Vulkan and the window system.
    * It allows Vulkan to present rendered images to the user.
    * 
    * Vulkan是平台无关的API，你必须向其提供一个window surface（VkSurfaceKHR），以和平台特定的窗口对接
    * 
*/
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <optional>
#include <set>
#include <map>
const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;
const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
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
    //我们通过图形队列在交换链图像上进行绘制操作，然后将图像提交给呈现队列来显示它们
    std::optional<uint32_t> graphicsFamily;//图形队列族索引
    std::optional<uint32_t> presentFamily;//呈现队列族索引

    // std::optional<uint32_t> computeFamily;//计算队列族索引
    // std::optional<uint32_t> transferFamily;//传输队列族索引
    // std::optional<uint32_t> sparseBindingFamily;//稀疏绑定队列族索引
    // std::optional<uint32_t> protectedFamily;//保护队列族索引

    //检查队列族是否支持VK_QUEUE_GRAPHICS_BIT
    bool isComplete() {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
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

    }

    void mainLoop() {
        while (!glfwWindowShouldClose(window)) {//循环直到窗口关闭
            glfwPollEvents();//处理事件:如果有事件触发，调用对应的回调函数
        }
    }

    void cleanup() {

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
        //如果使用其他windows系统，那么需要使用其他的扩展来创建表面对象
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

        createInfo.enabledExtensionCount = 0;

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

    // 检查物理设备是否可用
     bool isDeviceSuitable(VkPhysicalDevice device) {
        QueueFamilyIndices indices = findQueueFamilies(device);

        return indices.isComplete();
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
