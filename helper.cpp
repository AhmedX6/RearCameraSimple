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

void Helper::yuvtorgb(int Y, int U, int V, unsigned char *rgb)
{
    int r, g, b;
    static short L1[256], L2[256], L3[256], L4[256], L5[256];
    static int initialised;

    if (!initialised)
    {
        int i;
        initialised = 1;
        for (i = 0; i < 256; i++)
        {
            L1[i] = 1.164 * (i - 16);
            L2[i] = 1.596 * (i - 128);
            L3[i] = -0.813 * (i - 128);
            L4[i] = 2.018 * (i - 128);
            L5[i] = -0.391 * (i - 128);
        }
    }
#if 0
	r = 1.164*(Y-16) + 1.596*(V-128);
	g = 1.164*(Y-16) - 0.813*(U-128) - 0.391*(V-128);
	b = 1.164*(Y-16) + 2.018*(U-128);
#endif

    r = L1[Y] + L2[V];
    g = L1[Y] + L3[U] + L5[V];
    b = L1[Y] + L4[U];

    if (r < 0)
        r = 0;
    if (g < 0)
        g = 0;
    if (b < 0)
        b = 0;
    if (r > 255)
        r = 255;
    if (g > 255)
        g = 255;
    if (b > 255)
        b = 255;

    rgb[0] = r;
    rgb[1] = g;
    rgb[2] = b;
}

/* convert yuv to rgb */
void Helper::yuv_convert(unsigned char *buf, unsigned char *rgb, int xsize, int ysize)
{
    int i;

    for (i = 0; i < xsize * ysize; i += 2)
    {
        int Y1, Y2, U, V;

        Y1 = buf[2 * i + 0];
        Y2 = buf[2 * i + 2];
        U = buf[2 * i + 1];
        V = buf[2 * i + 3];

        yuvtorgb(Y1, U, V, &rgb[3 * i]);
        yuvtorgb(Y2, U, V, &rgb[3 * (i + 1)]);
    }
}

Helper::~Helper()
{
}