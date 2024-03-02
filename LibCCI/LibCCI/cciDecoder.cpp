#include "cciDecoder.h"
#include "fileSlices.h"
#include "lz4.h"

#include <fstream>
#include <string>
#include <vector>
#include <mutex>

typedef struct _CCI_HEADER {
    uint32_t magic;
    uint32_t headerSize;
    int64_t uncompressedSize;
    int64_t indexOffset;
    uint32_t blockSize;
    uint8_t version;
    uint8_t indexAlignment;
    uint16_t reserved;
} CCI_HEADER, * PCCI_HEADER;

typedef struct _CCIIndex {
    uint32_t value;
    bool compressed;
} CCIIndex;

typedef struct _CCIDetail {
    std::ifstream* stream;
    std::vector<CCIIndex> indexInfo;
    uint32_t startSector;
    uint32_t endSector;
} CCIDetail;

std::vector<CCIDetail> mCCIDetails;
std::mutex mMutex;
void* mHistoryBuffer;

#define CCI_LZ4_HISTORY_SIZE 2560

cciDecoder::cciDecoder()
{
    mHistoryBuffer = malloc(CCI_LZ4_HISTORY_SIZE);
}

cciDecoder::~cciDecoder()
{
    close();
    free(mHistoryBuffer);
}

bool cciDecoder::open(const std::string& filePath)
{
    close();

    std::vector<std::string> slices = fileSlices::getSlicesFromFilePath(filePath);

    auto sectorCount = 0U;
    for (auto i = 0; i < slices.size(); i++)
    {
        const auto& slice = slices.at(i);
        std::ifstream fileStream(slice, std::ios::binary);
        if (!fileStream.is_open()) 
        {
            fileStream.close();
            return false;
        }

        uint32_t header = 0;
        fileStream.read(reinterpret_cast<char*>(&header), sizeof(header));
        if (header != 0x4D494343) 
        {
            fileStream.close();
            return false;
        }

        uint32_t headerSize = 0;
        fileStream.read(reinterpret_cast<char*>(&headerSize), sizeof(headerSize));
        if (headerSize != 32) 
        {
            fileStream.close();
            return false;
        }

        uint64_t uncompressedSize = 0;
        fileStream.read(reinterpret_cast<char*>(&uncompressedSize), sizeof(uncompressedSize));

        uint64_t indexOffset = 0;
        fileStream.read(reinterpret_cast<char*>(&indexOffset), sizeof(indexOffset));

        uint32_t blockSize = 0;
        fileStream.read(reinterpret_cast<char*>(&blockSize), sizeof(blockSize));
        if (blockSize != 2048) 
        {
            fileStream.close();
            return false;
        }

        uint8_t version = 0;
        fileStream.read(reinterpret_cast<char*>(&version), sizeof(version));
        if (version != 1) 
        {
            fileStream.close();
            return false;
        }

        uint8_t indexAlignment = 0;
        fileStream.read(reinterpret_cast<char*>(&indexAlignment), sizeof(indexAlignment));
        if (indexAlignment != 2) 
        {
            fileStream.close();
            return false;
        }

        uint32_t sectors = static_cast<int>(uncompressedSize / blockSize);
        uint32_t entries = sectors + 1;

        fileStream.seekg(static_cast<std::streamoff>(indexOffset), std::ios::beg);

        std::vector<CCIIndex> indexInfo;
        for (auto j = 0U; j < entries; j++) {
            uint32_t index = 0;
            fileStream.read(reinterpret_cast<char*>(&index), sizeof(index));
            CCIIndex cciIndex { (static_cast<uint64_t>(index & 0x7FFFFFFF) << indexAlignment), (index & 0x80000000) > 0 };
            indexInfo.push_back(cciIndex);
        }

        CCIDetail cciDetail;
        cciDetail.stream = new std::ifstream(slice, std::ios::binary);
        cciDetail.indexInfo = indexInfo;
        cciDetail.startSector = sectorCount;
        cciDetail.endSector = sectorCount + sectors - 1;

        mCCIDetails.push_back(cciDetail);
        sectorCount += sectors;

        fileStream.close();
    }

    return true;
}

uint32_t cciDecoder::getTotalSectors()
{
    uint32_t totalSectors = 0;
    for (int i = 0; i < mCCIDetails.size(); i++)
    {
        uint32_t sectors = mCCIDetails.at(i).endSector + 1;
        if (sectors > totalSectors)
        {
            totalSectors = sectors;
        }
    }
    return totalSectors;
}

bool cciDecoder::readSector(uint32_t sector, void* buffer, uint32_t bufferSize)
{
    if (bufferSize != 2048 || mHistoryBuffer == nullptr)
    {
        return false;
    }

    mMutex.lock();

    bool result = false;
    for (auto i = 0; i < mCCIDetails.size(); i++)
    {
        auto cciDetail = &mCCIDetails.at(i);
        if (sector >= cciDetail->startSector && sector <= cciDetail->endSector)
        {
            auto sectorOffset = sector - cciDetail->startSector;
            auto indexInfo = &cciDetail->indexInfo[sectorOffset];
            auto position = indexInfo->value;
            auto compressed = indexInfo->compressed;
            auto size = static_cast<uint32_t>(cciDetail->indexInfo[sectorOffset + 1].value - position);

            cciDetail->stream->seekg(static_cast<std::streamoff>(position), std::ios::beg);

            if (size != 2048 || compressed)
            {
                uint8_t padding = 0;
                cciDetail->stream->read(reinterpret_cast<char*>(&padding), sizeof(padding));
                auto decompressBuffer = malloc(size);
                cciDetail->stream->read(reinterpret_cast<char*>(decompressBuffer), size);
                auto decodeBuffer = malloc(2048);

                if (decodeBuffer != nullptr && decompressBuffer != nullptr)
                {
                    auto decompressedSize = 0U;
                    auto decompressor = new lz4();
                    if (decompressor->unpackLZ4Data(reinterpret_cast<uint8_t*>(mHistoryBuffer), CCI_LZ4_HISTORY_SIZE, reinterpret_cast<uint8_t*>(decompressBuffer), size - (padding + 1), reinterpret_cast<uint8_t*>(decodeBuffer), 2048, &decompressedSize) == true && decompressedSize == 2048)
                    {
                        result = true;
                    }
                    delete(decompressor);
                    memcpy(buffer, decodeBuffer, 2048);
                }

                free(decodeBuffer);
                free(decompressBuffer);
            }
            else
            {
                cciDetail->stream->read(reinterpret_cast<char*>(buffer), 2048);
                result = true;
            }
            break;
        }
    }

    mMutex.unlock();
    return result;
}

void cciDecoder::close()
{
    if (mCCIDetails.size() > 0)
    {
        for (auto i = 0; i < mCCIDetails.size(); i++)
        {
            auto cciDetail = &mCCIDetails.at(i);
            cciDetail->stream->close();
            delete(cciDetail->stream);
        }
        mCCIDetails.clear();
    }
}