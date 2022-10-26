#include "StringUtils.h"
#include <objbase.h>
#include <combaseapi.h>

#include <catch_amalgamated.hpp>

#include <filesystem>
#include <fstream>
#include <string.h>

#include <FileSystem.h>
#include <Path.h>
#include <iostream>

#include <chrono>

namespace std_fs = std::filesystem;

struct Record {
    std::string name;
    bool isFile;
};

void createFile(std_fs::path filePath) {
    std::ofstream outputFile(filePath.u8string());
    outputFile.close();
}

std::vector<Record> getItemsInDirectory(std_fs::path dir) {
    std::vector<Record> result;
    FileSystem::SOARecord records;
    FileSystem::enumerateDirectory(Path(dir.u8string()), records);
    for(int i = 0; i < records.size(); i++) {
        Record r{};
        r.name = records.getName(i);
        r.isFile = records.isFile(i);
        result.push_back(r);
    }
    return result;
}

std::vector<std::string> getFilenamesInDirectory(std_fs::path dir) {
    std::vector<std::string> result;
    for(const auto& item : getItemsInDirectory(dir)) {
        result.push_back(item.name);
    }

    return result;
}

void refreshTestDirectory(std_fs::path TEST_PATH) {
    if(std_fs::exists(TEST_PATH)) {
        std_fs::remove_all(TEST_PATH);
    }

    std_fs::create_directory(TEST_PATH);
}

