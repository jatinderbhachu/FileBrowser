#pragma once
#include "FileOpsWorker.h"
#include "CommandParser.h"

struct GLFWwindow;
class BrowserWidget;

struct ImGuiInputTextCallbackData;

class Application
{
public:
    Application();
    ~Application();

    void run();

    std::vector<int> mCommandCompletionList;
    int mCmdCompletionSelection = -1;
    CommandParser mCmdParser;

private:

    static int CmdPalletInputTextCallback(ImGuiInputTextCallbackData* data);

    int mWindowWidth = 1280;
    int mWindowHeight = 720;

    GLFWwindow* mWindow;
    FileOpsWorker mFileOpsWorker;

    std::vector<BrowserWidget> mBrowserWidgets;
    int mCurrentFocusedWidget = -1;
    std::string mCmdPaletteInput;
    bool mCommandPaletteOpen = false;
};

