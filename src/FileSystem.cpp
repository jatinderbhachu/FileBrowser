#include "FileSystem.h"
#include "FileOpsProgressSink.h"
#include <fileapi.h>

#ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "StringUtils.h"
#include <Shellapi.h>

#include <ocidl.h>

#include <algorithm>

#include <shlobj.h>
#include <shlwapi.h>
#include <assert.h>

#include "Path.h"
#include "NaturalComparator.h"

namespace FileSystem {

void sortByName(SortDirection direction, std::vector<Record>& out_DirectoryItems) {
    if(direction == SortDirection::Ascending) {
        std::sort(out_DirectoryItems.begin(), out_DirectoryItems.end(), [](const Record& lhs, const Record& rhs) { return NaturalComparator(rhs.name, lhs.name); });
    } else {
        std::sort(out_DirectoryItems.begin(), out_DirectoryItems.end(), [](const Record& lhs, const Record& rhs) { return NaturalComparator(lhs.name, rhs.name); });
    }
}

void sortByType(SortDirection direction, std::vector<Record>& out_DirectoryItems) {
    if(direction == SortDirection::Ascending) {
        std::sort(out_DirectoryItems.begin(), out_DirectoryItems.end(), [](const Record& lhs, const Record& rhs) { return lhs.isFile() < rhs.isFile(); });
    } else {
        std::sort(out_DirectoryItems.begin(), out_DirectoryItems.end(), [](const Record& lhs, const Record& rhs) { return lhs.isFile() > rhs.isFile(); });
    }
}

bool createDirectory(const Path& path) {
    // returns 0 if failed.
    int result = CreateDirectoryW(path.wstr().c_str(), nullptr);

    return result != 0;
}

void getDriveLetters(std::vector<char> &out_driveLetters) {
    DWORD driveBits = GetLogicalDrives();

    for(int i = 0; i < 32; i++) {
        if(driveBits & (1 << i)) {
            out_driveLetters.push_back(static_cast<int>('A') + i);
        }
    }
}

void openFile(const Path& path) {
    ShellExecuteW(0, 0, path.wstr().c_str(), 0, 0, SW_SHOW);
}

// path must be an absolute path
bool enumerateDirectory(const Path& path, std::vector<Record>& out_DirectoryItems) {
    if(path.isEmpty()) return false;
    if(!doesPathExist(path)) return false;

    out_DirectoryItems.clear();

    WIN32_FIND_DATAW findFileData;
    HANDLE hFind;

    const std::string& pathStr = path.str();
    std::string dir = pathStr + "/*";
    std::wstring wString = Util::Utf8ToWstring(dir);

    hFind = FindFirstFileW(wString.c_str(), &findFileData);

    if(hFind == INVALID_HANDLE_VALUE) {
        printf("Can't find dir %s\n", dir.data());
        return false;
    }

    do {
        std::string filename = Util::WstringToUtf8(findFileData.cFileName);
        if(filename == "." || filename == "..") continue;

        // do not include system files
        if(findFileData.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM) continue;

        Record file;
        file.name = filename;

        FILETIME lastWriteTime{};
        FileTimeToLocalFileTime(&findFileData.ftLastWriteTime, &lastWriteTime);

        SYSTEMTIME systemTime{};
        FileTimeToSystemTime(&lastWriteTime, &systemTime);

        file.lastModified.year   = systemTime.wYear;
        file.lastModified.month  = systemTime.wMonth;
        file.lastModified.day    = systemTime.wDay;


        file.lastModified.isPM = systemTime.wHour < 12 ? false : true;

        file.lastModified.hour = systemTime.wHour;
        file.lastModified.hour %= 12;
        if(file.lastModified.hour == 0) {
            file.lastModified.hour = 12;
        }
        file.lastModified.minute = systemTime.wMinute;

        //file.lastModified.second = systemTime.wSecond;

        if(findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            file.attributes |= Record::Attributes::DIRECTORY;
        }

        if(findFileData.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) {
            file.attributes |= Record::Attributes::HIDDEN;
        }

        out_DirectoryItems.push_back(file);
    } while(FindNextFileW(hFind, &findFileData));

    FindClose(hFind);

    return true;
}

Path getCurrentProcessPath() {
    WCHAR fullPath[1024];

    DWORD numChars = GetCurrentDirectoryW(1024, &fullPath[0]);
    assert(numChars < 1024 && "Not enough space in buffer to retrieve full path");

    std::string converted = Util::WstringToUtf8(&fullPath[0]);

    return Path(converted);
}

bool doesPathExist(const Path& path) {
    DWORD result = GetFileAttributesW(path.wstr().data());

    return result != INVALID_FILE_ATTRIBUTES;
}

// NOTE: following operations shouldn't return false if the actual file operation failes... Error reporting is done through FileOpProgressSink

FileOperation::FileOperation() {
    mFlags = FOF_SILENT | FOF_NOERRORUI | FOF_NOCONFIRMATION;
}

bool FileOperation::init(FileOpProgressSink* ps) {
    mProgressSink = ps;

    HRESULT hr = CoCreateInstance(CLSID_FileOperation, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&mOperation));
    if(!SUCCEEDED(hr)) return false;

    if(mProgressSink != nullptr) {
        hr = mOperation->Advise(mProgressSink, &mProgressCookie);
    }

