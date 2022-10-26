#pragma once

#include "Path.h"
#include "SortDirection.h"
#include "DirectoryWatcher.h"

#include <vector>
#include <unordered_map>

struct ImDrawList;

namespace FileSystem {
    struct SOARecord;
}

class FileOpsWorker;
class DirectoryWatcher;

struct ImGuiTableSortSpecs;

class BrowserWidget
{
    struct MovePayload {
        Path sourcePath;
        FileSystem::SOARecord* sourceDisplayList;
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
    void update();

    Path getCurrentDirectory() const;

    void renameSelected(const std::string& from, const std::string& to);

    inline bool isOpen() const { return mIsOpen; }
    inline bool isFocused() const { return mIsFocused; }

private:
    void directorySegments();
    void directoryTable();
    void driveList();

    void updateSearch();
    void acceptMovePayload(Path target);
    void handleInput();

    FileOpsWorker* mFileOpsWorker;
    DirectoryWatcher mDirectoryWatcher;
    Path mCurrentDirectory;
    Path mPreviousDirectory;

    DisplayListType mDisplayListType = DisplayListType::DEFAULT;
    std::vector<DriveRecord> mDriveList;

    // SEARCH
    std::vector<bool> mHighlighted;
    int mCurrentHighlightIdx = -1;
    bool mHighlightNextItem = false;
    bool mSearchWindowOpen = false;
    std::string mSearchFilter;

    // SELECTION
    std::vector<bool> mSelected;
    int mNumSelected = 0;
    int mRangeSelectionStart = 0; // the first selected item when doing shift-click selection

    // EDIT name
    std::string mEditInput;
    int mEditIdx = -1;

    bool mDirectoryChanged = true;

    bool mIsOpen = true;
    bool mIsFocused = false;
    bool mWasFocused = false;

    std::vector<Path> mClipboard;
    MovePayload mMovePayload;

    ImGuiTableSortSpecs* mTableSortSpecs = nullptr;

    int mID;
    inline static int IDCounter = 0;
};

