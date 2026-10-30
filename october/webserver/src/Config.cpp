#include <webserver/Config.h>

#include <charconv>
#include <optional>
#include <string_view>
#include <system_error>

namespace webserver
{

namespace
{

constexpr int MIN_PORT = 1;
constexpr int MAX_PORT = 65535;

std::optional<std::uint16_t> ParsePort(std::string_view text)
{
	if (text.empty())
	{
		return std::nullopt;
	}

	std::uint32_t value = 0;
	const char* const begin = text.data();
	const char* const end = text.data() + text.size();
	const auto [pointer, error] = std::from_chars(begin, end, value);

	if (error != std::errc{} || pointer != end || value < MIN_PORT || value > MAX_PORT)
	{
		return std::nullopt;
	}

	return static_cast<std::uint16_t>(value);
}

} // namespace

ParseResult ParseCommandLine(int argc, char* const argv[])
{
	ParseResult result;

	for (int index = 1; index < argc; ++index)
	{
		const std::string_view argument{ argv[index] };

		if (argument == "--help")
		{
			result.status = ParseStatus::HELP;
			return result;
		}

		const auto requireValue = [&result, &index, argc, argv](std::string_view option) -> const char* {
			if (index + 1 >= argc)
			{
				result.status = ParseStatus::ERROR;
				result.message = "Missing value for option ";
				result.message += option;
				return nullptr;
			}

			++index;
			return argv[index];
		};

		if (argument == "-p" || argument == "--port")
		{
			const char* const value = requireValue(argument);
			if (value == nullptr)
			{
				return result;
			}

			const auto port = ParsePort(value);
			if (!port.has_value())
			{
				result.status = ParseStatus::ERROR;
				result.message = "Invalid port: ";
				result.message += value;
				result.message += " (expected 1..65535)";
				return result;
			}

			result.config.port = *port;
		}
		else if (argument == "-d" || argument == "--dir")
		{
			const char* const value = requireValue(argument);
			if (value == nullptr)
			{
				return result;
			}

			if (std::string_view{ value }.empty())
			{
				result.status = ParseStatus::ERROR;
				result.message = "Root directory must not be empty";
				return result;
			}

			result.config.rootDir = value;
		}
		else if (argument == "-h" || argument == "--host")
		{
			const char* const value = requireValue(argument);
			if (value == nullptr)
			{
				return result;
			}

			if (std::string_view{ value }.empty())
			{
				result.status = ParseStatus::ERROR;
				result.message = "Host must not be empty";
				return result;
			}

			result.config.host = value;
		}
		else
		{
			result.status = ParseStatus::ERROR;
			result.message = "Unknown option: ";
			result.message += argument;
			return result;
		}
	}

	return result;
}

std::string BuildUsage(std::string_view programName)
{
	std::string usage;
	usage += "Usage: ";
	usage += programName;
	usage += " [options]\n\n";
	usage += "Options:\n";
	usage += "  -p, --port <port>   TCP port to listen on (1..65535), default: 8080\n";
	usage += "  -d, --dir <path>    document root directory, default: ./public\n";
	usage += "  -h, --host <ip>     interface address, default: 0.0.0.0\n";
	usage += "      --help          show this help message\n\n";
	usage += "Examples:\n";
	usage += "  ";
	usage += programName;
	usage += " -p 8080 -d ./public\n";
	usage += "  sudo ";
	usage += programName;
	usage += " -p 80 -d /var/www/html\n";
	return usage;
}

} // namespace webserver