    return SUCCEEDED(hr);
}

void FileOperation::remove(const Path& path) {
    assert(mOperation != nullptr);
    //printf("Attempting to delete: %s\n", path.str().c_str());
    if(!doesPathExist(path)) {
        printf("Path %s does not exist.\n", path.str().c_str());
        return;
    }

    if(path.isDriveRoot()) {
        printf("Cannot delete drive root .. \n");
        return;
    }

    IShellItem* fileItem = nullptr;
    HRESULT hr = SHCreateItemFromParsingName(path.wstr().c_str(), NULL, IID_PPV_ARGS(&fileItem));
    if(!SUCCEEDED(hr)) {
        printf("SHCreateItemFromParsingName() failed\n");
        return;
    }

    mOperation->DeleteItem(fileItem, nullptr);

    fileItem->Release();
}

void FileOperation::move(const Path& itemPath, const Path& targetPath) {
    assert(mOperation != nullptr);
    if(!doesPathExist(itemPath)) {
        printf("%s does not exist\n", itemPath.str().c_str());
        return;
    }

    if(itemPath.isDriveRoot()) {
        printf("Cannot move drive root .. \n");
        return;
    }


    IShellItem* itemToMove = nullptr;
    IShellItem* destinationPath = nullptr;
    SHCreateItemFromParsingName(itemPath.wstr().c_str(), NULL, IID_PPV_ARGS(&itemToMove));
    SHCreateItemFromParsingName(targetPath.wstr().c_str(), NULL, IID_PPV_ARGS(&destinationPath));
    
    if(itemToMove == nullptr || destinationPath == nullptr) {
        printf("Failed to create shell items for source destination paths\n");
        return;
    }

    mOperation->MoveItem(itemToMove, destinationPath, NULL, NULL);

    itemToMove->Release();
    destinationPath->Release();
}

void FileOperation::copy(const Path& itemPath, const Path& targetPath) {
    assert(mOperation != nullptr);
    if(!doesPathExist(itemPath)) {
        printf("%s does not exist\n", itemPath.str().c_str());
        return;
    }

    if(itemPath.isDriveRoot()) {
        printf("Cannot move drive root .. \n");
        return;
    }


    IShellItem* itemToMove = nullptr;
    IShellItem* destinationPath = nullptr;
    SHCreateItemFromParsingName(itemPath.wstr().c_str(), NULL, IID_PPV_ARGS(&itemToMove));
    SHCreateItemFromParsingName(targetPath.wstr().c_str(), NULL, IID_PPV_ARGS(&destinationPath));
    
    if(itemToMove == nullptr || destinationPath == nullptr) {
        printf("Failed to create shell items for source destination paths\n");
        return;
    }

    mOperation->CopyItem(itemToMove, destinationPath, NULL, NULL);

    itemToMove->Release();
    destinationPath->Release();
}

void FileOperation::rename(const Path& itemPath, const std::string& newName) {
    assert(mOperation != nullptr);
    if(!doesPathExist(itemPath)) {
        printf("%s does not exist\n", itemPath.str().c_str());
        return;
    }

    if(itemPath.isDriveRoot()) {
        printf("Cannot rename drive root .. \n");
        return;
    }

    std::wstring wItemPath(itemPath.wstr());

    IShellItem* itemToRename = nullptr;
    SHCreateItemFromParsingName(itemPath.wstr().c_str(), NULL, IID_PPV_ARGS(&itemToRename));
    
    if(itemToRename == nullptr) {
        printf("Failed to create shell item for source item\n");
        return;
    }

    std::wstring wNewName(Util::Utf8ToWstring(newName));

    mOperation->RenameItem(itemToRename, wNewName.c_str(), NULL);

    itemToRename->Release();
}

void FileOperation::allowUndo(bool allow) {
    if(allow) {
        mFlags |= FOFX_ADDUNDORECORD | FOFX_RECYCLEONDELETE;
    }
}

void FileOperation::execute() {
    assert(mOperation != nullptr);

    mOperation->SetOperationFlags(mFlags);
    mOperation->PerformOperations();

    if(mProgressSink != nullptr) {
        mOperation->Unadvise(mProgressCookie);
    }

    mOperation->Release();
}


bool deleteFileOrDirectory(const Path& path, bool moveToRecycleBin, FileOpProgressSink* ps) {

    FileOperation operation;
    if(operation.init(ps)) {
        operation.remove(path);
        operation.allowUndo(moveToRecycleBin);
        operation.execute();
    }

    return true;
}

bool moveFileOrDirectory(const Path& itemPath, const Path& to, FileOpProgressSink* ps) {

    FileOperation operation;
    if(operation.init(ps)) {
        operation.move(itemPath, to);
        operation.execute();
    }

    return true;
}

bool copyFileOrDirectory(const Path& itemPath, const Path& to, FileOpProgressSink* ps) {

    FileOperation operation;
    if(operation.init(ps)) {
        operation.copy(itemPath, to);
        operation.execute();
    }

    return true;
}

bool renameFileOrDirectory(const Path& itemPath, const std::wstring& newName, FileOpProgressSink* ps) {

    FileOperation operation;
    if(operation.init(ps)) {
        operation.rename(itemPath, Util::WstringToUtf8(newName));
        operation.execute();
    }

    return true;
}

}
