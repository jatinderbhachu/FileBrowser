#pragma once
#include <string>
#include <vector>
#include "types.h"

class Path;

namespace FileOps {
    struct Record {
        std::string name;
        bool isFile = false;
        unsigned long attributes;
    };

    enum SortDirection {
        Ascending,
        Descending
    };

    // https://docs.microsoft.com/en-us/windows/win32/fileio/file-attribute-constants
    enum class FileAttributes {
        ARCHIVE = 0x20,
        COMPRESSED = 0x800,
        DEVICE = 0x40,
        DIRECTORY = 0x10,
        ENCRYPTED = 0x4000,
        HIDDEN = 0x2,
        INTEGRITY_STREAM = 0x8000,
        NORMAL = 0x80,
        NOT_CONTENT_INDEXED = 0x2000,
        NO_SCRUB_DATA = 0x20000,
        OFFLINE = 0x1000,
        READONLY = 0x1,
        RECALL_ON_DATA_ACCESS = 0x400000,
        RECALL_ON_OPEN = 0x40000,
        REPARSE_POINT = 0x400,
        SPARSE_FILE = 0x200,
        SYSTEM = 0x4,
        TEMPORARY = 0x100,
        VIRTUAL = 0x10000
    };
    constexpr u32 FileAttributesCount = 19;


    void enumerateDirectory(const Path& path, std::vector<Record>& out_DirectoryItems);
    void sortByName(SortDirection direction, std::vector<Record>& out_DirectoryItems);
    void sortByType(SortDirection direction, std::vector<Record>& out_DirectoryItems);

    bool doesPathExist(const Path& path);
    bool deleteFileOrDirectory(const Path& itemPath, bool moveToRecycleBin);
    bool moveFileOrDirectory(const Path& itemPath, const Path& toDirectory, const std::string& newName);
    bool renameFileOrDirectory(const Path& itemPath, const std::string& newName);
}
