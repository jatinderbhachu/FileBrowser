#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define GLFW_INCLUDE_NONE
#include "glad/glad.h"
#include <GLFW/glfw3.h>


#include <imgui.h>
#include <backends/imgui_impl_opengl3.h>
#include <backends/imgui_impl_glfw.h>
#include <misc/cpp/imgui_stdlib.h>

#include <iostream>
#include <combaseapi.h>

#include <codecvt>
#include <string.h>
#include <unordered_map>
#include <filesystem>
#include <fstream>
#include <memory.h>

#include "path.h"
#include "file_ops.h"
#include "browser_widget.h"

void makeSandboxFolder() {
    // c:/browser_test
    // a folder with 10_000 files
    std::filesystem::path testPath("C:/browser_test/runtime_test");

    if(std::filesystem::exists(testPath)) return;

    std::filesystem::create_directory(testPath);

    std::filesystem::path thousandFilesPath = testPath / "1k_files";
    std::filesystem::create_directory(thousandFilesPath);

    for(int i = 0; i < 1000; i++) {
        std::ofstream outputFile((thousandFilesPath / std::to_string(i)).u8string());
        outputFile << ".";
        outputFile.close();
    }
}

int main() {

    //Path myPath("C:\\Users\\jatinder\\");
    //Path rPath("./testdir/file.txt");

    //myPath.popSegment();
    //myPath.appendRelative(rPath);

#if 1
    makeSandboxFolder();
#endif

#if 1
    int width = 1280;
    int height = 720;

    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if(hr != S_OK) {
        printf("Failed to initialize COM library\n");
        return 0;
    }

    glfwInit();


    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    GLFWwindow* window = glfwCreateWindow(width, height, "File Browser", NULL, NULL);
    glfwMakeContextCurrent(window);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;       // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends

    const char* glsl_version = "#version 460";
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    io.Fonts->AddFontFromFileTTF("default.ttf", 16);

    ImVec4 clearColor{0.1f, 0.1f, 0.1f, 1.0f};

    std::vector<FileOps::Record> directoryItems(64);

    glfwSwapInterval(1);

    bool updateViewFlag = true;

    Path dir("C:\\browser_test");
    Browser browser(dir);

    MSG msg;
    while(!glfwWindowShouldClose(window)){
        glfwPollEvents();

        if(updateViewFlag) {
            FileOps::enumerateDirectory(dir, directoryItems);
            FileOps::sortByName(FileOps::SortDirection::Descending, directoryItems);
            FileOps::sortByType(FileOps::SortDirection::Descending, directoryItems);
            updateViewFlag = false;
        }

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGuiWindowFlags mainWindowFlags = 
            ImGuiWindowFlags_NoCollapse
            | ImGuiWindowFlags_NoResize
            | ImGuiWindowFlags_NoMove
            | ImGuiWindowFlags_NoTitleBar;

        glfwGetWindowSize(window, &width, &height);

        ImGui::SetNextWindowPos(ImVec2{0.0f, 0.0f});
        ImGui::SetNextWindowSize(ImVec2{(float)width, (float)height});
        ImGui::Begin("Main window", NULL, mainWindowFlags);

        browser.beginFrame();
        browser.draw();
        ImGui::End();


        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(clearColor.x * clearColor.w, clearColor.y * clearColor.w, clearColor.z * clearColor.w, clearColor.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();

    CoUninitialize();

#endif
    return 0;
}
