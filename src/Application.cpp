#include "Application.h"
#include "imgui_internal.h"
#include "tracy/Tracy.hpp"

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
#include "FileSystem.h"
#include "BrowserWidget.h"

#include "DefaultLayout.h"

static const char* DebugTestPath = "./browser_test/runtime_test";
static const char* DefaultFontFile = "Roboto-Medium.ttf";

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

    //io.Fonts->AddFontDefault();

    ImFontConfig defaultFontConf; defaultFontConf.PixelSnapH = true;
    ImFont* defaultFont = io.Fonts->AddFontFromFileTTF(DefaultFontFile, 16.0f, &defaultFontConf, NULL);

    static const ImWchar iconsRanges[] = { ICON_MIN_FK, ICON_MAX_16_FK, 0 };
    ImFontConfig iconsConfig; iconsConfig.MergeMode = true; iconsConfig.PixelSnapH = true;
    ImFont* iconFont = io.Fonts->AddFontFromFileTTF(FONT_ICON_FILE_NAME_FK, 16.0f, &iconsConfig, iconsRanges);

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

        ImGuiViewport* viewport = ImGui::GetMainViewport();
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

        static std::vector<int> widgetsToClose;
        widgetsToClose.clear();

        // update browser widgets
        for(int i = 0; i < mBrowserWidgets.size(); i++) {
            mBrowserWidgets[i].update();

            if(!mBrowserWidgets[i].isOpen()) {
                widgetsToClose.push_back(i);
            }
            
            if(mBrowserWidgets[i].isFocused()) mCurrentFocusedWidget = i;
        }

        // remove browser widgets that are flagged to be closed
        {
            // move all to end of vector
            int end = mBrowserWidgets.size();
            for(int i : widgetsToClose) {
                end--;
                std::swap(mBrowserWidgets[i], mBrowserWidgets[end]);
            }

            if(end <= mBrowserWidgets.size())
                mBrowserWidgets.erase(std::next(mBrowserWidgets.begin(), end), mBrowserWidgets.end());
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
                ImGui::SetNextWindowSize({mWindowWidth / 3.0f, 0.0f});
                ImGui::SetNextWindowPos({(mWindowWidth / 2.0f) - (mWindowWidth / 6.0f), mWindowHeight / 5.0f});

                ImGui::Begin("###CommandPalette", nullptr, commandWindowFlags);

                int inputFlags = ImGuiInputTextFlags_EnterReturnsTrue
                    //| ImGuiInputTextFlags_AutoSelectAll
                    | ImGuiInputTextFlags_CallbackEdit
                    | ImGuiInputTextFlags_CallbackCompletion
                    | ImGuiInputTextFlags_CallbackHistory
                    ;

                ImGui::SetNextItemWidth(-1.0f);
                ImGui::SetKeyboardFocusHere(0);
                if(ImGui::InputText("###CmdPaletteInput", &mCmdPaletteInput, inputFlags, Application::CmdPalletInputTextCallback, this) && mCurrentFocusedWidget != -1) {
                    // parse input
                    printf("Parsing command %s\n", mCmdPaletteInput.c_str());

                    mCmdParser.execute(mCmdPaletteInput, &mBrowserWidgets[mCurrentFocusedWidget]);

                    mCommandPaletteOpen = false;
                }

                std::vector<std::string_view> cmdNames = mCmdParser.GetCommandNames();
                if(mCmdPaletteInput.empty()) {
                    mCommandCompletionList.clear();
                    for(int i = 0; i < cmdNames.size(); i++) {
                        mCommandCompletionList.push_back(i);
                    }
                }

                // get list of matches
                for(int i = 0; i < mCommandCompletionList.size(); i++) {
                    int cmdIdx = mCommandCompletionList[i];

                    ImGui::Text("%s", cmdNames[cmdIdx].data());
                    if(i == mCmdCompletionSelection) {
                        ImVec2 cursor = ImGui::GetCursorScreenPos();
                        ImVec2 max{cursor.x + ImGui::CalcItemWidth(), cursor.y - ImGui::GetTextLineHeightWithSpacing()};
                        ImGui::GetWindowDrawList()->AddRect(cursor, max, IM_COL32(255, 0, 0, 255));

                        // show tooltip of command

                        ImGui::SetNextWindowSize({(mWindowWidth / 6.0f), 0.0f});
                        ImGui::SetNextWindowPos({(mWindowWidth / 2.0f) + (mWindowWidth / 6.0f), mWindowHeight / 5.0f});

                        CommandType cmdType = CommandParser::StrToCommandType(cmdNames[cmdIdx]);

                        ImGui::Begin("###CommandPaletteHint", nullptr, commandWindowFlags);

                        ImGui::TextWrapped("%s", CommandParser::CommandHint(cmdType).data());

                        ImGui::End();

                    }
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
            int statusBarFlags = ImGuiWindowFlags_NoDecoration
                | ImGuiWindowFlags_NoDocking;
            ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_MenuBar;
            float height = ImGui::GetFrameHeight();

            //ImGui::PushStyleColor(ImGuiCol_MenuBarBg, ImVec4(1.0f, 0.0f, 0.0f, 1.0f)); // Set window background to red
            ImGui::BeginViewportSideBar("##StatusBar", viewport, ImGuiDir_Down, height, window_flags);

            // consume result queue
            mFileOpsWorker.syncProgress();

            ImGui::BeginMenuBar();

            if(mFileOpsWorker.numOperationsInProgress() <= 0) {
                ImGui::Text("IDLE");
            } else {
                ImGui::Text(" %d Operation(s) in progress |", mFileOpsWorker.numOperationsInProgress());
            }

            // display the first in-progress file operation
            for(BatchFileOperation& op : mFileOpsWorker.mFileOperations) {
                if(op.idx >= 0) {
                    ImGui::Text("#%d - %d/%d", op.idx, op.currentProgress, op.totalProgress);
                    ImGui::SameLine();
                    ImGui::ProgressBar((float)op.currentProgress / (float)op.totalProgress);
                    break;
                }
            }

            ImGui::EndMenuBar();
            //ImGui::PopStyleColor();

            ImGui::End();
        }

        fileOperationHistory();

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

        FrameMark;
    }
}

