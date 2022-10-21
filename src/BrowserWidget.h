#pragma once

#include "Path.h"
#include "SortDirection.h"

#include <vector>
#include <unordered_map>

struct ImDrawList;

namespace FileSystem {
    struct Record;
}

class FileOpsWorker;

struct ImGuiTableSortSpecs;

class BrowserWidget
{
    struct MovePayload {
        Path sourcePath;
        std::vector<FileSystem::Record>* sourceDisplayList;
        std::vector<int> itemsToMove;
    };

    struct DriveRecord {
        std::string name;
        char letter;
    };

    enum class DisplayListType {
        DEFAULT = 0, // typical folders and files 
        DRIVE,       // list of drive 
        PATH_NOT_FOUND_ERROR        // path wasn't found, display error
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


    DisplayListType mDisplayListType = DisplayListType::DEFAULT;
    std::vector<FileSystem::Record> mDisplayList;
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

    FileSystem::SortDirection mSortDirection = FileSystem::SortDirection::Descending;
    ImGuiTableSortSpecs* mTableSortSpecs = nullptr;

    void* mDirChangeHandle = nullptr;
    int mRangeSelectionStart = -1; // the first selected item when doing shift-click selection

    int mID;
    inline static int IDCounter = 0;
};

