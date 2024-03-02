#pragma once

#include <string>

class cciDecoder
{
public:
	cciDecoder();
	~cciDecoder();
	bool open(const std::string& filePath);
	uint32_t getTotalSectors();
	bool readSector(uint32_t sector, void* buffer, uint32_t bufferSize);
	void close();
};

