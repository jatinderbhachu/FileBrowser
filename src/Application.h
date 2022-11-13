#pragma once
#include "FileOpsWorker.h"
#include "CommandParser.h"

struct GLFWwindow;
class BrowserWidget;

struct ImGuiInputTextCallbackData;

class Application
{
    struct QuickAccessLink {
        Path path;
        std::string displayName;
    };

public:
    Application();
    ~Application();

    void run();

private:

    static int CmdPalletInputTextCallback(ImGuiInputTextCallbackData* data);
    static int QuickAccessInputTextCallback(ImGuiInputTextCallbackData* data);


    void fileOperationStatusWindow();
    void fileOperationHistoryWindow();

    bool mHistoryWindowOpen = false;

    std::string mQuickAccessInput;
    bool mQuickAccessWindowOpen = false;

    std::string mCmdPaletteInput;
    bool mCommandPaletteOpen = false;

    std::vector<int> mCommandCompletionList;
    int mCmdCompletionSelection = -1;

    std::vector<int> mQuickAccessCompletionList;
    int mQuickAccessSelection = -1;


    int mWindowWidth = 1280;
    int mWindowHeight = 720;

    GLFWwindow* mWindow;
    FileOpsWorker mFileOpsWorker;
    CommandParser mCmdParser;

    std::vector<QuickAccessLink> mQuickAccessLinks;

    std::vector<BrowserWidget> mBrowserWidgets;
    int mCurrentFocusedWidget = -1;
};

