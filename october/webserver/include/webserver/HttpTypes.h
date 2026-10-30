#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace webserver
{

enum class HttpStatus
{
	OK = 200,
	BAD_REQUEST = 400,
	FORBIDDEN = 403,
	NOT_FOUND = 404,
	METHOD_NOT_ALLOWED = 405,
	INTERNAL_SERVER_ERROR = 500,
};

inline std::string_view GetReasonPhrase(HttpStatus status)
{
	switch (status)
	{
	case HttpStatus::OK:
		return "OK";
	case HttpStatus::BAD_REQUEST:
		return "Bad Request";
	case HttpStatus::FORBIDDEN:
		return "Forbidden";
	case HttpStatus::NOT_FOUND:
		return "Not Found";
	case HttpStatus::METHOD_NOT_ALLOWED:
		return "Method Not Allowed";
	case HttpStatus::INTERNAL_SERVER_ERROR:
		return "Internal Server Error";
	}

	return "Unknown";
}

struct HttpRequest
{
	std::string method;
	std::string target;
	std::string version;
};

struct HttpResponse
{
	HttpStatus status = HttpStatus::OK;
	std::string contentType = "application/octet-stream";
	std::vector<std::uint8_t> body;
};

} // namespace webserver
