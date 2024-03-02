#include "lz4.h"
#include <cstdint>
#include <corecrt_memory.h>

typedef struct _UserPtr
{
	uint8_t* dataIn;
	uint32_t dataInSize;
	uint32_t dataInPos;
	uint8_t* dataOut;
	uint32_t dataOutSize;
	uint32_t dataOutPos;
	bool bufferToSmall;
} UserPtr, * PUserPtr;

typedef unsigned char (*GET_BYTE) (PUserPtr user);
typedef void (*SEND_BYTES) (uint8_t* data, uint32_t numBytes, PUserPtr user);

constexpr auto CCI_LZ4_HISTORY_SIZE = 2560;

uint8_t getByteFromIn(PUserPtr user)
{
	if (user->dataInPos == user->dataInSize)
	{
		return 0;
	}
	return user->dataIn[user->dataInPos++];
}

void sendBytesToOut(uint8_t* data, uint32_t numBytes, PUserPtr user)
{
	if (user->dataOutPos + numBytes > user->dataOutSize)
	{
		user->bufferToSmall = true;
	}
	else
	{
		memcpy(user->dataOut + user->dataOutPos, data, numBytes);
	}
	user->dataOutPos += numBytes;
}

bool decompress(uint8_t* history, uint32_t historySize, GET_BYTE getByte, SEND_BYTES sendBytes, PUserPtr user)
{
	uint32_t blockOffset;
	uint32_t numWritten;
	uint32_t pos;
	uint32_t blockSize;
	uint8_t token;
	uint32_t numLiterals;
	uint8_t current;
	uint32_t delta;
	uint32_t matchLength;
	uint32_t referencePos;

	pos = 0;

	blockSize = user->dataInSize;
	if (blockSize == 0)
	{
		return false;
	}

	blockOffset = 0;
	numWritten = 0;
	while (blockOffset < blockSize)
	{
		token = getByte(user);
		blockOffset++;

		numLiterals = token >> 4;
		if (numLiterals == 15)
		{
			do
			{
				current = getByte(user);
				numLiterals += current;
				blockOffset++;
			} while (current == 255);
		}

		blockOffset += numLiterals;

		if (pos + numLiterals < historySize)
		{
			while (numLiterals-- > 0)
			{
				history[pos++] = getByte(user);
			}
		}
		else
		{
			while (numLiterals-- > 0)
			{
				history[pos++] = getByte(user);
				if (pos == historySize)
				{
					sendBytes(history, historySize, user);
					numWritten += historySize;
					pos = 0;
				}
			}
		}

		if (blockOffset == blockSize)
		{
			break;
		}

		delta = getByte(user);
		delta |= (unsigned int)getByte(user) << 8;
		if (delta == 0)
		{
			return false;
		}
		blockOffset += 2;

		matchLength = 4 + (token & 0x0F);
		if (matchLength == 4 + 0x0F)
		{
			do
			{
				current = getByte(user);
				matchLength += current;
				blockOffset++;
			} while (current == 255);
		}

		referencePos = (pos >= delta) ? (pos - delta) : (historySize + pos - delta);

		if (pos + matchLength < historySize && referencePos + matchLength < historySize)
		{
			if (pos >= referencePos + matchLength || referencePos >= pos + matchLength)
			{
				memcpy(history + pos, history + referencePos, matchLength);
				pos += matchLength;
			}
			else
			{
				while (matchLength-- > 0) 
				{
					history[pos++] = history[referencePos++];
				}
			}
		}
		else
		{
			while (matchLength-- > 0)
			{
				history[pos++] = history[referencePos++];
				if (pos == historySize)
				{
					sendBytes(history, historySize, user);
					numWritten += historySize;
					pos = 0;
				}
				referencePos %= historySize;
			}
		}
	}

	sendBytes(history, pos, user);
	return !user->bufferToSmall;
}

bool lz4::unpackLZ4Data(uint8_t* history, uint32_t historySize, uint8_t* inputBuffer, uint32_t inputBufferSize, uint8_t* outputBuffer, uint32_t outputBufferSize, uint32_t* decompressedSize)
{
	UserPtr user;
	unsigned char padding;

	padding = inputBuffer[0];

	memset(&user, 0, sizeof(user));
	user.dataIn = inputBuffer + 1;
	user.dataInSize = inputBufferSize - (padding + 1);
	user.dataOut = outputBuffer;
	user.dataOutSize = outputBufferSize;

	bool result = decompress(history, CCI_LZ4_HISTORY_SIZE, getByteFromIn, sendBytesToOut, &user);
	*decompressedSize = user.dataOutPos;
	return result;
}

