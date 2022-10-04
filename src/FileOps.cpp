#include "FileOps.h"
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

namespace FileOps {

inline static int GetChunk(const std::string str, int start) {
    if(start >= str.size()) return 1;
    char startChar = str[start];
    int length = 1;
    if(std::isdigit(startChar)) {
        // move until next char or end
        while(start++ < str.size()) {
            if(!std::isdigit(str[start])) {
                break;
            }
            length++;
        }
    } else {
        // move until next digit or end
        while(start++ < str.size()) {
            if(std::isdigit(str[start])) {
                break;
            }
            length++;
        }
    }

    return length;
}

inline static bool NaturalComparator(const std::string& lhs, const std::string& rhs) {
    if (lhs.empty())
        return false;
    if (rhs.empty())
        return true;

    int thisPos = 0;
    int thatPos = 0;

    while(thisPos < lhs.size() && thatPos < rhs.size()) {
        int thisChunkSize = GetChunk(lhs, thisPos);
        int thatChunkSize = GetChunk(rhs, thatPos);

        if (std::isdigit(lhs[thisPos]) && !std::isdigit(rhs[thatPos])) {
            return false;
        }
        if (!std::isdigit(lhs[thisPos]) && std::isdigit(rhs[thatPos])) {
            return true;
        }

        if(std::isdigit(lhs[thisPos]) && std::isdigit(rhs[thatPos])) {
            if(thisChunkSize == thatChunkSize) {
                for(int i = 0; i < thisChunkSize; i++) {
                    int res = lhs[thisPos + i] - rhs[thatPos + i];
                    if(res != 0) return res > 0;
                }
            } else {
                return thisChunkSize > thatChunkSize;
            }
        } else {
            int res = lhs.compare(thisPos, thisChunkSize, rhs, thatPos, thatChunkSize);
            if(res != 0) return res > 0;
        }

        if (thatChunkSize == 0) return false;
        if (thisChunkSize == 0) return true;

        thisPos += thisChunkSize;
        thatPos += thatChunkSize;
    }

    return lhs.size() > rhs.size();
};

void sortByName(SortDirection direction, std::vector<Record>& out_DirectoryItems) {
    if(direction == SortDirection::Ascending) {
        std::sort(out_DirectoryItems.begin(), out_DirectoryItems.end(), [](const Record& lhs, const Record& rhs) { return NaturalComparator(rhs.name, lhs.name); });
    } else {
        std::sort(out_DirectoryItems.begin(), out_DirectoryItems.end(), [](const Record& lhs, const Record& rhs) { return NaturalComparator(lhs.name, rhs.name); });
    }
}

void sortByType(SortDirection direction, std::vector<Record>& out_DirectoryItems) {
    if(direction == SortDirection::Ascending) {
        std::sort(out_DirectoryItems.begin(), out_DirectoryItems.end(), [](const Record& lhs, const Record& rhs) { return lhs.isFile < rhs.isFile; });
    } else {
        std::sort(out_DirectoryItems.begin(), out_DirectoryItems.end(), [](const Record& lhs, const Record& rhs) { return lhs.isFile > rhs.isFile; });
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
void enumerateDirectory(const Path& path, std::vector<Record>& out_DirectoryItems) {
    if(path.isEmpty()) return;
    //if(!doesPathExist(path)) return;

    out_DirectoryItems.clear();

    WIN32_FIND_DATAW findFileData;
    HANDLE hFind;

    const std::string& pathStr = path.str();
    std::string dir = pathStr + "/*";
    std::wstring wString = Util::Utf8ToWstring(dir);

    hFind = FindFirstFileW(wString.c_str(), &findFileData);

    if(hFind == INVALID_HANDLE_VALUE) {
        printf("Can't find dir %s\n", dir.data());
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

bool deleteFileOrDirectory(const Path& path, bool moveToRecycleBin, FileOpProgressSink* ps) {
    //printf("Attempting to delete: %s\n", path.str().c_str());
    if(!doesPathExist(path)) {
        printf("Path %s does not exist.\n", path.str().c_str());
        return false;
    }

    if(path.isDriveRoot()) {
        printf("Cannot delete drive root .. \n");
        return false;
    }

    std::wstring wPathStr(path.wstr());

    IFileOperation* fo = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileOperation, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&fo));
    if(!SUCCEEDED(hr)) {
        printf("cocreateinstance is null\n");

        return false;
    }

    DWORD cookie = 0;
    if(ps != nullptr) {
        hr = fo->Advise(ps, &cookie);
    }


    IShellItem* fileItem = nullptr;
    hr = SHCreateItemFromParsingName(wPathStr.c_str(), NULL, IID_PPV_ARGS(&fileItem));
    if(!SUCCEEDED(hr)) {
        printf("SHCreateItemFromParsingName() failed\n");

        return false;
    }

    DWORD flags = FOF_SILENT | FOF_NOERRORUI | FOF_NOCONFIRMATION;
    if(moveToRecycleBin) {
        flags |= FOFX_ADDUNDORECORD | FOFX_RECYCLEONDELETE;
    }

    fo->SetOperationFlags(flags);
    fo->DeleteItem(fileItem, NULL);
    hr = fo->PerformOperations();

    if(ps != nullptr) {
        fo->Unadvise(cookie);
    }

    fo->Release();

    return true;
}

bool moveFileOrDirectory(const Path& itemPath, const Path& to, FileOpProgressSink* ps) {
    if(!doesPathExist(itemPath)) {
        printf("%s does not exist\n", itemPath.str().c_str());
        return false;
    }

    if(itemPath.isDriveRoot()) {
        printf("Cannot move drive root .. \n");
        return false;
    }

    std::wstring wItemPath(itemPath.wstr());
    std::wstring wDestinationPath(to.wstr());

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

    DWORD cookie = 0;
    if(ps != nullptr) {
        result = fo->Advise(ps, &cookie);
    }

    DWORD flags = FOF_SILENT | FOF_NOERRORUI | FOF_NOCONFIRMATION | FOFX_ADDUNDORECORD;

    fo->SetOperationFlags(flags);
    fo->MoveItem(itemToMove, destinationDir, NULL, NULL);
    result = fo->PerformOperations();

    if(ps != nullptr) {
        fo->Unadvise(cookie);
    }

    fo->Release();

    return true;
}

bool copyFileOrDirectory(const Path& itemPath, const Path& to, FileOpProgressSink* ps) {
    if(!doesPathExist(itemPath)) {
        printf("%s does not exist\n", itemPath.str().c_str());
        return false;
    }
    std::wstring wItemPath(itemPath.wstr());
    std::wstring wDestinationPath(to.wstr());

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

    DWORD cookie = 0;
    if(ps != nullptr) {
        result = fo->Advise(ps, &cookie);
    }

    DWORD flags = FOF_SILENT | FOF_NOERRORUI | FOF_NOCONFIRMATION | FOFX_ADDUNDORECORD;

    result = fo->CopyItem(itemToMove, destinationDir, NULL, NULL);
    
    result = fo->SetOperationFlags(flags);
    result = fo->PerformOperations();
    
    if(ps != nullptr) {
        result = fo->Unadvise(cookie);
    }

    result = fo->Release();

    return true;
}

bool renameFileOrDirectory(const Path& itemPath, const std::wstring& newName, FileOpProgressSink* ps) {
    if(!doesPathExist(itemPath)) {
        printf("%s does not exist\n", itemPath.str().c_str());
        return false;
    }

    if(itemPath.isDriveRoot()) {
        printf("Cannot rename drive root .. \n");
        return false;
    }

    std::wstring wItemPath(itemPath.wstr());

    IShellItem* itemToRename = nullptr;
    SHCreateItemFromParsingName(wItemPath.data(), NULL, IID_PPV_ARGS(&itemToRename));
    
    if(itemToRename == nullptr) {
        printf("Failed to create shell item for source item\n");
        return false;
    }

    IFileOperation* fo = nullptr;
    HRESULT result = CoCreateInstance(CLSID_FileOperation, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&fo));
    if(!SUCCEEDED(result)) return false;

    DWORD cookie = 0;
    if(ps != nullptr) {
        result = fo->Advise(ps, &cookie);
    }

    DWORD flags = FOF_SILENT | FOF_NOERRORUI | FOF_NOCONFIRMATION | FOFX_ADDUNDORECORD;

    fo->SetOperationFlags(flags);
    fo->RenameItem(itemToRename, newName.c_str(), NULL);
    result = fo->PerformOperations();

    if(ps != nullptr) {
        fo->Unadvise(cookie);
    }

    fo->Release();

    return true;
}

}
