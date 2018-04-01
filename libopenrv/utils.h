#ifndef OPENRV_UTILS_H
#define OPENRV_UTILS_H

#include <stdint.h>
#include <stdlib.h>

/**
 * Collection of misc helper functions.
 **/
class Utils
{
public:
    static size_t dumpHexTo(char* dst, uint32_t dstSize, const uint8_t* src, uint32_t srcSize, bool* allDataWritten = nullptr);
    static size_t dumpHexLinesTo(char* dst, uint32_t dstSize, const uint8_t* src, uint32_t srcSize, bool dumpAscii = true, bool* allDataWritten = nullptr);
    static size_t writeUInt8AsHex(char* dst, uint8_t byte);
    static size_t writeUInt8AsHexWithPrefix(char* dst, uint8_t byte);
    static size_t writeUInt16AsHex(char* dst, uint16_t value);
    static size_t writeUInt16AsHexWithPrefix(char* dst, uint16_t value);
    static size_t writeUInt32AsHex(char* dst, uint32_t value);
    static size_t writeUInt32AsHexWithPrefix(char* dst, uint32_t value);
};

/**
 * Write @p byte to @p dst, as ascii hex with "0x" prefix.
 *
 * This write exactly 4 chars to @p dst.
 *
 * @return The number of bytes written to @p dst, always 4.
 **/
inline size_t Utils::writeUInt8AsHexWithPrefix(char* dst, uint8_t byte)
{
    dst[0] = '0';
    dst[1] = 'x';
    size_t s = writeUInt8AsHex(dst + 2, byte);
    return s + 2;
}

/**
 * Write @p byte to @p dst, as ascii hex.
 *
 * This write exactly 2 chars to @p dst.
 *
 * @return The number of bytes written to @p dst, always 2.
 **/
inline size_t Utils::writeUInt8AsHex(char* dst, uint8_t byte)
{
    uint8_t v1 = byte / 16;
    uint8_t v2 = byte % 16;
    if (v1 < 10) {
        dst[0] = '0' + v1;
    }
    else {
        dst[0] = 'a' + (v1 - 10);
    }
    if (v2 < 10) {
        dst[1] = '0' + v2;
    }
    else {
        dst[1] = 'a' + (v2 - 10);
    }
    return 2;
}

/**
 * Write @p value to @p dst, as ascii hex with "0x" prefix.
 *
 * This write exactly 6 chars to @p dst.
 *
 * @return The number of bytes written to @p dst, always 6.
 **/
inline size_t Utils::writeUInt16AsHexWithPrefix(char* dst, uint16_t value)
{
    dst[0] = '0';
    dst[1] = 'x';
    size_t s = 2;
    s += writeUInt16AsHex(dst + s, value);
    return s;
}

/**
 * Write @p value to @p dst, as ascii hex.
 *
 * This write exactly 4 chars to @p dst.
 *
 * @return The number of bytes written to @p dst, always 4.
 **/
inline size_t Utils::writeUInt16AsHex(char* dst, uint16_t value)
{
    uint8_t b1 = (value >> 8) & 0xff;
    uint8_t b2 = value & 0xff;
    size_t s = 2;
    s += writeUInt8AsHex(dst + s, b1);
    s += writeUInt8AsHex(dst + s, b2);
    return s;
}


/**
 * Write @p value to @p dst, as ascii hex with "0x" prefix.
 *
 * This write exactly 10 chars to @p dst.
 *
 * @return The number of bytes written to @p dst, always 10.
 **/
inline size_t Utils::writeUInt32AsHexWithPrefix(char* dst, uint32_t value)
{
    dst[0] = '0';
    dst[1] = 'x';
    size_t s = 2;
    s += writeUInt32AsHex(dst + s, value);
    return s;
}

/**
 * Write @p value to @p dst, as ascii hex.
 *
 * This write exactly 8 chars to @p dst.
 *
 * @return The number of bytes written to @p dst, always 8.
 **/
inline size_t Utils::writeUInt32AsHex(char* dst, uint32_t value)
{
    uint8_t b1 = (value >> 24) & 0xff;
    uint8_t b2 = (value >> 16) & 0xff;
    uint8_t b3 = (value >> 8) & 0xff;
    uint8_t b4 = value & 0xff;
    size_t s = 0;
    s += writeUInt8AsHex(dst + s, b1);
    s += writeUInt8AsHex(dst + s, b2);
    s += writeUInt8AsHex(dst + s, b3);
    s += writeUInt8AsHex(dst + s, b4);
    return s;
}

#endif

