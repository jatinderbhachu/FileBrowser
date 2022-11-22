#include "Path.h"

#include <iostream>
#include <assert.h>

#include "StringUtils.h"
#include "FileSystem.h"

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

Path& Path::operator=(const Path& rhs) {
    mText = rhs.mText;
    parse();

    return *this;
}

Path& Path::operator=(const Path&& rhs) {
    mText = std::move(rhs.mText),
    mSegments = std::move(rhs.mSegments),
    mType = rhs.mType;

    return *this;
}

/**
  * Parses the path string in mText into segments and determines the PathType
  */
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

    size_t pos      = 0;
    size_t prevPos  = 0;
    while(pos <= mText.size()) {
        if(mText[pos] == SEPARATOR || pos == mText.size()) {
            // padding excludes SEPARATOR from the segments
            int padding = mSegments.empty() ? 0 : 1;
            size_t start = prevPos + padding;
            size_t length = pos - prevPos - padding;
            std::string_view segment(pathStr.substr(start, length));

            assert(!segment.empty() && "Path segment is empty");
            mSegments.push_back(segment);

            prevPos = pos;
        }
        pos++;
    }

    // index of ":\\" 
    size_t driveRootIdx = pathStr.find(DRIVE_ROOT);
    char lastChar = pathStr.back();

    // is "C:" or contains ":\\"
    if(lastChar == ':' || driveRootIdx != std::string::npos) {
        mType = PATH_ABSOLUTE;
    } else {
        mType = PATH_RELATIVE;
    }
}

const std::string& Path::str() const {
    return mText;
}

std::wstring Path::wstr() const {
    return Util::Utf8ToWstring(mText);
}

void Path::popSegment() {
    size_t idxOfLastSeparator = mText.find_last_of( SEPARATOR );

    if(idxOfLastSeparator != std::string::npos) {
        mText.erase(idxOfLastSeparator, std::string::npos);
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

    for(auto relativeSegment : mSegments) {
        if(relativeSegment == CURRENT_PATH) {
            // do nothing ..
        } else if (relativeSegment == PARENT_PATH) {
            baseSegments.pop_back();
        } else {
            baseSegments.push_back(relativeSegment);
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

    return lastSegment.rfind('.') != std::string::npos;
}

std::string Path::getFileExtension() {
    if(hasFileExtension()) {
        std::string_view lastSegment = mSegments.back();

        size_t dotPos = lastSegment.rfind('.');
        return std::string(lastSegment.substr(dotPos));
    }
    return "";
}

std::string Path::getLastSegment() const {
    if(!mSegments.empty()) {
        return std::string(mSegments.back());
    }
    return "";
}

bool Path::isDriveRoot() const {
    // is : the last character
    size_t colonIdx = mText.find(':');
    return colonIdx == (mText.size() - 1);
}

Path::PathType Path::getType() const {
    return mType;
}

bool Path::isEmpty() const {
    return mType == PATH_EMPTY;
}

/**
  * Returns path of the parent directory
  * e.g. "C:/test/abc" will return "C:/test" 
  */
std::string Path::getParentStr() {
    if(mSegments.empty()) return "";

    std::string result("");

    for(size_t i = 0; i < mSegments.size() - 1; i++) {
        result.append(mSegments[i]);
        result.append(std::string(1, SEPARATOR));
    }

    result.pop_back();

    return result;
}

std::vector<std::string_view> Path::getSegments() const {
    return mSegments;
}

int Path::getSegmentCount() const {
    return mSegments.size();
}
