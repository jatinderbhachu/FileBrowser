#pragma once

#include "types.h"

#include <string>
#include <vector>

class Path 
{
public:
    enum PathType {
       PATH_ABSOLUTE,
       PATH_RELATIVE
    };

    Path(const std::string& pathStr);
    Path(const Path& other);

    const std::string& string() const;
    std::wstring wstring() const;


    void popSegment();
    void appendRelative(const Path&);
    void appendName(const std::string&);


    bool hasFileExtension();
    std::string getFileExtension();

    PathType getType() const;

    std::vector<std::string_view> getSegments() const;
private:
    void parse();

    std::string mText;
    std::vector<std::string_view> mSegments;

    PathType mType;
};