TEST_CASE( "File operations", "[simple]" ) {
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if(hr != S_OK) {
        printf("Failed to initialize COM library\n");
    }

    // make a dummy test direcory
    std_fs::path TEST_PATH = std_fs::current_path() / "TEMP";
    refreshTestDirectory(TEST_PATH);

    SECTION("list items in empty directory") {
        FileSystem::SOARecord items;
        bool success = FileSystem::enumerateDirectory(TEST_PATH.u8string(), items);
        REQUIRE(success);
        REQUIRE(items.size() == 0);
    }

    SECTION("list files in full directory") {
        // create some files
        std::vector<std::string> expectedNames = { "test1.txt", "somefile", "yee" };
        for(const auto& fileName : expectedNames) {
            createFile(TEST_PATH / fileName);
        }

        std::vector<std::string> actualNames = getFilenamesInDirectory(TEST_PATH);
        REQUIRE_THAT(actualNames, Catch::Matchers::UnorderedEquals(expectedNames));

        for(const auto& item : getItemsInDirectory(TEST_PATH)) {
            REQUIRE(item.isFile);
        }
    }

    SECTION("list folders in full directory") {
        // create some files
        std::vector<std::string> folderNames = { "test1", "folder1", "yee" };
        for(const auto& folderName : folderNames) {
            std_fs::create_directory(TEST_PATH / folderName);
        }

        std::vector<std::string> actualNames = getFilenamesInDirectory(TEST_PATH);
        REQUIRE_THAT(actualNames, Catch::Matchers::UnorderedEquals(folderNames));

        for(const auto& item : getItemsInDirectory(TEST_PATH)) {
            REQUIRE(!item.isFile);
        }
    }

    SECTION("delete file") {
        std_fs::path fileToDelete = TEST_PATH / "delete_me.txt";

        createFile(fileToDelete);
        REQUIRE_THAT(getFilenamesInDirectory(TEST_PATH), Catch::Matchers::UnorderedEquals(std::vector<std::string>({"delete_me.txt"})));

        bool wasDeleted = FileSystem::deleteFileOrDirectory(fileToDelete.u8string(), false);
        REQUIRE_THAT(getFilenamesInDirectory(TEST_PATH), Catch::Matchers::UnorderedEquals(std::vector<std::string>({})));
        REQUIRE(wasDeleted);
    }

    SECTION("delete directory") {
        std_fs::path pathToDelete = TEST_PATH / "deleteme";

        // TODO: replace with functions from FileSystem::
        std_fs::create_directory(pathToDelete);

        bool wasDeleted = FileSystem::deleteFileOrDirectory(pathToDelete.u8string(), false);
        REQUIRE_THAT(getFilenamesInDirectory(TEST_PATH), Catch::Matchers::UnorderedEquals(std::vector<std::string>({})));

        REQUIRE(wasDeleted == true);
    }

    SECTION("move file") {
        const std::string filename = "move_me.txt";
        std_fs::path targetPath = TEST_PATH / "targetPath";
        std_fs::path fileToMove = TEST_PATH / filename;

        std_fs::create_directory(targetPath);
        createFile(fileToMove);

        REQUIRE_THAT(getFilenamesInDirectory(TEST_PATH), Catch::Matchers::UnorderedEquals(std::vector<std::string>({"targetPath", filename })));

        bool wasMoved = FileSystem::moveFileOrDirectory(fileToMove.u8string(), targetPath.u8string());
        REQUIRE(wasMoved);
        REQUIRE_THAT(getFilenamesInDirectory(TEST_PATH), Catch::Matchers::UnorderedEquals(std::vector<std::string>({"targetPath"})));
        REQUIRE_THAT(getFilenamesInDirectory(targetPath), Catch::Matchers::UnorderedEquals(std::vector<std::string>({ filename })));
    }

    SECTION("rename file") {
        const std::string filename = "rename_me.txt";
        const std::string newName = "newname.txt";

        std_fs::path fileToRename = TEST_PATH / filename;
        createFile(fileToRename);
        REQUIRE_THAT(getFilenamesInDirectory(TEST_PATH), Catch::Matchers::UnorderedEquals(std::vector<std::string>({ filename })));

        bool wasRenamed = FileSystem::renameFileOrDirectory(fileToRename.u8string(), Util::Utf8ToWstring(newName));
        REQUIRE_THAT(getFilenamesInDirectory(TEST_PATH), Catch::Matchers::UnorderedEquals(std::vector<std::string>({ newName })));
        REQUIRE(wasRenamed);
    }

    SECTION("Batch delete operations") {
        // create some files
        std::vector<std::string> expectedNames = { "test1.txt", "somefile", "yee" };
        for(const auto& fileName : expectedNames) {
            createFile(TEST_PATH / fileName);
        }

        REQUIRE_THAT(getFilenamesInDirectory(TEST_PATH), Catch::Matchers::UnorderedEquals(expectedNames));

        FileSystem::FileOperation op;

        op.init();
        for(const auto& fileName : expectedNames) {
            op.remove(Path((TEST_PATH / fileName).u8string()));
        }
        op.execute();

        REQUIRE_THAT(getFilenamesInDirectory(TEST_PATH), Catch::Matchers::UnorderedEquals(std::vector<std::string>({})));
    }

    SECTION("Batch copy operations") {
        std_fs::path targetPath = TEST_PATH / "target";

        std_fs::create_directories(targetPath);

        // create some files
        std::vector<std::string> expectedNames = { "test1.txt", "somefile", "yee" };
        for(const auto& fileName : expectedNames) {
            createFile(TEST_PATH / fileName);
        }

        REQUIRE_THAT(getFilenamesInDirectory(targetPath), Catch::Matchers::UnorderedEquals(std::vector<std::string>({})));

        FileSystem::FileOperation op;

        op.init();
        for(const auto& fileName : expectedNames) {
            Path itemPath((TEST_PATH / fileName).u8string());
            op.copy(itemPath, targetPath.u8string());
        }
        op.execute();

        REQUIRE_THAT(getFilenamesInDirectory(targetPath), Catch::Matchers::UnorderedEquals(expectedNames));
        REQUIRE_THAT(getFilenamesInDirectory(TEST_PATH), Catch::Matchers::UnorderedEquals(std::vector<std::string>({ "somefile", "target", "test1.txt", "yee" })));
    }

    SECTION("Batch move operations") {
        std_fs::path targetPath = TEST_PATH / "target";

        std_fs::create_directories(targetPath);

        // create some files
        std::vector<std::string> expectedNames = { "test1.txt", "somefile", "yee" };
        for(const auto& fileName : expectedNames) {
            createFile(TEST_PATH / fileName);
        }

        REQUIRE_THAT(getFilenamesInDirectory(targetPath), Catch::Matchers::UnorderedEquals(std::vector<std::string>({})));

        FileSystem::FileOperation op;

        op.init();
        for(const auto& fileName : expectedNames) {
            Path itemPath((TEST_PATH / fileName).u8string());
            op.move(itemPath, targetPath.u8string());
        }
        op.execute();

        REQUIRE_THAT(getFilenamesInDirectory(targetPath), Catch::Matchers::UnorderedEquals(expectedNames));
        REQUIRE_THAT(getFilenamesInDirectory(TEST_PATH), Catch::Matchers::UnorderedEquals(std::vector<std::string>({ "target" })));
    }

    SECTION("Batch mix operations") {
        std_fs::path targetPath = TEST_PATH / "target";

        std_fs::create_directories(targetPath);

        // create some files
        std::vector<std::string> expectedNames = { "test1.txt", "somefile", "yee" };
        for(const auto& fileName : expectedNames) {
            createFile(TEST_PATH / fileName);
        }

        REQUIRE_THAT(getFilenamesInDirectory(targetPath), Catch::Matchers::UnorderedEquals(std::vector<std::string>({})));

        FileSystem::FileOperation op;

        op.init();
        op.copy(Path((TEST_PATH / "test1.txt").u8string()), targetPath.u8string());
        op.move(Path((TEST_PATH / "somefile").u8string()), targetPath.u8string());
        op.remove(Path((TEST_PATH / "yee").u8string()));
        op.execute();

        REQUIRE_THAT(getFilenamesInDirectory(targetPath), Catch::Matchers::UnorderedEquals(std::vector<std::string>({ "test1.txt", "somefile" })));
        REQUIRE_THAT(getFilenamesInDirectory(TEST_PATH), Catch::Matchers::UnorderedEquals(std::vector<std::string>({ "target", "test1.txt" })));
    }

    if(std_fs::exists(TEST_PATH)) {
        std_fs::remove_all(TEST_PATH);
    }
    CoUninitialize();
}

