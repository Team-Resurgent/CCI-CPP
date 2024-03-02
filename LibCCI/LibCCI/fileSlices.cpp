#include "fileSlices.h"
#include "dirent.h"

#include <vector>
#include <algorithm>
#include <string>

#ifdef _WIN32
const char PATH_SEPARATOR = '\\';
#else
const char PATH_SEPARATOR = '/';
#endif

std::string getExtension(const std::string& filename) 
{
    size_t lastDot = filename.find_last_of(".");
    if (lastDot != std::string::npos) 
    {
        return filename.substr(lastDot);
    }
    return "";
}

std::string getFileNameWithoutExtension(const std::string& filename) 
{
    size_t lastDot = filename.find_last_of(".");
    if (lastDot != std::string::npos) 
    {
        return filename.substr(0, lastDot);
    }
    return filename;
}

std::string getParentDirectory(const std::string& filename) 
{
    size_t lastSlash = filename.find_last_of(PATH_SEPARATOR);
    if (lastSlash != std::string::npos)
    {
        return filename.substr(0, lastSlash);
    }
    return "";
}

std::vector<std::string> getFilesInDirectory(const std::string& directory) 
{
    std::vector<std::string> files;

    DIR* dir;
    if ((dir = opendir(directory.c_str())) != nullptr) 
    {
        struct dirent* ent;
        while ((ent = readdir(dir)) != nullptr) 
        {
            std::string filename = ent->d_name;
            if (filename != "." && filename != "..") 
            {
                files.push_back(directory + PATH_SEPARATOR + filename);
            }
        }
        closedir(dir);
    }

    return files;
}

bool mathFilePath(const std::string& pattern, const std::string& filePath)
{
    if (pattern.length() != filePath.length())
    {
        return false;
    }
    auto result = true;
    for (auto i = 0; i < pattern.length(); i++)
    {
        const auto& patternChar = pattern[i];
        const auto& filePathChar = filePath[i];
        if (patternChar == '?' || patternChar == filePathChar)
        {
            continue;
        }
        result = false;
        break;
    }
    return result;
}

std::vector<std::string> fileSlices::getSlicesFromFilePath(const std::string& filePath) 
{
    std::vector<std::string> slices;
    std::string extension = getExtension(filePath);
    std::string fileWithoutExtension = getFileNameWithoutExtension(filePath);
    std::string subExtension = getExtension(fileWithoutExtension);

    if (subExtension.length() == 2 && isdigit(subExtension[1])) 
    {
        std::string fileWithoutSubExtension = getFileNameWithoutExtension(fileWithoutExtension);
        std::string directory = getParentDirectory(filePath);
        std::vector<std::string> files = getFilesInDirectory(directory);
        std::string pattern = fileWithoutSubExtension + ".?" + extension;

        for (const auto& file : files) 
        {
            if (!mathFilePath(pattern, file))
            {
                continue;
            }
            slices.push_back(file);
        }

        std::sort(slices.begin(), slices.end());
        return slices;
    }

    return { filePath };
}