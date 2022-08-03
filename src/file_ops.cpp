#include "file_ops.h"

#ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>


#include "string_util.h"
#include <Shellapi.h>
#include <ocidl.h>

#include <algorithm>

#include <shlobj.h>

#include "path.h"

namespace FileOps {

void sortByName(SortDirection direction, std::vector<Record>& out_DirectoryItems) {
    if(direction == Ascending) {
        std::sort(out_DirectoryItems.begin(), out_DirectoryItems.end(), [](const Record& lhs, const Record& rhs) { return lhs.name > rhs.name; });
    } else {
        std::sort(out_DirectoryItems.begin(), out_DirectoryItems.end(), [](const Record& lhs, const Record& rhs) { return lhs.name < rhs.name; });
    }
}

void sortByType(SortDirection direction, std::vector<Record>& out_DirectoryItems) {
    if(direction == Ascending) {
        std::sort(out_DirectoryItems.begin(), out_DirectoryItems.end(), [](const Record& lhs, const Record& rhs) { return lhs.isFile < rhs.isFile; });
    } else {
        std::sort(out_DirectoryItems.begin(), out_DirectoryItems.end(), [](const Record& lhs, const Record& rhs) { return lhs.isFile > rhs.isFile; });
    }
}

// path must be an absolute path
void enumerateDirectory(const Path& path, std::vector<Record>& out_DirectoryItems) {
    out_DirectoryItems.clear();

    WIN32_FIND_DATAW findFileData;
    HANDLE hFind;

    const std::string& pathStr = path.string();
    std::string dir = pathStr + "/*";
    std::wstring wString = Util::Utf8ToWstring(dir);

    hFind = FindFirstFileW(wString.c_str(), &findFileData);

    if(hFind == INVALID_HANDLE_VALUE) {
        printf("Can't find dir %s", dir.data());
        return;
    }

    do {
        std::string filename = Util::WstringToUtf8(findFileData.cFileName);
        if(filename == "." || filename == "..") continue;

        // do not include system files
        if(findFileData.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM) continue;

        Record file;
        file.name = filename;

        file.attributes = findFileData.dwFileAttributes;
        if(findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            file.isFile = false;
        } else {
            file.isFile = true;
        }

        out_DirectoryItems.push_back(file);
    } while(FindNextFileW(hFind, &findFileData));
}

bool doesPathExist(const Path& path) {
    DWORD result = GetFileAttributesW(path.wstring().data());

    return result != INVALID_FILE_ATTRIBUTES;
}

bool deleteFileOrDirectory(const Path& path, bool moveToRecycleBin) {
    if(!doesPathExist(path)) return false;
    std::wstring wPathStr(path.wstring());

    IShellItem* fileItem = nullptr;
    SHCreateItemFromParsingName(wPathStr.data(), NULL, IID_PPV_ARGS(&fileItem));
    if(fileItem == nullptr) return false;

    IFileOperation* fo = nullptr;
    HRESULT result = CoCreateInstance(CLSID_FileOperation, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&fo));
    if(!SUCCEEDED(result)) return false;

    DWORD flags = FOF_SILENT | FOF_NOERRORUI | FOF_NOCONFIRMATION;
    if(moveToRecycleBin) {
        flags |= FOFX_ADDUNDORECORD | FOFX_RECYCLEONDELETE;
    }

    fo->SetOperationFlags(flags);
    fo->DeleteItem(fileItem, NULL);
    result = fo->PerformOperations();
    fo->Release();

    return true;
}


bool moveFileOrDirectory(const Path& itemPath, const Path& to, const std::string& newName) {
    if(!doesPathExist(itemPath)) {
        printf("%s does not exist\n", itemPath.string().c_str());
        return false;
    }
    std::wstring wItemPath(itemPath.wstring());
    std::wstring wDestinationPath(to.wstring());
    std::wstring wNewName(Util::Utf8ToWstring(newName));

    IShellItem* itemToMove = nullptr;
    IShellItem* destinationDir = nullptr;
    SHCreateItemFromParsingName(wItemPath.data(), NULL, IID_PPV_ARGS(&itemToMove));
    SHCreateItemFromParsingName(wDestinationPath.data(), NULL, IID_PPV_ARGS(&destinationDir));
    if(itemToMove == nullptr || destinationDir == nullptr) {
        printf("Failed to create shell items for source destination paths\n");
        return false;
    }
    IFileOperation* fo = nullptr;
    HRESULT result = CoCreateInstance(CLSID_FileOperation, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&fo));
    if(!SUCCEEDED(result)) return false;

    DWORD flags = FOF_SILENT | FOF_NOERRORUI | FOF_NOCONFIRMATION | FOFX_ADDUNDORECORD;

    fo->SetOperationFlags(flags);
    fo->MoveItem(itemToMove, destinationDir, wNewName.data(), NULL);
    result = fo->PerformOperations();
    if(!SUCCEEDED(result)) return false;
    fo->Release();

    return true;
}


bool renameFileOrDirectory(const Path& itemPath, const std::string& newName) {
    Path parentPath(itemPath.string());
    parentPath.popSegment();
    return moveFileOrDirectory(itemPath, parentPath, newName);
}


}
