#include "CommandParser.h"
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
#include <memory.h>
#include "FileOpsWorker.h"

#include <IconsForkAwesome.h>

static const char* DebugTestPath = "./browser_test/runtime_test";


int main() {
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

    io.Fonts->AddFontDefault();

    static const ImWchar iconsRanges[] = { ICON_MIN_FK, ICON_MAX_16_FK, 0 };
    ImFontConfig iconsConfig; iconsConfig.MergeMode = true; iconsConfig.PixelSnapH = true;
    io.Fonts->AddFontFromFileTTF(FONT_ICON_FILE_NAME_FK, 16.0f, &iconsConfig, iconsRanges);


    std::vector<FileOps::Record> out_DirectoryItems;
    Path emptyDir("");
    FileOps::enumerateDirectory(emptyDir, out_DirectoryItems);

    for(FileOps::Record record : out_DirectoryItems) {
        printf(": %s\n", record.name.c_str());
    }


    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends

    const char* glsl_version = "#version 460";
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    ImVec4 clearColor{0.1f, 0.1f, 0.1f, 1.0f};

    std::vector<FileOps::Record> directoryItems(64);

    glfwSwapInterval(1);

    Path dir(DebugTestPath);
    dir.toAbsolute();


    std::vector<BrowserWidget> browserWidgets;
    for(int i = 0; i < 2; i++) {
        browserWidgets.push_back(BrowserWidget(dir, &fileOpsWorker));
    }
    
    bool commandPaletteOpen = false;
    std::string commandPaletteInput = "";
    CommandParser cmdParser;

    int currentFocusedWidget = -1;

    MSG msg;
    while(!glfwWindowShouldClose(window)){
        glfwPollEvents();

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        glfwGetWindowSize(window, &width, &height);

        ImGuiWindowFlags containerWindowFlags = ImGuiWindowFlags_NoDocking;

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        containerWindowFlags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
        containerWindowFlags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
        containerWindowFlags |= ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

        ImGui::Begin("ContainerWindow", NULL, containerWindowFlags);

        ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;
        ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);

        ImGui::PopStyleVar(3);

        bool isWindowFocused = false;
        for(int i = 0; i < browserWidgets.size(); i++) {
            // pass index as unique id
            browserWidgets[i].draw(i, isWindowFocused);
            if(isWindowFocused) currentFocusedWidget = i;
        }

        // command palette
        {
            if(glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS 
                    && (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS)) {
                commandPaletteOpen = true;
            }

            int commandWindowFlags = ImGuiWindowFlags_NoDecoration
                | ImGuiWindowFlags_NoDocking
                ;

            if(commandPaletteOpen) {
                ImGui::SetNextWindowSize({width / 3.0f, 50.0f});
                ImGui::SetNextWindowPos({(width / 2.0f) - (width / 6.0f), height / 5.0f});

                ImGui::Begin("###CommandPalette", nullptr, commandWindowFlags);

                int inputFlags = ImGuiInputTextFlags_EnterReturnsTrue
                    | ImGuiInputTextFlags_AutoSelectAll
                    ;

                ImGui::SetNextItemWidth(-1.0f);
                ImGui::SetKeyboardFocusHere(0);
                if(ImGui::InputText("###CommandPaletteInput", &commandPaletteInput, inputFlags) && currentFocusedWidget != -1) {
                    // parse input
                    printf("Parsing command %s\n", commandPaletteInput.c_str());

                    cmdParser.execute(commandPaletteInput, &browserWidgets[currentFocusedWidget]);

                    commandPaletteOpen = false;
                }

                if(ImGui::IsKeyPressed(ImGuiKey_Escape)) commandPaletteOpen = false;
                ImGui::End();
            }
        }

        //ImGui::ShowMetricsWindow();

        {
            ImGui::Begin("File ops progress");

            // consume result queue
            fileOpsWorker.syncProgress();

            // display any in progress operations
            for(FileOp& op : fileOpsWorker.mFileOperations) {
                if(op.idx >= 0) {
                    switch(op.opType) {
                        case FileOpType::FILE_OP_COPY:
                            {
                                ImGui::Text("Copying %s to %s progress %d/%d \n", op.from.str().c_str(), op.to.str().c_str(), op.currentProgress, op.totalProgress);
                            } break;
                        case FileOpType::FILE_OP_MOVE:
                            {
                                ImGui::Text("Moving %s to %s progress %d/%d \n", op.from.str().c_str(), op.to.str().c_str(), op.currentProgress, op.totalProgress);
                            } break;
                        case FileOpType::FILE_OP_DELETE:
                            {
                                ImGui::Text("Deleting %s progress %d/%d \n", op.from.str().c_str(), op.currentProgress, op.totalProgress);
                            } break;
                    }
                }
            }

            ImGui::End();
        }


        ImGui::End(); // end container

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
