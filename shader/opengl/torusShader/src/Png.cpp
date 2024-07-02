///////////////////////////////////////////////////////////////////////////////
// Png.cpp
// =======
// PNG image loader/writer class using LodePNG lib written by Lode Vandevenne.
// http://members.gamedev.net/lode/projects/LodePNG/
//
// Dependency: This class requires lodepng.h/.cpp
//
// 2018-08-10: Replaced &data[0] to .data() function.
// 2013-01-23: Changed data size to side_t for 64bit.
// 2009-09-17: Currently support only 32-bit RGBA read/save.
//
//  AUTHOR: Song Ho Ahn (song.ahn@gmail.com)
// CREATED: 2009-09-17
// UPDATED: 2018-08-10
///////////////////////////////////////////////////////////////////////////////

#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>                      // for memcpy()
#include "lodepng.h"
#include "Png.h"
using namespace Image;



///////////////////////////////////////////////////////////////////////////////
// default constructor
///////////////////////////////////////////////////////////////////////////////
Png::Png() : width(0), height(0), bitCount(0), dataSize(0), errorMessage("No error.")
{
}



///////////////////////////////////////////////////////////////////////////////
// default destructor
///////////////////////////////////////////////////////////////////////////////
Png::~Png()
{
}



///////////////////////////////////////////////////////////////////////////////
// clear out the exsiting values
///////////////////////////////////////////////////////////////////////////////
void Png::init()
{
    width = height = bitCount = 0;
    dataSize = 0;
    std::vector<unsigned char>().swap(data);
    errorMessage = "No error.";
}



///////////////////////////////////////////////////////////////////////////////
// print itself for debug
///////////////////////////////////////////////////////////////////////////////
void Png::printSelf() const
{
    std::cout << "===== Png =====\n"
              << "Width: " << width << " pixels\n"
              << "Height: " << height << " pixels\n"
              << "Bit Count: " << bitCount << " bits\n"
              << "Data Size: " << dataSize  << " bytes\n"
              << std::endl;
}



///////////////////////////////////////////////////////////////////////////////
// read a PNG image header infos and datafile and load
// It uses LogePNG lib.
///////////////////////////////////////////////////////////////////////////////
bool Png::read(const char* fileName)
{
    this->init();   // clear out all values

    // check NULL pointer
    if(!fileName)
    {
        errorMessage = "File name is not defined (NULL pointer).";
        return false;
    }

    std::vector<unsigned char> buffer;
    LodePNG::loadFile(buffer, fileName);    // load file to memory
    if(buffer.empty())
    {
        errorMessage = "Failed to open the PNG file to read.";
        return false;
    }

    // decode PNG file
    LodePNG::Decoder decoder;
    decoder.decode(data, &buffer[0], (unsigned)buffer.size());
    if(decoder.hasError())
    {
        std::stringstream ss;
        ss << "Failed to decode PNG file [code:" << decoder.getError() << "]." << std::ends;
        errorMessage = ss.str();
        return false;
    }

    width = decoder.getWidth();
    height = decoder.getHeight();
    bitCount = 32;                  // always 32 bit
    dataSize = data.size();

    return true;
}



///////////////////////////////////////////////////////////////////////////////
// save an image as a PNG format
// NOTE: support only 32-bit RGBA data
///////////////////////////////////////////////////////////////////////////////
bool Png::save(const char* fileName, int w, int h, int channelCount, const unsigned char* data)
{
    // reset error message
    errorMessage = "No error.";

    if(!fileName || !data)
    {
        errorMessage = "File name is not specified (NULL pointer).";
        return false;
    }

    if(w == 0 || h == 0)
    {
        errorMessage = "Zero width or height.";
        return false;
    }

    if(channelCount != 4)
    {
        errorMessage = "Png::save() currently supports 32-bit RGBA data.";
        return false;
    }

    // create encoder and set settings
    LodePNG::Encoder encoder;
    encoder.getSettings().zlibsettings.windowSize = 2048;

    // encode and save
    std::vector<unsigned char> buffer;
    encoder.encode(buffer, data, w, h);
    LodePNG::saveFile(buffer, fileName);

    return true;
}
