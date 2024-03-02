// LibCCI.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <fstream>

#include "cciDecoder.h"

int main()
{
    auto decoder = new cciDecoder();
    decoder->open("G:\\Xbox360\\007.1.cci");

    std::ofstream outputFile("G:\\Xbox360\\test.bin", std::ios::binary); // Open file in binary mode
    if (!outputFile.is_open()) {
        std::cerr << "Error opening file!" << std::endl;
        return 1;
    }

    auto sectorBuffer = reinterpret_cast<uint8_t*>(malloc(2048));

    auto totalSectors = decoder->getTotalSectors();
    for (auto i = 0U; i < totalSectors; i++)
    {
        //std::cout << ("Sector " + std::to_string(i) + "\n");
        decoder->readSector(i, sectorBuffer, 2048);
        outputFile.write(reinterpret_cast<char*>(sectorBuffer), 2048);
    }
    
    outputFile.close();

    decoder->close();

    std::cout << "Hello World!\n";
    return 1;
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
