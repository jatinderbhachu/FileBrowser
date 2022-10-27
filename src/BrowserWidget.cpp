#include "BrowserWidget.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <misc/cpp/imgui_stdlib.h>
#include <regex>
#include "FileSystem.h"
#include "FileOpsWorker.h"
#include "DirectoryWatcher.h"

#include <tracy/Tracy.hpp>

#include <IconsForkAwesome.h>

#ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

inline static const char* MOVE_PAYLOAD = "MOVE_PAYLOAD";

inline static const char* DISPLAY_COLUMN_ICON = "Icon##DisplayColumn";
inline static const char* DISPLAY_COLUMN_NAME = "Name##DisplayColumn";
inline static const char* DISPLAY_COLUMN_LAST_MODIFIED = "LastModified##DisplayColumn";
inline static const char* DISPLAY_COLUMN_SIZE = "Size##DisplayColumn";

inline static ImGuiID DISPLAY_COLUMN_NAME_ID;
inline static ImGuiID DISPLAY_COLUMN_LAST_MODIFIED_ID;
inline static ImGuiID DISPLAY_COLUMN_SIZE_ID;


inline static std::string PrettyPrintSize(uint64_t size) {
    for(auto unit : { " B", " KB", " MB", " GB", " TB", " PB", " EB", " ZB" }) {
        if(size < 1024.0) return std::string(std::to_string(size) + unit);

        size /= 1024.0;
    }
    return std::string(std::to_string(size) + " B");
}

BrowserWidget::BrowserWidget(const Path& path, FileOpsWorker* fileOpsWorker) 
    : mCurrentDirectory(path),
    mDirectoryChanged(true),
    mFileOpsWorker(fileOpsWorker)
{
    mDirectoryWatcher.changeDirectory(mCurrentDirectory);
    mID = IDCounter++;
    if(FileSystem::doesPathExist(path)) {
        setCurrentDirectory(path);
    } else {
        setCurrentDirectory(FileSystem::getCurrentProcessPath());
    }

    DISPLAY_COLUMN_NAME_ID          = ImHashStr(DISPLAY_COLUMN_NAME,            strlen(DISPLAY_COLUMN_NAME), 0);
    DISPLAY_COLUMN_LAST_MODIFIED_ID = ImHashStr(DISPLAY_COLUMN_LAST_MODIFIED,   strlen(DISPLAY_COLUMN_LAST_MODIFIED), 0);
    DISPLAY_COLUMN_SIZE_ID          = ImHashStr(DISPLAY_COLUMN_SIZE,            strlen(DISPLAY_COLUMN_SIZE), 0);
}

void BrowserWidget::setCurrentDirectory(const Path& path) {
    mDirectoryChanged = true;
    mCurrentDirectory = path;
}

Path BrowserWidget::getCurrentDirectory() const {
    return mCurrentDirectory;
}

void BrowserWidget::renameSelected(const std::string& from, const std::string& to) {
    if(mSelection.count() <= 0) return;

    const std::vector<size_t>& itemsToRename = mSelection.indexes;

    FileSystem::SOARecord& displayList = mDirectoryWatcher.mRecords;

    BatchFileOperation fileOperation{};
    for(int itemToRename : itemsToRename) {
        const std::string& name = displayList.getName(itemToRename);

        Path sourcePath(mCurrentDirectory);
        sourcePath.appendName(name);

        std::string newName = std::regex_replace( name, std::regex(from), to );
        if(newName == name) continue;

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

        FileSystem::SOARecord& sourceDisplayList = *movePayload->sourceDisplayList;

        BatchFileOperation fileOperation{};
        for(int sourceIndex : movePayload->itemsToMove) {
            const std::string& sourceItemName = sourceDisplayList.getName(sourceIndex);

            Path sourcePath(movePayload->sourcePath); sourcePath.appendName(sourceItemName);

            if(sourcePath.isEmpty() || targetPath.isEmpty()) continue;

            BatchFileOperation::Operation op;
            op.from = sourcePath;
            op.to = targetPath;
            op.opType = FileOpType::FILE_OP_MOVE;
            fileOperation.operations.push_back(op);
        }
        mFileOpsWorker->addFileOperation(fileOperation);

        mSelection.clear();
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
        mDirectoryChanged = true;
    }

    ImGui::SameLine();

    if(ImGui::Button("Refresh")) {
        mDirectoryChanged = true;
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

    if(mTableSortSpecs != nullptr) {
        if(mTableSortSpecs->SpecsDirty && mTableSortSpecs->Specs != nullptr) {
            for(int i = 0; i < mTableSortSpecs->SpecsCount; i++) {
                FileSystem::SortDirection dir = mTableSortSpecs->Specs[i].SortDirection == ImGuiSortDirection_Ascending 
                    ? FileSystem::SortDirection::Ascending : FileSystem::SortDirection::Descending;

                switch(mTableSortSpecs->Specs[i].ColumnIndex) {
                    case 1:  // name
                        {
                            mDirectoryWatcher.setSort(DirectorySortFlags::DIRECTORY_SORT_NAME, dir);
                        } break;
                    case 2:  // date
                        {
                            mDirectoryWatcher.setSort(DirectorySortFlags::DIRECTORY_SORT_DATE, dir);
                        } break;
                    case 3:  // file size
                        {
                            mDirectoryWatcher.setSort(DirectorySortFlags::DIRECTORY_SORT_FILE_SIZE, dir);
                        } break;
                }
            }

            mTableSortSpecs->SpecsDirty = false;
        }
    }

    if(mDirectoryChanged) {
        mDirectoryChanged = false;
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
            if(FileSystem::doesPathExist(mCurrentDirectory)) {
                mDisplayListType = DisplayListType::DEFAULT;
                mDirectoryWatcher.changeDirectory(mCurrentDirectory);
            } else {
                mDisplayListType = DisplayListType::PATH_NOT_FOUND_ERROR;
            }
        }

        mSelection.clear();

        mHighlighted.assign(mHighlighted.size(), false);
        mCurrentHighlightIdx = -1;

        mEditIdx = -1;
        mEditInput.clear();
    }

    if(mDirectoryWatcher.update()) {
        mSelection.clear();
    }

    ImGui::End();
}

