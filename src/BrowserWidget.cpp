#include "BrowserWidget.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <regex>
#include "FileOps.h"
#include "FileOpsWorker.h"

#ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

inline static const char* MOVE_PAYLOAD = "MOVE_PAYLOAD";

BrowserWidget::BrowserWidget(const Path& path, FileOpsWorker* fileOpsWorker) 
    : mCurrentDirectory(path),
    mUpdateFlag(true),
    mFileOpsWorker(fileOpsWorker)
{
    setCurrentDirectory(path);
}

static inline OVERLAPPED overlapped;

void BrowserWidget::setCurrentDirectory(const Path& path) {
    mUpdateFlag = true;
    if(path.getType() == Path::PATH_RELATIVE) {
        mCurrentDirectory = path;
    }
}

Path BrowserWidget::getCurrentDirectory() const {
    return mCurrentDirectory;
}


void BrowserWidget::renameSelected(const std::string& from, const std::string& to) {
    if(mNumSelected <= 0) return;

    std::vector<int> itemsToRename;
    for(int i = 0; i < mSelected.size(); i++) {
        if(mSelected[i]) itemsToRename.push_back(i);
    }

    for(int itemToRename : itemsToRename) {
        FileOps::Record& item = mDisplayList[itemToRename];

        Path sourcePath(mCurrentDirectory);
        sourcePath.appendName(item.name);

        std::string newName = std::regex_replace( item.name, std::regex(from), to );
        Path targetPath(newName);

        FileOp fileOperation{};
        fileOperation.from = sourcePath;
        fileOperation.to = targetPath;
        fileOperation.opType = FileOpType::FILE_OP_RENAME;
        mFileOpsWorker->addFileOperation(fileOperation);

    }

}

bool BeginDrapDropTargetWindow(const char* payload_type)
{
    using namespace ImGui;
    ImRect inner_rect = GetCurrentWindow()->InnerRect;
    if (BeginDragDropTargetCustom(inner_rect, GetID("##WindowBgArea"))) {
        if (const ImGuiPayload* payload = AcceptDragDropPayload(payload_type, ImGuiDragDropFlags_AcceptBeforeDelivery | ImGuiDragDropFlags_AcceptNoDrawDefaultRect))
        {
            if (payload->IsPreview())
            {
                ImDrawList* draw_list = GetForegroundDrawList();
                draw_list->AddRectFilled(inner_rect.Min, inner_rect.Max, GetColorU32(ImGuiCol_DragDropTarget, 0.05f));
                draw_list->AddRect(inner_rect.Min, inner_rect.Max, GetColorU32(ImGuiCol_DragDropTarget), 0.0f, 0, 2.0f);
            }
            if (payload->IsDelivery())
                return true;
        }
        EndDragDropTarget();
    }
    return false;
}

