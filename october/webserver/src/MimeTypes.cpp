#include <webserver/MimeTypes.h>

#include <array>
#include <cctype>
#include <string>
#include <string_view>
#include <utility>

namespace webserver
{

namespace
{

using namespace std::string_view_literals;

constexpr std::array MIME_TYPES{
	std::pair{ "html"sv, "text/html"sv },
	std::pair{ "htm"sv, "text/html"sv },
	std::pair{ "css"sv, "text/css"sv },
	std::pair{ "js"sv, "application/javascript"sv },
	std::pair{ "mjs"sv, "application/javascript"sv },
	std::pair{ "json"sv, "application/json"sv },
	std::pair{ "xml"sv, "application/xml"sv },
	std::pair{ "txt"sv, "text/plain"sv },
	std::pair{ "csv"sv, "text/csv"sv },
	std::pair{ "png"sv, "image/png"sv },
	std::pair{ "jpg"sv, "image/jpeg"sv },
	std::pair{ "jpeg"sv, "image/jpeg"sv },
	std::pair{ "gif"sv, "image/gif"sv },
	std::pair{ "svg"sv, "image/svg+xml"sv },
	std::pair{ "webp"sv, "image/webp"sv },
	std::pair{ "ico"sv, "image/x-icon"sv },
	std::pair{ "bmp"sv, "image/bmp"sv },
	std::pair{ "pdf"sv, "application/pdf"sv },
	std::pair{ "wasm"sv, "application/wasm"sv },
	std::pair{ "woff"sv, "font/woff"sv },
	std::pair{ "woff2"sv, "font/woff2"sv },
	std::pair{ "ttf"sv, "font/ttf"sv },
	std::pair{ "otf"sv, "font/otf"sv },
	std::pair{ "mp4"sv, "video/mp4"sv },
	std::pair{ "webm"sv, "video/webm"sv },
	std::pair{ "mp3"sv, "audio/mpeg"sv },
	std::pair{ "ogg"sv, "audio/ogg"sv },
	std::pair{ "zip"sv, "application/zip"sv },
	std::pair{ "gz"sv, "application/gzip"sv },
};

std::string ToLower(std::string value)
{
	for (char& character : value)
	{
		character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
	}
	return value;
}

} // namespace

std::string_view GetMimeType(const std::filesystem::path& path)
{
	std::string extension = path.extension().string();

	if (extension.empty())
	{
		return "application/octet-stream";
	}

	if (extension.front() == '.')
	{
		extension.erase(extension.begin());
	}

	if (extension.empty())
	{
		return "application/octet-stream";
	}

	extension = ToLower(std::move(extension));

	for (const auto& [key, value] : MIME_TYPES)
	{
		if (extension == key)
		{
			return value;
		}
	}

	return "application/octet-stream";
}

} // namespace webserver
