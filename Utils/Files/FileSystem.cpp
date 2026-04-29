#include "FileSystem.h"

#include <filesystem>
#include <fstream>

#include "Debug.h"

namespace drez::file_system {

std::string Read(std::string_view path) {
    std::ifstream file(std::filesystem::path{path}, std::ios::binary | std::ios::ate);
    DebugCheckCritical(file.is_open(), "Failed to open file {}", std::string(path));

    int64_t     fileSize = file.tellg();
    std::string buf(fileSize, '\0');
    file.seekg(0, std::ios::beg);
    file.read(&buf[0], fileSize);
    file.close();

    return buf;
}

std::string GetFileName(const std::string &path) {
    size_t pos = path.find_last_of("/\\");
    // If no slash found
    if (pos == std::string::npos) {
        return path;
    }
    return path.substr(pos + 1);
}

std::string GetFileNameWithoutExtension(const std::string &path) {
    std::filesystem::path p(path);
    return p.stem().string();
}

std::vector<std::string> GetFilesWithExtension(std::string_view rootDir, std::string_view extension) {
    std::vector<std::string> files;

    std::filesystem::path root(rootDir);
    if (!std::filesystem::exists(root) || !std::filesystem::is_directory(root)) {
        return files;
    }

    for (const auto &entry: std::filesystem::recursive_directory_iterator(root)) {
        if (entry.is_regular_file() && entry.path().extension() == std::string{extension}) {
            std::string path = entry.path().generic_string();
            std::ranges::replace(path, '\\', '/');
            files.push_back(path);
        }
    }

    return files;
}

} // namespace drez::file_system
