#pragma once

#include "types.h"

#include "Path.h"
#include "SortDirection.h"

#include <vector>
#include <unordered_map>

struct ImDrawList;

namespace FileOps {
    struct Record;
}

class FileOpsWorker;

class BrowserWidget
{

    struct MovePayload {
        Path sourcePath;
        std::vector<FileOps::Record>* sourceDisplayList;
        std::vector<int> itemsToMove;
    };

public:
    BrowserWidget(const Path& path, FileOpsWorker* fileOpsWorker);

    void setCurrentDirectory(const Path& path);
    void draw(int id);

private:
    FileOpsWorker* mFileOpsWorker;
    Path mCurrentDirectory;

    std::vector<FileOps::Record> mDisplayList;
    std::vector<bool> mSelected;
    int mNumSelected = 0;
    bool mUpdateFlag;

    std::vector<Path> mClipboard;
    MovePayload mMovePayload;

    FileOps::SortDirection mSortDirection = FileOps::SortDirection::Descending;

    void* mDirChangeHandle = nullptr;
};

