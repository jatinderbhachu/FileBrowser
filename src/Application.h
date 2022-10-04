#pragma once
#include "FileOpsWorker.h"
#include "CommandParser.h"

struct GLFWwindow;
class BrowserWidget;

class Application
{
public:
    Application();
    ~Application();

    void run();

private:
    int mWindowWidth = 1280;
    int mWindowHeight = 720;

    GLFWwindow* mWindow;
    FileOpsWorker mFileOpsWorker;

    std::vector<BrowserWidget> mBrowserWidgets;
    int mCurrentFocusedWidget = -1;

    CommandParser mCmdParser;
    std::string mCmdPaletteInput;
    bool mCommandPaletteOpen = false;
};

