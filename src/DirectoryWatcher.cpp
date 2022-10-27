#include "DirectoryWatcher.h"
#include "Path.h"
#include "FileSystem.h"
#include <assert.h>

#ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>


bool DirectoryWatcher::update() {

    bool wasUpdated = false;

    DWORD waitStatus;
    waitStatus = WaitForSingleObject(mDirChangeHandle, 0);

    // check if any changes occurred
    switch(waitStatus) {
        case WAIT_OBJECT_0:
            {
                mUpdateDirectory = true;
                mUpdateSort = true;
                FindNextChangeNotification(mDirChangeHandle);
            } break;
        default:
            {
            } break;
    }

    if(mUpdateDirectory) {
        wasUpdated = true;
        mUpdateDirectory = false;
        mRecords.clear();
        errors = FileSystem::enumerateDirectory(mDirectory, mRecords);

        if(mDirChangeHandle != INVALID_HANDLE_VALUE) {
            FindCloseChangeNotification(mDirChangeHandle);
        }

        mDirChangeHandle = FindFirstChangeNotificationW(mDirectory.wstr().data(), FALSE, 
                FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME
                );
    }

    if(mUpdateSort) {
        mUpdateSort = false;

        if(mSortFlags & DIRECTORY_SORT_NAME) {
            mRecords.sortByName(mSortDirection);
        } 

        if(mSortFlags & DIRECTORY_SORT_DATE) {
            mRecords.sortByLastModified(mSortDirection);
        } 

        if(mSortFlags & DIRECTORY_SORT_FILE_SIZE) {
            mRecords.sortBySize(mSortDirection);
        } 

        mRecords.sortByType(mSortDirection);
    }

    return wasUpdated;
}

void DirectoryWatcher::changeDirectory(const Path& newPath) {
    mUpdateDirectory = true;
    mUpdateSort = true;
    mDirectory = newPath;
}

void DirectoryWatcher::setSort(DirectorySortFlags sortFlags, FileSystem::SortDirection direction) {
    mUpdateSort = true;
    mSortFlags = sortFlags;
    mSortDirection = direction;
}

