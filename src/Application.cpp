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

    // FIXME: Need to fix keyboard navigation
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

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

    mQuickAccessLinks.push_back({ FileSystem::getKnownFolderPath(FileSystem::KnownFolder::Documents), "Documents" });
    mQuickAccessLinks.push_back({ FileSystem::getKnownFolderPath(FileSystem::KnownFolder::Desktop), "Desktop" });
    mQuickAccessLinks.push_back({ FileSystem::getKnownFolderPath(FileSystem::KnownFolder::Downloads), "Downloads" });
    mQuickAccessLinks.push_back({ FileSystem::getKnownFolderPath(FileSystem::KnownFolder::Music), "Music" });
    mQuickAccessLinks.push_back({ FileSystem::getKnownFolderPath(FileSystem::KnownFolder::Videos), "Videos" });
    mQuickAccessLinks.push_back({ FileSystem::getKnownFolderPath(FileSystem::KnownFolder::Pictures), "Pictures" });
    mQuickAccessLinks.push_back({ FileSystem::getKnownFolderPath(FileSystem::KnownFolder::ProgramData), "ProgramData" });
    mQuickAccessLinks.push_back({ FileSystem::getKnownFolderPath(FileSystem::KnownFolder::ProgramFilesX64), "ProgramFilesX64" });
    mQuickAccessLinks.push_back({ FileSystem::getKnownFolderPath(FileSystem::KnownFolder::ProgramFilesX86), "ProgramFilesX86" });
    mQuickAccessLinks.push_back({ FileSystem::getKnownFolderPath(FileSystem::KnownFolder::RoamingAppData), "RoamingAppData" });
    mQuickAccessLinks.push_back({ FileSystem::getKnownFolderPath(FileSystem::KnownFolder::LocalAppData), "LocalAppData" });
    
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

        std::vector<int> widgetsToClose;

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

        // Command Palette
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
                ImGui::Text("Command Palette");

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
                    mCmdPaletteInput.clear();
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

        // Quick Access 
        {
            // TODO: check if command palette is open

            if(glfwGetKey(mWindow, GLFW_KEY_O) == GLFW_PRESS 
                    && (glfwGetKey(mWindow, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS || glfwGetKey(mWindow, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS)) {
                mQuickAccessWindowOpen = true;
            }

            int quickWindowFlags = ImGuiWindowFlags_NoDecoration
                | ImGuiWindowFlags_NoDocking
                ;

            if(mQuickAccessWindowOpen) {
                ImGui::SetNextWindowSize({mWindowWidth / 3.0f, 0.0f});
                ImGui::SetNextWindowPos({(mWindowWidth / 2.0f) - (mWindowWidth / 6.0f), mWindowHeight / 5.0f});

                ImGui::Begin("###QuickAccess", nullptr, quickWindowFlags);
                ImGui::Text("Quick Access");

                int inputFlags = ImGuiInputTextFlags_EnterReturnsTrue
                    //| ImGuiInputTextFlags_AutoSelectAll
                    | ImGuiInputTextFlags_CallbackEdit
                    | ImGuiInputTextFlags_CallbackCompletion
                    | ImGuiInputTextFlags_CallbackHistory
                    ;

                ImGui::SetNextItemWidth(-1.0f);
                ImGui::SetKeyboardFocusHere(0);
                if(ImGui::InputText("###QuickAccessInput", &mQuickAccessInput, inputFlags, Application::QuickAccessInputTextCallback, this) && mCurrentFocusedWidget != -1) {

                    if(mQuickAccessSelection != -1) {
                        QuickAccessLink& link = mQuickAccessLinks[mQuickAccessCompletionList[mQuickAccessSelection]];
                        BrowserWidget& widget = mBrowserWidgets[mCurrentFocusedWidget];

                        widget.setCurrentDirectory(link.path);
                    }

                    mQuickAccessInput.clear();
                    mQuickAccessWindowOpen = false;
                }

                if(mQuickAccessInput.empty()) {
                    mQuickAccessCompletionList.clear();
                    for(int i = 0; i < mQuickAccessLinks.size(); i++) {
                        mQuickAccessCompletionList.push_back(i);
                    }
                }

                // get list of matches
                for(int i = 0; i < mQuickAccessCompletionList.size(); i++) {
                    int linkIdx = mQuickAccessCompletionList[i];

                    ImGui::Text("%s", mQuickAccessLinks[linkIdx].displayName.data());
                    if(i == mQuickAccessSelection) {
                        ImVec2 cursor = ImGui::GetCursorScreenPos();
                        ImVec2 max{cursor.x + ImGui::CalcItemWidth(), cursor.y - ImGui::GetTextLineHeightWithSpacing()};
                        ImGui::GetWindowDrawList()->AddRect(cursor, max, IM_COL32(255, 0, 0, 255));
                    }
                }

                if(ImGui::IsKeyPressed(ImGuiKey_Escape)) mQuickAccessWindowOpen = false;
                ImGui::End();
            }
        }

        // Status Bar
        {
            int statusBarFlags = ImGuiWindowFlags_NoDecoration
                | ImGuiWindowFlags_NoDocking;
            ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_MenuBar;
            float height = ImGui::GetFrameHeight();

            //ImGui::PushStyleColor(ImGuiCol_MenuBarBg, ImVec4(1.0f, 0.0f, 0.0f, 1.0f)); // Set window background to red
            ImGui::BeginViewportSideBar("##StatusBar", viewport, ImGuiDir_Down, height, windowFlags);

            ImGui::BeginMenuBar();

            if(mFileOpsWorker.numOperationsInProgress() <= 0) {
                ImGui::Text("IDLE");
            } else {
                ImGui::Text(" %d Operation(s) in progress |", mFileOpsWorker.numOperationsInProgress());
            }

            // display the first in-progress file operation
            if(mFileOpsWorker.numOperationsInProgress() > 0) {
                BatchFileOperation& op = mFileOpsWorker.getCurrentOperation();
                if(op.idx >= 0) {
                    ImGui::Text("#%d - %d/%d", op.idx, op.currentProgress, op.totalProgress);
                    ImGui::SameLine();
                    if(mFileOpsWorker.isPaused()) {
                        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
                        ImGui::ProgressBar((float)op.currentProgress / (float)op.totalProgress);
                        ImGui::PopStyleColor();
                    } else {
                        ImGui::ProgressBar((float)op.currentProgress / (float)op.totalProgress);
                    }
                }
            }

            ImGui::EndMenuBar();
            //ImGui::PopStyleColor();

            ImGui::End();
        }


        fileOperationStatusWindow();
        fileOperationHistoryWindow();

        // Debug stuff
        {
            //ImGui::Begin("debug window");
            //ImGui::ShowMetricsWindow();
            //ImGui::ShowFontSelector("Font selector");
            //ImGui::ShowStyleEditor();
            //ImGui::End();
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

        FrameMark;
    }
}

void Application::fileOperationHistoryWindow() {
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

// show window for any current file operations
void Application::fileOperationStatusWindow() {

    if(mFileOpsWorker.numOperationsInProgress() > 0) {
        ImGui::SetNextWindowSize({mWindowWidth / 3.0f, 0.0f}, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos({(mWindowWidth / 2.0f) - (mWindowWidth / 6.0f), mWindowHeight / 5.0f}, ImGuiCond_FirstUseEver);

        int fileOpStatusFlags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoCollapse;
        ImGui::Begin("File Operation Status", NULL, fileOpStatusFlags);

        if(mFileOpsWorker.isPaused()) {
            if(ImGui::Button(ICON_FK_PLAY)) {
                mFileOpsWorker.resumeOperation();
            }
        } else {
            if(ImGui::Button(ICON_FK_PAUSE)) {
                mFileOpsWorker.flagPauseOperation();
            }
        }

        // display the first in-progress file operation
        if(mFileOpsWorker.numOperationsInProgress() > 0) {
            BatchFileOperation& op = mFileOpsWorker.getCurrentOperation();

            const std::string& desc = op.currentOpDescription;
            switch(op.currentOpType) {
                case FileOpType::FILE_OP_COPY:
                    {
                        ImGui::Text("Copying %s", desc.c_str());
                    } break;
                case FileOpType::FILE_OP_MOVE:
                    {
                        ImGui::Text("Moving %s", desc.c_str());
                    } break;
                case FileOpType::FILE_OP_DELETE:
                    {
                        ImGui::Text("Deleting %s", desc.c_str());
                    } break;
                case FileOpType::FILE_OP_RENAME:
                    {
                        ImGui::Text("Renaming %s", desc.c_str());
                    } break;
            }

            if(mFileOpsWorker.isPaused()) {
                ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
                ImGui::ProgressBar((float)op.currentProgress / (float)op.totalProgress);
                ImGui::PopStyleColor();
            } else {
                ImGui::ProgressBar((float)op.currentProgress / (float)op.totalProgress);
            }
        }

        ImGui::End();
    }
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

int Application::QuickAccessInputTextCallback(ImGuiInputTextCallbackData* data) {
    Application* app = static_cast<Application*>(data->UserData);
    if(app == nullptr) return 0;

    std::string_view buffer(data->Buf, data->BufTextLen);

    app->mQuickAccessCompletionList.clear();

    // get all commands
    auto xMatches = [&data, &buffer](const std::string_view itemName) {
        if(data->BufTextLen > 0) {
            return itemName.find(buffer) != std::string::npos;
        } else {
            return true;
        }
    };

    for(int i = 0; i < app->mQuickAccessLinks.size(); i++) {
        if(xMatches(app->mQuickAccessLinks[i].displayName)) {
            app->mQuickAccessCompletionList.push_back(i);
        }
    }

    if(data->EventKey == ImGuiKey_UpArrow) {
        app->mQuickAccessSelection--;
    } else if(data->EventKey == ImGuiKey_DownArrow) {
        app->mQuickAccessSelection++;
    }

    if(app->mQuickAccessCompletionList.empty()) {
        app->mQuickAccessSelection = -1;
    } else {
        if(app->mQuickAccessSelection >= static_cast<int>(app->mQuickAccessCompletionList.size())) {
            app->mQuickAccessSelection = 0;
        } else if(app->mQuickAccessSelection < 0) {
            app->mQuickAccessSelection = app->mQuickAccessCompletionList.size() - 1;
        }
    }

    if(data->EventKey == ImGuiKey_Tab) {
        data->DeleteChars(0, data->BufTextLen);
        data->InsertChars(0, app->mQuickAccessLinks[app->mQuickAccessCompletionList[app->mQuickAccessSelection]].displayName.data());
        data->BufDirty = true;
    } 

    return 0;
}
