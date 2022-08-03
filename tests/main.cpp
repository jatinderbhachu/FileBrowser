#include <objbase.h>
#include <combaseapi.h>

#include <catch_amalgamated.hpp>

#include <filesystem>
#include <fstream>

#include <file_ops.h>
#include <iostream>


namespace fs = std::filesystem;

void createFile(fs::path filePath) {
    std::ofstream outputFile(filePath.u8string());
    outputFile << ".";
    outputFile.close();
}

std::vector<FileOps::Record> getItemsInDirectory(fs::path dir) {
    std::vector<FileOps::Record> result;
    FileOps::enumerateDirectory(dir.u8string(), result);
    return result;
}

std::vector<std::string> getFilenamesInDirectory(fs::path dir) {
    std::vector<std::string> result;
    for(const auto& item : getItemsInDirectory(dir)) {
        result.push_back(item.name);
    }

    return result;
}

void refreshTestDirectory(fs::path testDir) {
    if(fs::exists(testDir)) {
        fs::remove_all(testDir);
    }

    fs::create_directory(testDir);
}

TEST_CASE( "File operations", "[simple]" ) {

    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if(hr != S_OK) {
        printf("Failed to initialize COM library\n");
    }

    // make a dummy test direcory
    fs::path testDir = fs::current_path() / "TEMP";
    refreshTestDirectory(testDir);


    //printf("Working directory: %s\n", testDir.u8string().c_str());

    SECTION("list items in empty directory") {
        std::vector<FileOps::Record> items;
        FileOps::enumerateDirectory(testDir.u8string(), items);
        REQUIRE(items.size() == 0);
    }

    SECTION("list files in full directory") {
        // create some files
        std::vector<std::string> fileNames = { "test1.txt", "somefile", "yee" };
        for(const auto& fileName : fileNames) {
            createFile(testDir / fileName);
        }

        std::vector<std::string> actualNames = getFilenamesInDirectory(testDir);
        REQUIRE_THAT(actualNames, Catch::Matchers::UnorderedEquals(fileNames));

        for(const auto& item : getItemsInDirectory(testDir)) {
            REQUIRE(item.isFile);
        }
    }

    SECTION("list folders in full directory") {
        // create some files
        std::vector<std::string> folderNames = { "test1", "folder1", "yee" };
        for(const auto& folderName : folderNames) {
            fs::create_directory(testDir / folderName);
        }

        std::vector<std::string> actualNames = getFilenamesInDirectory(testDir);
        REQUIRE_THAT(actualNames, Catch::Matchers::UnorderedEquals(folderNames));

        for(const auto& item : getItemsInDirectory(testDir)) {
            REQUIRE(!item.isFile);
        }
    }

    SECTION("delete file") {
        fs::path fileToDelete = testDir / "delete_me.txt";

        createFile(fileToDelete);
        REQUIRE_THAT(getFilenamesInDirectory(testDir), Catch::Matchers::UnorderedEquals(std::vector<std::string>({"delete_me.txt"})));

        bool wasDeleted = FileOps::deleteFileOrDirectory(fileToDelete.u8string(), false);
        REQUIRE_THAT(getFilenamesInDirectory(testDir), Catch::Matchers::UnorderedEquals(std::vector<std::string>({})));

        REQUIRE(wasDeleted);
    }


    SECTION("move file") {
        const std::string filename = "move_me.txt";
        fs::path newDir = testDir / "newDir";
        fs::path fileToMove = testDir / filename;

        fs::create_directory(newDir);
        createFile(fileToMove);

        REQUIRE_THAT(getFilenamesInDirectory(testDir), Catch::Matchers::UnorderedEquals(std::vector<std::string>({"newDir", filename })));

        bool wasMoved = FileOps::moveFileOrDirectory(fileToMove.u8string(), newDir.u8string(), filename);
        REQUIRE_THAT(getFilenamesInDirectory(testDir), Catch::Matchers::UnorderedEquals(std::vector<std::string>({"newDir"})));
        REQUIRE_THAT(getFilenamesInDirectory(newDir),  Catch::Matchers::UnorderedEquals(std::vector<std::string>({ filename })));
        REQUIRE(wasMoved);
    }

    SECTION("rename file") {
        const std::string filename = "rename_me.txt";
        const std::string newName = "newname.txt";

        fs::path fileToRename = testDir / filename;
        createFile(fileToRename);
        REQUIRE_THAT(getFilenamesInDirectory(testDir), Catch::Matchers::UnorderedEquals(std::vector<std::string>({ filename })));

        bool wasRenamed = FileOps::renameFileOrDirectory(fileToRename.u8string(), newName);
        REQUIRE_THAT(getFilenamesInDirectory(testDir), Catch::Matchers::UnorderedEquals(std::vector<std::string>({ newName })));
        REQUIRE(wasRenamed);
    }

    CoUninitialize();
}
