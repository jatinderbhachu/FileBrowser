#include "Application.h"

#include <IconsForkAwesome.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define GLFW_INCLUDE_NONE
#include "glad/glad.h"
#include <GLFW/glfw3.h>

#include <imgui.h>
#include <backends/imgui_impl_opengl3.h>
#include <backends/imgui_impl_glfw.h>
#include <misc/cpp/imgui_stdlib.h>


#include "Path.h"
#include "FileOps.h"
#include "BrowserWidget.h"

#include "DefaultLayout.h"

static const char* DebugTestPath = "./browser_test/runtime_test";

Application::Application() {
    glfwInit();

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    mWindow = glfwCreateWindow(mWindowWidth, mWindowHeight, "File Browser", NULL, NULL);
    glfwMakeContextCurrent(mWindow);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    // load hard coded layout default for now...
    ImGui::LoadIniSettingsFromMemory(DefaultLayoutSettings);

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;       // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking

    io.Fonts->AddFontDefault();

    static const ImWchar iconsRanges[] = { ICON_MIN_FK, ICON_MAX_16_FK, 0 };
    ImFontConfig iconsConfig; iconsConfig.MergeMode = true; iconsConfig.PixelSnapH = true;
    ImFont* iconFont = io.Fonts->AddFontFromFileTTF(FONT_ICON_FILE_NAME_FK, 13.0f, &iconsConfig, iconsRanges);

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    const char* glsl_version = "#version 460";
    ImGui_ImplGlfw_InitForOpenGL(mWindow, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // vsync
    glfwSwapInterval(1);

    Path baseDir(DebugTestPath);
    baseDir.toAbsolute();

    for(int i = 0; i < 2; i++) {
        mBrowserWidgets.push_back(BrowserWidget(baseDir, &mFileOpsWorker));
    }

}

Application::~Application() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwTerminate();
}

void Application::run() {
    while(!glfwWindowShouldClose(mWindow)){
        glfwPollEvents();

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        glfwGetWindowSize(mWindow, &mWindowWidth, &mWindowHeight);

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

        // ctrl+n for new window
        if(ImGui::IsKeyDown(ImGuiKey_LeftCtrl) && ImGui::IsKeyPressed(ImGuiKey_N)) {
            mBrowserWidgets.push_back(BrowserWidget(Path(""), &mFileOpsWorker));
        }

        bool isWindowFocused = false;
        // update browser widgets
        for(int i = 0; i < mBrowserWidgets.size(); i++) {
            // pass index as unique id
            mBrowserWidgets[i].update(i, isWindowFocused);
            if(isWindowFocused) mCurrentFocusedWidget = i;
        }

        // command palette
        {
            if(glfwGetKey(mWindow, GLFW_KEY_P) == GLFW_PRESS 
                    && (glfwGetKey(mWindow, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS || glfwGetKey(mWindow, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS)) {
                mCommandPaletteOpen = true;
            }

            int commandWindowFlags = ImGuiWindowFlags_NoDecoration
                | ImGuiWindowFlags_NoDocking
                ;

            if(mCommandPaletteOpen) {
                ImGui::SetNextWindowSize({mWindowWidth / 3.0f, 50.0f});
                ImGui::SetNextWindowPos({(mWindowWidth / 2.0f) - (mWindowWidth / 6.0f), mWindowHeight / 5.0f});

                ImGui::Begin("###CommandPalette", nullptr, commandWindowFlags);

                int inputFlags = ImGuiInputTextFlags_EnterReturnsTrue
                    | ImGuiInputTextFlags_AutoSelectAll
                    ;

                ImGui::SetNextItemWidth(-1.0f);
                ImGui::SetKeyboardFocusHere(0);
                if(ImGui::InputText("###CmdPaletteInput", &mCmdPaletteInput, inputFlags) && mCurrentFocusedWidget != -1) {
                    // parse input
                    printf("Parsing command %s\n", mCmdPaletteInput.c_str());

                    mCmdParser.execute(mCmdPaletteInput, &mBrowserWidgets[mCurrentFocusedWidget]);

                    mCommandPaletteOpen = false;
                }

                if(ImGui::IsKeyPressed(ImGuiKey_Escape)) mCommandPaletteOpen = false;
                ImGui::End();
            }
        }

        //ImGui::Begin("debug window");
        //ImGui::ShowMetricsWindow();
        //ImGui::ShowFontSelector("Font selector");
        //ImGui::ShowStyleEditor();
        //ImGui::End();

        {
            ImGui::Begin("File ops progress");

            // consume result queue
            mFileOpsWorker.syncProgress();

            // display any in-progress operations
            for(FileOp& op : mFileOpsWorker.mFileOperations) {
                if(op.idx >= 0) {
                    std::string fromLastSegment = op.from.getLastSegment();
                    std::string toLastSegment = op.to.getLastSegment();
                    switch(op.opType) {
                        case FileOpType::FILE_OP_COPY:
                            {
                                ImGui::Text("Copying %s to %s", fromLastSegment.c_str(), toLastSegment.c_str());
                            } break;
                        case FileOpType::FILE_OP_MOVE:
                            {
                                ImGui::Text("Moving %s to %s", fromLastSegment.c_str(), toLastSegment.c_str());
                            } break;
                        case FileOpType::FILE_OP_DELETE:
                            {
                                ImGui::Text("Deleting %s", fromLastSegment.c_str());
                            } break;
                        case FileOpType::FILE_OP_RENAME:
                            {
                                ImGui::Text("Rename %s", fromLastSegment.c_str());
                            } break;
                    }
                    ImGui::SameLine();
                    ImGui::ProgressBar((float)op.currentProgress / (float)op.totalProgress);
                }
            }

            ImGui::End();
        }


        ImGui::End(); // end container

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(mWindow, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);

        ImVec4 clearColor{0.1f, 0.1f, 0.1f, 1.0f};
        glClearColor(clearColor.x * clearColor.w, clearColor.y * clearColor.w, clearColor.z * clearColor.w, clearColor.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(mWindow);
    }
}
