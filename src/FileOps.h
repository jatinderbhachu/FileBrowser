#pragma once
#include <string>
#include <vector>
#include "SortDirection.h"

class Path;
class FileOpProgressSink;

struct IFileOperation;

namespace FileOps {
    struct Record {
        std::string name;
        bool isFile = false;
        unsigned long attributes;
    };

    class FileOperation {
        public:
            FileOperation();
            bool init(FileOpProgressSink* ps = nullptr);
            void remove(const Path& itemPath);
            void move(const Path& itemPath, const Path& toDirectory);
            void copy(const Path& itemPath, const Path& toDirectory);
            void rename(const Path& itemPath, const std::string& newName);
            void allowUndo(bool allow);
            void execute();
        private:
            IFileOperation* mOperation = nullptr;
            FileOpProgressSink* mProgressSink = nullptr;
            unsigned long mProgressCookie;
            unsigned long mFlags = 0;
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
    constexpr int FileAttributesCount = 19;


    bool enumerateDirectory(const Path& path, std::vector<Record>& out_DirectoryItems);
    void sortByName(SortDirection direction, std::vector<Record>& out_DirectoryItems);
    void sortByType(SortDirection direction, std::vector<Record>& out_DirectoryItems);
    bool createDirectory(const Path& path);

    void getDriveLetters(std::vector<char>& out_driveLetters);

    void openFile(const Path& filePath);
    Path getCurrentProcessPath();

    bool doesPathExist(const Path& path);
    bool deleteFileOrDirectory(const Path& itemPath, bool moveToRecycleBin, FileOpProgressSink* ps = nullptr);
    bool moveFileOrDirectory(const Path& itemPath, const Path& toDirectory, FileOpProgressSink* ps = nullptr);
    bool copyFileOrDirectory(const Path& itemPath, const Path& toDirectory, FileOpProgressSink* ps = nullptr);
    bool renameFileOrDirectory(const Path& itemPath, const std::wstring& newName, FileOpProgressSink* ps = nullptr);
};
