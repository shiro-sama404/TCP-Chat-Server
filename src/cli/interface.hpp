#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <optional>


enum class CommandType
{
    Register,
    Login,
    List,
    Msg,
    Logout,
    Delete,
    Quit,
    Unknown
};

struct Command
{
    CommandType type = CommandType::Unknown;
    std::vector<std::string> args;
};


class Interface
{
public:
    Interface() = default;
    void run();
    static void help();

private:
    static Command parse(const std::string& line);
    static void prompt();
    static void error(const std::string& msg);
};