void Application::fileOperationHistory() {
    if(glfwGetKey(mWindow, GLFW_KEY_H) == GLFW_PRESS 
            && (glfwGetKey(mWindow, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS || glfwGetKey(mWindow, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS)) {
        mHistoryWindowOpen = true;
    }

    if(!mHistoryWindowOpen) return;

    if(!ImGui::Begin("File operation history", &mHistoryWindowOpen)) {
        ImGui::End();
        return;
    }

    for(const BatchFileOperation& batchOp : mFileOpsWorker.mHistory) {
    
        for(const BatchFileOperation::Operation& op : batchOp.operations) {
            std::string fromLastSegment = op.from.getLastSegment();
            std::string toLastSegment = op.to.getLastSegment();
            switch(op.opType) {
                case FileOpType::FILE_OP_COPY:
                    {
                        ImGui::Text("Copy %s to %s", fromLastSegment.c_str(), toLastSegment.c_str());
                    } break;
                case FileOpType::FILE_OP_MOVE:
                    {
                        ImGui::Text("Move %s to %s", fromLastSegment.c_str(), toLastSegment.c_str());
                    } break;
                case FileOpType::FILE_OP_DELETE:
                    {
                        ImGui::Text("Delete %s", fromLastSegment.c_str());
                    } break;
                case FileOpType::FILE_OP_RENAME:
                    {
                        ImGui::Text("Rename %s to %s", fromLastSegment.c_str(), op.to.str().c_str());
                    } break;
            }
        }

        ImGui::Separator();
    }

    ImGui::End();

}

int Application::CmdPalletInputTextCallback(ImGuiInputTextCallbackData* data) {
    Application* app = static_cast<Application*>(data->UserData);
    if(app == nullptr) return 0;

    std::string_view buffer(data->Buf, data->BufTextLen);

    app->mCommandCompletionList.clear();

    // get all commands
    std::vector<std::string_view> cmdNames = app->mCmdParser.GetCommandNames();

    auto xMatches = [&data, &buffer](const std::string_view cmdName) {
        if(data->BufTextLen > 0) {
            return cmdName.find(buffer) != std::string::npos;
        } else {
            return true;
        }
    };

    for(int i = 0; i < cmdNames.size(); i++) {
        std::string_view name = cmdNames[i];
        if(xMatches(name)) {
            app->mCommandCompletionList.push_back(i);
        }
    }

    if(data->EventKey == ImGuiKey_UpArrow) {
        app->mCmdCompletionSelection--;
    } else if(data->EventKey == ImGuiKey_DownArrow) {
        app->mCmdCompletionSelection++;
    }

    if(app->mCommandCompletionList.empty()) {
        app->mCmdCompletionSelection = -1;
    } else {
        if(app->mCmdCompletionSelection >= static_cast<int>(app->mCommandCompletionList.size())) {
            app->mCmdCompletionSelection = 0;
        } else if(app->mCmdCompletionSelection < 0) {
            app->mCmdCompletionSelection = app->mCommandCompletionList.size() - 1;
        }
    }

    if(data->EventKey == ImGuiKey_Tab) {
        data->DeleteChars(0, data->BufTextLen);
        data->InsertChars(0, cmdNames[app->mCommandCompletionList[app->mCmdCompletionSelection]].data());
        data->BufDirty = true;
    } 

    return 0;
}

