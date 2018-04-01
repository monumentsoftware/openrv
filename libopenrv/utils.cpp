#include "utils.h"

#include <stdio.h>
#include <algorithm>
#include <string.h>
#include <ctype.h>

/**
 * Dump a buffer as hex, decimal and optionally ascii to @p dst.
 *
 * This function writes 8 bytes from @p src per line and writes as many lines as required (up until
 * @p dstSize is reached). The @p dst ist always terminated with a NUL-byte.
 *
 * If @p dst is NULL, the data is written to stdout instead (@p dstSize is ignored).
 *
 * @return The number of bytes written to @p dst (or to stdout), not including the terminating
 *         NUL-byte.
 **/
size_t Utils::dumpHexLinesTo(char* dst, uint32_t dstSize, const uint8_t* src, uint32_t srcSize, bool dumpAscii, bool* allDataWritten)
{
    const size_t lineMax = 256;
    char line[lineMax + 1];
    line[lineMax] = 0;
    size_t writtenBytes = 0;
    uint32_t srcPos = 0;
    if (dst && dstSize > 0) {
        dst[0] = '\0';
    }
    while (srcPos < srcSize) {
        size_t linePos = 0;
        linePos += writeUInt32AsHexWithPrefix(line, (uint32_t)srcPos);
        line[linePos] = ':';
        linePos++;
        const size_t maxBytes = std::min(srcSize - srcPos, (uint32_t)8);
        for (size_t i = 0; i < maxBytes; i++) {
            line[linePos] = ' ';
            linePos++;
            linePos += writeUInt8AsHex(line + linePos, src[srcPos + i]);
        }
        for (size_t i = maxBytes; i < 8; i++) {
            line[linePos + 0] = ' ';
            line[linePos + 1] = ' ';
            line[linePos + 2] = ' ';
            linePos += 3;
        }
        line[linePos + 0] = ' ';
        line[linePos + 1] = ' ';
        line[linePos + 2] = '|';
        line[linePos + 3] = ' ';
        linePos += 4;
        for (size_t i = 0; i < maxBytes; i++) {
            linePos += sprintf(line + linePos, " %3d", src[srcPos + i]);
        }
        for (size_t i = maxBytes; i < 8; i++) {
            line[linePos + 0] = ' ';
            line[linePos + 1] = ' ';
            line[linePos + 2] = ' ';
            line[linePos + 3] = ' ';
            linePos += 4;
        }
        if (dumpAscii) {
            line[linePos + 0] = ' ';
            line[linePos + 1] = ' ';
            line[linePos + 2] = '|';
            line[linePos + 3] = ' ';
            linePos += 4;
            for (size_t i = 0; i < maxBytes; i++) {
                int b = src[srcPos + i];
                if (b < 127 && isprint(b)) {
                    linePos += sprintf(line + linePos, " %c", (char)b);
                }
                else {
                    line[linePos + 0] = ' ';
                    line[linePos + 1] = '.';
                    linePos += 2;
                }
            }
        }
        line[linePos] = '\n';
        linePos++;
        line[linePos] = '\0';
        if (dst == nullptr) {
            printf("%s", line);
            writtenBytes += linePos;
        }
        else {
            if (dstSize < writtenBytes + linePos + 1) {
                if (allDataWritten) {
                    *allDataWritten = false;
                }
                return writtenBytes;
            }
            memcpy(dst + writtenBytes, line, linePos);
            writtenBytes += linePos;
            dst[writtenBytes] = '\0';
        }
        srcPos += maxBytes;
    }
    if (allDataWritten) {
        *allDataWritten = true;
    }
    return writtenBytes;
}

/**
 * Write the bytes from @p src in ascii hex to @p dst, separated by spaces. No newlines are inserted
 * by this function and no ascii or decimal representation is printed. The @p dst is always
 * terminated by a NUL byte.
 *
 * If @p dst is NULL, the data is written to stdout instead (@p dstSize is ignored then).
 **/
size_t Utils::dumpHexTo(char* dst, uint32_t dstSize, const uint8_t* src, uint32_t srcSize, bool* allDataWritten)
{
    uint32_t dstPos = 0;
    if (dst) {
        for (uint32_t srcPos = 0; srcPos < srcSize; srcPos++) {
            if (dstSize < dstPos + 3) {
                if (allDataWritten) {
                    *allDataWritten = false;
                }
                dst[dstPos] = '\0';
                return dstPos;
            }
            dstPos += writeUInt8AsHex(dst + dstPos, src[srcPos]);
            dst[dstPos] = ' ';
            dstPos++;
        }
        dst[dstPos] = '\0';
    }
    else {
        char buf[3];
        buf[2] = 0;
        for (uint32_t srcPos = 0; srcPos < srcSize; srcPos++) {
            if (srcPos != 0) {
                printf(" ");
                dstPos += 1;
            }
            writeUInt8AsHex(buf, src[srcPos]);
            printf("%s", buf);
            dstPos += 2;
        }
        printf("\n");
        dstPos += 1;
    }
    return dstPos;
}