void BrowserWidget::draw(int id) {
    ImGuiIO& io = ImGui::GetIO();

    std::string widgetUniqueName = std::string("BrowserWidget###" + std::to_string(id));

    ImGuiWindowFlags mainWindowFlags = ImGuiWindowFlags_NoCollapse;
    ImGui::Begin(widgetUniqueName.c_str(), NULL, mainWindowFlags);

    if(BeginDrapDropTargetWindow(MOVE_PAYLOAD)) {
        const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(MOVE_PAYLOAD, ImGuiDragDropFlags_SourceAutoExpirePayload);
        if(payload != nullptr) {
            void* voidPayload = nullptr;

            memcpy(&voidPayload, payload->Data, payload->DataSize);
            MovePayload* movePayload = (MovePayload*)voidPayload;

            std::vector<FileOps::Record>& sourceDisplayList = *movePayload->sourceDisplayList;

            for(int sourceIndex : movePayload->itemsToMove) {
                const FileOps::Record& sourceItem = sourceDisplayList[sourceIndex];

                Path sourcePath(movePayload->sourcePath); sourcePath.appendName(sourceItem.name);
                Path targetPath(mCurrentDirectory);

                FileOp fileOperation{};
                fileOperation.from = sourcePath;
                fileOperation.to = targetPath;
                fileOperation.opType = FileOpType::FILE_OP_MOVE;
                mFileOpsWorker->addFileOperation(fileOperation);
            }
        }
        ImGui::EndDragDropTarget();
    }

    directorySegments();

    ImGui::NewLine();

    // up directory button
    if(ImGui::Button("..")) {
        mCurrentDirectory.popSegment();
        mUpdateFlag = true;
    }

    ImGui::SameLine();

    if(ImGui::Button("Refresh")) {
        mUpdateFlag = true;
    }

    browserTable();

    // TODO: following copy/delete operations should be batched together
    // TODO: copy list of items to actual SYSTEM clipboard :)

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

    // TODO: should check which columns to sort by. For now just sort by file name
    if(mTableSortSpecs != nullptr) {
        if(mTableSortSpecs->SpecsDirty) {
            mUpdateFlag = true;
            mTableSortSpecs->SpecsDirty = false;
            if(mTableSortSpecs->Specs->SortDirection == ImGuiSortDirection_Ascending) {
                mSortDirection = FileOps::SortDirection::Ascending;
            } else {
                mSortDirection = FileOps::SortDirection::Descending;
            }
        }
    }

    if(mUpdateFlag) {
        FileOps::enumerateDirectory(mCurrentDirectory, mDisplayList);
        FileOps::sortByName(mSortDirection, mDisplayList);
        FileOps::sortByType(mSortDirection, mDisplayList);

        mSelected.assign(mSelected.size(), false);
        mNumSelected = 0;

        mUpdateFlag = false;

        if(mDirChangeHandle != INVALID_HANDLE_VALUE) {
            FindCloseChangeNotification(mDirChangeHandle);
        }

        mDirChangeHandle = FindFirstChangeNotificationW(mCurrentDirectory.wstr().data(), FALSE, 
                FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME
                );
    }

    ImGui::End();
}

