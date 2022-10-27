#pragma once

#include <string>
#include <vector>

class Path 
{
public:
    enum PathType {
       PATH_ABSOLUTE,
       PATH_RELATIVE,
       PATH_EMPTY,
    };

    Path();
    Path(const std::string& pathStr);
    Path(const Path& other);
    Path(Path&& other);
    Path& operator=(const Path& rhs);

    const std::string& str() const;
    std::wstring wstr() const;

    void popSegment();
    void appendRelative(const Path&);
    void appendName(const std::string&);

    void toAbsolute();

    bool hasFileExtension();
    std::string getFileExtension();

    std::string getLastSegment() const;

    bool isDriveRoot() const;

    PathType getType() const;
    bool isEmpty() const;

    std::vector<std::string_view> getSegments() const;
private:
    void parse();

    std::string mText;
    std::vector<std::string_view> mSegments;

    PathType mType;
};