TEST_CASE("Path", "[simple]") {
    SECTION("absolute path") {
        Path path("C:/abs/path");
        REQUIRE(path.getType() == Path::PATH_ABSOLUTE);

        path = Path("C:/");
        REQUIRE(path.getType() == Path::PATH_ABSOLUTE);
    }

    SECTION("Relative path") {
        Path path("../abs/path");
        REQUIRE(path.getType() == Path::PATH_RELATIVE);

        path = Path("./abs/path");
        REQUIRE(path.getType() == Path::PATH_RELATIVE);

        path = Path("..");
        REQUIRE(path.getType() == Path::PATH_RELATIVE);
    }

    SECTION("Empty path") {
        Path path("");
        REQUIRE(path.isEmpty());
        REQUIRE(path.getSegments().empty());
    }

    SECTION("Separator") {
        const std::string expected = "C:\\abs\\path";
        REQUIRE(Path("C:/abs/path").str()   == expected);
        REQUIRE(Path("C:\\abs\\path").str() == expected);
    }

    SECTION("Convert to absolute") {
        std::string expectedAbsolute = std_fs::current_path().u8string();
        
        Path relativePath(".");
        relativePath.toAbsolute();

        REQUIRE(relativePath.str() == expectedAbsolute);
    }

    SECTION("Parse segments") {
        {
            Path path("C:/abs/path");
            std::vector<std::string_view> segments = path.getSegments();
            std::vector<std::string_view> expectedSegments = {"C:", "abs", "path"};

            REQUIRE(segments.size() == 3);
            REQUIRE_THAT(segments, Catch::Matchers::Equals(expectedSegments));
        }


        {
            Path path("../abs/path");

            std::vector<std::string_view> segments = path.getSegments();
            std::vector<std::string_view> expectedSegments = {"..", "abs", "path"};

            REQUIRE(segments.size() == 3);
            REQUIRE_THAT(segments, Catch::Matchers::Equals(expectedSegments));
        }


        {
            Path path("");

            std::vector<std::string_view> segments = path.getSegments();
            std::vector<std::string_view> expectedSegments = {};

            REQUIRE(segments.size() == 0);
            REQUIRE_THAT(segments, Catch::Matchers::Equals(expectedSegments));
        }
    }
    
    SECTION("Append segment") {
        Path path("");
        path.appendName("C:");

        std::vector<std::string_view> expectedSegments = { "C:" };
        REQUIRE_THAT(path.getSegments(), Catch::Matchers::Equals(expectedSegments));

        path.appendName("abs");

        expectedSegments = { "C:", "abs"};
        REQUIRE_THAT(path.getSegments(), Catch::Matchers::Equals(expectedSegments));
    }

    SECTION("Pop segment") {
        Path path("C:/abs/path");
        
        std::vector<std::string_view> expectedSegments = { "C:", "abs", "path" };
        REQUIRE_THAT(path.getSegments(), Catch::Matchers::Equals(expectedSegments));
        
        path.popSegment();

        expectedSegments = { "C:", "abs" };
        REQUIRE_THAT(path.getSegments(), Catch::Matchers::Equals(expectedSegments));

        path.popSegment();

        expectedSegments = { "C:" };
        REQUIRE_THAT(path.getSegments(), Catch::Matchers::Equals(expectedSegments));

        path.popSegment();

        expectedSegments = { };
        REQUIRE_THAT(path.getSegments(), Catch::Matchers::Equals(expectedSegments));
    }

    SECTION("Append path") {
        Path path("C:/abs");
        Path rel("./relative/");

        path.appendRelative(rel);

        REQUIRE(path.str() == "C:\\abs\\relative");
        REQUIRE(path.getSegments().size() == 3);
    }
}
