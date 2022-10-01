#pragma once

#include "types.h"

#include "Path.h"

#include <vector>
#include <unordered_map>

struct ImDrawList;

namespace FileOps {
    struct Record;
}

class FileOpsWorker;

class BrowserWidget
{

public:
    BrowserWidget(const Path& path, FileOpsWorker* fileOpsWorker);

    void beginFrame();
    void setCurrentDirectory(const Path& path);
    void draw();

private:
    FileOpsWorker* mFileOpsWorker;
    Path mCurrentDirectory;

    std::vector<FileOps::Record> mDisplayList;
    std::vector<bool> mSelected;
    std::vector<int> mMovePayload;
    int mNumSelected = 0;
    bool mUpdateFlag;

    std::vector<Path> mClipboard;

    void* mDirChangeHandle = nullptr;

    int mYScroll;
};

