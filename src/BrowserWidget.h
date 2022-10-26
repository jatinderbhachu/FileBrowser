#pragma once

#include "Path.h"
#include "SortDirection.h"
#include "DirectoryWatcher.h"

#include <vector>
#include <unordered_map>
#include <numeric>

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

    struct Selection {
        std::vector<bool> selected;
        std::vector<size_t> indexes;
        int rangeSelectionStart = 0; // the first selected item when doing shift-click selection
        
        inline void clear() { 
            selected.assign(selected.size(), false); 
            indexes.clear();
        }

        inline void toggle(size_t i) {
            selected[i] = !selected[i];
            
            rangeSelectionStart = i;
            indexes.push_back(i);
        }

        inline void selectOne(size_t i) {
            clear();

            selected[i] = true;
            indexes.push_back(i);
            rangeSelectionStart = i;
        }

        inline void selectRange(size_t end) {
            size_t start, selectionSize = 0;

            clear();

            if(rangeSelectionStart > end) {
                start = end;
                selectionSize = (rangeSelectionStart + 1) - start;
            } else if(rangeSelectionStart < end) {
                start = rangeSelectionStart;
                selectionSize = (end + 1) - start;
            } else if(rangeSelectionStart == end) {
                rangeSelectionStart = end;
                start = rangeSelectionStart;
                selectionSize = 1;
            }

            indexes.resize(selectionSize);

            std::fill_n(selected.begin() + start, selectionSize, true);
            std::iota(indexes.begin(), indexes.begin() + selectionSize, start);
        }

        inline size_t   count() { return indexes.size(); }
        inline void     resize(size_t size) { selected.resize(size); }
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

    Selection mSelection;

    // EDIT file name
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

