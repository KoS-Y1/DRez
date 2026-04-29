//
// Created by y1 on 2025-09-07.
//

#pragma once

#include <string>
#include <vector>

namespace drez::file_system {
std::string              Read(std::string_view path);
std::string              GetFileName(const std::string &path);
std::string              GetFileNameWithoutExtension(const std::string &path);
std::vector<std::string> GetFilesWithExtension(std::string_view rootDir, std::string_view extension);
} // namespace drez::file_system
