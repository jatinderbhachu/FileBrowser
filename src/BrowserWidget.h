#pragma once

#include "Path.h"
#include "SortDirection.h"

#include <vector>
#include <unordered_map>

struct ImDrawList;

namespace FileOps {
    struct Record;
}

class FileOpsWorker;

struct ImGuiTableSortSpecs;

class BrowserWidget
{
    struct MovePayload {
        Path sourcePath;
        std::vector<FileOps::Record>* sourceDisplayList;
        std::vector<int> itemsToMove;
    };

    struct DriveRecord {
        std::string name;
        char letter;
    };

public:
    BrowserWidget(const Path& path, FileOpsWorker* fileOpsWorker);

    void setCurrentDirectory(const Path& path);
    void update(bool& isFocused, bool& isOpenFlag);

    Path getCurrentDirectory() const;

    void renameSelected(const std::string& from, const std::string& to);

private:
    void directorySegments();
    void directoryTable();
    void driveList();

    void updateSearch();

    FileOpsWorker* mFileOpsWorker;
    Path mCurrentDirectory;

    std::vector<FileOps::Record> mDisplayList;
    
    std::vector<DriveRecord> mDriveList;

    std::vector<bool> mHighlighted;
    int mCurrentHighlightIdx = -1;
    bool mSearchWindowOpen = false;
    bool mHighlightNextItem = false;
    std::string mSearchFilter;

    std::vector<bool> mSelected;
    int mNumSelected = 0;
    bool mUpdateFlag = true;

    std::vector<Path> mClipboard;
    MovePayload mMovePayload;

    FileOps::SortDirection mSortDirection = FileOps::SortDirection::Descending;
    ImGuiTableSortSpecs* mTableSortSpecs = nullptr;

    void* mDirChangeHandle = nullptr;
    int mRangeSelectionStart = -1; // the first selected item when doing shift-click selection

    int mID;
    inline static int IDCounter = 0;
};

