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

void BrowserWidget::acceptMovePayload(Path targetPath) {
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

            if(sourcePath.isEmpty() || targetPath.isEmpty()) continue;

            BatchFileOperation::Operation op;
            op.from = sourcePath;
            op.to = targetPath;
            op.opType = FileOpType::FILE_OP_MOVE;
            fileOperation.operations.push_back(op);
        }
        mFileOpsWorker->addFileOperation(fileOperation);
    }
}

void BrowserWidget::update() {
    ImGuiIO& io = ImGui::GetIO();

    const std::string windowTitle = mCurrentDirectory.isEmpty() ? "This PC" : mCurrentDirectory.getLastSegment();
    std::string windowID = std::string(windowTitle + "###BrowserWidget" + std::to_string(mID));

    ImGuiWindowFlags mainWindowFlags = ImGuiWindowFlags_NoCollapse;

    if(!ImGui::Begin(windowID.c_str(), &mIsOpen, mainWindowFlags)){
        ImGui::End();
        return;
    }

    mWasFocused = mIsFocused;
    mIsFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

    if(BeginDrapDropTargetWindow(MOVE_PAYLOAD)) {
        acceptMovePayload(mCurrentDirectory);
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

    updateSearch();

    handleInput();

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

        mEditIdx = -1;
        mEditInput.clear();

        if(mDirChangeHandle != INVALID_HANDLE_VALUE) {
            FindCloseChangeNotification(mDirChangeHandle);
        }

        mDirChangeHandle = FindFirstChangeNotificationW(mCurrentDirectory.wstr().data(), FALSE, 
                FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME
                );
    }

    ImGui::End();
}

void BrowserWidget::handleInput() {
    if(mIsFocused) {
        if(ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            mSelected.assign(mSelected.size(), false);
            mCurrentHighlightIdx = -1;

            mNumSelected = 0;
            if(!mSearchWindowOpen) {
                mHighlighted.assign(mHighlighted.size(), false);
            }

            mEditIdx = -1;
            mEditInput.clear();
        }

        // rename key
        if(ImGui::IsKeyPressed(ImGuiKey_F2)) {
            mEditIdx = mSelected.empty() ? -1 : mRangeSelectionStart;
            mEditInput = mDisplayList[mEditIdx].name;
            mSelected.assign(mSelected.size(), false);
            mNumSelected = 0;
        }

        if(ImGui::IsKeyPressed(ImGuiKey_Slash) && !mSearchWindowOpen) {
            // show a search window
            mSearchWindowOpen = true;
        }

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
    }

    // clicked away...
    if(!mIsFocused && mWasFocused) {
        mEditIdx = -1;
        mEditInput.clear();
    }
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
            Path targetPath(mCurrentDirectory);

            int numPop = dirSegments.size() - i;
            while(--numPop > 0) targetPath.popSegment();
            acceptMovePayload(targetPath);

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

    static ImGuiTableFlags tableFlags = 
        ImGuiTableFlags_SizingStretchSame 
        | ImGuiTableFlags_Resizable 
        | ImGuiTableFlags_NoPadInnerX
        | ImGuiTableFlags_ContextMenuInBody 
        | ImGuiTableFlags_Sortable;

    // early out if table is being clipped
    if(!ImGui::BeginTable("DirectoryList", 3, tableFlags)) {
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

    ImGui::TableSetupColumn("Last Modified", nameColumnFlags);

    ImGuiListClipper clipper;
    clipper.Begin(displayList.size());

    mTableSortSpecs = ImGui::TableGetSortSpecs();

    ImGui::TableHeadersRow();

    bool isSelectingMultiple = io.KeyCtrl;
    bool isSelectingRange = io.KeyShift;

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

                    if(mRangeSelectionStart < 0) {
                        mRangeSelectionStart = i;
                        start = i;
                        end = i;
                    } else if(mRangeSelectionStart > i) {
                        start = i;
                        end = mRangeSelectionStart + 1;
                    } else if(mRangeSelectionStart < i) {
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

                if(ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                    if(item.isFile()) {
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

            if(!item.isFile() && ImGui::BeginDragDropTarget()) {
                Path targetPath(mCurrentDirectory); 
                targetPath.appendName(item.name);

                acceptMovePayload(targetPath);
                
                ImGui::EndDragDropTarget();
            }

            ImGui::SameLine(0.0f, 0.0f);

            if(item.isFile()) {
                ImGui::Text(ICON_FK_FILE_TEXT);
            } else {
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(222, 199, 53, 255));
                ImGui::Text(ICON_FK_FOLDER);
                ImGui::PopStyleColor();
            }

            ImGui::TableNextColumn();

            // Edit file name
            if(mEditIdx == i) {
                int inputFlags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll;

                std::string inputID = "###EditInput" + std::to_string(i);
                if(ImGui::InputText(inputID.c_str(), &mEditInput, inputFlags)) {
                    // rename current selection to mEditInput
                    BatchFileOperation batchOp;
                    BatchFileOperation::Operation op;
                    Path targetItem = mCurrentDirectory;
                    targetItem.appendName(mDisplayList[mEditIdx].name);

                    Path newName(mEditInput);
                    
                    op.opType = FileOpType::FILE_OP_RENAME;
                    op.from = targetItem;
                    op.to = newName;
                    batchOp.operations.push_back(op);

                    mFileOpsWorker->addFileOperation(batchOp);

                    mEditIdx = -1;
                }

                if (ImGui::IsItemHovered() || (!ImGui::IsAnyItemActive() && !ImGui::IsMouseClicked(0)))
                    ImGui::SetKeyboardFocusHere(-1); // Auto focus previous widget
            } else {
                ImGui::Text(item.name.c_str());
            }

            if(mHighlighted[i]) {
                ImVec2 cursor = ImGui::GetCursorScreenPos();
                ImVec2 max{cursor.x + ImGui::CalcItemWidth(), cursor.y - ImGui::GetTextLineHeightWithSpacing()};

                drawList->AddRectFilled(cursor, max, IM_COL32(220, 199, 53, 100));

                if(i == mCurrentHighlightIdx) {
                    drawList->AddRect(cursor, max, IM_COL32(255, 0, 0, 255));
                }
            }

            ImGui::TableNextColumn();
            ImGui::Text("%u-%02u-%02u %02u:%02u %s",
                    item.lastModified.year,
                    item.lastModified.month,
                    item.lastModified.day,
                    item.lastModified.hour,
                    item.lastModified.minute,
                    item.lastModified.isPM ? "PM" : "AM");


            ImGui::PopID();
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

