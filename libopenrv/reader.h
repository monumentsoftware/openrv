/*
 * Copyright (C) 2018 Monument-Software GmbH
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef OPENRV_READER_H
#define OPENRV_READER_H

#if !defined(_MSC_VER)
#include <arpa/inet.h>
#else // _MSC_VER
#include <WinSock2.h>
#endif // _MSC_VER
#include <string.h>

#include <libopenrv/libopenrv.h>

class Reader
{
public:
    static uint8_t readUInt8(const char* buffer);
    static uint16_t readUInt16(const char* buffer);
    static uint32_t readUInt32(const char* buffer);
    static int32_t readInt32(const char* buffer);
    static void readPixel(uint8_t* outRgb, const uint8_t* buffer, const orv_communication_pixel_format_t& format);
    static void readPixel8Bit(uint8_t* outRgb, const uint8_t* buffer, const orv_communication_pixel_format_t& format);
    static void readPixel16Bit(uint8_t* outRgb, const uint8_t* buffer, const orv_communication_pixel_format_t& format);
    static void readPixel32Bit(uint8_t* outRgb, const uint8_t* buffer, const orv_communication_pixel_format_t& format);
};

/**
 * @pre @p buffer is non-NULL and has at least 4 bytes
 **/
inline uint32_t Reader::readUInt32(const char* buffer)
{
    uint32_t tmp;
    memcpy(&tmp, buffer, 4);
    return ntohl(tmp);
}

/**
 * @pre @p buffer is non-NULL and has at least 4 bytes
 **/
inline int32_t Reader::readInt32(const char* buffer)
{
    int32_t tmp;
    memcpy(&tmp, buffer, 4);
    return (int32_t)ntohl((uint32_t)tmp);
}

/**
 * @pre @p buffer is non-NULL and has at least 2 bytes
 **/
inline uint16_t Reader::readUInt16(const char* buffer)
{
    uint16_t tmp;
    memcpy(&tmp, buffer, 2);
    return ntohs(tmp);
}

/**
 * @pre @p buffer is non-NULL
 **/
inline uint8_t Reader::readUInt8(const char* buffer)
{
    return buffer[0];
}

/**
 * @pre @p buffer is non-NULL
 * @pre @p outRgb can hold at least 3 byte (RGB data)
 *
 * Read a single pixel from @p buffer in the format specified by @p format.
 *
 * The pixel is placed into @p outRgb as 1 byte per color channel (RGB data).
 **/
inline void Reader::readPixel(uint8_t* outRgb, const uint8_t* buffer, const orv_communication_pixel_format_t& format)
{
    switch (format.mBitsPerPixel) {
        case 8:
            readPixel8Bit(outRgb, buffer, format);
            break;
        case 16:
            readPixel16Bit(outRgb, buffer, format);
            break;
        case 32:
            readPixel32Bit(outRgb, buffer, format);
            break;
        default:
            memset(outRgb, 0, 3);
            break;
    }
}

/**
 * @pre @p buffer is non-NULL
 * @pre @p outRgb can hold at least 3 byte (RGB data)
 *
 * Read a single pixel from @p buffer in 8 bits per pixel format.
 *
 * The pixel is placed into @p outRgb as 1 byte per color channel (RGB data).
 **/
inline void Reader::readPixel8Bit(uint8_t* outRgb, const uint8_t* buffer, const orv_communication_pixel_format_t& format)
{
    // NOTE: no pixel swapping required for 1-byte pixels
    uint8_t v = buffer[0];
    for (int i = 0; i < 3; i++) {
        if (format.mColorMax[i] > 0) {
            uint8_t c = (v >> format.mColorShift[i]) & format.mColorMax[i];
            outRgb[i] = (uint8_t)(((uint16_t)c * 255) / format.mColorMax[i]);
        }
        else {
            outRgb[i] = 0;
        }
    }
}

/**
 * @pre @p buffer is a non-NULL array of at least 2 bytes
 * @pre @p outRgb can hold at least 3 byte (RGB data)
 *
 * Read a single pixel from @p buffer in 16 bits per pixel format.
 *
 * The pixel is placed into @p outRgb as 1 byte per color channel (RGB data).
 **/
inline void Reader::readPixel16Bit(uint8_t* outRgb, const uint8_t* buffer, const orv_communication_pixel_format_t& format)
{
    uint16_t v;
    memcpy(&v, buffer, 2);
    if (format.mBigEndian) { // we assume little endian host-order
        v = ntohs(v);
    }
    for (int i = 0; i < 3; i++) {
        if (format.mColorMax[i] > 0) {
            uint16_t c = (v >> format.mColorShift[i]) & format.mColorMax[i];
            outRgb[i] = (uint8_t)(((uint32_t)c * 255) / format.mColorMax[i]);
        }
        else {
            outRgb[i] = 0;
        }
    }
}

/**
 * @pre @p buffer is a non-NULL array of at least 4 bytes
 * @pre @p outRgb can hold at least 3 byte (RGB data)
 *
 * Read a single pixel from @p buffer in 32 bits per pixel format.
 *
 * The pixel is placed into @p outRgb as 1 byte per color channel (RGB data).
 **/
inline void Reader::readPixel32Bit(uint8_t* outRgb, const uint8_t* buffer, const orv_communication_pixel_format_t& format)
{
    uint32_t v;
    memcpy(&v, buffer, 4);
    if (format.mBigEndian) { // we assume little endian host-order
        v = ntohl(v);
    }
    for (int i = 0; i < 3; i++) {
        if (format.mColorMax[i] > 0) {
            // NOTE: a single color channel can be at most uint16_t, because mColorMax is a uint16_t
            uint16_t c = (v >> format.mColorShift[i]) & format.mColorMax[i];
            outRgb[i] = (uint8_t)(((uint32_t)c * 255) / format.mColorMax[i]);
        }
        else {
            outRgb[i] = 0;
        }
    }
}



#endif

