#pragma once

#include <vector>
#include <string>

class fileSlices
{
public:
	static std::vector<std::string> getSlicesFromFilePath(const std::string& filename);
};

