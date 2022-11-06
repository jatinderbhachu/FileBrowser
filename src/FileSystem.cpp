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

void SOARecord::sortByName(SortDirection direction) {
    if(direction == SortDirection::Ascending) {
        std::stable_sort(indexes.begin(), indexes.end(), [&](const size_t& lhs, const size_t& rhs) { 
                return NaturalComparator(names[rhs], names[lhs]); 
        });
    } else {
        std::stable_sort(indexes.begin(), indexes.end(), [&](const size_t& lhs, const size_t& rhs) { 
                return NaturalComparator(names[lhs], names[rhs]); 
        });
    }
}

void SOARecord::sortByType(SortDirection direction) {
    if(direction == SortDirection::Ascending) {
        std::stable_sort(indexes.begin(), indexes.end(), [&](const size_t& lhs, const size_t& rhs) { 
                return !(attributes[lhs] & FileAttributes::DIRECTORY) < !(attributes[rhs] & FileAttributes::DIRECTORY);
        });
    } else {
        std::stable_sort(indexes.begin(), indexes.end(), [&](const size_t& lhs, const size_t& rhs) { 
                return !(attributes[lhs] & FileAttributes::DIRECTORY) > !(attributes[rhs] & FileAttributes::DIRECTORY);
        });
    }
}


void SOARecord::sortByLastModified(SortDirection direction) {
    if(direction == SortDirection::Ascending) {
        std::stable_sort(indexes.begin(), indexes.end(), [&](const size_t& lhs, const size_t& rhs) { 
                return lastModifiedNumbers[lhs] > lastModifiedNumbers[rhs];
        });
    } else {
        std::stable_sort(indexes.begin(), indexes.end(), [&](const size_t& lhs, const size_t& rhs) { 
                return lastModifiedNumbers[lhs] < lastModifiedNumbers[rhs];
        });
    }
}

void SOARecord::sortBySize(SortDirection direction) {
    if(direction == SortDirection::Ascending) {
        std::stable_sort(indexes.begin(), indexes.end(), [&](const size_t& lhs, const size_t& rhs) { 
                return sizes[lhs] > sizes[rhs];
        });
    } else {
        std::stable_sort(indexes.begin(), indexes.end(), [&](const size_t& lhs, const size_t& rhs) { 
                return sizes[lhs] < sizes[rhs];
        });
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

void getDriveNames(std::vector<std::string> &out_driveNames) {
    std::vector<char> driveLetters;
    getDriveLetters(driveLetters);

    WCHAR driveName[MAX_PATH];
    for(char driveLetter : driveLetters) {
        
        std::wstring drivePath = Util::Utf8ToWstring(std::string(1, driveLetter) + ":\\");

        GetVolumeInformationW(drivePath.data(),
                                                driveName,
                                                MAX_PATH,
                                                NULL,
                                                NULL,
                                                NULL,
                                                NULL,
                                                0);
        std::string name = Util::WstringToUtf8(driveName);

        // TODO: verify this...
        // name appears to be empty when the volume is the windows boot drive (C drive), so just set it to "This PC"
        if(name.empty()) {
            name = "This PC";
        }

        out_driveNames.push_back(name);
    }
}

GUID KnownFolderToGUID(KnownFolder type) {
    switch(type) {
        case KnownFolder::Desktop: return FOLDERID_Desktop;
        case KnownFolder::Documents: return FOLDERID_Documents;
        case KnownFolder::Downloads: return FOLDERID_Downloads;
        case KnownFolder::Music: return FOLDERID_Music;
        case KnownFolder::Videos: return FOLDERID_Videos;
        case KnownFolder::Pictures: return FOLDERID_Pictures;
        case KnownFolder::ProgramData: return FOLDERID_ProgramData;
        case KnownFolder::ProgramFilesX64: return FOLDERID_ProgramFilesX64;
        case KnownFolder::ProgramFilesX86: return FOLDERID_ProgramFilesX86;
        case KnownFolder::RoamingAppData: return FOLDERID_RoamingAppData;
        case KnownFolder::LocalAppData: return FOLDERID_LocalAppData;
    }
}

Path getKnownFolderPath(KnownFolder folderType) {
    Path result("");

    PWSTR str;

    HRESULT res = SHGetKnownFolderPath(KnownFolderToGUID(folderType), 0, NULL, &str);

    if(res == S_OK) {
        result = Path( Util::WstringToUtf8(str) );

        delete str;
    }

    return result;
}

void openFile(const Path& path) {
    ShellExecuteW(0, 0, path.wstr().c_str(), 0, 0, SW_SHOW);
}

bool enumerateDirectory(const Path& path, SOARecord& out_DirectoryItems) {
    if(path.isEmpty()) return false;
    if(!doesPathExist(path)) return false;

    out_DirectoryItems.clear();

    WIN32_FIND_DATAW findFileData;
    HANDLE hFind;

    const std::string& pathStr = path.str();
    const std::string dir = pathStr + "/*";
    const std::wstring wString = Util::Utf8ToWstring(dir);

    hFind = FindFirstFileExW(wString.c_str(), FindExInfoBasic, &findFileData, FindExSearchNameMatch, NULL, FIND_FIRST_EX_LARGE_FETCH);

    if(hFind == INVALID_HANDLE_VALUE) {
        printf("Can't find dir %s\n", dir.data());
        return false;
    }

    out_DirectoryItems.clear();

    size_t counter = 0;

    do {
        std::string filename = Util::WstringToUtf8(findFileData.cFileName);
        if(filename == "." || filename == "..") continue;

        // do not include system files
        if(findFileData.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM) continue;

        FILETIME lastWriteTime{};
        FileTimeToLocalFileTime(&findFileData.ftLastWriteTime, &lastWriteTime);

        SYSTEMTIME systemTime{};
        FileTimeToSystemTime(&lastWriteTime, &systemTime);

        Timestamp lastModified;
        
        lastModified.year   = systemTime.wYear;
        lastModified.month  = systemTime.wMonth;
        lastModified.day    = systemTime.wDay;

        lastModified.isPM = systemTime.wHour < 12 ? false : true;

        lastModified.hour = systemTime.wHour;
        lastModified.hour %= 12;
        if(lastModified.hour == 0) {
            lastModified.hour = 12;
        }

        lastModified.minute = systemTime.wMinute;
        uint64_t lastModifiedN = (static_cast<uint64_t>(lastWriteTime.dwHighDateTime) << 32) | static_cast<uint64_t>(lastWriteTime.dwLowDateTime);

        int attribute = 0;
        if(findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            attribute |= FileAttributes::DIRECTORY;
        }

        if(findFileData.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) {
            attribute |= FileAttributes::HIDDEN;
        }

        uint64_t size = (static_cast<uint64_t>(findFileData.nFileSizeHigh) << 32) | static_cast<uint64_t>(findFileData.nFileSizeLow);

        out_DirectoryItems.indexes.push_back(counter++);
        out_DirectoryItems.names.push_back(filename);
        out_DirectoryItems.attributes.push_back(attribute);
        out_DirectoryItems.lastModifiedDates.push_back(lastModified);
        out_DirectoryItems.lastModifiedNumbers.push_back(lastModifiedN);
        out_DirectoryItems.sizes.push_back(size);
    } while(FindNextFileW(hFind, &findFileData));

    FindClose(hFind);

    return true;
}

Path getCurrentProcessPath() {
    WCHAR fullPath[MAX_PATH];

    DWORD numChars = GetCurrentDirectoryW(MAX_PATH, &fullPath[0]);
    assert(numChars < MAX_PATH && "Not enough space in buffer to retrieve full path");

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
