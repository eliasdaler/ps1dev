#pragma once

#include <filesystem>

struct TimFile;

TimFile readTimFile(const std::filesystem::path& path);
