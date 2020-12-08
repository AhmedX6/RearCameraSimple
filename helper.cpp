#define LOG_TAG "Helper"

#include "helper.h"

Helper::Helper()
{
}

std::string Helper::basename(const std::string &pathname)
{
    return {std::find_if(pathname.rbegin(), pathname.rend(),
                         [](char c) { return c == '/'; })
                .base(),
            pathname.end()};
}

std::vector<std::string> Helper::listDirectory(const std::string &path, const std::string &)
{
    std::vector<std::string> vec;
    DIR *dir = opendir(path.c_str());
    if (dir != nullptr)
    {
        struct dirent *entry;
        while ((entry = readdir(dir)) != nullptr)
        {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            {
                continue;
            }
            else
            {
                std::string fullpath(path);
                fullpath += entry->d_name;
                vec.push_back(fullpath);
            }
        }
    }
    else
    {
        ALOGD("Failed to open %s", path.c_str());
    }

    return vec;
}

Helper::~Helper()
{
}