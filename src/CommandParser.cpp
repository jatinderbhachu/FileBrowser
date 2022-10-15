#include "CommandParser.h"
#include "BrowserWidget.h"
#include "FileOps.h"
#include <sstream>
#include <unordered_map>
#include <assert.h>

#include <filesystem>
#include <fstream>

inline static std::unordered_map<CommandType, std::string> CommandToStrMap;
inline static std::unordered_map<std::string_view, CommandType> StrToCommandMap;
inline static std::vector<std::string_view> CommandNames;

CommandParser::CommandParser()
{
    // register command name conversion maps
    if(CommandNames.empty()) {
        auto xRegister = [](std::string_view name, CommandType cmd) {
            CommandToStrMap[cmd] = name;
            StrToCommandMap[name] = cmd;
            CommandNames.push_back(name);
        };

        xRegister("replace", CommandType::REPLACE);
        xRegister("mkdir", CommandType::MKDIR);
        xRegister("make_debug_dir", CommandType::MAKE_DEBUG_DIR);
    }
}

CommandParser::~CommandParser()
{
}


std::string_view CommandParser::CommandTypeToStr(CommandType cmd) {
    return CommandToStrMap.count(cmd) > 0 
        ? CommandToStrMap.at(cmd)
        : "UNKNOWN_COMMAND";
}

CommandType CommandParser::StrToCommandType(std::string_view str) {
    return StrToCommandMap.count(str) > 0 
        ? StrToCommandMap.at(str)
        : CommandType::UNKNOWN;
}

void CommandParser::execute(const std::string& input, BrowserWidget* focusedWidget) {
    assert(focusedWidget != nullptr);
    Command cmd = parse(input);

    switch(cmd.type) {
        case CommandType::REPLACE: 
            {
                const std::string& arg1 = cmd.args[0];
                const std::string& arg2 = cmd.args[1];
                printf("[CMD] replace %s with %s \n", arg1.c_str(), arg2.c_str());
                focusedWidget->renameSelected(arg1, arg2);
            } break;
        case CommandType::MKDIR: 
            {
                printf("[CMD] mdir ");
                for(const auto& arg : cmd.args) {
                    printf("%s ", arg.c_str());
                }
                printf("\n");

                Path dir = focusedWidget->getCurrentDirectory();
                dir.appendName(cmd.args[0]);

                bool success = FileOps::createDirectory(dir);
            } break;
        case CommandType::MAKE_DEBUG_DIR:
            {
                printf("[CMD] making debug directory\n");

                Path currentDirectory = focusedWidget->getCurrentDirectory();

                currentDirectory.appendName("browser_test");

                // TODO: replace with functions from FileOps::

                // a folder with 1000 files
                std::filesystem::path testPath(currentDirectory.str());

                if(!std::filesystem::exists(testPath)) {
                    std::filesystem::create_directories(testPath);
                }

                std::filesystem::path thousandFilesPath = testPath / "1k_files";

                if(!std::filesystem::exists(thousandFilesPath)) {
                    std::filesystem::create_directory(thousandFilesPath);
                    for(int i = 0; i < 1000; i++) {
                        std::ofstream outputFile((thousandFilesPath / std::to_string(i)).string());
                        outputFile << ".";
                        outputFile.close();
                    }
                }

            } break;
        case CommandType::UNKNOWN:
            {
                printf("[CMD] UNKNOWN_CMD... \n");
            } break;
        default: break;
    }
}

Command CommandParser::parse(std::string_view input) {
    Command result{};

    // determine the type 
    int start = 0;
    while(input[start] == ' ') { start++; } // eat spaces

    int end = start;
    while(end < input.size() && input[end] != ' ') { end++; } // find next space

    result.type = StrToCommandType(input.substr(start, end - start));

    std::string_view args;
    if(end < input.size()) {
        args = input.substr(end + 1);
        parseArguments(result, args);
    }

    return result;
}

inline static void split(std::string_view input, char delim, std::vector<std::string>& out_tokens) {
    int start = 0;
    int end = 0;
    while(end < input.size()) {
        if(input[end] == delim) {
            std::string_view item = input.substr(start, end-start);
            if(!item.empty()) {
                out_tokens.push_back(std::string(item));
            }
            start = end + 1;
        }
        
        end++;
    }

    std::string_view item = input.substr(start);
    if(!item.empty()) out_tokens.push_back(std::string(item));
}

void CommandParser::parseArguments(Command& cmd, std::string_view args) {
    // TODO: return an error code if args are invalid

    switch(cmd.type) {
        case CommandType::REPLACE: 
            {
                // output 2 args
                // TODO: replace "this" "that" 
                // TODO: replace "this thing" "that that"

                // split by space
                split(args, ' ', cmd.args);

                int numArgs = cmd.args.size();

                // invalid number of args
                if(numArgs != 2) {
                    cmd.type = CommandType::UNKNOWN;
                    return;
                }

            } break;
        case CommandType::MKDIR: 
            {
                // mkdir dir1 dir2 dir3

                // split by space
                split(args, ' ', cmd.args);

                if(cmd.args.empty()) cmd.type = CommandType::UNKNOWN;
            } break;
        default:
            return;
    }
}

const std::vector<std::string_view>& CommandParser::GetCommandNames() {
    return CommandNames;
}
