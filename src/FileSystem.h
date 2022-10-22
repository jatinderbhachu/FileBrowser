#pragma once
#include <string>
#include <vector>
#include "SortDirection.h"

class Path;
class FileOpProgressSink;

struct IFileOperation;

namespace FileSystem {

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

    struct Timestamp {
        uint16_t year;
        uint16_t month;
        uint16_t day;

        uint16_t hour;
        uint16_t minute;
        bool isPM;
    };

    // a file/folder
    struct Record {
        enum Attributes : int {
            NONE        = 0,
            DIRECTORY   = 1 << 0,
            HIDDEN      = 1 << 1,
        };

        std::string name;
        int attributes = Attributes::NONE;

        Timestamp lastModified;
        
        inline bool isFile() const { return !(attributes & Attributes::DIRECTORY); }
    };


    bool enumerateDirectory(const Path& path, std::vector<Record>& out_DirectoryItems);
    void sortByType(SortDirection direction, std::vector<Record>& out_DirectoryItems);
    void sortByName(SortDirection direction, std::vector<Record>& out_DirectoryItems);
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
