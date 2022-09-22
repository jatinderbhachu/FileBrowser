#include "path.h"

#include "string_util.h"
#include "file_ops.h"

#include <iostream>
#include <assert.h>

static const char SEPARATOR = '\\';
static const std::string CURRENT_PATH = ".";
static const std::string PARENT_PATH = "..";


Path::Path(const std::string& pathStr) 
    : mText(pathStr) {
    parse();
}


Path::Path(const Path& other) {
    mSegments = other.getSegments();
    mText = other.str();
    mType = other.getType();
}


void Path::parse() {
    assert(!mText.empty() && "Path is empty");

    // check if string contains any forward slashes
    for(int i = 0; i < mText.size(); i++) {
        if(mText[i] == '/') mText[i] = SEPARATOR;
    }

    if(mText[mText.size() - 1] == SEPARATOR) mText.pop_back();

    mSegments.clear();

    // parse into segments separated by SEPARATOR
    std::string_view pathStr(mText);

    size_t pos = 0;
    size_t prev = 0;
    while(pos <= mText.size()) {
        if(mText[pos] == SEPARATOR || pos == mText.size()) {
            int padding = (prev > 0 ? 1 : 0);
            std::string_view segment(pathStr.substr(prev + padding, pos - prev - padding));

            assert(!segment.empty() && "Path segment is empty");
            mSegments.push_back(segment);

            prev = pos;
        }
        pos++;
    }

    // is absolute or relative
    assert(mSegments.size() > 0 && "Failed to parse path");

    size_t colonIndex = pathStr.find(':');
    if(colonIndex != std::string::npos && pathStr.size() > (1+colonIndex) && pathStr[colonIndex + 1] == SEPARATOR) {
        mType = PATH_ABSOLUTE;
        //std::cout << mText << " Path is ABSOLUTE\n";
    } else {
        mType = PATH_RELATIVE;
        //std::cout << mText << " Path is RELATIVE\n";
    }
}

const std::string& Path::str() const {
    return mText;
}

std::wstring Path::wstr() const {
    return Util::Utf8ToWstring(mText);
}

void Path::popSegment() {
    size_t indexOfLast = mText.find_last_of( SEPARATOR );

    if(indexOfLast != std::string::npos) {
        //mSegments.pop_back();
        mText.erase(indexOfLast, std::string::npos);
        parse();
    }
}

void Path::appendRelative(const Path& relativePath) {
    assert(relativePath.getType() == PATH_RELATIVE && "Path is not relative.");

    auto relativeSegments = relativePath.getSegments();

    std::string stringToAppend;

    for(std::string_view segment : relativeSegments) {
        if(segment[0] == '.') continue;
        stringToAppend += SEPARATOR;
        stringToAppend += std::string(segment);
    }

    mText += stringToAppend;
    parse();
}

void Path::appendName(const std::string& name) {
    mText += SEPARATOR + name;
    parse();
}

void Path::toAbsolute() {
    assert(mType == PATH_RELATIVE);

    Path processPath = FileOps::getCurrentProcessPath();
    std::vector<std::string_view> baseSegments = processPath.getSegments();

    for(auto relSegment : mSegments) {
        if(relSegment == CURRENT_PATH) {
            // do nothing ..
        } else if (relSegment == PARENT_PATH) {
            baseSegments.pop_back();
        } else {
            baseSegments.push_back(relSegment);
        }
    }

    std::string result("");
    for(auto segment : baseSegments) {
        result.append(segment);
        result.append(std::string(1, SEPARATOR));
    }

    mText = result;
    parse();
}

Path::PathType Path::getType() const {
    return mType;
}

std::vector<std::string_view> Path::getSegments() const {
    return mSegments;
}
