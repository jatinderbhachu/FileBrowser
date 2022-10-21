#include "BrowserWidget.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <misc/cpp/imgui_stdlib.h>
#include <regex>
#include "FileSystem.h"
#include "FileOpsWorker.h"

#include <IconsForkAwesome.h>

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
    mID = IDCounter++;
    if(FileSystem::doesPathExist(path)) {
        setCurrentDirectory(path);
    } else {
        std::vector<char> driveLetters;
        FileSystem::getDriveLetters(driveLetters);

        assert(!driveLetters.empty());
        std::string rootDrive = std::string(1, driveLetters[0]) + ":\\";
        setCurrentDirectory(Path(rootDrive));
    }
}

void BrowserWidget::setCurrentDirectory(const Path& path) {
    mUpdateFlag = true;
    mCurrentDirectory = path;
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

    BatchFileOperation fileOperation{};
    for(int itemToRename : itemsToRename) {
        FileSystem::Record& item = mDisplayList[itemToRename];

        Path sourcePath(mCurrentDirectory);
        sourcePath.appendName(item.name);

        std::string newName = std::regex_replace( item.name, std::regex(from), to );
        Path targetPath(newName);

        BatchFileOperation::Operation op;
        op.from = sourcePath;
        op.to = targetPath;
        op.opType = FileOpType::FILE_OP_RENAME;
        fileOperation.operations.push_back(op);
    }
    mFileOpsWorker->addFileOperation(fileOperation);
}

