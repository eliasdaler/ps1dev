#pragma once

#include <filesystem>

struct TimFile;

void writeTimFile(const TimFile& timFile, const std::filesystem::path& path);
