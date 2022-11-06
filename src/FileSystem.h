#pragma once
#include <string>
#include <vector>
#include "SortDirection.h"

#include <guiddef.h>

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

    enum FileAttributes : int {
        NONE        = 0,
        DIRECTORY   = 1 << 0,
        HIDDEN      = 1 << 1,
    };

    // https://learn.microsoft.com/en-us/windows/win32/shell/knownfolderid
    enum class KnownFolder {
        Documents,
        Desktop,
        Downloads,
        Music,
        Videos,
        Pictures,
        ProgramData,
        ProgramFilesX64,
        ProgramFilesX86,
        RoamingAppData,
        LocalAppData
    };

    GUID KnownFolderToGUID(KnownFolder);
    
    struct SOARecord {
        std::vector<size_t>         indexes;
        std::vector<std::string>    names;
        std::vector<int>            attributes;
        std::vector<Timestamp>      lastModifiedDates;
        std::vector<uint64_t>       lastModifiedNumbers;
        std::vector<uint64_t>       sizes;

        inline void clear() {
            indexes.clear();
            names.clear();
            attributes.clear();
            lastModifiedDates.clear();
            lastModifiedNumbers.clear();
            sizes.clear();
        }

        inline size_t size() { return indexes.size(); }

        inline const std::string&   getName(size_t i)               { return names[indexes[i]]; }
        inline const bool           isFile(size_t i)                { return !(attributes[indexes[i]] & FileAttributes::DIRECTORY); }
        inline const Timestamp&     getLastModifiedDate(size_t i)   { return lastModifiedDates[indexes[i]]; }
        inline const uint64_t&      getLastModifiedNumber(size_t i) { return lastModifiedNumbers[indexes[i]]; }
        inline const uint64_t       getSize(size_t i)               { return sizes[indexes[i]]; }

        void sortByName(SortDirection);
        void sortByType(SortDirection);
        void sortByLastModified(SortDirection);
        void sortBySize(SortDirection);
    };

    bool enumerateDirectory(const Path& path, SOARecord& out_DirectoryItems);
    bool createDirectory(const Path& path);

    void getDriveLetters(std::vector<char>& out_driveLetters);
    void getDriveNames(std::vector<std::string>& out_driveNames);

    Path getKnownFolderPath(KnownFolder);

    void openFile(const Path& filePath);
    Path getCurrentProcessPath();

    bool doesPathExist(const Path& path);
    bool deleteFileOrDirectory(const Path& itemPath, bool moveToRecycleBin, FileOpProgressSink* ps = nullptr);
    bool moveFileOrDirectory(const Path& itemPath, const Path& toDirectory, FileOpProgressSink* ps = nullptr);
    bool copyFileOrDirectory(const Path& itemPath, const Path& toDirectory, FileOpProgressSink* ps = nullptr);
    bool renameFileOrDirectory(const Path& itemPath, const std::wstring& newName, FileOpProgressSink* ps = nullptr);
};
