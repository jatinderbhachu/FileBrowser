#include <mutex>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define GLFW_INCLUDE_NONE
#include "glad/glad.h"
#include <GLFW/glfw3.h>

#include "Path.h"
#include "FileOps.h"
#include "BrowserWidget.h"

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
#include "FileOpsWorker.h"


static const char* DebugTestPath = "./browser_test/runtime_test";

void makeSandboxFolder() {
    // c:/browser_test
    // a folder with 10_000 files
    std::filesystem::path testPath(DebugTestPath);

    if(!std::filesystem::exists(testPath)) {
        std::filesystem::create_directories(testPath);
        printf("Test path %s does not exist. Creating it..", testPath.string().c_str());
    }

    std::filesystem::path thousandFilesPath = testPath / "1k_files";

    if(!std::filesystem::exists(thousandFilesPath)) {
        std::filesystem::create_directory(thousandFilesPath);
        for(int i = 0; i < 1000; i++) {
            std::ofstream outputFile((thousandFilesPath / std::to_string(i)).string());
            outputFile << ".";
            outputFile.close();
        }
    }

}





int main() {

#if 1
    makeSandboxFolder();

    //Path fileToDelete(DebugTestPath);
    //fileToDelete.appendName("unity.mkv");
    //fileToDelete.toAbsolute();

    //FileOps::deleteFileOrDirectory(fileToDelete, false);
    //{
        //Path fileToCopy(DebugTestPath);
        //fileToCopy.appendName("unity.mkv");
        //fileToCopy.toAbsolute();
        //Path toDirectory(DebugTestPath);
        //toDirectory.appendName("temp");
        //toDirectory.toAbsolute();

        //FileOps::copyFileOrDirectory(fileToCopy, toDirectory, "copiedFile.mkv");
    //}
#endif

    FileOpsWorker fileOpsWorker;

    int width = 1280;
    int height = 720;


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

    //io.Fonts->AddFontFromFileTTF("default.ttf", 16);

    ImVec4 clearColor{0.1f, 0.1f, 0.1f, 1.0f};

    std::vector<FileOps::Record> directoryItems(64);

    glfwSwapInterval(1);

    bool updateViewFlag = true;

    Path dir(DebugTestPath);
    dir.toAbsolute();
    BrowserWidget browser(dir, &fileOpsWorker);

    MSG msg;
    while(!glfwWindowShouldClose(window)){
        glfwPollEvents();

        static bool CanAdd = true;

        if(glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS && CanAdd) {
            CanAdd = false;
            printf("Add to queue..\n");
            FileOp op;
            op.opType = FileOpType::FILE_OP_COPY;
            op.from = Path("./browser_test/runtime_test/unity.mkv");
            op.to = Path("./browser_test/runtime_test/temp");
            fileOpsWorker.addFileOperation(op);
        }

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
            | ImGuiWindowFlags_NoTitleBar
            | ImGuiWindowFlags_NoBringToFrontOnFocus;

        glfwGetWindowSize(window, &width, &height);

        ImGui::SetNextWindowPos(ImVec2{0.0f, 0.0f});
        ImGui::SetNextWindowSize(ImVec2{(float)width, (float)height});

        ImGui::Begin("Main window", NULL, mainWindowFlags);
        browser.beginFrame();
        browser.draw();
        ImGui::End();


        ImGui::Begin("progress window");

        // consume result queue
        fileOpsWorker.syncProgress();

        // display any in progress operations
        for(FileOp& op : fileOpsWorker.mFileOperations) {
            if(op.idx >= 0) {
                ImGui::Text("Operation %d progress %d/%d \n", op.idx, op.currentProgress, op.totalProgress);
            }
        }

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

    return 0;
}