void BrowserWidget::directorySegments() {
    auto dirSegments = mCurrentDirectory.getSegments();

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
            const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(MOVE_PAYLOAD);
            if(payload != nullptr && payload->Data != nullptr) {
                void* voidPayload = nullptr;

                memcpy(&voidPayload, payload->Data, payload->DataSize);
                MovePayload* movePayload = (MovePayload*)voidPayload;

                std::vector<FileOps::Record>& sourceDisplayList = *movePayload->sourceDisplayList;

                for(int sourceIndex : movePayload->itemsToMove) {
                    const FileOps::Record& sourceItem = sourceDisplayList[sourceIndex];

                    Path sourcePath(movePayload->sourcePath); sourcePath.appendName(sourceItem.name);
                    Path targetPath(mCurrentDirectory);

                    int numPop = dirSegments.size() - i;
                    while(--numPop > 0) targetPath.popSegment();

                    FileOp fileOperation{};
                    fileOperation.from = sourcePath;
                    fileOperation.to = targetPath;
                    fileOperation.opType = FileOpType::FILE_OP_MOVE;
                    mFileOpsWorker->addFileOperation(fileOperation);
                }
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

void BrowserWidget::browserTable() {
    std::vector<FileOps::Record>& displayList = mDisplayList;
    ImGuiIO& io = ImGui::GetIO();

    // early out if window is being clipped
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_HorizontalScrollbar;
    if(!ImGui::BeginChild("DirectoryView", ImGui::GetContentRegionAvail(), false, window_flags)) {
        ImGui::EndChild();
        return;
    }

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

    // early out if table is being clipped
    if(!ImGui::BeginTable("BrowserWidget", 2, tableFlags)) {
        ImGui::EndTable();
        return;
    }

    int iconColumnFlags = 
        ImGuiTableColumnFlags_NoHeaderLabel 
        | ImGuiTableColumnFlags_WidthFixed 
        | ImGuiTableColumnFlags_NoResize 
        | ImGuiTableColumnFlags_IndentDisable
        | ImGuiTableColumnFlags_NoSort;
    ImVec2 iconColumnSize = ImGui::CalcTextSize("[]");
    ImGui::TableSetupColumn("icon", iconColumnFlags, iconColumnSize.x);

    int nameColumnFlags = ImGuiTableColumnFlags_IndentDisable | ImGuiTableColumnFlags_PreferSortDescending;
    ImGui::TableSetupColumn("Name", nameColumnFlags);

    ImGuiListClipper clipper;
    clipper.Begin(displayList.size());

    bool isSelectingMultiple = io.KeyCtrl;
    bool isSelectingRange = io.KeyShift;

    mSelected.resize(displayList.size());

    mTableSortSpecs = ImGui::TableGetSortSpecs();

    ImGui::TableHeadersRow();

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
                    mNumSelected++;
                    mRangeSelectionStart = i;
                } else if(isSelectingRange) {
                    int start, end = -1;

                    if(mRangeSelectionStart > i) {
                        start = i;
                        end = mRangeSelectionStart + 1;
                    } else {
                        start = mRangeSelectionStart;
                        end = i + 1;
                    }

                    mSelected.assign(mSelected.size(), false);

                    for(int j = start; j < end; j++) {
                        mSelected[j] = true;
                    }
                    mNumSelected = end - start;
                } else {
                    mSelected.assign(mSelected.size(), false);
                    mSelected[i] = true;
                    mNumSelected = 1;
                    mRangeSelectionStart = i;
                }

                if(ImGui::IsMouseDoubleClicked(0) && !item.isFile) {
                    mCurrentDirectory.appendName(item.name);
                    mUpdateFlag = true;
                }
            }

            isSelected = mSelected[i];

            if(mNumSelected > 0) {
                if(ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceNoPreviewTooltip)) {
                    // can only DragDrop a selected item
                    if(isSelected) {
                        mMovePayload.itemsToMove.clear();
                        mMovePayload.sourcePath = mCurrentDirectory;
                        mMovePayload.sourceDisplayList = &mDisplayList;

                        for(int j = 0; j < mSelected.size(); j++) {
                            if(mSelected[j]) mMovePayload.itemsToMove.push_back(j); 
                        }
                        
                        size_t payloadSize = sizeof(void*);
                        void* payloadPtr = static_cast<void*>(&mMovePayload);
                        bool accepted = ImGui::SetDragDropPayload(MOVE_PAYLOAD, &payloadPtr, payloadSize, ImGuiCond_Once);
                    } else {
                        mSelected.assign(mSelected.size(), false);
                        mNumSelected = 0;
                    }
                    ImGui::EndDragDropSource();
                }
            }

            if(!item.isFile && ImGui::BeginDragDropTarget()) {
                const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(MOVE_PAYLOAD, ImGuiDragDropFlags_SourceAutoExpirePayload);
                if(payload != nullptr) {
                    void* voidPayload = nullptr;

                    memcpy(&voidPayload, payload->Data, payload->DataSize);
                    MovePayload* movePayload = (MovePayload*)voidPayload;


                    std::vector<FileOps::Record>& sourceDisplayList = *movePayload->sourceDisplayList;

                    for(int sourceIndex : movePayload->itemsToMove) {
                        const FileOps::Record& sourceItem = sourceDisplayList[sourceIndex];

                        Path sourcePath(movePayload->sourcePath); sourcePath.appendName(sourceItem.name);
                        Path targetPath(mCurrentDirectory); targetPath.appendName(item.name);

                        FileOp fileOperation{};
                        fileOperation.from = sourcePath;
                        fileOperation.to = targetPath;
                        fileOperation.opType = FileOpType::FILE_OP_MOVE;
                        mFileOpsWorker->addFileOperation(fileOperation);
                    }
                    mUpdateFlag = true;
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
}
