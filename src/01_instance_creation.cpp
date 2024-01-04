#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <vector>
const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

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

    }
    void createInstance() {
        // to create an instance we'll first have to
        // fill in a struct with some information about our application.
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Hello Triangle";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "No Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_0;
        
        //A lot of information in Vulkan is passed 
        //through structs instead of function parameters 
        //它告诉Vulkan驱动程序需要使用的全局扩展和验证层
        //全局是说它对整个程序有效，而不是特定于设备
        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;

        //glfw extensions
        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions;
        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);//获取GLFW需要的扩展
        createInfo.enabledExtensionCount = glfwExtensionCount;
        createInfo.ppEnabledExtensionNames = glfwExtensions;

        //validation layers
        createInfo.enabledLayerCount = 0;

        //  create instance
        //  param1: Pointer to struct with creation info
        //  param2: Pointer to custom allocator callbacks
        //  param3: Pointer to the variable that stores the handle to the new object
        if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
            throw std::runtime_error("failed to create instance!");
        }


        //  check extensions
        uint32_t extensionCount = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);//获取可用扩展数量
        std::vector<VkExtensionProperties> extensions(extensionCount);//创建扩展数组
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());//获取可用扩展列表
        for(const auto& extension : extensions) {
            std::cout << '\t' << extension.extensionName << std::endl;
        }
    }

    void mainLoop() {
        while (!glfwWindowShouldClose(window)) {//循环直到窗口关闭
            glfwPollEvents();//处理事件:如果有事件触发，调用对应的回调函数
        }
    }

    void cleanup() {
        vkDestroyInstance(instance, nullptr);//销毁vulkan实例
        glfwDestroyWindow(window);//销毁窗口
        glfwTerminate();//终止GLFW
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
