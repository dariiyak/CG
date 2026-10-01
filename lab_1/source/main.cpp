#include <csignal>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#define GLFW_INCLUDE_NONE
#include "application.hpp"
#include "graphics_internal.hpp"
#include <GLFW/glfw3.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>
#include <imgui.h>

namespace {
volatile std::sig_atomic_t stop_requested = 0;
}
int main(int argc, char **argv) {
    std::signal(SIGINT, [](int) { stop_requested = 1; });
    std::signal(SIGTERM, [](int) { stop_requested = 1; });
#ifdef __APPLE__
    // Finder does not inherit shell variables. The SDK stays outside the app;
    // reconfigure CMake if the installed SDK is moved.
    if (!std::getenv("VK_DRIVER_FILES") && !std::getenv("VK_ICD_FILENAMES"))
        setenv("VK_DRIVER_FILES", LAB_SDK_ROOT "/share/vulkan/icd.d/MoltenVK_icd.json", 0);
    if (!std::getenv("VK_LAYER_PATH") && !std::getenv("VK_ADD_LAYER_PATH"))
        setenv("VK_ADD_LAYER_PATH", LAB_SDK_ROOT "/share/vulkan/explicit_layer.d", 0);
#endif
    int limit = 0, preset = 0;
    bool resizeTest = false, animate = false;
    std::string capture;
    try {
        for (int i = 1; i < argc; ++i) {
            std::string a = argv[i];
            if (a == "--frames" && i + 1 < argc)
                limit = std::stoi(argv[++i]);
            else if (a == "--preset" && i + 1 < argc)
                preset = std::stoi(argv[++i]);
            else if (a == "--capture" && i + 1 < argc)
                capture = argv[++i];
            else if (a == "--animate")
                animate = true;
            else if (a == "--resize-test")
                resizeTest = true;
            else
                throw std::runtime_error("Usage: cg-lab1 [--frames N] [--preset 0|1|2] [--capture "
                                         "file.ppm] [--resize-test] [--animate]");
        }
        if (limit < 0 || preset < 0 || preset > 2)
            throw std::runtime_error("Invalid command-line value");
        if (!capture.empty() && limit == 0)
            limit = 30;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
    if (!glfwInit()) {
        std::cerr << "GLFW initialization failed\n";
        return 1;
    }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow *window =
        glfwCreateWindow(1280, 820, "CG Lab 1 - Yakovleva - Variant 3", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return 1;
    }
    glfwSetWindowSizeLimits(window, 760, 600, GLFW_DONT_CARE, GLFW_DONT_CARE);
    glfwSetFramebufferSizeCallback(window, [](GLFWwindow *, int w, int h) {
        if (w > 0 && h > 0)
            graphics::internal::resize(uint32_t(w), uint32_t(h));
    });
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    bool platform = ImGui_ImplGlfw_InitForVulkan(window, true), graphics = false, app = false;
    int result = 0;
    try {
        if (!platform)
            throw std::runtime_error("ImGui GLFW initialization failed");
        graphics = graphics::internal::initialize(window);
        if (!graphics)
            throw std::runtime_error("Vulkan initialization failed");
        app = application::initialize();
        if (!app)
            throw std::runtime_error("Application initialization failed");
        application::preset(preset);
        if (animate)
            application::playAnimation();
        int frame = 0;
        while (!stop_requested && !glfwWindowShouldClose(window) && (!limit || frame < limit)) {
            glfwPollEvents();
            int w, h;
            glfwGetFramebufferSize(window, &w, &h);
            if (w == 0 || h == 0) {
                glfwWaitEvents();
                continue;
            }
            auto fd = graphics::internal::prepare();
            ImGui_ImplVulkan_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();
            application::update(glfwGetTime());
            ImGui::Render();
            application::render(fd);
            if (!capture.empty() && frame == limit - 1)
                graphics::internal::captureNextFrame(capture.c_str());
            graphics::internal::submitAndPresent();
            ++frame;
            if (resizeTest && frame == 10)
                glfwSetWindowSize(window, 1000, 720);
            if (resizeTest && frame == 25)
                glfwSetWindowSize(window, 1280, 820);
        }
        std::cout << "Rendered " << frame
                  << " frames; validation errors: " << graphics::internal::validationErrorCount()
                  << '\n';
    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << '\n';
        result = 1;
    }
    if (app)
        application::shutdown();
    if (graphics)
        graphics::internal::shutdown();
    if (graphics::internal::validationErrorCount())
        result = 1;
    if (platform)
        ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return result;
}
