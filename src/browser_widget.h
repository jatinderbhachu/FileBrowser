#pragma once

#include "types.h"

#include "path.h"

#include <vector>
#include <unordered_map>

struct ImDrawList;

namespace FileOps {
    struct Record;
}

class Browser
{
public:
    Browser(const Path& path);

    void beginFrame();
    void setCurrentDirectory(const Path& path);
    void draw();

private:
    ImDrawList* mDrawList;
    Path mCurrentDirectory;
    std::vector<FileOps::Record> mDisplayList;
    std::unordered_map<int, bool> mSelected;
    bool mUpdateFlag;

    int mYScroll;
};