void BrowserWidget::handleInput() {
    FileSystem::SOARecord& displayList = mDirectoryWatcher.mRecords;

    if(mIsFocused) {
        if(ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            mSelection.clear();

            if(!mSearchWindowOpen) {
                mHighlighted.assign(mHighlighted.size(), false);
            }

            mEditIdx = -1;
            mEditInput.clear();
        }

        // rename key
        if(ImGui::IsKeyPressed(ImGuiKey_F2)) {
            mEditIdx = mSelection.count() == 0 ? -1 : mSelection.rangeSelectionStart;
            mEditInput = displayList.getName(mEditIdx);
            mSelection.clear();
        }

        if(ImGui::IsKeyPressed(ImGuiKey_Slash) && !mSearchWindowOpen) {
            // show a search window
            mSearchWindowOpen = true;
        }

        // TODO: copy list of items to actual SYSTEM clipboard :)
        // copy selected items to "clipboard"
        if(ImGui::IsKeyDown(ImGuiKey_ModCtrl) && ImGui::IsKeyPressed(ImGuiKey_C, false)) {
            mClipboard.clear();
            for(size_t i : mSelection.indexes) {
                Path itemPath = mCurrentDirectory;
                itemPath.appendName(displayList.getName(i));
                mClipboard.push_back(itemPath);
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
            for(size_t i : mSelection.indexes) {
                Path itemPath = mCurrentDirectory;
                itemPath.appendName(displayList.getName(i));

                BatchFileOperation::Operation op;
                op.from = itemPath;
                op.opType = FileOpType::FILE_OP_DELETE;
                fileOperation.operations.push_back(op);
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
        mPreviousDirectory = mCurrentDirectory;
        while(!mCurrentDirectory.isEmpty()) 
            mCurrentDirectory.popSegment();

        mDirectoryChanged = true;
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
            mPreviousDirectory = mCurrentDirectory;
            while(--numPop > 0) mCurrentDirectory.popSegment();
            mDirectoryChanged = true;
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
    FileSystem::SOARecord& displayList = mDirectoryWatcher.mRecords;

    mSelection.resize(displayList.size());
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
        | ImGuiTableFlags_NoBordersInBodyUntilResize
        | ImGuiTableFlags_Resizable 
        | ImGuiTableFlags_Hideable
        | ImGuiTableFlags_ContextMenuInBody 
        | ImGuiTableFlags_Reorderable
        | ImGuiTableFlags_Sortable;

    // early out if table is being clipped
    if(!ImGui::BeginTable("DirectoryList", 4, tableFlags)) {
        ImGui::EndTable();
        return;
    }

    int iconColumnFlags = 
        ImGuiTableColumnFlags_NoHeaderLabel 
        | ImGuiTableColumnFlags_WidthFixed 
        | ImGuiTableColumnFlags_NoReorder
        | ImGuiTableColumnFlags_NoResize 
        | ImGuiTableColumnFlags_NoHide 
        | ImGuiTableColumnFlags_IndentDisable
        | ImGuiTableColumnFlags_NoSort;

    ImVec2 iconColumnSize = ImGui::CalcTextSize(ICON_FK_FOLDER);
    ImGui::TableSetupColumn(DISPLAY_COLUMN_ICON, iconColumnFlags, iconColumnSize.x);

    int nameColumnFlags = ImGuiTableColumnFlags_IndentDisable | ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_PreferSortAscending | ImGuiTableColumnFlags_NoHide;
    ImGui::TableSetupColumn(DISPLAY_COLUMN_NAME, nameColumnFlags);
    ImGui::TableSetupColumn(DISPLAY_COLUMN_LAST_MODIFIED, ImGuiTableColumnFlags_IndentDisable);
    ImGui::TableSetupColumn(DISPLAY_COLUMN_SIZE, ImGuiTableColumnFlags_IndentDisable);

    ImGuiListClipper clipper;
    clipper.Begin(displayList.size());

    mTableSortSpecs = ImGui::TableGetSortSpecs();

    ImGui::TableHeadersRow();

    bool isSelectingMultiple = io.KeyCtrl;
    bool isSelectingRange = io.KeyShift;


    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImGui::PushID("##FileRecords");
    while(clipper.Step()) {
        for(int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
            ImGui::PushID(i);

            bool isSelected = mSelection.selected[i];

            const std::string& itemName = displayList.getName(i);
            const bool itemIsFile = displayList.isFile(i);
            const FileSystem::Timestamp& itemLastModified = displayList.getLastModifiedDate(i);
            const uint64_t itemSize = displayList.getSize(i);

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            if(ImGui::Selectable("##selectable", isSelected, ImGuiSelectableFlags_AllowDoubleClick | ImGuiSelectableFlags_SpanAllColumns )) {
                if(isSelectingMultiple) {
                    mSelection.toggle(i);
                } else if(isSelectingRange) {
                    mSelection.selectRange(i);
                } else {
                    mSelection.selectOne(i);
                }

                if(ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                    if(itemIsFile) {
                        Path filePath = mCurrentDirectory;
                        filePath.appendName(itemName);
                        FileSystem::openFile(filePath);
                    } else {
                        mPreviousDirectory = mCurrentDirectory;
                        mCurrentDirectory.appendName(itemName);
                        mDirectoryChanged = true;
                    }
                }
            }

            isSelected = mSelection.selected[i];

            if(mSelection.count() > 0) {
                if(ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceNoPreviewTooltip)) {
                    // can only DragDrop a selected item
                    if(isSelected) {
                        mMovePayload.itemsToMove.clear();
                        mMovePayload.sourcePath = mCurrentDirectory;
                        mMovePayload.sourceDisplayList = &displayList;

                        for(size_t j : mSelection.indexes) {
                            mMovePayload.itemsToMove.push_back(j); 
                        }
                        
                        size_t payloadSize = sizeof(void*);
                        void* payloadPtr = static_cast<void*>(&mMovePayload);
                        bool accepted = ImGui::SetDragDropPayload(MOVE_PAYLOAD, &payloadPtr, payloadSize, ImGuiCond_Once);
                    } else {
                        mSelection.clear();
                    }
                    ImGui::EndDragDropSource();
                }
            }

            if(!itemIsFile && ImGui::BeginDragDropTarget()) {
                Path targetPath(mCurrentDirectory); 
                targetPath.appendName(itemName);

                acceptMovePayload(targetPath);
                
                ImGui::EndDragDropTarget();
            }

            ImGui::SameLine(0.0f, 0.0f);

            if(itemIsFile) {
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

                if(ImGui::InputText("###EditInput", &mEditInput, inputFlags)) {
                    // rename current selection to mEditInput
                    BatchFileOperation batchOp;
                    BatchFileOperation::Operation op;
                    Path targetItem = mCurrentDirectory;
                    targetItem.appendName(itemName);

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
                ImGui::Text(itemName.c_str());
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
            ImGui::Text("%u-%02u-%02u  %02u:%02u %s",
                    itemLastModified.year,
                    itemLastModified.month,
                    itemLastModified.day,
                    itemLastModified.hour,
                    itemLastModified.minute,
                    itemLastModified.isPM ? "PM" : "AM");

            ImGui::TableNextColumn();
            if(itemIsFile) {
                ImGui::TextUnformatted(PrettyPrintSize(itemSize).c_str());
            }

            ImGui::PopID();
        }
    }
    ImGui::PopID();

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

    int nameColumnFlags = ImGuiTableColumnFlags_IndentDisable | ImGuiTableColumnFlags_PreferSortAscending;
    ImGui::TableSetupColumn("Name", nameColumnFlags);

    ImGuiListClipper clipper;
    clipper.Begin(mDriveList.size());

    mSelection.resize(mDriveList.size());

    mTableSortSpecs = ImGui::TableGetSortSpecs();

    ImGui::TableHeadersRow();

    while(clipper.Step()) {
        for(int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
            std::string uniqueID = "##DriveRecord" + std::to_string(i);
            ImGui::PushID(uniqueID.c_str());

            const DriveRecord& item = mDriveList[i];
            bool isSelected = mSelection.selected[i];

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            if(ImGui::Selectable("##selectable", isSelected, ImGuiSelectableFlags_AllowDoubleClick | ImGuiSelectableFlags_SpanAllColumns )) {
                if(ImGui::IsMouseDoubleClicked(0)) {
                    mPreviousDirectory = mCurrentDirectory;

                    mCurrentDirectory.appendName(std::string(1, item.letter) + ":");
                    
                    mDirectoryChanged = true;
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
    FileSystem::SOARecord& displayList = mDirectoryWatcher.mRecords;

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
            for(int i = 0; i < displayList.size(); i++) {
                const std::string& name = displayList.getName(i);
                if(name.find(mSearchFilter) != std::string::npos) {
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