bool BeginDrapDropTargetWindow(const char* payload_type) {
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

void BrowserWidget::update(bool& isFocused, bool& isOpenFlag) {
    ImGuiIO& io = ImGui::GetIO();

    std::string widgetUniqueName = std::string("BrowserWidget###" + std::to_string(mID));

    ImGuiWindowFlags mainWindowFlags = ImGuiWindowFlags_NoCollapse;

    if(!ImGui::Begin(widgetUniqueName.c_str(), &isOpenFlag, mainWindowFlags)){
        ImGui::End();
        return;
    }

    isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

    updateSearch();

    if(BeginDrapDropTargetWindow(MOVE_PAYLOAD)) {
        const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(MOVE_PAYLOAD, ImGuiDragDropFlags_SourceAutoExpirePayload);
        if(payload != nullptr) {
            void* voidPayload = nullptr;

            memcpy(&voidPayload, payload->Data, payload->DataSize);
            MovePayload* movePayload = (MovePayload*)voidPayload;

            std::vector<FileSystem::Record>& sourceDisplayList = *movePayload->sourceDisplayList;

            BatchFileOperation fileOperation{};
            for(int sourceIndex : movePayload->itemsToMove) {
                const FileSystem::Record& sourceItem = sourceDisplayList[sourceIndex];

                Path sourcePath(movePayload->sourcePath); sourcePath.appendName(sourceItem.name);
                Path targetPath(mCurrentDirectory);

                if(sourcePath.isEmpty() || targetPath.isEmpty()) continue;

                BatchFileOperation::Operation op;
                op.from = sourcePath;
                op.to = targetPath;
                op.opType = FileOpType::FILE_OP_MOVE;
                fileOperation.operations.push_back(op);
            }
            mFileOpsWorker->addFileOperation(fileOperation);
        }
        ImGui::EndDragDropTarget();
    }

    directorySegments();

    // up directory button
    if(ImGui::Button("..")) {
        mCurrentDirectory.popSegment();
        mUpdateFlag = true;
    }

    ImGui::SameLine();

    if(ImGui::Button("Refresh")) {
        mUpdateFlag = true;
    }

    switch(mDisplayListType) {
        case DisplayListType::DEFAULT: 
            {
                directoryTable();
            } break;
        case DisplayListType::DRIVE: 
            {
                driveList();
            } break;
        case DisplayListType::PATH_NOT_FOUND_ERROR: 
            {
                ImGui::Text("Path not found...");
            } break;
    }

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
        BatchFileOperation fileOperation{};
        for(Path itemPath : mClipboard) {
            BatchFileOperation::Operation op;
            op.from = itemPath;
            op.to = mCurrentDirectory;
            op.opType = FileOpType::FILE_OP_COPY;
            fileOperation.operations.push_back(op);
        }
        mFileOpsWorker->addFileOperation(fileOperation);
    }

    // delete selected
    if(ImGui::IsKeyPressed(ImGuiKey_Delete, false)) {
        BatchFileOperation fileOperation{};
        for(size_t i = 0; i < mSelected.size(); i++) {
            if(mSelected[i]) {
                Path itemPath = mCurrentDirectory;
                itemPath.appendName(mDisplayList[i].name);

                BatchFileOperation::Operation op;
                op.from = itemPath;
                op.opType = FileOpType::FILE_OP_DELETE;
                fileOperation.operations.push_back(op);
            }
        }
        mFileOpsWorker->addFileOperation(fileOperation);
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
        if(mTableSortSpecs->SpecsDirty && mTableSortSpecs->Specs != nullptr) {
            mUpdateFlag = true;
            mTableSortSpecs->SpecsDirty = false;
            if(mTableSortSpecs->Specs->SortDirection == ImGuiSortDirection_Ascending) {
                mSortDirection = FileSystem::SortDirection::Ascending;
            } else {
                mSortDirection = FileSystem::SortDirection::Descending;
            }
        }
    }

    if(mUpdateFlag) {
        mUpdateFlag = false;
        if(mCurrentDirectory.isEmpty()) {
            std::vector<char> driveLetters;
            FileSystem::getDriveLetters(driveLetters);
            mDriveList.clear();
            for(char driveLetter : driveLetters) {
                DriveRecord driveRecord{};
                driveRecord.letter = driveLetter;

                // TODO: get the actual drive name
                driveRecord.name = std::string(1, driveLetter);
                
                mDriveList.push_back(driveRecord);
            }
            mDisplayListType = DisplayListType::DRIVE;
        } else {
            if(FileSystem::enumerateDirectory(mCurrentDirectory, mDisplayList)) {
                mDisplayListType = DisplayListType::DEFAULT;
            } else {
                mDisplayListType = DisplayListType::PATH_NOT_FOUND_ERROR;
            }
        }

        FileSystem::sortByName(mSortDirection, mDisplayList);
        FileSystem::sortByType(mSortDirection, mDisplayList);

        mSelected.assign(mSelected.size(), false);
        mNumSelected = 0;

        mHighlighted.assign(mHighlighted.size(), false);
        mCurrentHighlightIdx = -1;


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
    ImGuiStyle style = ImGui::GetStyle();

    float childHeight = ImGui::GetTextLineHeightWithSpacing();

    ImGuiWindowFlags childFlags = ImGuiWindowFlags_NoDecoration;

    if(!ImGui::BeginChild("DirectorySegments", ImVec2(0, childHeight), false, childFlags)) {
        ImGui::EndChild();
        return;
    }

    std::string uniqueID = "This PC###ThisPCSegment";
    const ImVec2 text_size = ImGui::CalcTextSize("This PC", false, false);
    ImVec2 cursorPos = ImGui::GetCursorPos();

    if(ImGui::Selectable(uniqueID.c_str(), false, ImGuiSelectableFlags_None | ImGuiSelectableFlags_AllowItemOverlap, text_size)) {
        while(!mCurrentDirectory.isEmpty()) 
            mCurrentDirectory.popSegment();

        mUpdateFlag = true;
    }

    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();

    std::vector<std::string_view> dirSegments = mCurrentDirectory.getSegments();

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

                std::vector<FileSystem::Record>& sourceDisplayList = *movePayload->sourceDisplayList;

                BatchFileOperation fileOperation{};
                for(int sourceIndex : movePayload->itemsToMove) {
                    const FileSystem::Record& sourceItem = sourceDisplayList[sourceIndex];

                    Path sourcePath(movePayload->sourcePath); sourcePath.appendName(sourceItem.name);
                    Path targetPath(mCurrentDirectory);

                    int numPop = dirSegments.size() - i;
                    while(--numPop > 0) targetPath.popSegment();

                    if(!(sourcePath.isEmpty() && targetPath.isEmpty())) continue;

                    BatchFileOperation::Operation op;
                    op.from = sourcePath;
                    op.to = targetPath;
                    op.opType = FileOpType::FILE_OP_MOVE;
                    fileOperation.operations.push_back(op);
                }
                mFileOpsWorker->addFileOperation(fileOperation);
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

    ImGui::EndChild();
}

void BrowserWidget::directoryTable() {
    std::vector<FileSystem::Record>& displayList = mDisplayList;

    mSelected.resize(displayList.size());
    mHighlighted.resize(displayList.size());

    ImGuiIO& io = ImGui::GetIO();

    // early out if window is being clipped
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_HorizontalScrollbar;
    if(!ImGui::BeginChild("DirectoryView", ImGui::GetContentRegionAvail(), false, window_flags)) {
        ImGui::EndChild();
        return;
    }

    static const FileSystem::FileAttributes allAttribs[] = {
        FileSystem::FileAttributes::ARCHIVE,
        FileSystem::FileAttributes::COMPRESSED,
        FileSystem::FileAttributes::DEVICE,
        FileSystem::FileAttributes::DIRECTORY,
        FileSystem::FileAttributes::ENCRYPTED,
        FileSystem::FileAttributes::HIDDEN,
        FileSystem::FileAttributes::INTEGRITY_STREAM,
        FileSystem::FileAttributes::NORMAL,
        FileSystem::FileAttributes::NOT_CONTENT_INDEXED,
        FileSystem::FileAttributes::NO_SCRUB_DATA,
        FileSystem::FileAttributes::OFFLINE,
        FileSystem::FileAttributes::READONLY,
        FileSystem::FileAttributes::RECALL_ON_DATA_ACCESS,
        FileSystem::FileAttributes::RECALL_ON_OPEN,
        FileSystem::FileAttributes::REPARSE_POINT,
        FileSystem::FileAttributes::SPARSE_FILE,
        FileSystem::FileAttributes::SYSTEM,
        FileSystem::FileAttributes::TEMPORARY,
        FileSystem::FileAttributes::VIRTUAL
    };

    static ImGuiTableFlags tableFlags = 
        ImGuiTableFlags_SizingStretchSame 
        | ImGuiTableFlags_Resizable 
        | ImGuiTableFlags_NoPadInnerX
        | ImGuiTableFlags_ContextMenuInBody 
        | ImGuiTableFlags_Sortable;

    // early out if table is being clipped
    if(!ImGui::BeginTable("DirectoryList", 2, tableFlags)) {
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
    bool isClearingSelection = ImGui::IsKeyPressed(ImGuiKey_Escape);

    if(ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && isClearingSelection) {
        mSelected.assign(mSelected.size(), false);
        mCurrentHighlightIdx = -1;

        mNumSelected = 0;
        if(!mSearchWindowOpen) {
            mHighlighted.assign(mHighlighted.size(), false);
        }
    }

    mTableSortSpecs = ImGui::TableGetSortSpecs();

    ImGui::TableHeadersRow();

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    while(clipper.Step()) {
        for(int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
            std::string uniqueID = "##FileRecord" + std::to_string(i);
            ImGui::PushID(uniqueID.c_str());

            const FileSystem::Record& item = displayList[i];
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

                if(ImGui::IsMouseDoubleClicked(0)) {
                    if(item.isFile) {
                        Path filePath = mCurrentDirectory;
                        filePath.appendName(item.name);
                        FileSystem::openFile(filePath);
                    } else {
                        mCurrentDirectory.appendName(item.name);
                        mUpdateFlag = true;
                    }
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


                    std::vector<FileSystem::Record>& sourceDisplayList = *movePayload->sourceDisplayList;

                    BatchFileOperation fileOperation{};
                    for(int sourceIndex : movePayload->itemsToMove) {
                        const FileSystem::Record& sourceItem = sourceDisplayList[sourceIndex];

                        Path sourcePath(movePayload->sourcePath); sourcePath.appendName(sourceItem.name);
                        Path targetPath(mCurrentDirectory); targetPath.appendName(item.name);

                        if(sourcePath.isEmpty() || targetPath.isEmpty()) continue;

                        BatchFileOperation::Operation op;
                        op.from = sourcePath;
                        op.to = targetPath;
                        op.opType = FileOpType::FILE_OP_MOVE;
                        fileOperation.operations.push_back(op);
                    }
                    mFileOpsWorker->addFileOperation(fileOperation);
                    mUpdateFlag = true;
                }

                ImGui::EndDragDropTarget();
            }

            ImGui::SameLine(0.0f, 0.0f);

            if(item.isFile) {
                ImGui::Text(ICON_FK_FILE_TEXT);
            } else {
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(222, 199, 53, 255));
                ImGui::Text(ICON_FK_FOLDER);
                ImGui::PopStyleColor();
            }

            ImGui::TableNextColumn();
            ImGui::Text(item.name.c_str());

            if(mHighlighted[i]) {
                ImVec2 cursor = ImGui::GetCursorScreenPos();
                ImVec2 max{cursor.x + ImGui::CalcItemWidth(), cursor.y - ImGui::GetTextLineHeightWithSpacing()};

                drawList->AddRectFilled(cursor, max, IM_COL32(220, 199, 53, 100));

                if(i == mCurrentHighlightIdx) {
                    drawList->AddRect(cursor, max, IM_COL32(255, 0, 0, 255));
                }
            }

            ImGui::PopID();
            /*
            ImGui::TableSetColumnIndex(2);
            for(int i = 0; i < FileSystem::FileAttributesCount; i++) {
                //ImGui::SameLine();
                unsigned long attr = static_cast<unsigned long>(allAttribs[i]);
                unsigned long actual = attr & item.attributes;

                switch(static_cast<FileSystem::FileAttributes>(actual)) {
                    case FileSystem::FileAttributes::DIRECTORY:
                    {
                        ImGui::Text("Directory | ");
                    } break;
                    case FileSystem::FileAttributes::ARCHIVE:
                    {
                        ImGui::Text("archive | ");
                    } break;
                    case FileSystem::FileAttributes::COMPRESSED:
                    {
                        ImGui::Text("compressed | ");
                    } break;
                    case FileSystem::FileAttributes::DEVICE:
                    {
                        ImGui::Text("device | ");
                    } break;
                    case FileSystem::FileAttributes::ENCRYPTED:
                    {
                        ImGui::Text("encryted | ");
                    } break;
                    case FileSystem::FileAttributes::HIDDEN:
                    {
                        ImGui::Text("hidden | ");
                    } break;
                    case FileSystem::FileAttributes::INTEGRITY_STREAM:
                    {
                        ImGui::Text("integrity stream | ");
                    } break;
                    case FileSystem::FileAttributes::NORMAL:
                    {
                        ImGui::Text("normal | ");
                    } break;
                    case FileSystem::FileAttributes::NOT_CONTENT_INDEXED:
                    {
                        ImGui::Text("not indexed | ");
                    } break;
                    case FileSystem::FileAttributes::NO_SCRUB_DATA:
                    {
                        ImGui::Text("no scrubs | ");
                    } break;
                    case FileSystem::FileAttributes::OFFLINE:
                    {
                        ImGui::Text("offline | ");
                    } break;
                    case FileSystem::FileAttributes::READONLY:
                    {
                        ImGui::Text("readonly | ");
                    } break;
                    case FileSystem::FileAttributes::RECALL_ON_DATA_ACCESS:
                    {
                        ImGui::Text("recall on data access | ");
                    } break;
                    case FileSystem::FileAttributes::RECALL_ON_OPEN:
                    {
                        ImGui::Text("recall on open | ");
                    } break;
                    case FileSystem::FileAttributes::REPARSE_POINT:
                    {
                        ImGui::Text("Reparse point | ");
                    } break;
                    case FileSystem::FileAttributes::SPARSE_FILE:
                    {
                        ImGui::Text("Sparse file| ");
                    } break;
                    case FileSystem::FileAttributes::SYSTEM:
                    {
                        ImGui::Text("System | ");
                    } break;
                    case FileSystem::FileAttributes::TEMPORARY:
                    {
                        ImGui::Text("Temporary | ");
                    } break;
                    case FileSystem::FileAttributes::VIRTUAL:
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

    clipper.End();

    if(mCurrentHighlightIdx >= 0 && mHighlightNextItem) {
        mHighlightNextItem = false;
        float item_pos_y = clipper.StartPosY + (clipper.ItemsHeight * mCurrentHighlightIdx);
        ImGui::SetScrollFromPosY(item_pos_y - ImGui::GetWindowPos().y);
    }

    ImGui::EndTable();

    ImGui::EndChild();
}

void BrowserWidget::driveList() {
    ImGuiIO& io = ImGui::GetIO();

    // early out if window is being clipped
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_HorizontalScrollbar;
    if(!ImGui::BeginChild("DirectoryView", ImGui::GetContentRegionAvail(), false, window_flags)) {
        ImGui::EndChild();
        return;
    }


    static ImGuiTableFlags tableFlags = 
        ImGuiTableFlags_SizingStretchSame 
        | ImGuiTableFlags_Resizable 
        | ImGuiTableFlags_NoPadInnerX
        | ImGuiTableFlags_ContextMenuInBody 
        | ImGuiTableFlags_Sortable;

    // early out if table is being clipped
    if(!ImGui::BeginTable("DriveList", 2, tableFlags)) {
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
    clipper.Begin(mDriveList.size());

    mSelected.resize(mDriveList.size());

    mTableSortSpecs = ImGui::TableGetSortSpecs();

    ImGui::TableHeadersRow();

    while(clipper.Step()) {
        for(int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
            std::string uniqueID = "##DriveRecord" + std::to_string(i);
            ImGui::PushID(uniqueID.c_str());

            const DriveRecord& item = mDriveList[i];
            bool isSelected = mSelected[i];

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            if(ImGui::Selectable("##selectable", isSelected, ImGuiSelectableFlags_AllowDoubleClick | ImGuiSelectableFlags_SpanAllColumns )) {
                if(ImGui::IsMouseDoubleClicked(0)) {
                    mCurrentDirectory.appendName(std::string(1, item.letter) + ":");
                    mUpdateFlag = true;
                }
            }

            ImGui::SameLine(0.0f, 0.0f);

            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(222, 199, 53, 255));
            ImGui::Text(ICON_FK_CIRCLE_O_NOTCH);
            ImGui::PopStyleColor();

            ImGui::TableNextColumn();
            ImGui::Text(item.name.c_str());

            ImGui::PopID();
        }
    }

    clipper.End();

    ImGui::EndTable();

    ImGui::EndChild();
}

void BrowserWidget::updateSearch() {
    bool isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    if(ImGui::IsKeyPressed(ImGuiKey_Slash) && !mSearchWindowOpen && isFocused) {
        // show a search window
        mSearchWindowOpen = true;
    }

    int searchWindowFlags = ImGuiWindowFlags_NoDecoration
        | ImGuiWindowFlags_NoDocking;

    if(mSearchWindowOpen) {
        float width = ImGui::GetWindowWidth();
        float height = ImGui::GetWindowHeight();

        

        float lineHeight = ImGui::GetTextLineHeightWithSpacing();
        ImGui::SetNextWindowSize({width, lineHeight});
        ImGui::SetNextWindowPos({ImGui::GetWindowPos().x, height - lineHeight * 2.0f});

        ImGui::Begin("###SearchWindow", nullptr, searchWindowFlags);

        int inputFlags = ImGuiInputTextFlags_AutoSelectAll;

        ImGui::SetNextItemWidth(-1.0f);
        ImGui::SetKeyboardFocusHere(0);
        if(ImGui::InputText("###SearchInput", &mSearchFilter, inputFlags) && !mSearchFilter.empty()) {
            mHighlighted.assign(mHighlighted.size(), false);
            mCurrentHighlightIdx = -1;
            for(int i = 0; i < mDisplayList.size(); i++) {
                FileSystem::Record& item = mDisplayList[i];
                if(item.name.find(mSearchFilter) != std::string::npos) {
                    if(mCurrentHighlightIdx < 0) mCurrentHighlightIdx = i;
                    mHighlighted[i] = true;
                }
            }
            mHighlightNextItem = true;
        }

        if(ImGui::IsKeyPressed(ImGuiKey_Enter)) {
            mSearchWindowOpen = false;
        }

        if(ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            if(mSearchFilter.empty()) {
                mHighlighted.assign(mHighlighted.size(), false);
                mCurrentHighlightIdx = -1;
            }
            mSearchWindowOpen = false;
        } 
        ImGui::End();
    }

    if(mCurrentHighlightIdx >= 0 && ImGui::IsKeyPressed(ImGuiKey_N)) {
        int start = mCurrentHighlightIdx + 1;
        int end = mHighlighted.size();
        int diff = 1;

        if(ImGui::IsKeyDown(ImGuiKey_LeftShift)) {
            start = mCurrentHighlightIdx - 1;
            end = 0;
            diff = -1;
        }

        for(int i = start; i != end; i += diff) {
            if(mHighlighted[i]) {
                mCurrentHighlightIdx = i;
                break;
            }
        }
        mHighlightNextItem = true;
    }
}

