#include "Path.h"

#include "StringUtils.h"
#include "FileSystem.h"

#include <iostream>
#include <assert.h>

static const char SEPARATOR = '\\';
static const std::string CURRENT_PATH = ".";
static const std::string PARENT_PATH = "..";

Path::Path()
    : mText(".") {
    parse();
}

Path::Path(const std::string& pathStr) 
    : mText(pathStr) {
    parse();
}

Path::Path(Path&& other)
    : mText(std::move(other.mText)),
    mSegments(std::move(other.mSegments)),
    mType(other.mType)
{
}

Path::Path(const Path& other)
    : mText(other.mText)
{
    parse();
}

Path& Path::operator=(const Path &rhs) {
    mText = rhs.mText;
    parse();

    return *this;
}


void Path::parse() {
    if(mText.empty()) {
        mType = PathType::PATH_EMPTY;
        return;
    }

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

    // idx of ":\"
    std::string rootStr = std::string(":") + std::string(1, SEPARATOR);
    size_t rootIdx = pathStr.find(rootStr);
    size_t colonIdx = pathStr.find(':');
    if(colonIdx == (pathStr.size() - 1) || rootIdx != std::string::npos) {
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
    } else {
        mSegments.clear();
        mText = "";
        mType = PathType::PATH_EMPTY;
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
    if(mText.empty()) {
        mText += name + SEPARATOR;
    } else {
        mText += SEPARATOR + name;
    }
    parse();
}

void Path::toAbsolute() {
    if(mType == PATH_ABSOLUTE) return;

    Path processPath = FileSystem::getCurrentProcessPath();
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

bool Path::hasFileExtension() {
    // get last segment and check if it has '.'
    if(mSegments.empty()) return false;

    std::string_view lastSegment = mSegments.back();

    return lastSegment.find('.') != std::string::npos;
}

std::string Path::getFileExtension() {
    if(hasFileExtension()) {
        std::string_view lastSegment = mSegments.back();

        size_t dotPos = lastSegment.find('.');
        return std::string(lastSegment.substr(dotPos));
    }
    return "";
}

std::string Path::getLastSegment() {
    if(!mSegments.empty()) {
        return std::string(mSegments.back());
    }
    return "";
}

bool Path::isDriveRoot() const {
    size_t colonIdx = mText.find(':');

    // is : the last character
    return colonIdx == (mText.size() - 1);
}

Path::PathType Path::getType() const {
    return mType;
}

bool Path::isEmpty() const {
    return mType == PATH_EMPTY;
}

std::vector<std::string_view> Path::getSegments() const {
    return mSegments;
}
