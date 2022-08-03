#include "browser_widget.h"
#include <imgui.h>
#include <imgui_internal.h>
#include "file_ops.h"


Browser::Browser(const Path& path) 
    : mDrawList(nullptr),
    mCurrentDirectory(path),
    mUpdateFlag(true)
{ }

void Browser::beginFrame() {
    const ImU32 flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::Begin("filebrowser", NULL, flags);
    mDrawList = ImGui::GetWindowDrawList();
    ImGui::End();

}

void Browser::setCurrentDirectory(const Path& path) {
    mUpdateFlag = true;
    mCurrentDirectory = path;
}

void Browser::draw() {
    std::vector<FileOps::Record>& displayList = mDisplayList;
    ImGuiIO& io = ImGui::GetIO();

    mYScroll += io.MouseWheel;
    if(mYScroll < 0) mYScroll = 0;
    if(mYScroll >= mDisplayList.size()) mYScroll = mDisplayList.size() - 1;

    const std::string& currentDirStr = mCurrentDirectory.string();

    auto dirSegments = mCurrentDirectory.getSegments();


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

                    printf("Drag %s onto %s\n", sourcePath.string().c_str(), targetPath.string().c_str());
                    bool success = FileOps::moveFileOrDirectory(sourcePath, targetPath, sourceItem.name);
                    assert(success);
                    mUpdateFlag = true;
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


    static ImGuiTableFlags tableFlags = ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersV | ImGuiTableFlags_ContextMenuInBody | ImGuiTableFlags_Sortable;

    ImGui::BeginTable("Browser", 2, tableFlags);
    ImGui::TableSetupColumn("icon");
    ImGui::TableSetupColumn("Name");

    ImGuiListClipper clipper;
    clipper.Begin(displayList.size());

    bool isSelectingMultiple = io.KeyCtrl;
    while(clipper.Step()) {
        for(u32 i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
            const FileOps::Record& item = displayList[i];
            //ImGui::PushID(i);

            bool isSelected = mSelected.count(i) > 0 ? mSelected[i] : false;
            std::string uniqueID = "##FileRecord" + std::to_string(i);

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            if(ImGui::Selectable(uniqueID.c_str(), isSelected, ImGuiSelectableFlags_AllowDoubleClick | ImGuiSelectableFlags_SpanAllColumns )) {
                if(isSelectingMultiple) {
                    mSelected[i] = !mSelected[i];
                } else {
                    mSelected.clear();
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

                    printf("Drag %s onto %s\n", sourcePath.string().c_str(), targetPath.string().c_str());
                    bool success = FileOps::moveFileOrDirectory(sourcePath, targetPath, sourceItem.name);
                    assert(success);
                    mUpdateFlag = true;
                }

                ImGui::EndDragDropTarget();
            }

            ImGui::SameLine();
            ImGui::Text(item.isFile ? " " : "[]");
            ImGui::SameLine();

            ImGui::TableSetColumnIndex(1);
            ImGui::Text(item.name.c_str());


            //ImGui::PopID();
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

    if(mUpdateFlag) {
        FileOps::enumerateDirectory(mCurrentDirectory, displayList);
        FileOps::sortByName(FileOps::SortDirection::Descending, displayList);
        FileOps::sortByType(FileOps::SortDirection::Ascending, displayList);

        mSelected.clear();

        mUpdateFlag = false;
    }
}
