#pragma once

#include <filesystem>
#include <string_view>

namespace webserver
{

std::string_view GetMimeType(const std::filesystem::path& path);

}
