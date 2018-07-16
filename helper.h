#ifndef HELPER_H_
#define HELPER_H_

#include <iostream>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <list>
#include <inttypes.h>

#include <binder/IServiceManager.h>
#include <cutils/properties.h>
#include <utils/Log.h>
#include <utils/SystemClock.h>

class Helper
{
  public:
	Helper();
	~Helper();

	static std::string basename(const std::string &);
	static std::vector<std::string> listDirectory(const std::string &, const std::string &);
	static void yuv_convert(unsigned char *buf, unsigned char *rgb, int xsize, int ysize);
	static inline void yuvtorgb(int Y, int U, int V, unsigned char *rgb);
};

#endif // !HELPER_H_
