#pragma once
#include <unordered_map>
#include "Path.h"
#include "SortDirection.h"
#include "FileSystem.h"

enum DirectorySortFlags {
    DIRECTORY_SORT_NONE      = 0,
    DIRECTORY_SORT_NAME      = 1 << 1,
    DIRECTORY_SORT_DATE      = 1 << 2,
    DIRECTORY_SORT_FILE_SIZE = 1 << 3,
};

class DirectoryWatcher {
    using DirectoryPath = std::string;

public:
    void setSort(DirectorySortFlags flags, FileSystem::SortDirection);
    void changeDirectory(const Path& newPath);
    bool update();

    Path mDirectory;

    FileSystem::SOARecord mRecords;
    FileSystem::SortDirection mSortDirection;
    int mSortFlags = DIRECTORY_SORT_NAME;
    bool mUpdateDirectory = true;
    bool mUpdateSort = true;
    void* mDirChangeHandle = nullptr;
    bool errors = false; // FIXME: replace with enum
};
