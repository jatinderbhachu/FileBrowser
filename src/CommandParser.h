#pragma once
#include <string_view>
#include <vector>
#include <string>

enum class CommandType {
    REPLACE = 0,
    MKDIR,
    UNKNOWN,
};

struct Command {
    CommandType type = CommandType::UNKNOWN;
    std::vector<std::string> args;
};

class BrowserWidget;

class CommandParser
{
public:
    CommandParser();
    ~CommandParser();

    static std::string_view CommandTypeToStr(CommandType cmd);
    static CommandType StrToCommandType(std::string_view str);

    void execute(const std::string& input, BrowserWidget* focusedWidget);

private:
    Command parse(std::string_view input);
    void parseArguments(Command& cmd, std::string_view args);
    
};
