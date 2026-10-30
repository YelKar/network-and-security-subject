#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace webserver
{

struct ServerConfig
{
	std::string host = "0.0.0.0";
	std::uint16_t port = 8080;
	std::filesystem::path rootDir = "./public";
};

enum class ParseStatus
{
	OK,
	HELP,
	ERROR,
};

struct ParseResult
{
	ParseStatus status = ParseStatus::OK;
	ServerConfig config;
	std::string message;
};

ParseResult ParseCommandLine(int argc, char* const argv[]);
std::string BuildUsage(std::string_view programName);

} // namespace webserver
