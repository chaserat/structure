///////////////////////////////////////////////////////////////////////////////
// Png.h
// =====
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

#ifndef IMAGE_PNG_H
#define IMAGE_PNG_H

#include <string>
#include <vector>

namespace Image
{
    class Png
    {
    public:
        // ctor/dtor
        Png();
        ~Png();

        // load image header and data from a png file
        bool read(const char* fileName);

        // save an image as PNG format
        bool save(const char* fileName, int width, int height, int channelCount, const unsigned char* data);

        // getters
        int getWidth() const;                       // return width of image in pixel
        int getHeight() const;                      // return height of image in pixel
        int getBitCount() const;                    // return the number of bits per pixel (8, 24, or 32)
        std::size_t getDataSize() const;            // return data size in bytes
        const unsigned char* getData() const;       // return the pointer to image data

        void printSelf() const;                     // print itself for debug purpose
        const char* getError() const;               // return last error message

    protected:


    private:
        // member functions
        void init();                                // clear the existing values

        // member variables
        int width;
        int height;
        int bitCount;
        std::size_t dataSize;
        std::vector<unsigned char> data;            // data with default BGR order
        std::string errorMessage;
    };


    ///////////////////////////////////////////////////////////////////////////
    // inline functions
    ///////////////////////////////////////////////////////////////////////////
    inline int Png::getWidth() const { return width; }
    inline int Png::getHeight() const { return height; }

    // return bits per pixel, 8 means grayscale, 24 means RGB color, 32 means RGBA
    inline int Png::getBitCount() const { return bitCount; }

    inline std::size_t Png::getDataSize() const { return dataSize; }
    inline const unsigned char* Png::getData() const { return data.data(); }

    inline const char* Png::getError() const { return errorMessage.c_str(); }
}

#endif // IMAGE_PNG_H
