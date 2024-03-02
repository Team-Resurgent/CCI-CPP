#pragma once

#include <cstdint>

class lz4
{
public:
    bool unpackLZ4Data(uint8_t* history, uint32_t historySize, uint8_t* inputBuffer, uint32_t inputBufferSize, uint8_t* outputBuffer, uint32_t outputBufferSize, uint32_t* decompressedSize);
};