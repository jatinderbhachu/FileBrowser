#include "BrowserWidget.h"
#include <imgui.h>
#include <imgui_internal.h>
#include "FileOps.h"
#include "FileOpsWorker.h"

#ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>


BrowserWidget::BrowserWidget(const Path& path, FileOpsWorker* fileOpsWorker) 
    : mDrawList(nullptr),
    mCurrentDirectory(path),
    mUpdateFlag(true),
    mFileOpsWorker(fileOpsWorker)
{
    setCurrentDirectory(path);
}

void BrowserWidget::beginFrame() {
    const ImU32 flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::Begin("filebrowser", NULL, flags);
    mDrawList = ImGui::GetWindowDrawList();
    ImGui::End();

}

static inline OVERLAPPED overlapped;

void BrowserWidget::setCurrentDirectory(const Path& path) {
    mUpdateFlag = true;
    if(path.getType() == Path::PATH_RELATIVE) {
        mCurrentDirectory = path;
    }
}

void BrowserWidget::draw() {
    std::vector<FileOps::Record>& displayList = mDisplayList;
    ImGuiIO& io = ImGui::GetIO();

    mYScroll += io.MouseWheel;
    if(mYScroll < 0) mYScroll = 0;
    if(mYScroll >= mDisplayList.size()) mYScroll = mDisplayList.size() - 1;

    const std::string& currentDirStr = mCurrentDirectory.str();

    auto dirSegments = mCurrentDirectory.getSegments();


    // display current directory segments
    {
        for(int i = 0; i < dirSegments.size(); i++) {
            ImGui::PushID(i);
            std::string uniqueID = "##DirSegment" + std::to_string(i);
            const ImVec2 text_size = ImGui::CalcTextSize(&dirSegments[i].front(), &dirSegments[i].back() + 1, false, false);
            ImVec2 cursorPos = ImGui::GetCursorPos();

            if(ImGui::Selectable(uniqueID.c_str(), false, ImGuiSelectableFlags_None | ImGuiSelectableFlags_AllowItemOverlap, text_size)) {
                int numPop = dirSegments.size() - i;
                while(--numPop > 0) mCurrentDirectory.popSegment();
                mUpdateFlag = true;
            }

            if(ImGui::BeginDragDropTarget()) {
                const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("TEST_PAYLOAD");
                if(payload != nullptr) {
                    int sourceIndex = *(const int*) payload->Data;
                    const FileOps::Record& sourceItem = displayList[sourceIndex];

                    Path sourcePath(mCurrentDirectory); sourcePath.appendName(sourceItem.name);
                    Path targetPath(mCurrentDirectory);

                    int numPop = dirSegments.size() - i;
                    while(--numPop > 0) targetPath.popSegment();

                    FileOp fileOperation{};
                    fileOperation.from = sourcePath;
                    fileOperation.to = targetPath;
                    fileOperation.opType = FileOpType::FILE_OP_MOVE;
                    mFileOpsWorker->addFileOperation(fileOperation);

                    //mUpdateFlag = true;
                }
                ImGui::EndDragDropTarget();
            }


            ImGui::SameLine();
            ImGui::SetCursorPos({cursorPos.x, ImGui::GetCursorPos().y});
            ImGui::TextEx(&dirSegments[i].front(), &dirSegments[i].back() + 1);
            ImGui::SameLine();
            ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
            ImGui::SameLine();
            ImGui::PopID();
        }
    }

    ImGui::Separator();
    ImGui::Separator();

    // up directory button
    if(ImGui::Button("..")) {
        mCurrentDirectory.popSegment();
        mUpdateFlag = true;
    }

    // list all the items
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_HorizontalScrollbar;
    ImGui::BeginChild("DirectoryView", ImGui::GetContentRegionAvail(), false, window_flags);

    static const FileOps::FileAttributes allAttribs[] = {
        FileOps::FileAttributes::ARCHIVE,
        FileOps::FileAttributes::COMPRESSED,
        FileOps::FileAttributes::DEVICE,
        FileOps::FileAttributes::DIRECTORY,
        FileOps::FileAttributes::ENCRYPTED,
        FileOps::FileAttributes::HIDDEN,
        FileOps::FileAttributes::INTEGRITY_STREAM,
        FileOps::FileAttributes::NORMAL,
        FileOps::FileAttributes::NOT_CONTENT_INDEXED,
        FileOps::FileAttributes::NO_SCRUB_DATA,
        FileOps::FileAttributes::OFFLINE,
        FileOps::FileAttributes::READONLY,
        FileOps::FileAttributes::RECALL_ON_DATA_ACCESS,
        FileOps::FileAttributes::RECALL_ON_OPEN,
        FileOps::FileAttributes::REPARSE_POINT,
        FileOps::FileAttributes::SPARSE_FILE,
        FileOps::FileAttributes::SYSTEM,
        FileOps::FileAttributes::TEMPORARY,
        FileOps::FileAttributes::VIRTUAL
    };


    static ImGuiTableFlags tableFlags = 
        ImGuiTableFlags_SizingStretchSame 
        | ImGuiTableFlags_Resizable 
        | ImGuiTableFlags_NoPadInnerX
        | ImGuiTableFlags_ContextMenuInBody 
        | ImGuiTableFlags_Sortable;

    ImGui::BeginTable("BrowserWidget", 2, tableFlags);
    int iconColumnFlags = ImGuiTableColumnFlags_NoHeaderLabel | ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_IndentDisable;
    ImVec2 iconColumnSize = ImGui::CalcTextSize("[]");

    ImGui::TableSetupColumn("icon", iconColumnFlags, iconColumnSize.x);
    
    ImGui::TableSetupColumn("Name");

    ImGuiListClipper clipper;
    clipper.Begin(displayList.size());

    bool isSelectingMultiple = io.KeyCtrl;

    mSelected.resize(displayList.size());

    while(clipper.Step()) {
        for(u32 i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
            std::string uniqueID = "##FileRecord" + std::to_string(i);
            ImGui::PushID(uniqueID.c_str());

            const FileOps::Record& item = displayList[i];
            bool isSelected = mSelected[i];

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            if(ImGui::Selectable("##selectable", isSelected, ImGuiSelectableFlags_AllowDoubleClick | ImGuiSelectableFlags_SpanAllColumns )) {
                if(isSelectingMultiple) {
                    mSelected[i] = !mSelected[i];
                } else {
                    mSelected.assign(mSelected.size(), false);
                    mSelected[i] = true;
                }


                if(ImGui::IsMouseDoubleClicked(0) && !item.isFile) {
                    mCurrentDirectory.appendName(item.name);
                    mUpdateFlag = true;
                }
            }

            if(ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                ImGui::SetDragDropPayload("TEST_PAYLOAD", &i, sizeof(int));

                ImGui::EndDragDropSource();
            }

            if(!item.isFile && ImGui::BeginDragDropTarget()) {
                const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("TEST_PAYLOAD");
                if(payload != nullptr) {
                    int sourceIndex = *(const int*) payload->Data;
                    const FileOps::Record& sourceItem = displayList[sourceIndex];
                    Path sourcePath(mCurrentDirectory); sourcePath.appendName(sourceItem.name);
                    Path targetPath(mCurrentDirectory); targetPath.appendName(item.name);

                    FileOp fileOperation{};
                    fileOperation.from = sourcePath;
                    fileOperation.to = targetPath;
                    fileOperation.opType = FileOpType::FILE_OP_MOVE;
                    mFileOpsWorker->addFileOperation(fileOperation);
                    //mUpdateFlag = true;
                }

                ImGui::EndDragDropTarget();
            }

            ImGui::SameLine(0.0f, 0.0f);
            ImGui::Text(item.isFile ? " " : "[]");

            ImGui::TableNextColumn();
            ImGui::Text(item.name.c_str());

            ImGui::PopID();
            /*
            ImGui::TableSetColumnIndex(2);
            for(u32 i = 0; i < FileOps::FileAttributesCount; i++) {
                //ImGui::SameLine();
                unsigned long attr = static_cast<unsigned long>(allAttribs[i]);
                unsigned long actual = attr & item.attributes;

                switch(static_cast<FileOps::FileAttributes>(actual)) {
                    case FileOps::FileAttributes::DIRECTORY:
                    {
                        ImGui::Text("Directory | ");
                    } break;
                    case FileOps::FileAttributes::ARCHIVE:
                    {
                        ImGui::Text("archive | ");
                    } break;
                    case FileOps::FileAttributes::COMPRESSED:
                    {
                        ImGui::Text("compressed | ");
                    } break;
                    case FileOps::FileAttributes::DEVICE:
                    {
                        ImGui::Text("device | ");
                    } break;
                    case FileOps::FileAttributes::ENCRYPTED:
                    {
                        ImGui::Text("encryted | ");
                    } break;
                    case FileOps::FileAttributes::HIDDEN:
                    {
                        ImGui::Text("hidden | ");
                    } break;
                    case FileOps::FileAttributes::INTEGRITY_STREAM:
                    {
                        ImGui::Text("integrity stream | ");
                    } break;
                    case FileOps::FileAttributes::NORMAL:
                    {
                        ImGui::Text("normal | ");
                    } break;
                    case FileOps::FileAttributes::NOT_CONTENT_INDEXED:
                    {
                        ImGui::Text("not indexed | ");
                    } break;
                    case FileOps::FileAttributes::NO_SCRUB_DATA:
                    {
                        ImGui::Text("no scrubs | ");
                    } break;
                    case FileOps::FileAttributes::OFFLINE:
                    {
                        ImGui::Text("offline | ");
                    } break;
                    case FileOps::FileAttributes::READONLY:
                    {
                        ImGui::Text("readonly | ");
                    } break;
                    case FileOps::FileAttributes::RECALL_ON_DATA_ACCESS:
                    {
                        ImGui::Text("recall on data access | ");
                    } break;
                    case FileOps::FileAttributes::RECALL_ON_OPEN:
                    {
                        ImGui::Text("recall on open | ");
                    } break;
                    case FileOps::FileAttributes::REPARSE_POINT:
                    {
                        ImGui::Text("Reparse point | ");
                    } break;
                    case FileOps::FileAttributes::SPARSE_FILE:
                    {
                        ImGui::Text("Sparse file| ");
                    } break;
                    case FileOps::FileAttributes::SYSTEM:
                    {
                        ImGui::Text("System | ");
                    } break;
                    case FileOps::FileAttributes::TEMPORARY:
                    {
                        ImGui::Text("Temporary | ");
                    } break;
                    case FileOps::FileAttributes::VIRTUAL:
                    {
                        ImGui::Text("Virtual | ");
                    } break;
                    default:
                    {
                        ImGui::Text("");
                    }
                }
                break;
            }
            */
        }
    }

    ImGui::EndTable();

    ImGui::EndChild();

    // copy selected items to "clipboard"
    if(ImGui::IsKeyDown(ImGuiKey_ModCtrl) && ImGui::IsKeyPressed(ImGuiKey_C, false)) {
        mClipboard.clear();
        for(size_t i = 0; i < mSelected.size(); i++) {
            if(mSelected[i]) {
                Path itemPath = mCurrentDirectory;
                itemPath.appendName(mDisplayList[i].name);
                mClipboard.push_back(itemPath);
            }
        }
    }


    // TODO: following copy/delete operations should be batched together

    // paste items from clipboard
    if(ImGui::IsKeyDown(ImGuiKey_ModCtrl) && ImGui::IsKeyPressed(ImGuiKey_V, false)) {
        for(Path itemPath : mClipboard) {
            FileOp fileOperation{};
            fileOperation.from = itemPath;
            fileOperation.to = mCurrentDirectory;
            fileOperation.opType = FileOpType::FILE_OP_COPY;
            mFileOpsWorker->addFileOperation(fileOperation);
        }
    }

    // delete selected
    if(ImGui::IsKeyPressed(ImGuiKey_Delete, false)) {
        for(size_t i = 0; i < mSelected.size(); i++) {
            if(mSelected[i]) {
                
                Path itemPath = mCurrentDirectory;
                itemPath.appendName(mDisplayList[i].name);

                FileOp fileOperation{};
                fileOperation.from = itemPath;
                fileOperation.opType = FileOpType::FILE_OP_DELETE;
                mFileOpsWorker->addFileOperation(fileOperation);
            }
        }
    }


    // get directory change events...


    {
        std::wstring pathWString = mCurrentDirectory.wstr();
        DWORD waitStatus;

        waitStatus = WaitForSingleObject(mDirChangeHandle, 0);

        switch(waitStatus) {
            case WAIT_OBJECT_0:
                {
                    mUpdateFlag = true;
                    FindNextChangeNotification(mDirChangeHandle);
                } break;
            default:
                {
                } break;
        }
    }


    if(mUpdateFlag) {
        //printf("Updating directory... %s\n", mCurrentDirectory.str().c_str());
        FileOps::enumerateDirectory(mCurrentDirectory, displayList);
        FileOps::sortByName(FileOps::SortDirection::Descending, displayList);
        FileOps::sortByType(FileOps::SortDirection::Ascending, displayList);

        mSelected.assign(mSelected.size(), false);

        mUpdateFlag = false;

        if(mDirChangeHandle != INVALID_HANDLE_VALUE) {
            FindCloseChangeNotification(mDirChangeHandle);
        }

        mDirChangeHandle = FindFirstChangeNotificationW(mCurrentDirectory.wstr().data(), FALSE, 
                FILE_NOTIFY_CHANGE_FILE_NAME
                );
    }
}
