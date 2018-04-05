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

#include "messageparser.h"
#include <libopenrv/orv_logging.h>
#include <libopenrv/libopenrv.h>
#include <libopenrv/orv_error.h>
#include "reader.h"
#include "writer.h"
#include "orv_context.h"
#include "rectdataparser.h"

#include <assert.h>
#include <sys/types.h>
#include <algorithm>
#include <list>
#include <string.h>
#include <zlib.h>

#define UNUSED(x) (void)x


/**
 * Do not accept more subrectangles than this value from remote. This protects against memory
 * garbage sent by the server or received/read (e.g. due to implementation error) causing the client
 * to allocate huge buffers.
 **/
#define ORV_MAX_RRE_SUBRECTANGLES_COUNT 1000000


/**
 * Maximal number of bytes for a single rect buffer to allocate for zlib compressions. If server
 * sends larger rects, an error is generated.
 **/
#define ORV_MAX_COMPRESSED_RECT_BUFFER_SIZE (1024*1024*100)

namespace openrv {
namespace vnc {

/**
 * @param currentPixelFormat The pixel format that the communication takes place in.
 *        The pointer must remain valid for the lifetime of this object.
 *        The contents of this object @em must remain @em unchanged for the full duration of reading
 *        a rect. The pixel format may only change @em before or @em after a rect has been fully
 *        read. In particular, after changing the format, both @ref reset() and @ref
 *        setCurrentRect() must be called on this object before any additional data of a rect is
 *        being read.
 * @param currentFramebufferWidth The current width of the framebuffer. Identical to @ref
 *        orv_framebuffer_t::mWidth of @p mFramebuffer, but lives in the calling thread and
 *        therefore does not need to be protected with a mutex.
 *        The pointer must remain valid for the lifetime of this object.
 *        The value of this variable @em must remain @em unchanged for the full duration of reading
 *        a rect, similar to @p currentPixelFormat.
 * @param currentFramebufferHeight The current height of the framebuffer. Identical to @ref
 *        orv_framebuffer_t::mHeight of @p mFramebuffer, but lives in the calling thread and
 *        therefore does not need to be protected with a mutex.
 *        The pointer must remain valid for the lifetime of this object.
 *        The value of this variable @em must remain @em unchanged for the full duration of reading
 *        a rect, similar to @p currentPixelFormat.
 **/
RectDataParserBase::RectDataParserBase(orv_context_t* ctx, const orv_communication_pixel_format_t* currentPixelFormat, const uint16_t* currentFramebufferWidth, const uint16_t* currentFramebufferHeight)
    : mContext(ctx),
      mCurrentPixelFormat(*currentPixelFormat),
      mCurrentFramebufferWidth(*currentFramebufferWidth),
      mCurrentFramebufferHeight(*currentFramebufferHeight)
{
}

/**
 * Set the location and size of the rect that should be read by this object.
 *
 * This is called once prior to any @ref readRectData() or @ref finishRect() calls.
 **/
void RectDataParserBase::setCurrentRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
    mCurrentRect.mX = x;
    mCurrentRect.mY = y;
    mCurrentRect.mW = w;
    mCurrentRect.mH = h;
}

/**
 * Reset this object, so that the next rect can be parsed.
 *
 * This is called prior to parsing a new rect and/or after a rect has been finished.
 * Derived classes should reset their internal state here.
 *
 * Reimplementations of this function must call the base implementation.
 **/
void RectDataParserBase::reset()
{
    setCurrentRect(0, 0, 0, 0);
}

/**
 * Reset this object for a new connection.
 *
 * This function is called when the connection to the remote server is closed and is meant to free
 * up resources that would normally persist over messages (but not over connections).
 *
 * The default implementation calls @ref reset().
 **/
void RectDataParserBase::resetConnection()
{
    reset();
}

/**
 * @param framebufferMutex Mutex for the @p framebuffer.
 *        The pointer must remain valid for the lifetime of this object.
 * @param framebuffer The framebuffer that the data is written to. Must be protected by @p
 *        framebufferMutex.
 *        The pointer must remain valid for the lifetime of this object.
 **/
RectDataParserRealRectBase::RectDataParserRealRectBase(orv_context_t* ctx, std::mutex* framebufferMutex, orv_framebuffer_t* framebuffer, const orv_communication_pixel_format_t* currentPixelFormat, const uint16_t* currentFramebufferWidth, const uint16_t* currentFramebufferHeight)
    : RectDataParserBase(ctx, currentPixelFormat, currentFramebufferWidth, currentFramebufferHeight),
      mFramebufferMutex(*framebufferMutex),
      mFramebuffer(*framebuffer)
{
}

/**
 * @pre The @ref mFramebufferMutex is LOCKED
 *
 * Check that the parameters (position, dimensions and bytes per pixel) of @ref mCurrentRect
 * are compatible with the framebuffer.
 *
 * If this function returns TRUE, this function guarantees that
 * @li The position plus width/height of the current rectangle does not exceed @ref
 *     mCurrentFramebufferWidth and @ref mCurrentFramebufferHeight
 * @li The @ref mCurrentFramebufferWidth and @ref mCurrentFramebufferHeight matches the
 *     corresponding values in @ref mFramebuffer
 *
 * @return TRUE if a rect with the parameter in @ref mCurrentRect can be written into the
 *         @ref mFramebuffer, otherwise FALSE. If this function returns FALSE,
 *         the @p error is set accordingly.
 **/
bool RectDataParserRealRectBase::checkRectParametersForFramebufferMutexLocked(orv_error_t* error)
{
    if ((uint32_t)mCurrentRect.mX + (uint32_t)mCurrentRect.mW > mCurrentFramebufferWidth ||
        (uint32_t)mCurrentRect.mY + (uint32_t)mCurrentRect.mH > mCurrentFramebufferHeight) {
        orv_error_set(error, ORV_ERR_GENERIC, 0, "Invalid rect received, exceeds framebuffer dimensions. Rect: %dx%d at %dx%d, framebuffer: %ux%u", (int)mCurrentRect.mW, (int)mCurrentRect.mH, (int)mCurrentRect.mX, (int)mCurrentRect.mY, (unsigned int)mCurrentFramebufferWidth, (unsigned int)mCurrentFramebufferHeight);
        return false;
    }

    if (mCurrentFramebufferWidth != mFramebuffer.mWidth ||
        mCurrentFramebufferHeight != mFramebuffer.mHeight) {
        // mCurrentFramebuffer* and mFramebuffer.* should always match.
        orv_error_set(error, ORV_ERR_GENERIC, 0, "Internal error: Framebuffer sizes out of sync.");
        return false;
    }
    return true;
}

/**
 * Calculate the size of the buffer required to store pixels for the specified rect size with the
 * specified @p bitsPerPixel.
 *
 * If the calculated values are out of range, @p bufferSize is set to 0 and an error is returned.
 *
 * NOTE: A buffer of size 0 may still be valid (e.g. rectWidth or rectHeight 0), the caller should
 *       either ensure this does not happen or handle this case.
 *
 * @return TRUE on success (@p bufferSize holds a valid value, which may include 0), FALSE on error
 *         (the specified parameters are not valid). On error, @p bufferSize is set to 0.
 **/
bool RectDataParserRealRectBase::calculateRectBufferSizeFor(uint32_t* bufferSize, uint16_t rectWidth, uint16_t rectHeight, uint8_t bitsPerPixel)
{
    // multiplication of 2 uint16_t and a uint8_t may be up to 40 bits, so a uint64_t can always hold it
    const uint64_t expectedBytes = ((uint64_t)rectWidth) * ((uint64_t)rectHeight) * ((uint64_t)(bitsPerPixel/8));
    if (expectedBytes > 0xffffffff) {
        // result exceeds 32 bit
        *bufferSize = 0;
        return false;
    }
    *bufferSize = (uint32_t)expectedBytes;
    return true;
}

/**
 * Fill a subrect of the specified @p rectData with the @p color (@p bpp bytes).
 *
 * NOTE: The caller is responsible to ensure the specified subrect is actually a subrect of the
 * rect.
 *
 * @param rectData The pointer to the output rectangle.
 *                 This must be a buffer of size @ref calculateRectBufferSizeFor() with the @p
 *                 rectWidth and @p bpp and a height large enough for the specified subrect.
 * @param rectWidth The width of the rect in @p rectData
 * @param subrectXInRect The x position of the subrectangle that is to-be filled, relative to the
 *        full rect.
 * @param subrectYInRect The y position of the subrectangle that is to-be filled, relative to the
 *        full rect.
 * @param subrectWidth The width of the subrect that is to-be filled.
 * @param subrectHeight The height of the subrect that is to-be filled.
 **/
void RectDataParserRealRectBase::fillSubrectInRect(uint8_t* rectData, uint16_t rectWidth, uint16_t subrectXInRect, uint16_t subrectYInRect, uint16_t subrectWidth, uint16_t subrectHeight, const uint8_t* color, uint8_t bpp)
{
    if (bpp == 1) {
        // simple special case: we can use memset with the color.
        for (int y = 0; y < subrectHeight; y++) {
            const uint16_t rectY = subrectYInRect + y;
            uint8_t* dstLine = rectData + (rectY * rectWidth + subrectXInRect) * bpp;
            memset(dstLine, color[0], subrectWidth);
        }
        return;
    }
    for (int y = 0; y < subrectHeight; y++) {
        const uint16_t rectY = subrectYInRect + y;
        uint8_t* dstLine = rectData + (rectY * rectWidth + subrectXInRect) * bpp;
        for (int x = 0; x < subrectWidth; x++) {
            for (uint8_t i = 0; i < bpp; i++) {
                *(dstLine + x * bpp + i) = color[i];
            }
        }
    }
}

RectDataParserRaw::RectDataParserRaw(struct orv_context_t* context, std::mutex* framebufferMutex, orv_framebuffer_t* framebuffer, const orv_communication_pixel_format_t* currentPixelFormat, const uint16_t* currentFramebufferWidth, const uint16_t* currentFramebufferHeight)
    : RectDataParserRealRectBase(context, framebufferMutex, framebuffer, currentPixelFormat, currentFramebufferWidth, currentFramebufferHeight)
{
}

RectDataParserRaw::~RectDataParserRaw()
{
    clear();
}

void RectDataParserRaw::clear()
{
    free(mCurrentRectData);
    mCurrentRectData = nullptr;
    mCurrentRectDataSize = 0;
    mExpectedBytes = 0;
    mConsumed = 0;
}

/**
 * @pre The size of the @ref mCurrentRect does not exceed the size of the framebuffer
 * @pre This function is allowed to allocate up to width*height*bytesPerPixel of buffer data
 **/
uint32_t RectDataParserRaw::readRectData(const char* buffer, uint32_t bufferSize, orv_error_t* error)
{
    if (!mCurrentRectData) {
        if ((uint32_t)mCurrentRect.mX + (uint32_t)mCurrentRect.mW > mCurrentFramebufferWidth ||
            (uint32_t)mCurrentRect.mY + (uint32_t)mCurrentRect.mH > mCurrentFramebufferHeight) {
            orv_error_set(error, ORV_ERR_PROTOCOL_ERROR, 20, "Invalid rect received, exceeds framebuffer dimensions. Rect: %dx%d at %dx%d, framebuffer: %ux%u", (int)mCurrentRect.mW, (int)mCurrentRect.mH, (int)mCurrentRect.mX, (int)mCurrentRect.mY, (unsigned int)mCurrentFramebufferWidth, (unsigned int)mCurrentFramebufferHeight);
            return 0;
        }

        uint32_t expectedBytes = 0;
        if (!calculateRectBufferSizeFor(&expectedBytes, mCurrentRect.mW, mCurrentRect.mH, mCurrentPixelFormat.mBitsPerPixel)) {
            // protocol error, server sent garbage.
            orv_error_set(error, ORV_ERR_PROTOCOL_ERROR, 10, "Server sent rect of size %dx%d with %d bytes per pixel in raw encoding, which exceeds 32 bit size. This is impossible, invalid data received.", (int)mCurrentRect.mW, (int)mCurrentRect.mH, (int)mCurrentPixelFormat.mBitsPerPixel/8);
            return 0;
        }

        free(mCurrentRectData);
        mCurrentRectDataSize = expectedBytes;
        mCurrentRectData = (uint8_t*)malloc(mCurrentRectDataSize);
    }

    if (!mCurrentRectData || mCurrentRectDataSize == 0) {
        orv_error_set(error, ORV_ERR_GENERIC, 0, "Internal error, mCurrentRectData not initialized");
        return 0;
    }
    uint32_t copy = std::min(bufferSize, mCurrentRectDataSize - mConsumed);
    if (copy > 0) {
        memcpy(mCurrentRectData + mConsumed, buffer, copy);
        mConsumed += copy;
    }
    return copy;
}

bool RectDataParserRaw::canFinishRect() const
{
    if (mCurrentRectData && (mConsumed >= mCurrentRectDataSize)) {
        return true;
    }
    return false;
}

void RectDataParserRaw::finishRect(orv_error_t* error)
{
    std::unique_lock<std::mutex> lock(mFramebufferMutex);
    if (!checkRectParametersForFramebufferMutexLocked(error)) {
        return;
    }
    ORV_DEBUG(mContext, "Performing framebuffer update for RAW data");
    const int remoteBpp = mCurrentPixelFormat.mBitsPerPixel / 8;
    if ((int)mCurrentRect.mW * (int)mCurrentRect.mH * remoteBpp != (int)mCurrentRectDataSize) {
        orv_error_set(error, ORV_ERR_GENERIC, 0, "Invalid rect data size %u for rect %dx%d at bpp %d", (unsigned int)mCurrentRectDataSize, (int)mCurrentRect.mX, (int)mCurrentRect.mY, (int)remoteBpp);
        return;
    }
    if (mFramebuffer.mBytesPerPixel != 3) {
        orv_error_set(error, ORV_ERR_GENERIC, 0, "Internal error: %s assumes internal RGB framebuffer, but have bytesperpixel %d", __PRETTY_FUNCTION__, (int)mFramebuffer.mBytesPerPixel);
        return;
    }

    // TODO: optimizations.
    //  1) provide dedicated optimized function for the most common case(s).
    //     this will eliminate a lot of overhead.
    //     -> in particular: 32 BitsPerPixel, 24 Depth, RGB each 8 bit
    //     -> probably also an implementation for RGB 565 data (most common 16 bit format),
    //        this would be useful as a preset for lower-bandwidth situations
    //  2) maybe eliminate the mBigEndian check: we can easily force the client to always request
    //     little endian (make sure sendSetPixelFormat() is always sent at least once prior to
    //     receiving data and make sure mBigEndian is always false there)
    //  3) scaling from mColorMax to our internal 0..255 range is rather expensive.
    //     we should improve this. (most important improvement covered by (1) though, as we can
    //     eliminate it there entirely)

    if (mCurrentPixelFormat.mBitsPerPixel == 8) {
        for (int srcY = 0; srcY < mCurrentRect.mH; srcY++) {
            const int dstY = mCurrentRect.mY + srcY;
            for (int srcX = 0; srcX < mCurrentRect.mW; srcX++) {
                const int dstX = mCurrentRect.mX + srcX;
                const uint8_t* pSrc = mCurrentRectData + (srcY * mCurrentRect.mW + srcX) * remoteBpp;
                uint8_t* pDst = mFramebuffer.mFramebuffer + (dstY * mFramebuffer.mWidth + dstX) * mFramebuffer.mBytesPerPixel;
                Reader::readPixel8Bit(pDst, pSrc, mCurrentPixelFormat);
            }
        }
    }
    else if (mCurrentPixelFormat.mBitsPerPixel == 16) {
        for (int srcY = 0; srcY < mCurrentRect.mH; srcY++) {
            const int dstY = mCurrentRect.mY + srcY;
            for (int srcX = 0; srcX < mCurrentRect.mW; srcX++) {
                const int dstX = mCurrentRect.mX + srcX;
                const uint8_t* pSrc = mCurrentRectData + (srcY * mCurrentRect.mW + srcX) * remoteBpp;
                uint8_t* pDst = mFramebuffer.mFramebuffer + (dstY * mFramebuffer.mWidth + dstX) * mFramebuffer.mBytesPerPixel;
                Reader::readPixel16Bit(pDst, pSrc, mCurrentPixelFormat);
            }
        }
    }
    else if (mCurrentPixelFormat.mBitsPerPixel == 32) {
        for (int srcY = 0; srcY < mCurrentRect.mH; srcY++) {
            const int dstY = mCurrentRect.mY + srcY;
            for (int srcX = 0; srcX < mCurrentRect.mW; srcX++) {
                const int dstX = mCurrentRect.mX + srcX;
                const uint8_t* pSrc = mCurrentRectData + (srcY * mCurrentRect.mW + srcX) * remoteBpp;
                uint8_t* pDst = mFramebuffer.mFramebuffer + (dstY * mFramebuffer.mWidth + dstX) * mFramebuffer.mBytesPerPixel;
                Reader::readPixel32Bit(pDst, pSrc, mCurrentPixelFormat);
            }
        }
    }
    else {
        orv_error_set(error, ORV_ERR_GENERIC, 0, "Invalid value for BitsPerPixel: %d", (int)mCurrentPixelFormat.mBitsPerPixel);
        return;
    }
}


void RectDataParserRaw::reset()
{
    RectDataParserRealRectBase::reset();
    clear();
}


RectDataParserCopyRect::RectDataParserCopyRect(struct orv_context_t* context, std::mutex* framebufferMutex, orv_framebuffer_t* framebuffer, const orv_communication_pixel_format_t* currentPixelFormat, const uint16_t* currentFramebufferWidth, const uint16_t* currentFramebufferHeight)
    : RectDataParserRealRectBase(context, framebufferMutex, framebuffer, currentPixelFormat, currentFramebufferWidth, currentFramebufferHeight)
{
}

RectDataParserCopyRect::~RectDataParserCopyRect()
{
}

uint32_t RectDataParserCopyRect::readRectData(const char* buffer, uint32_t bufferSize, orv_error_t* error)
{
    if (mHasHeader) {
        orv_error_set(error, ORV_ERR_GENERIC, 0, "Internal error in CopyRect encoding: Data already fully read, but attempted to read more.");
        return 0;
    }
    if (bufferSize < 4) {
        // need more data
        return 0;
    }
    mSrcX = Reader::readUInt16(buffer);
    mSrcY = Reader::readUInt16(buffer + 2);
    mHasHeader = true;
    return 4;
}

void RectDataParserCopyRect::finishRect(orv_error_t* error)
{
    std::unique_lock<std::mutex> lock(mFramebufferMutex);
    if (!checkRectParametersForFramebufferMutexLocked(error)) {
        return;
    }
    ORV_DEBUG(mContext, "Performing framebuffer update for CopyRect data");
    const size_t lineSize = mCurrentRect.mW * mFramebuffer.mBytesPerPixel;
    uint8_t* rect = (uint8_t*)malloc(mCurrentRect.mH * lineSize);
    for (int y = 0; y < mCurrentRect.mH; y++) {
        const uint8_t* src = mFramebuffer.mFramebuffer + ((mSrcY + y) * mFramebuffer.mWidth + mSrcX) * mFramebuffer.mBytesPerPixel;
        memcpy(rect + y * lineSize, src, lineSize);
    }
    for (int y = 0; y < mCurrentRect.mH; y++) {
        uint8_t* dst = mFramebuffer.mFramebuffer + ((mCurrentRect.mY + y) * mFramebuffer.mWidth + mCurrentRect.mX) * mFramebuffer.mBytesPerPixel;
        memcpy(dst, rect + y * lineSize, lineSize);
    }
    free(rect);
    //ORV_DEBUG(mContext, "Finished performing framebuffer update for CopyRect data, rect x=%d y=%d w=%d h=%d", (int)mCurrentRect.mX, (int)mCurrentRect.mY, (int)mCurrentRect.mW, (int)mCurrentRect.mH);
}

bool RectDataParserCopyRect::canFinishRect() const
{
    if (mHasHeader) {
        return true;
    }
    return false;
}

void RectDataParserCopyRect::reset()
{
    RectDataParserRealRectBase::reset();
    mHasHeader = false;
    mSrcX = 0;
    mSrcY = 0;
}



RectDataParserRRE::RectDataParserRRE(struct orv_context_t* context, std::mutex* framebufferMutex, orv_framebuffer_t* framebuffer, const orv_communication_pixel_format_t* currentPixelFormat, const uint16_t* currentFramebufferWidth, const uint16_t* currentFramebufferHeight, bool isCompressedRRE)
    : RectDataParserRealRectBase(context, framebufferMutex, framebuffer, currentPixelFormat, currentFramebufferWidth, currentFramebufferHeight),
      mIsCompressedRRE(isCompressedRRE)
{
}

RectDataParserRRE::~RectDataParserRRE()
{
    clear();
}

void RectDataParserRRE::reset()
{
    RectDataParserRealRectBase::reset();
    clear();
}

void RectDataParserRRE::clear()
{
    mTotalSubRectanglesCount = 0;
    mFinishedSubRectanglesCount = 0;
    mHasRREHeader = false;
    free(mSubRectangles);
    mSubRectangles = nullptr;
    memset(mBackgroundPixelValue, 0, 3);
}



uint32_t RectDataParserRRE::readRectData(const char* buffer, uint32_t bufferSize, orv_error_t* error)
{
    uint32_t consumed = 0;
    if (!mHasRREHeader) {
        if (bufferSize < (uint32_t)(4 + (mCurrentPixelFormat.mBitsPerPixel / 8))) {
            // need more data
            return 0;
        }
        mTotalSubRectanglesCount = Reader::readUInt32(buffer);
        Reader::readPixel(mBackgroundPixelValue, (const uint8_t*)buffer + 4, mCurrentPixelFormat);
        consumed += 4 + (mCurrentPixelFormat.mBitsPerPixel / 8);
        if (mTotalSubRectanglesCount > ORV_MAX_RRE_SUBRECTANGLES_COUNT) {
            orv_error_set(error, ORV_ERR_GENERIC, 0, "Remote tried to send %d subrectangles in RRE encoding, but only %d are allowed by this client.", (int)mTotalSubRectanglesCount, ORV_MAX_RRE_SUBRECTANGLES_COUNT);
            return 0;
        }
        mFinishedSubRectanglesCount = 0;
        mHasRREHeader = true;
        free(mSubRectangles);
        mSubRectangles = nullptr;
        //ORV_DEBUG(mContext, "Received RRE header, expecting %d sub-rectangles", (int)mTotalSubRectanglesCount);
        if (mTotalSubRectanglesCount > 0) {
            mSubRectangles = (SubRectangle*)malloc(mTotalSubRectanglesCount * sizeof(SubRectangle));
        }
    }
    const size_t bytesPerSubRect = (mIsCompressedRRE ? (4*1) : (4*2)) + (mCurrentPixelFormat.mBitsPerPixel / 8);
    while ((mFinishedSubRectanglesCount < mTotalSubRectanglesCount) && bufferSize - consumed >= bytesPerSubRect) {
        SubRectangle* r = mSubRectangles + mFinishedSubRectanglesCount;
        const char* b = buffer + consumed;
        Reader::readPixel(r->mPixelValue, (uint8_t*)b, mCurrentPixelFormat);
        if (mIsCompressedRRE) {
            r->mX = Reader::readUInt8(b + (mCurrentPixelFormat.mBitsPerPixel / 8) + 0);
            r->mY = Reader::readUInt8(b + (mCurrentPixelFormat.mBitsPerPixel / 8) + 1);
            r->mW = Reader::readUInt8(b + (mCurrentPixelFormat.mBitsPerPixel / 8) + 2);
            r->mH = Reader::readUInt8(b + (mCurrentPixelFormat.mBitsPerPixel / 8) + 3);
        }
        else {
            r->mX = Reader::readUInt16(b + (mCurrentPixelFormat.mBitsPerPixel / 8) + 0);
            r->mY = Reader::readUInt16(b + (mCurrentPixelFormat.mBitsPerPixel / 8) + 2);
            r->mW = Reader::readUInt16(b + (mCurrentPixelFormat.mBitsPerPixel / 8) + 4);
            r->mH = Reader::readUInt16(b + (mCurrentPixelFormat.mBitsPerPixel / 8) + 6);
        }

        if ((uint32_t)r->mX + (uint32_t)r->mW > (uint32_t)mCurrentRect.mW ||
                (uint32_t)r->mY + (uint32_t)r->mH > (uint32_t)mCurrentRect.mH) {
            orv_error_set(error, ORV_ERR_PROTOCOL_ERROR, 0, "Error in RRE encoding: Subrect %d with bounds x=%d,y=%d,w=%d,h=%d exceeds bounds of full rectangle (x=%d,y=%d,w=%d,h=%d)", (int)mFinishedSubRectanglesCount, (int)r->mX, (int)r->mY, (int)r->mW, (int)r->mH, (int)mCurrentRect.mX, (int)mCurrentRect.mY, (int)mCurrentRect.mW, (int)mCurrentRect.mH);
            return 0;
        }

        //ORV_DEBUG(mContext, "RRE encoding: Received sub-rectangle number %d of %d: x=%d, y=%d, w=%d, h=%d", (int)mFinishedSubRectanglesCount, (int)mTotalSubRectanglesCount, (int)r->mX, (int)r->mY, (int)r->mW, (int)r->mH);
        consumed += bytesPerSubRect;
        mFinishedSubRectanglesCount++;
    }
    return consumed;
}

bool RectDataParserRRE::canFinishRect() const
{
    if (mHasRREHeader && (mFinishedSubRectanglesCount >= mTotalSubRectanglesCount)) {
        return true;
    }
    return false;
}

void RectDataParserRRE::finishRect(orv_error_t* error)
{
    std::unique_lock<std::mutex> lock(mFramebufferMutex);
    if (!checkRectParametersForFramebufferMutexLocked(error)) {
        return;
    }
    ORV_DEBUG(mContext, "Performing framebuffer update for RRE data");
    if (mFramebuffer.mBytesPerPixel != 3) {
        orv_error_set(error, ORV_ERR_GENERIC, 0, "Internal error: %s assumes the internal framebuffer uses 3 bytes per pixel, have %d", __func__, (int)mFramebuffer.mBytesPerPixel);
        return;
    }
    const int bytesPerPixel = 3; // SubRectangle uses 3 bytes per pixel. Framebuffer must do so as well.
    for (int rectY = 0; rectY < mCurrentRect.mH; rectY++) {
        const int dstY = mCurrentRect.mY + rectY;
        for (int rectX = 0; rectX < mCurrentRect.mW; rectX++) {
            const int dstX = mCurrentRect.mX + rectX;
            uint8_t* pDst = mFramebuffer.mFramebuffer + (dstY * mFramebuffer.mWidth + dstX) * mFramebuffer.mBytesPerPixel;
            for (int i = 0; i < bytesPerPixel; i++) {
                pDst[i] = mBackgroundPixelValue[i];
            }
        }
    }
    for (int subrectIndex = 0; subrectIndex < (int)mFinishedSubRectanglesCount; subrectIndex++) {
        const SubRectangle* subrect = mSubRectangles + subrectIndex;
        for (int rectY = 0; rectY < subrect->mH; rectY++) {
            const int dstY = mCurrentRect.mY + subrect->mY + rectY;
            for (int rectX = 0; rectX < subrect->mW; rectX++) {
                const int dstX = mCurrentRect.mX + subrect->mX + rectX;
                uint8_t* pDst = mFramebuffer.mFramebuffer + (dstY * mFramebuffer.mWidth + dstX) * mFramebuffer.mBytesPerPixel;
                for (int i = 0; i < bytesPerPixel; i++) {
                    pDst[i] = subrect->mPixelValue[i];
                }
            }
        }
    }
    //ORV_DEBUG(mContext, "Finished performing framebuffer update for RRE data");
}


RectDataParserHextile::RectDataParserHextile(struct orv_context_t* context, std::mutex* framebufferMutex, orv_framebuffer_t* framebuffer, const orv_communication_pixel_format_t* currentPixelFormat, const uint16_t* currentFramebufferWidth, const uint16_t* currentFramebufferHeight)
    : RectDataParserRealRectBase(context, framebufferMutex, framebuffer, currentPixelFormat, currentFramebufferWidth, currentFramebufferHeight)
{
}

RectDataParserHextile::~RectDataParserHextile()
{
    clear();
}

void RectDataParserHextile::reset()
{
    RectDataParserRealRectBase::reset();
    clear();
}

void RectDataParserHextile::clear()
{
    mIsInitialized = false;
    mCurrentTileIndex = 0;
    mExpectedTileColumns = 0;
    mExpectedTileRows = 0;
    mExpectedTotalTiles = 0;
    free(mCurrentRectData);
    mCurrentRectData = nullptr;
    mCurrentRectDataSize = 0;
    memset(mCurrentBackgroundColor, 0, mMaxBytesPerPixel);
    memset(mCurrentForegroundColor, 0, mMaxBytesPerPixel);
    clearCurrentTile();
}

void RectDataParserHextile::clearCurrentTile()
{
    mFinishedTile = false;
    mCurrentTileSubencodingMask = 0;
    mCurrentTileSubencodingMaskRead = false;
    mCurrentTileDidReadBackgroundColor = false;
    mCurrentTileDidReadForegroundColor = false;
    mCurrentTileDidReadAnySubrects = false;
    mCurrentTileSubrects = 0;
    mCurrentTileDataBytesRead = 0;
}

uint32_t RectDataParserHextile::readRectData(const char* buffer, uint32_t bufferSize, orv_error_t* error)
{
    if (mCurrentPixelFormat.mBitsPerPixel <= 0 || (mCurrentPixelFormat.mBitsPerPixel/8) > mMaxBytesPerPixel) {
        orv_error_set(error, ORV_ERR_GENERIC, 0, "Internal error: Invalid BitsPerPixel %d", (int)mCurrentPixelFormat.mBitsPerPixel);
        return 0;
    }
    uint32_t consumed = 0;
    if (!mIsInitialized) {
        // each tile has size 16x16 pixels, the last row and/or column may have less.
        const uint8_t maxTileWidth = 16;
        const uint8_t maxTileHeight = 16;
        mExpectedTileColumns = ((uint32_t)mCurrentRect.mW + (maxTileWidth - 1)) / maxTileWidth;
        mExpectedTileRows = ((uint32_t)mCurrentRect.mH + (maxTileHeight - 1)) / maxTileHeight;
        mExpectedTotalTiles = (uint32_t)mExpectedTileRows * (uint32_t)mExpectedTileColumns;
        mCurrentTileSubencodingMask = 0;
        mCurrentTileSubencodingMaskRead = false;
        mCurrentTileIndex = 0;
        mIsInitialized = true;
        if (mCurrentTileIndex >= mExpectedTotalTiles) {
            return 0;
        }
        uint32_t expectedTotalRectSize = 0;
        if (!calculateRectBufferSizeFor(&expectedTotalRectSize, mCurrentRect.mW, mCurrentRect.mH, mCurrentPixelFormat.mBitsPerPixel)) {
            // protocol error, server sent garbage.
            orv_error_set(error, ORV_ERR_PROTOCOL_ERROR, 0, "Server sent rect of size %dx%d with %d bytes per pixel in Hextile encoding. This is impossible, invalid data received.", (int)mCurrentRect.mW, (int)mCurrentRect.mH, (int)mCurrentPixelFormat.mBitsPerPixel/8);
            return 0;
        }
        free(mCurrentRectData);
        mCurrentRectData = (uint8_t*)malloc(std::max((uint32_t)1, expectedTotalRectSize));
        mCurrentRectDataSize = expectedTotalRectSize;
        //ORV_DEBUG(mContext, "Initialized reader for Hextile encoding. Expecting %dx%d tiles, currentRect: Width=%d,Height=%d", (int)mExpectedTileColumns, (int)mExpectedTileRows, (int)mCurrentRect.mW, (int)mCurrentRect.mH);
    }

    while (consumed < bufferSize && mCurrentTileIndex < mExpectedTotalTiles) {
        uint32_t c = readTileData(buffer + consumed, bufferSize - consumed, error);
        if (error->mHasError) {
            return 0;
        }
        if (c == 0) {
            return consumed;
        }
        consumed += c;
        if (mFinishedTile) {
            mCurrentTileIndex++;
            clearCurrentTile();
        }
    }
    return consumed;
}

/**
 * Helper function for @ref readRectData() that reads exactly one tile.
 *
 * When this function is called, the hextile encoding must have been initialized for the current
 * rect already, in particular the output data buffer must have been allocated.
 **/
uint32_t RectDataParserHextile::readTileData(const char* buffer, uint32_t bufferSize, orv_error_t* error)
{
    if (mCurrentTileIndex >= mExpectedTotalTiles) {
        orv_error_set(error, ORV_ERR_GENERIC, 0, "Internal error in Hextile encoding: readTileData() called, but have no tile to read");
        return 0;
    }
    uint32_t consumed = 0;
    if (!mCurrentTileSubencodingMaskRead) {
        if (bufferSize < 1) {
            return 0;
        }
        mCurrentTileSubencodingMask = Reader::readUInt8(buffer);
        mCurrentTileSubencodingMaskRead = true;
        consumed += 1;
        //ORV_DEBUG(mContext, "Started reading tile %d of %d, subencoding mask: 0x%02x", (int)mCurrentTileIndex+1, (int)mExpectedTotalTiles, (int)mCurrentTileSubencodingMask);
        const uint8_t allFlags = SubencodingFlagRaw | SubencodingFlagBackgroundSpecified | SubencodingFlagForegroundSpecified | SubencodingFlagAnySubrects | SubencodingFlagSubrectsColoured;
        if (mCurrentTileSubencodingMask & (~allFlags)) {
            ORV_WARNING(mContext, "Read SubencodingFlagRaw 0x%02x from server, out of which 0x%02x makes no sense to us. This may indicate that we read garbage from server! CurrentTile: %d out of %d (%dx%d)", (int)mCurrentTileSubencodingMask, (int)(mCurrentTileSubencodingMask & (~allFlags)), (int)mCurrentTileIndex+1, (int)mExpectedTotalTiles, (int)mExpectedTileColumns, (int)mExpectedTileRows);
        }
    }

    // NOTE: We read tile data into mCurrentTileDataBuffer first, so we can easily interrupt reading
    //       if bufferSize is insufficient.
    //       Once tile is fully read, we copy into mCurrentRectData.
    //       Once rect is being finished, we copy into the framebuffer (as usual with other encodings).

    if (mCurrentTileSubencodingMask & SubencodingFlagRaw) {
        if (consumed >= bufferSize) {
            // need more data
            return consumed;
        }
        // other flags in SubencodingMask have no meaning, tile is simply raw encoded.
        const uint8_t tileWidth = calculateTileWidth(mCurrentTileIndex, mExpectedTileColumns, mCurrentRect.mW);
        const uint8_t tileHeight = calculateTileHeight(mCurrentTileIndex, mExpectedTileColumns, mExpectedTileRows, mCurrentRect.mH);
        const uint32_t expectedBytes = tileWidth * tileHeight * (mCurrentPixelFormat.mBitsPerPixel / 8);
        if (mCurrentTileDataBytesRead < expectedBytes) {
            uint32_t readBytes = std::min(bufferSize - consumed, expectedBytes - mCurrentTileDataBytesRead);
            memcpy(mCurrentTileDataBuffer + mCurrentTileDataBytesRead, buffer + consumed, readBytes);
            consumed += readBytes;
            mCurrentTileDataBytesRead += readBytes;
        }
        if (mCurrentTileDataBytesRead >= expectedBytes) {
            //ORV_DEBUG(mContext, "Hextile: Finished reading tile %d in Raw encoding (%d bytes), tileWidth=%d,tileHeight=%d", (int)mCurrentTileIndex+1, (int)mCurrentTileDataBytesRead, (int)tileWidth, (int)tileHeight);
            // tile is fully ready, move from temporary tile buffer to rect buffer.
            const uint16_t tileXInRect = (mCurrentTileIndex % mExpectedTileColumns) * 16;
            const uint16_t tileYInRect = (mCurrentTileIndex / mExpectedTileColumns) * 16;
            const uint8_t bpp = mCurrentPixelFormat.mBitsPerPixel / 8;
            for (int y = 0; y < tileHeight; y++) {
                const uint8_t* srcLine = mCurrentTileDataBuffer + (y * tileWidth) * bpp;
                const uint16_t rectX = tileXInRect;
                const uint16_t rectY = tileYInRect + y;
                uint8_t* dstLine = mCurrentRectData + (rectY * mCurrentRect.mW + rectX) * bpp;
                memcpy(dstLine, srcLine, tileWidth * bpp);
            }
            mFinishedTile = true;
        }
    }
    else {
        if ((mCurrentTileSubencodingMask & SubencodingFlagBackgroundSpecified) && !mCurrentTileDidReadBackgroundColor) {
            const uint8_t bpp = mCurrentPixelFormat.mBitsPerPixel / 8;
            if (bufferSize < consumed + bpp) {
                // need more data
                return consumed;
            }
            for (uint8_t i = 0; i < bpp; i++) {
                mCurrentBackgroundColor[i] = buffer[consumed + i];
            }
            consumed += bpp;
            mCurrentTileDidReadBackgroundColor = true;
        }
        if ((mCurrentTileSubencodingMask & SubencodingFlagForegroundSpecified) && !mCurrentTileDidReadForegroundColor) {
            // NOTE: implies SubencodingFlagSubrectsColoured is NOT set
            const uint8_t bpp = mCurrentPixelFormat.mBitsPerPixel / 8;
            if (bufferSize < consumed + bpp) {
                // need more data
                return consumed;
            }
            for (uint8_t i = 0; i < bpp; i++) {
                mCurrentForegroundColor[i] = buffer[consumed + i];
            }
            consumed += bpp;
            mCurrentTileDidReadForegroundColor = true;
        }
        if ((mCurrentTileSubencodingMask & SubencodingFlagForegroundSpecified) && (mCurrentTileSubencodingMask & SubencodingFlagSubrectsColoured)) {
            orv_error_set(error, ORV_ERR_PROTOCOL_ERROR, 0, "Protocol error: Server sent Hextile tile with Foreground and SubrectsColoured flags set. This is invalid.");
            return 0;
        }
        if ((mCurrentTileSubencodingMask & SubencodingFlagAnySubrects) && !mCurrentTileDidReadAnySubrects) {
            if (bufferSize < consumed + 1) {
                // need more data
                return consumed;
            }
            mCurrentTileSubrects = Reader::readUInt8(buffer + consumed);
            consumed += 1;
            mCurrentTileDidReadAnySubrects = true;
            //ORV_DEBUG(mContext, " Hextile: Tile %d has %d subrects", (int)mCurrentTileIndex+1, (int)mCurrentTileSubrects);
        }

        uint32_t expectedBytesTileData = 0;
        // if SubencodingFlagAnySubrects is set, no further data is required. otherwise we need to
        // read subrects.
        if (mCurrentTileSubencodingMask & SubencodingFlagAnySubrects) {
            uint8_t bytesPerPixel = 2;
            if (mCurrentTileSubencodingMask & SubencodingFlagSubrectsColoured) {
                bytesPerPixel = (2 + mCurrentPixelFormat.mBitsPerPixel / 8);
            }
            expectedBytesTileData += mCurrentTileSubrects * bytesPerPixel;
        }

        if (mCurrentTileDataBytesRead < expectedBytesTileData) {
            if (consumed >= bufferSize) {
                // need more data
                return consumed;
            }
            uint32_t copy = std::min(bufferSize - consumed, expectedBytesTileData - mCurrentTileDataBytesRead);
            memcpy(mCurrentTileDataBuffer + mCurrentTileDataBytesRead, buffer + consumed, copy);
            consumed += copy;
            mCurrentTileDataBytesRead += copy;
        }

        if (mCurrentTileDataBytesRead >= expectedBytesTileData) {
            // tile fully read, copy data to mCurrentRectData.
            const uint16_t tileXInRect = (mCurrentTileIndex % mExpectedTileColumns) * 16;
            const uint16_t tileYInRect = (mCurrentTileIndex / mExpectedTileColumns) * 16;
            const uint8_t tileWidth = calculateTileWidth(mCurrentTileIndex, mExpectedTileColumns, mCurrentRect.mW);
            const uint8_t tileHeight = calculateTileHeight(mCurrentTileIndex, mExpectedTileColumns, mExpectedTileRows, mCurrentRect.mH);
            RectDataParserRealRectBase::fillSubrectInRect(mCurrentRectData, mCurrentRect.mW,
                    tileXInRect, tileYInRect, tileWidth, tileHeight,
                    mCurrentBackgroundColor, mCurrentPixelFormat.mBitsPerPixel/8);
            int subrects = 0;
            if (mCurrentTileSubencodingMask & SubencodingFlagAnySubrects) {
                subrects = mCurrentTileSubrects;
            }
            const uint8_t bpp = mCurrentPixelFormat.mBitsPerPixel / 8;
            uint8_t colorBuffer[mMaxBytesPerPixel] = {};
            const uint8_t* color = colorBuffer;
            if (!(mCurrentTileSubencodingMask & SubencodingFlagSubrectsColoured)) {
                color = mCurrentForegroundColor;
            }
            uint32_t dataBufferPos = 0;
            for (int subrect = 0; subrect < subrects; subrect++) {
                if (mCurrentTileSubencodingMask & SubencodingFlagSubrectsColoured) {
                    for (int i = 0; i < bpp; i++) {
                        colorBuffer[i] = Reader::readUInt8((char*)mCurrentTileDataBuffer + dataBufferPos + i);
                    }
                    dataBufferPos += bpp;
                }
                const uint8_t x_y = Reader::readUInt8((char*)mCurrentTileDataBuffer + dataBufferPos + 0);
                const uint8_t w_h = Reader::readUInt8((char*)mCurrentTileDataBuffer + dataBufferPos + 1);
                dataBufferPos += 2;
                const uint8_t subrectX = (x_y >> 4) & 0x0F;
                const uint8_t subrectY = x_y & 0x0F;
                const uint8_t subrectWidth = ((w_h >> 4) & 0x0F) + 1;
                const uint8_t subrectHeight = (w_h & 0x0F) + 1;
                if (((uint32_t)tileXInRect) + subrectX + subrectWidth > mCurrentRect.mW || ((uint32_t)tileYInRect) + subrectY + subrectHeight > mCurrentRect.mH ) {
                    orv_error_set(error, ORV_ERR_PROTOCOL_ERROR, 0, "Error in Hextile encoding: Subrect %d of tile %d is out of bounds: x=%d,y=%d,w=%d,h=%d for rect w=%d,h=%d, tileXInRect=%d, tileYInRect=%d, tileWidth=%d, tileHeight=%d",
                            (int)subrect, (int)mCurrentTileIndex,
                            (int)subrectX, (int)subrectY, (int)subrectWidth, (int)subrectHeight,
                            (int)mCurrentRect.mW, (int)mCurrentRect.mH,
                            (int)tileXInRect, (int)tileYInRect, (int)tileWidth, (int)tileHeight);
                    return 0;
                }
                const uint16_t subrectXInRect = tileXInRect + subrectX;
                const uint16_t subrectYInRect = tileYInRect + subrectY;
                RectDataParserRealRectBase::fillSubrectInRect(mCurrentRectData, mCurrentRect.mW,
                        subrectXInRect, subrectYInRect, subrectWidth, subrectHeight,
                        color, bpp);
            }
            //ORV_DEBUG(mContext, "Hextile: Finished reading tile %d in non-Raw encoding (%d data bytes), tileXInRect=%d, tileYInRect=%d, tileWidth=%d, tileHeight=%d", (int)mCurrentTileIndex+1, (int)mCurrentTileDataBytesRead, (int)tileXInRect, (int)tileYInRect, (int)tileWidth, (int)tileHeight);
            mFinishedTile = true;
        }
    }
    return consumed;
}

bool RectDataParserHextile::canFinishRect() const
{
    if (mCurrentTileIndex >= mExpectedTotalTiles) {
        return true;
    }
    return false;
}

void RectDataParserHextile::finishRect(orv_error_t* error)
{
    std::unique_lock<std::mutex> lock(mFramebufferMutex);
    if (!checkRectParametersForFramebufferMutexLocked(error)) {
        return;
    }
    ORV_DEBUG(mContext, "Performing framebuffer update for Hextile data");
    if (mFramebuffer.mBytesPerPixel != 3) {
        orv_error_set(error, ORV_ERR_GENERIC, 0, "Internal error: %s assumes the internal framebuffer uses 3 bytes per pixel, have %d", __func__, (int)mFramebuffer.mBytesPerPixel);
        return;
    }
    const uint8_t remoteBpp = mCurrentPixelFormat.mBitsPerPixel / 8;
    for (int rectY = 0; rectY < mCurrentRect.mH; rectY++) {
        const int dstY = mCurrentRect.mY + rectY;
        const uint8_t* srcRectLine = mCurrentRectData + rectY * mCurrentRect.mW * remoteBpp;
        for (int rectX = 0; rectX < mCurrentRect.mW; rectX++) {
            const int dstX = mCurrentRect.mX + rectX;
            uint8_t* pDst = mFramebuffer.mFramebuffer + (dstY * mFramebuffer.mWidth + dstX) * mFramebuffer.mBytesPerPixel;
            const uint8_t* pSrc = srcRectLine + rectX * remoteBpp;
            Reader::readPixel(pDst, pSrc, mCurrentPixelFormat);
        }
    }
}




RectDataParserCursor::RectDataParserCursor(struct orv_context_t* context, std::mutex* cursorMutex, orv_cursor_t* cursorData, const orv_communication_pixel_format_t* currentPixelFormat, const uint16_t* currentFramebufferWidth, const uint16_t* currentFramebufferHeight)
    : RectDataParserBase(context, currentPixelFormat, currentFramebufferWidth, currentFramebufferHeight),
      mCursorMutex(*cursorMutex),
      mCursorData(*cursorData)
{
}

RectDataParserCursor::~RectDataParserCursor()
{
    clear();
}

uint32_t RectDataParserCursor::readRectData(const char* buffer, uint32_t bufferSize, orv_error_t* error)
{
    if (!mIsInitialized) {
        // multiplication of 2 uint16_t and a uint8_t may be up to 40 bits, so a uint64_t can always hold it
        uint64_t expectedCursorBytesTmp = ((uint64_t)mCurrentRect.mW) * ((uint64_t)mCurrentRect.mH) * ((uint64_t)(mCurrentPixelFormat.mBitsPerPixel / 8));
        if (expectedCursorBytesTmp > 0xffffffff) {
            // result exceeds 32 bit. protocol error, server sent garbage.
            orv_error_set(error, ORV_ERR_PROTOCOL_ERROR, 0, "Server sent rect of size %dx%d with %d bytes per pixel in Cursor encoding, which exceeds 32 bit size. This is impossible, invalid data received.", (int)mCurrentRect.mW, (int)mCurrentRect.mH, (int)mCurrentPixelFormat.mBitsPerPixel/8);
            return 0;
        }
        mExpectedCursorBytes = (uint32_t)expectedCursorBytesTmp;
        free(mCursor);
        mCursor = (uint8_t*)malloc(std::max((size_t)mExpectedCursorBytes, (size_t)1));
        mCursorBytesRead = 0;

        // multiplication of 2 uint16_t always fit into a 32 bit uint.
        mExpectedCursorBitmaskBytes = ((((uint32_t)mCurrentRect.mW) + 7) / 8) * ((uint32_t)mCurrentRect.mH);
        free(mCursorMask);
        mCursorMask = (uint8_t*)malloc(std::max((size_t)mExpectedCursorBitmaskBytes, (size_t)1));
        mCursorBitmaskBytesRead = 0;

        mIsInitialized = true;

        if (mCurrentRect.mW == 0 && mCurrentRect.mH == 0) {
            // server sent 0x0 rect. finished reading.
            ORV_DEBUG(mContext, "Server sent empty rect in Cursor pseudo-encoding. Not reading any data.");
            return 0;
        }
    }
    if (mCursorBytesRead >= mExpectedCursorBytes && mCursorBitmaskBytesRead >= mExpectedCursorBitmaskBytes) {
        orv_error_set(error, ORV_ERR_GENERIC, 0, "Internal error in Cursor pseudo-encoding: Data already fully read, but attempted to read more.");
        return 0;
    }
    uint32_t readCursor = (mExpectedCursorBytes - mCursorBytesRead);
    readCursor = std::min(readCursor, bufferSize);
    if (readCursor > 0) {
        memcpy(mCursor + mCursorBytesRead, buffer, readCursor);
        mCursorBytesRead += readCursor;
    }
    uint32_t readCursorBitmask = (mExpectedCursorBitmaskBytes - mCursorBitmaskBytesRead);
    readCursorBitmask = std::min(readCursorBitmask, (bufferSize - readCursor));
    if (readCursorBitmask > 0) {
        memcpy(mCursorMask + mCursorBitmaskBytesRead, buffer + readCursor, readCursorBitmask);
        mCursorBitmaskBytesRead += readCursorBitmask;
    }
    return readCursor + readCursorBitmask;
}

void RectDataParserCursor::finishRect(orv_error_t* error)
{
    std::unique_lock<std::mutex> lock(mCursorMutex);
    if (!mIsInitialized || mCursorBytesRead < mExpectedCursorBytes || mCursorBitmaskBytesRead < mExpectedCursorBitmaskBytes) {
        orv_error_set(error, ORV_ERR_GENERIC, 0, "Internal error: Tried to finish Cursor pseudo-rect although data is not fully read");
        return;
    }
    const uint32_t bytesPerPixel = 4; // we always use RGBA data
    const uint64_t cursorSizeTmp = (uint64_t)mCurrentRect.mW * (uint64_t)mCurrentRect.mH * (uint64_t)bytesPerPixel;
    if (cursorSizeTmp > 0xffffffff) {
        orv_error_set(error, ORV_ERR_GENERIC, 0, "Cursor size %dx%d with %d bytes per pixel exceeds valid 32 bit size. This is impossible, invalid data received.", (int)mCurrentRect.mW, (int)mCurrentRect.mH, (int)bytesPerPixel);
        return;
    }
    ORV_DEBUG(mContext, "Performing update for Cursor data");
    const uint32_t cursorSize = (uint32_t)cursorSizeTmp;
    const uint32_t minCursorCapacity = std::max((uint32_t)cursorSize, (uint32_t)1);
    mCursorData.mIsValid = false; // set to true on success
    mCursorData.mHotspotX = mCurrentRect.mX;
    mCursorData.mHotspotY = mCurrentRect.mY;
    mCursorData.mWidth = mCurrentRect.mW;
    mCursorData.mHeight = mCurrentRect.mH;
    mCursorData.mBytesPerPixel = bytesPerPixel;
    mCursorData.mBitsPerPixel = bytesPerPixel * 8;
    mCursorData.mCursorSize = cursorSize;
    if (minCursorCapacity > mCursorData.mCursorCapacity || !mCursorData.mCursor) {
        free(mCursorData.mCursor);
        mCursorData.mCursor = (uint8_t*)malloc(minCursorCapacity);
        mCursorData.mCursorCapacity = minCursorCapacity;
    }
    int remoteBpp = mCurrentPixelFormat.mBitsPerPixel / 8;
    if (mCurrentPixelFormat.mBitsPerPixel == 8) {
        for (int srcY = 0; srcY < mCurrentRect.mH; srcY++) {
            const int dstY = srcY;
            for (int srcX = 0; srcX < mCurrentRect.mW; srcX++) {
                const int dstX = srcX;
                const uint8_t* pSrc = mCursor + (srcY * mCurrentRect.mW + srcX) * remoteBpp;
                uint8_t* pDst = mCursorData.mCursor + (dstY * mCursorData.mWidth + dstX) * mCursorData.mBytesPerPixel;
                Reader::readPixel8Bit(pDst, pSrc, mCurrentPixelFormat);
            }
        }
    }
    else if (mCurrentPixelFormat.mBitsPerPixel == 16) {
        for (int srcY = 0; srcY < mCurrentRect.mH; srcY++) {
            const int dstY = srcY;
            for (int srcX = 0; srcX < mCurrentRect.mW; srcX++) {
                const int dstX = srcX;
                const uint8_t* pSrc = mCursor + (srcY * mCurrentRect.mW + srcX) * remoteBpp;
                uint8_t* pDst = mCursorData.mCursor + (dstY * mCursorData.mWidth + dstX) * mCursorData.mBytesPerPixel;
                Reader::readPixel16Bit(pDst, pSrc, mCurrentPixelFormat);
            }
        }
    }
    else if (mCurrentPixelFormat.mBitsPerPixel == 32) {
        for (int srcY = 0; srcY < mCurrentRect.mH; srcY++) {
            const int dstY = srcY;
            for (int srcX = 0; srcX < mCurrentRect.mW; srcX++) {
                const int dstX = srcX;
                const uint8_t* pSrc = mCursor + (srcY * mCurrentRect.mW + srcX) * remoteBpp;
                uint8_t* pDst = mCursorData.mCursor + (dstY * mCursorData.mWidth + dstX) * mCursorData.mBytesPerPixel;
                Reader::readPixel32Bit(pDst, pSrc, mCurrentPixelFormat);
            }
        }
    }
    else {
        orv_error_set(error, ORV_ERR_GENERIC, 0, "Invalid value for BitsPerPixel: %d", (int)mCurrentPixelFormat.mBitsPerPixel);
        return;
    }
    const uint32_t lineWidth = (mCursorData.mWidth + 7) / 8;
    for (int y = 0; y < mCursorData.mHeight; y++) {
        const uint8_t* bitLine = mCursorMask + y * lineWidth;
        for (int x = 0; x < mCursorData.mWidth; x++) {
            const uint8_t byte = bitLine[x / 8];
            const uint8_t bitIndex = 8 - (x%8) - 1;
            if (byte & (0x1 << bitIndex)) {
                mCursorData.mCursor[(y * mCursorData.mWidth + x) * mCursorData.mBytesPerPixel + 3] = 255;
            }
            else {
                mCursorData.mCursor[(y * mCursorData.mWidth + x) * mCursorData.mBytesPerPixel + 3] = 0;
            }
        }
    }

    if (mCursorData.mWidth > 0 && mCursorData.mHeight > 0) {
        mCursorData.mIsValid = true;
    }
}

bool RectDataParserCursor::canFinishRect() const
{
    if (!mIsInitialized) {
        return false;
    }
    if (mCursorBytesRead < mExpectedCursorBytes) {
        return false;
    }
    if (mCursorBitmaskBytesRead < mExpectedCursorBitmaskBytes) {
        return false;
    }
    return true;
}

void RectDataParserCursor::reset()
{
    RectDataParserBase::reset();
    clear();
}

void RectDataParserCursor::clear()
{
    free(mCursor);
    mCursor = nullptr;
    free(mCursorMask);
    mCursorMask = nullptr;
    mExpectedCursorBytes = 0;
    mExpectedCursorBitmaskBytes = 0;
    mCursorBytesRead = 0;
    mCursorBitmaskBytesRead = 0;
    mIsInitialized = false;
}

RectDataParserZlibPlain::RectDataParserZlibPlain(struct orv_context_t* context, std::mutex* framebufferMutex, orv_framebuffer_t* framebuffer, const orv_communication_pixel_format_t* currentPixelFormat, const uint16_t* currentFramebufferWidth, const uint16_t* currentFramebufferHeight, const char* owningEncodingString)
    : RectDataParserRealRectBase(context, framebufferMutex, framebuffer, currentPixelFormat, currentFramebufferWidth, currentFramebufferHeight)
{
    mOwningEncodingString = strdup(owningEncodingString);
}

RectDataParserZlibPlain::~RectDataParserZlibPlain()
{
    clear();
    // NOTE: remove zlib stream here, not in clear(), because stream persists over messages
    destroyZStream();
    free(mOwningEncodingString);
}

void RectDataParserZlibPlain::reset()
{
    RectDataParserRealRectBase::reset();
    clear();
}

void RectDataParserZlibPlain::resetConnection()
{
    RectDataParserRealRectBase::resetConnection();
    clear();
    // NOTE: remove zlib stream here, not in clear(), because stream persists over messages
    destroyZStream();
}

/**
 * Free the zlib resources. This should be called when the connection is closed only, as the stream
 * persists over messages of the same connection.
 **/
void RectDataParserZlibPlain::destroyZStream()
{
    if (mZStream) {
        inflateEnd(mZStream);
        free(mZStream);
        mZStream = nullptr;
    }
}

uint32_t RectDataParserZlibPlain::readRectData(const char* buffer, uint32_t bufferSize, orv_error_t* error)
{
    uint32_t consumed = 0;
    if (!mHasZlibHeader) {
        if (bufferSize < 4) {
            return 0;
        }
        mExpectedCompressedDataLength = Reader::readUInt32(buffer);
        mCompressedDataReceived = 0;
        mCompressedDataUncompressedLength = 0;
        consumed = 4;
        if (mExpectedCompressedDataLength > ORV_MAX_COMPRESSED_RECT_BUFFER_SIZE) {
            orv_error_set(error, ORV_ERR_PROTOCOL_ERROR, 0, "Server tried to allocate %u bytes for zlib data in encoding '%s', which exceeds valid size. Refusing to do so.", (uint32_t)mExpectedCompressedDataLength, mOwningEncodingString);
            return 0;
        }
        mCompressedData = (uint8_t*)malloc(std::max((size_t)1, (size_t)mExpectedCompressedDataLength));
        mHasZlibHeader = true;
        if (mExpectedCompressedDataLength == 0) {
            return consumed;
        }
    }

    if (!mZStream) {
        mZStream = (struct ::z_stream_s*)malloc(sizeof(struct ::z_stream_s));
        memset(mZStream, 0, sizeof(struct ::z_stream_s));
        mZStream->zalloc = Z_NULL;
        mZStream->zfree = Z_NULL;
        mZStream->opaque = Z_NULL;
        mZStream->avail_in = 0;
        mZStream->next_in = Z_NULL;
        int ret = inflateInit(mZStream);
        if (ret != Z_OK) {
            orv_error_set(error, ORV_ERR_GENERIC, 0, "Internal error: Failed to initialize zlib stream for encoding '%s'. Zlib error code: %d", mOwningEncodingString, (int)ret);
            return 0;
        }
    }

    if (mCompressedDataReceived >= mExpectedCompressedDataLength) {
        orv_error_set(error, ORV_ERR_GENERIC, 0, "Internal error in zlib reader for encoding '%s': Data already fully read, but attempted to read more.", mOwningEncodingString);
        return 0;
    }
    if (consumed >= bufferSize) {
        return consumed;
    }
    size_t readBytes = std::min(bufferSize - consumed, mExpectedCompressedDataLength - mCompressedDataReceived);
    memcpy(mCompressedData + mCompressedDataReceived, buffer + consumed, readBytes);
    mCompressedDataReceived += readBytes;
    consumed += readBytes;

    return consumed;
}

void RectDataParserZlibPlain::clear()
{
    mExpectedCompressedDataLength = 0;
    mCompressedDataReceived = 0;
    free(mCompressedData);
    mCompressedData = nullptr;
    mCompressedDataUncompressedLength = 0;
    mHasZlibHeader = false;

    // NOTE: do NOT reset the zlib stream, the same stream is re-used in the connection!
}

// unused
bool RectDataParserZlibPlain::canFinishRect() const
{
    return hasAllCompressedData();
}

// unused
void RectDataParserZlibPlain::finishRect(orv_error_t* error)
{
    UNUSED(error);
}

/**
 * @return The total number of (compressed) bytes that are currently being read by this object.
 *         Always 0 if the size header was not yet read.
 **/
uint32_t RectDataParserZlibPlain::totalExpectedCompressedBytes() const
{
    if (mHasZlibHeader) {
        return mExpectedCompressedDataLength;
    }
    return 0;
}

/**
 * @return TRUE if all compressed data has been received and is ready to uncompress, otherwise
 *         FALSE.
 *         Note that partial uncompressing may still be possible if this function returns FALSE.
 **/
bool RectDataParserZlibPlain::hasAllCompressedData() const
{
    if (mHasZlibHeader && mCompressedDataReceived >= mExpectedCompressedDataLength) {
        return true;
    }
    return false;
}

/**
 * @return TRUE if this object has data available that can be uncompressed, otherwise FALSE.
 *         This function merely checks if at least some bytes available that are not yet
 *         uncompressed, however zlib may still require more data for successful uncompressing,
 *         i.e. uncompress may still fail.
 **/
bool RectDataParserZlibPlain::hasUncompressibleData() const
{
    if (mHasZlibHeader) {
        if (mCompressedDataUncompressedLength < mCompressedDataReceived) {
            return true;
        }
        return false;
    }
    return false;
}

/**
 * @return The number of @em compressed bytes that remain in the current communication. Note that
 *         this value may be larger than 0 even if @p bufferSize is large enough, if not all data
 *         was received from remote yet. If @p bufferSize is large enough and @ref
 *         hasAllCompressedData() is TRUE, this function always returns 0.
 *         This function always returns 0 on error, the caller @em must check @p error.
 **/
uint32_t RectDataParserZlibPlain::uncompressTo(uint8_t* buffer, uint32_t bufferSize, uint32_t* uncompressedSize, orv_error_t* error)
{
    *uncompressedSize = 0;
    if (!mHasZlibHeader) {
        return 0;
    }
    if (mCompressedDataReceived < mCompressedDataUncompressedLength) {
        orv_error_set(error, ORV_ERR_GENERIC, 0, "Internal error: Invalid uncompressed data length %u, only %u bytes received so far", (unsigned int)mCompressedDataUncompressedLength, (unsigned int)mCompressedDataReceived);
        return 0;
    }
    mZStream->avail_in = mCompressedDataReceived - mCompressedDataUncompressedLength;
    mZStream->next_in = mCompressedData + mCompressedDataUncompressedLength;
    mZStream->avail_out = bufferSize;
    mZStream->next_out = buffer;

    int ret = inflate(mZStream, Z_NO_FLUSH);
    switch (ret) {
        case Z_STREAM_ERROR:
            orv_error_set(error, ORV_ERR_GENERIC, 0, "Zlib returned Z_STREAM_ERROR while decompressing rectangle for encoding '%s'.", mOwningEncodingString);
            return 0;
        case Z_DATA_ERROR:
            orv_error_set(error, ORV_ERR_GENERIC, 0, "Zlib returned Z_DATA_ERROR while decompressing rectangle for encoding '%s'.", mOwningEncodingString);
            return 0;
        case Z_MEM_ERROR:
            orv_error_set(error, ORV_ERR_GENERIC, 0, "Zlib returned Z_MEM_ERROR while decompressing rectangle for encoding '%s'.", mOwningEncodingString);
            return 0;
        default:
            orv_error_set(error, ORV_ERR_GENERIC, 0, "Zlib returned %d while decompressing rectangle for encoding '%s'.", ret, mOwningEncodingString);
            return 0;
        case Z_BUF_ERROR:
            // insufficient data in input buffer or output buffer too small. NOTE: non-fatal error, simply call again.
            *uncompressedSize = 0;
            return mExpectedCompressedDataLength - mCompressedDataUncompressedLength;
        case Z_STREAM_END:
        case Z_OK:
        {
            // at least some data was processed.
            uint32_t processedData = (mCompressedDataReceived - mCompressedDataUncompressedLength) - mZStream->avail_in;
            mCompressedDataUncompressedLength += processedData;
            *uncompressedSize = (bufferSize - mZStream->avail_out);
            return mExpectedCompressedDataLength - mCompressedDataUncompressedLength;
        }
    }
    // should never be reached
    orv_error_set(error, ORV_ERR_GENERIC, 0, "Internal error: inflate ret=%d", (int)ret);
    return 0;
}


RectDataParserZlib::RectDataParserZlib(struct orv_context_t* context, std::mutex* framebufferMutex, orv_framebuffer_t* framebuffer, const orv_communication_pixel_format_t* currentPixelFormat, const uint16_t* currentFramebufferWidth, const uint16_t* currentFramebufferHeight)
    : RectDataParserRaw(context, framebufferMutex, framebuffer, currentPixelFormat, currentFramebufferWidth, currentFramebufferHeight),
      mZlibPlainParser(context, framebufferMutex, framebuffer, currentPixelFormat, currentFramebufferWidth, currentFramebufferHeight, "Zlib")
{
}

RectDataParserZlib::~RectDataParserZlib()
{
    clear();
}

void RectDataParserZlib::resetConnection()
{
    RectDataParserRaw::resetConnection();
    clear();
    mZlibPlainParser.resetConnection();
}

uint32_t RectDataParserZlib::readRectData(const char* buffer, uint32_t bufferSize, orv_error_t* error)
{
    uint32_t consumed = mZlibPlainParser.readRectData(buffer, bufferSize, error);
    if (consumed == 0 || error->mHasError) {
        return 0;
    }

    if (mUncompressedDataSize == 0 || !mUncompressedData) {
        if (!calculateRectBufferSizeFor(&mUncompressedDataSize, mCurrentRect.mW, mCurrentRect.mH, mCurrentPixelFormat.mBitsPerPixel)) {
            // protocol error, server sent garbage.
            orv_error_set(error, ORV_ERR_PROTOCOL_ERROR, 0, "Server sent rect of size %dx%d with %d bytes per pixel in Zlib encoding, which exceeds 32 bit size. This is impossible, invalid data received.", (int)mCurrentRect.mW, (int)mCurrentRect.mH, (int)mCurrentPixelFormat.mBitsPerPixel/8);
            return 0;
        }
        free(mUncompressedData);
        mUncompressedData = nullptr;
        mUncompressedDataOffset = 0;
        if (mUncompressedDataSize == 0) {
            orv_error_set(error, ORV_ERR_GENERIC, 0, "Internal error: Invalid uncompressed data size 0");
            return 0;
        }
        mUncompressedData = (uint8_t*)malloc(mUncompressedDataSize);
    }

    if (!mZlibPlainParser.hasUncompressibleData()) {
        return consumed;
    }
    uint32_t uncompressedBytes = 0;
    uint32_t remainingBytes = mZlibPlainParser.uncompressTo(mUncompressedData + mUncompressedDataOffset, mUncompressedDataSize - mUncompressedDataOffset, &uncompressedBytes, error);
    if (error->mHasError) {
        return 0;
    }
    mUncompressedDataOffset += uncompressedBytes;

    // TODO: optimize:
    // after each uncompress, we can deliver (partial) data to RectDataParserRaw::readData()
    // and generate partial rect events (for partial updates).
    // -> note that this optimization makes little sense atm, as we do not yet support partial
    //    updates anyway, so we can just as well uncompress in one go.
    //    most of the time this is good enough anyway - but on slow connections partial updates
    //    would be useful.
    if (!mZlibPlainParser.hasAllCompressedData()) {
        return consumed;
    }
    if (remainingBytes > 0) {
        orv_error_set(error, ORV_ERR_PROTOCOL_ERROR, 0, "Failed to uncompress data in zlib encoding, have %d uncompressed bytes left", (int)uncompressedBytes);
        return 0;
    }
    if (mUncompressedDataOffset != mUncompressedDataSize) {
        orv_error_set(error, ORV_ERR_GENERIC, 0, "Unexpected size of uncompressed data in zlib encoding: Have %u bytes, expected %u", (unsigned int)mUncompressedDataOffset, (unsigned int)mUncompressedDataSize);
        return 0;
    }

    uint32_t readBytesRaw = RectDataParserRaw::readRectData((char*)mUncompressedData, mUncompressedDataSize, error);
    if (error->mHasError) {
        return 0;
    }
    if (readBytesRaw != mUncompressedDataSize) {
        orv_error_set(error, ORV_ERR_GENERIC, 0, "Internal error: Uncompressed %u bytes to %u bytes, but underlying raw encoding read %u bytes.", (unsigned int)mZlibPlainParser.totalExpectedCompressedBytes(), (unsigned int)mUncompressedDataSize, (unsigned int)readBytesRaw);
        return 0;
    }

    return consumed;
}

void RectDataParserZlib::clear()
{
    free(mUncompressedData);
    mUncompressedData = nullptr;
    mUncompressedDataSize = 0;
    mUncompressedDataOffset = 0;
}

void RectDataParserZlib::reset()
{
    RectDataParserRaw::reset();
    clear();
    mZlibPlainParser.reset();
}


RectDataParserZRLE::RectDataParserZRLE(struct orv_context_t* context, std::mutex* framebufferMutex, orv_framebuffer_t* framebuffer, const orv_communication_pixel_format_t* currentPixelFormat, const uint16_t* currentFramebufferWidth, const uint16_t* currentFramebufferHeight)
    : RectDataParserRealRectBase(context, framebufferMutex, framebuffer, currentPixelFormat, currentFramebufferWidth, currentFramebufferHeight),
      mZlibPlainParser(context, framebufferMutex, framebuffer, currentPixelFormat, currentFramebufferWidth, currentFramebufferHeight, "ZRLE")
{
}

RectDataParserZRLE::~RectDataParserZRLE()
{
    clear();
    free(mCurrentTileDataBuffer);
}

/**
 * @return The maximum number of bytes a single tile in ZRLE encoding may require (in uncompressed
 *         data).
 *         This can be used to allocate the buffer of uncompressed data.
 **/
uint32_t RectDataParserZRLE::maxBytesPerZRLETile()
{
    // the maximum of ZRLE encoding is the maximum of each subencoding plus the tileHeader.
    constexpr const uint8_t tileHeader = 1; // subencoding type
    constexpr const uint8_t maxBpp = 4;

    // subencoding 0: Raw bytes
    constexpr const uint32_t maxRawBytes = mMaxTileWidth * mMaxTileHeight * maxBpp;

    // subencoding 1: Solid tile
    constexpr const uint32_t maxSolidBytes = maxBpp;

    // subencoding 2..16: Packed palette types
    // -> this is the palette followed by an index into the palette for every pixel in the tile.
    //    for palette size 5..16 the index is 4 bit long, i.e. we have paletteSize*bpp palette data
    //    plus w*h/2 pixel data
    constexpr const uint32_t packedPaletteTypesMaxPaletteSize = 16;
    constexpr const uint32_t maxPackedPixelSize = ((mMaxTileWidth + 1) / 2) * mMaxTileHeight;
    constexpr const uint32_t maxPackedPaletteTypes = packedPaletteTypesMaxPaletteSize * maxBpp + maxPackedPixelSize;

    // subencoding 17..127: Unused
    // subencoding 128: Plain RLE
    // -> worst case: each pixel is encoded by a separate run, i.e. each pixel takes maxBpp plus 1
    //    byte for the length of the run.
    constexpr const uint32_t maxRLESize = mMaxTileWidth * mMaxTileHeight * (maxBpp + 1);

    // subencoding 129: unused
    // subencoding 130..255: Palette RLE
    // -> worst case: paletteSize=(255-128)*4 bytes, plus for *every* pixel 1 byte paletteIndex and
    //    run-length encoding (one byte as well) <-- stupid (not using run-length encoding would be
    //    equivalent but shorter in size), but allowed in the protocol.
    constexpr const uint8_t paletteRLEBytesPerPixel = 2; // palette index plus one byte for the length of the run
    constexpr const uint32_t paletteRLEMaxPaletteSize = 255 - 128; // palette size defined as value subencodingType-128
    constexpr const uint32_t maxPaletteRLESize = paletteRLEMaxPaletteSize * maxBpp + mMaxTileWidth * mMaxTileHeight * paletteRLEBytesPerPixel;

    return tileHeader + std::max({maxRawBytes, maxSolidBytes, maxPackedPaletteTypes, maxRLESize, maxPaletteRLESize});
}

void RectDataParserZRLE::resetConnection()
{
    RectDataParserRealRectBase::resetConnection();
    mZlibPlainParser.resetConnection();
    clear();
}

void RectDataParserZRLE::reset()
{
    RectDataParserRealRectBase::reset();
    mZlibPlainParser.reset();
    clear();
}

void RectDataParserZRLE::clear()
{
    free(mUncompressedData);
    mUncompressedData = nullptr;
    mUncompressedDataMaxSize = 0;
    mUncompressedDataOffset = 0;
    mUncompressedConsumedOffset = 0;
    mZrleBytesPerPixel = 0;
    mZrleByteOffsetOfUncompressedPixel = 0;
    mCurrentTileIndex = 0;
    mExpectedTileRows = 0;
    mExpectedTileColumns = 0;
    mExpectedTotalTiles = 0;
    free(mCurrentRectData);
    mCurrentRectData = nullptr;
    mCurrentRectDataSize = 0;
    clearCurrentTile();
}

void RectDataParserZRLE::clearCurrentTile()
{
    mFinishedTile = false;
    mCurrentTileSubencodingType = 0;
    mCurrentTileSubencodingTypeRead = false;
    mCurrentTileRLEPixelsDone = 0;
    mCurrentTileDataBytesRead = 0;
}


uint32_t RectDataParserZRLE::readRectData(const char* buffer, uint32_t bufferSize, orv_error_t* error)
{
    if (!mCurrentTileDataBuffer) {
        mCurrentTileDataBuffer = (uint8_t*)malloc(maxBytesPerZRLETile());
    }
    uint32_t consumed = mZlibPlainParser.readRectData(buffer, bufferSize, error);
    if (consumed == 0 || error->mHasError) {
        return 0;
    }

    if (mUncompressedDataMaxSize == 0 || !mUncompressedData) {
        bool omitLeastSignificantByte = false;
        mZrleBytesPerPixel = calculateZrleBytesPerPixel(mCurrentPixelFormat, &omitLeastSignificantByte);
#warning FIXME: omitLeastSignificantByte=true is untested
        mZrleByteOffsetOfUncompressedPixel = omitLeastSignificantByte ? 1 : 0;
        // each tile has size 64x64 pixels, the last row and/or column may have less.
        mExpectedTileColumns = ((uint32_t)mCurrentRect.mW + (mMaxTileWidth - 1)) / mMaxTileWidth;
        mExpectedTileRows = ((uint32_t)mCurrentRect.mH + (mMaxTileHeight - 1)) / mMaxTileHeight;
        mExpectedTotalTiles = (uint32_t)mExpectedTileRows * (uint32_t)mExpectedTileColumns;
        mCurrentTileSubencodingType = 0;
        mCurrentTileSubencodingTypeRead = false;
        mCurrentTileIndex = 0;
        if (mCurrentTileIndex >= mExpectedTotalTiles) {
            return 0;
        }
        if (!calculateMaxUncompressedDataSize(&mUncompressedDataMaxSize, mExpectedTotalTiles, mZrleBytesPerPixel)) {
            // protocol error, server sent garbage.
            orv_error_set(error, ORV_ERR_PROTOCOL_ERROR, 0, "Unable to calculate output buffer size for current rect in ZRLE encoding, server probably sent invalid data");
            return 0;
        }
        uint32_t expectedTotalRectSize = 0;
        if (!calculateRectBufferSizeFor(&expectedTotalRectSize, mCurrentRect.mW, mCurrentRect.mH, mCurrentPixelFormat.mBitsPerPixel)) {
            // protocol error, server sent garbage.
            orv_error_set(error, ORV_ERR_PROTOCOL_ERROR, 0, "Server sent rect of size %dx%d with %d bytes per pixel in Hextile encoding. This is impossible, invalid data received.", (int)mCurrentRect.mW, (int)mCurrentRect.mH, (int)mCurrentPixelFormat.mBitsPerPixel/8);
            return 0;
        }
        free(mUncompressedData);
        mUncompressedData = nullptr;
        mUncompressedDataOffset = 0;
        mUncompressedConsumedOffset = 0;
        free(mCurrentRectData);
        mCurrentRectData = nullptr;
        mCurrentRectDataSize = 0;
        if (mUncompressedDataMaxSize == 0) {
            orv_error_set(error, ORV_ERR_GENERIC, 0, "Internal error: Invalid uncompressed data size 0");
            return 0;
        }
        mUncompressedData = (uint8_t*)malloc(mUncompressedDataMaxSize);
        mCurrentRectData = (uint8_t*)malloc(std::max((uint32_t)1, expectedTotalRectSize));
        mCurrentRectDataSize = expectedTotalRectSize;
    }

    if (!mZlibPlainParser.hasUncompressibleData()) {
        return consumed;
    }
    uint32_t uncompressedBytes = 0;
    uint32_t remainingBytes = mZlibPlainParser.uncompressTo(mUncompressedData + mUncompressedDataOffset, mUncompressedDataMaxSize - mUncompressedDataOffset, &uncompressedBytes, error);
    if (error->mHasError) {
        return 0;
    }
    if (mZlibPlainParser.hasAllCompressedData() && remainingBytes != 0) {
        orv_error_set(error, ORV_ERR_GENERIC, 0, "Internal error in ZRLE encoding: Received all compressed data, but not all data could be uncompressed, have %d remaining bytes.", (int)remainingBytes);
        return 0;
    }
    mUncompressedDataOffset += uncompressedBytes;
    //ORV_DEBUG(mContext, "ZRLE: Have uncompressed data offset=%d", (int)mUncompressedConsumedOffset);

    while (mUncompressedConsumedOffset < mUncompressedDataOffset && mCurrentTileIndex < mExpectedTotalTiles) {
        //ORV_DEBUG(mContext, "ZRLE: Reading tile index %d (out of %d tiles) at offset %d of %d, totalTileColumns=%d, totalTileRows=%d", (int)mCurrentTileIndex, (int)mExpectedTotalTiles, (int)mUncompressedConsumedOffset, (int)mUncompressedDataOffset, (int)mExpectedTileColumns, (int)mExpectedTileRows);
        uint32_t c = readTileData((const char*)mUncompressedData + mUncompressedConsumedOffset, mUncompressedDataOffset - mUncompressedConsumedOffset, error);
        if (error->mHasError) {
            return 0;
        }
        if (c == 0) {
            //ORV_DEBUG(mContext, "  more data needed for tile index %d", (int)mCurrentTileIndex);
            if (mZlibPlainParser.hasAllCompressedData()) {
                orv_error_set(error, ORV_ERR_GENERIC, 0, "Internal error in ZRLE encoding: All data received from remote, but failed to parse data for tile, parser claimed to wait for more data. Current tile: %d", (int)mCurrentTileIndex);
                return 0;
            }
            return consumed;
        }
        //ORV_DEBUG(mContext, "  Consumed %d bytes for tile index %d", (int)c, (int)mCurrentTileIndex);
        mUncompressedConsumedOffset += c;
        if (mFinishedTile) {
            //ORV_DEBUG(mContext, "  Tile index %d finished", (int)mCurrentTileIndex);
            mCurrentTileIndex++;
            clearCurrentTile();
        }
    }

    return consumed;
}

uint32_t RectDataParserZRLE::readTileData(const char* buffer, uint32_t bufferSize, orv_error_t* error)
{
    if (mCurrentTileIndex >= mExpectedTotalTiles) {
        orv_error_set(error, ORV_ERR_GENERIC, 0, "Internal error in ZRLE encoding: readTileData() called, but have no tile to read");
        return 0;
    }

    uint32_t consumed = 0;
    if (!mCurrentTileSubencodingTypeRead) {
        if (bufferSize < consumed + 1) {
            return consumed;
        }
        mCurrentTileSubencodingType = Reader::readUInt8(buffer + consumed);
        mCurrentTileSubencodingTypeRead = true;
        consumed += 1;
        //ORV_DEBUG(mContext, "Started reading tile index %d, subencoding type: %d", (int)mCurrentTileIndex, (int)mCurrentTileSubencodingType);
    }

    // NOTE: We read tile data into mCurrentTileDataBuffer first, so we can easily interrupt reading
    //       if bufferSize is insufficient.
    //       Once tile is fully read, we copy into mCurrentRectData.
    //       Once rect is being finished, we copy into the framebuffer (as usual with other encodings).

    if (mCurrentTileSubencodingType == 0) {
        uint32_t c = readTileDataRaw(buffer + consumed, bufferSize - consumed, error);
        if (error->mHasError) {
            return 0;
        }
        consumed += c;
    }
    else if (mCurrentTileSubencodingType == 1) {
        // Solid color
        uint32_t c = readTileDataSolidColor(buffer + consumed, bufferSize - consumed, error);
        if (error->mHasError) {
            return 0;
        }
        consumed += c;
    }
    else if (mCurrentTileSubencodingType >= 2 && mCurrentTileSubencodingType <= 16) {
        // Packed palette types
        uint32_t c = readTileDataPackedPaletteTypes(buffer + consumed, bufferSize - consumed, error);
        if (error->mHasError) {
            return 0;
        }
        consumed += c;
    }
    else if ((mCurrentTileSubencodingType >= 17 && mCurrentTileSubencodingType <= 127) || (mCurrentTileSubencodingType == 129)) {
        orv_error_set(error, ORV_ERR_PROTOCOL_ERROR, 0, "Server sent ZRLE subencoding type %d for tile %d - type is invalid in ZRLE.", (int)mCurrentTileSubencodingType, (int)mCurrentTileIndex);
        return 0;
    }
    else if (mCurrentTileSubencodingType == 128) {
        // Plain RLE
        uint32_t c = readTileDataPlainRLE(buffer + consumed, bufferSize - consumed, error);
        if (error->mHasError) {
            return 0;
        }
        consumed += c;
    }
    else if (mCurrentTileSubencodingType >= 130 && mCurrentTileSubencodingType <= 255) {
        // Palette RLE
        uint32_t c = readTileDataPaletteRLE(buffer + consumed, bufferSize - consumed, error);
        if (error->mHasError) {
            return 0;
        }
        consumed += c;
    }
    else {
        orv_error_set(error, ORV_ERR_GENERIC, 0, "Internal error in ZRLE encoding, unhandled subencoding type %d", (int)mCurrentTileSubencodingType);
        return 0;
    }
    return consumed;
}

uint32_t RectDataParserZRLE::readTileDataRaw(const char* buffer, uint32_t bufferSize, orv_error_t* error)
{
    if (bufferSize < 1) {
        // need more data
        return 0;
    }
    UNUSED(error);
    uint32_t consumed = 0;
    const uint8_t tileWidth = calculateTileWidth(mCurrentTileIndex, mExpectedTileColumns, mCurrentRect.mW);
    const uint8_t tileHeight = calculateTileHeight(mCurrentTileIndex, mExpectedTileColumns, mExpectedTileRows, mCurrentRect.mH);
    const uint32_t expectedBytes = tileWidth * tileHeight * mZrleBytesPerPixel;
    // NOTE: we read into mCurrentTileDataBuffer first, so we can parse the tile in one go once
    //       fully received.
    if (mCurrentTileDataBytesRead < expectedBytes) {
        uint32_t readBytes = std::min(bufferSize - consumed, expectedBytes - mCurrentTileDataBytesRead);
        memcpy(mCurrentTileDataBuffer + mCurrentTileDataBytesRead, buffer + consumed, readBytes);
        consumed += readBytes;
        mCurrentTileDataBytesRead += readBytes;
        //ORV_DEBUG(mContext, "  Tile index %d in Raw encoding: Consumed %d more of %d bytes, total: %d", (int)mCurrentTileIndex, (int)readBytes, (int)expectedBytes, (mCurrentTileDataBytesRead));
    }
    if (mCurrentTileDataBytesRead >= expectedBytes) {
        const uint16_t tileXInRect = (mCurrentTileIndex % mExpectedTileColumns) * mMaxTileWidth;
        const uint16_t tileYInRect = (mCurrentTileIndex / mExpectedTileColumns) * mMaxTileHeight;
        uint8_t color[4] = {};
        const uint8_t srcBpp = mZrleBytesPerPixel;
        const uint8_t dstBpp = mCurrentPixelFormat.mBitsPerPixel/8;
        for (int y = 0; y < tileHeight; y++) {
            const uint8_t* srcLine = mCurrentTileDataBuffer + (y * tileWidth) * srcBpp;
            const uint16_t rectX = tileXInRect;
            const uint16_t rectY = tileYInRect + y;
            uint8_t* dstLine = mCurrentRectData + (rectY * mCurrentRect.mW + rectX) * dstBpp;
            for (int x = 0; x < tileWidth; x++) {
                makeUncompressedPixel(color, srcLine + x * srcBpp, mZrleBytesPerPixel, mCurrentPixelFormat.mBitsPerPixel/8, mZrleByteOffsetOfUncompressedPixel);
                for (int i = 0; i < dstBpp; i++) {
                    dstLine[x * dstBpp + i] = color[i];
                }
            }
        }
        mFinishedTile = true;
        //ORV_DEBUG(mContext, "  Tile index %d in Raw encoding finished", (int)mCurrentTileIndex);
    }
    return consumed;
}

uint32_t RectDataParserZRLE::readTileDataSolidColor(const char* buffer, uint32_t bufferSize, orv_error_t* error)
{
    if (bufferSize < mZrleBytesPerPixel) {
        // need more data
        return 0;
    }
    UNUSED(error);
    uint32_t consumed = mZrleBytesPerPixel;
    const uint8_t tileWidth = calculateTileWidth(mCurrentTileIndex, mExpectedTileColumns, mCurrentRect.mW);
    const uint8_t tileHeight = calculateTileHeight(mCurrentTileIndex, mExpectedTileColumns, mExpectedTileRows, mCurrentRect.mH);
    const uint16_t tileXInRect = (mCurrentTileIndex % mExpectedTileColumns) * mMaxTileWidth;
    const uint16_t tileYInRect = (mCurrentTileIndex / mExpectedTileColumns) * mMaxTileHeight;
    uint8_t color[4] = {};
    makeUncompressedPixel(color, (const uint8_t*)buffer, mZrleBytesPerPixel, mCurrentPixelFormat.mBitsPerPixel/8, mZrleByteOffsetOfUncompressedPixel);
    RectDataParserRealRectBase::fillSubrectInRect(mCurrentRectData, mCurrentRect.mW, tileXInRect, tileYInRect, tileWidth, tileHeight, color, mCurrentPixelFormat.mBitsPerPixel/8);
    mFinishedTile = true;
    return consumed;
}


uint32_t RectDataParserZRLE::readTileDataPackedPaletteTypes(const char* buffer, uint32_t bufferSize, orv_error_t* error)
{
    if (bufferSize < 1) {
        // need more data
        return 0;
    }
    UNUSED(error);
    uint32_t consumed = 0;
    const uint8_t tileWidth = calculateTileWidth(mCurrentTileIndex, mExpectedTileColumns, mCurrentRect.mW);
    const uint8_t tileHeight = calculateTileHeight(mCurrentTileIndex, mExpectedTileColumns, mExpectedTileRows, mCurrentRect.mH);
    const uint8_t paletteSize = mCurrentTileSubencodingType;
    const uint8_t bitsPerIndex = (paletteSize == 2) ? 1 : (
                                     (paletteSize <= 4) ? 2 : 4
                                 );
    const uint8_t pixelIndexMask = (paletteSize == 2) ? 1 : (
                                       (paletteSize <= 4) ? 3 : 15
                                   ); // max valid index value == mask
    const uint8_t indexesPerByte = 8 / bitsPerIndex;
    const uint32_t packedPixelsBytesPerRow = ((tileWidth + indexesPerByte - 1) / indexesPerByte); // rows are padded to full bytes
    const uint32_t expectedBytes = paletteSize * mZrleBytesPerPixel + packedPixelsBytesPerRow * tileHeight;
    // NOTE: we read into mCurrentTileDataBuffer first, so we can parse the tile in one go once
    //       fully received.
    if (mCurrentTileDataBytesRead < expectedBytes) {
        uint32_t readBytes = std::min(bufferSize - consumed, expectedBytes - mCurrentTileDataBytesRead);
        memcpy(mCurrentTileDataBuffer + mCurrentTileDataBytesRead, buffer + consumed, readBytes);
        consumed += readBytes;
        mCurrentTileDataBytesRead += readBytes;
        //ORV_DEBUG(mContext, "  Tile index %d in PackedPaletteTypes encoding: Consumed %d more of %d bytes, total: %d", (int)mCurrentTileIndex, (int)readBytes, (int)expectedBytes, (mCurrentTileDataBytesRead));
    }
    if (mCurrentTileDataBytesRead >= expectedBytes) {
        const uint16_t tileXInRect = (mCurrentTileIndex % mExpectedTileColumns) * mMaxTileWidth;
        const uint16_t tileYInRect = (mCurrentTileIndex / mExpectedTileColumns) * mMaxTileHeight;
        const uint8_t* palette = (uint8_t*)buffer + 0;
        const uint8_t* packedPixels = (uint8_t*)buffer + paletteSize * mZrleBytesPerPixel;
        uint8_t color[4] = {};
        for (uint8_t pixelY = 0; pixelY < tileHeight; pixelY++) {
            const uint8_t* packedPixelsRow = packedPixels + packedPixelsBytesPerRow * pixelY;
            const uint16_t rectY = tileYInRect + pixelY;
            for (uint8_t pixelX = 0; pixelX < tileWidth; pixelX++) {
                const uint16_t rectX = tileXInRect + pixelX;
                const uint32_t byteIndexOfPixel = pixelX / indexesPerByte;
                const uint32_t indexOfPixelInByte = pixelX % indexesPerByte;
                const uint8_t byte = packedPixelsRow[byteIndexOfPixel];
                const uint8_t paletteIndex = (byte >> (indexesPerByte - 1 - indexOfPixelInByte) * bitsPerIndex) & pixelIndexMask;
                if (paletteIndex >= paletteSize) {
                    ORV_ERROR(mContext, "xxx byte=%d paletteIndex=%d indexes=%d indexOf=%d bits=%d mask=%d",(int)byte, (int)paletteIndex, (int)indexesPerByte, (int)indexOfPixelInByte, (int)bitsPerIndex, (int)pixelIndexMask);
                    orv_error_set(error, ORV_ERR_PROTOCOL_ERROR, 0, "Invalid palette index %d for palette of size %d in ZRLE encoding", (int)paletteIndex, (int)paletteSize);
                    return 0;
                }
                const uint8_t* compressedColor = palette + mZrleBytesPerPixel * paletteIndex;
                makeUncompressedPixel(color, compressedColor, mZrleBytesPerPixel, mCurrentPixelFormat.mBitsPerPixel/8, mZrleByteOffsetOfUncompressedPixel);
                uint8_t* dst = mCurrentRectData + (rectY * mCurrentRect.mW + rectX) * (mCurrentPixelFormat.mBitsPerPixel/8);
                for (int i = 0; i < mCurrentPixelFormat.mBitsPerPixel/8; i++) {
                    dst[i] = color[i];
                }
            }
        }
        //ORV_DEBUG(mContext, "  Tile index %d in PackedPaletteTypes encoding finished", (int)mCurrentTileIndex);
        mFinishedTile = true;
    }
    return consumed;
}

uint32_t RectDataParserZRLE::readTileDataPlainRLE(const char* buffer, uint32_t bufferSize, orv_error_t* error)
{
    uint32_t consumed = 0;
    const uint8_t tileWidth = calculateTileWidth(mCurrentTileIndex, mExpectedTileColumns, mCurrentRect.mW);
    const uint8_t tileHeight = calculateTileHeight(mCurrentTileIndex, mExpectedTileColumns, mExpectedTileRows, mCurrentRect.mH);
    const uint32_t currentTilePixels = tileWidth * tileHeight;
    //ORV_DEBUG(mContext, "  Tile index %d with tileW=%d, tileH=%d in plain RLE encoding...", (int)mCurrentTileIndex, (int)tileWidth, (int)tileHeight);
    uint8_t color[4] = {};
    while (mCurrentTileRLEPixelsDone < currentTilePixels) {
        // We always read a full "run" in one iteration. If not enough data is available, we wait
        // for more before reading data.
        if (bufferSize < consumed + mZrleBytesPerPixel + 1) {
            return consumed;
        }
        const uint8_t* compressedColor = (uint8_t*)buffer + consumed;
        uint32_t runLength = readRunLength((uint8_t*)buffer + consumed + mZrleBytesPerPixel, bufferSize - consumed - mZrleBytesPerPixel, error);
        if (error->mHasError) {
            return 0;
        }
        if (runLength == 0) {
            // need more data
            return consumed;
        }
        if (mCurrentTileRLEPixelsDone + runLength > currentTilePixels) {
            orv_error_set(error, ORV_ERR_PROTOCOL_ERROR, 0, "Run length in ZRLE encoding of %d yields total length of %d, which exceeds tile size of %d, garbage received.", (int)runLength, (int)(mCurrentTileRLEPixelsDone + runLength), (int)currentTilePixels);
            return 0;
        }

        makeUncompressedPixel(color, compressedColor, mZrleBytesPerPixel, mCurrentPixelFormat.mBitsPerPixel/8, mZrleByteOffsetOfUncompressedPixel);
        writeRunLengthColorToCurrentRect(color, mCurrentTileIndex, tileWidth, mCurrentTileRLEPixelsDone, runLength);

        mCurrentTileRLEPixelsDone += runLength;
        //ORV_DEBUG(mContext, "    Tile index %d in plain RLE encoding: runLength=%d, pixelsOfTileDone: %d", (int)mCurrentTileIndex, (int)runLength, (int)mCurrentTileRLEPixelsDone);
        const uint8_t byteCountOfRunLength = (runLength - 1) / 255 + 1;
        consumed += mZrleBytesPerPixel + byteCountOfRunLength;
    }
    mFinishedTile = true;
    //ORV_DEBUG(mContext, "  Tile index %d in plain RLE encoding finished", (int)mCurrentTileIndex);
    return consumed;
}

uint32_t RectDataParserZRLE::readTileDataPaletteRLE(const char* buffer, uint32_t bufferSize, orv_error_t* error)
{
    if (bufferSize < 1) {
        // need more data
        return 0;
    }
    uint32_t consumed = 0;
    const uint8_t paletteSize = mCurrentTileSubencodingType - 128;
    const uint32_t expectedPaletteBytes = paletteSize * mZrleBytesPerPixel;
    // NOTE: the palette is read into mCurrentTileDataBuffer
    if (mCurrentTileDataBytesRead < expectedPaletteBytes) {
        uint32_t readBytes = std::min(bufferSize - consumed, expectedPaletteBytes - mCurrentTileDataBytesRead);
        memcpy(mCurrentTileDataBuffer + mCurrentTileDataBytesRead, buffer + consumed, readBytes);
        consumed += readBytes;
        mCurrentTileDataBytesRead += readBytes;
        //ORV_DEBUG(mContext, "  Tile index %d in PaletteRLE encoding: Consumed %d more of %d palette bytes, total: %d", (int)mCurrentTileIndex, (int)readBytes, (int)expectedPaletteBytes, (int)mCurrentTileDataBytesRead);
        if (mCurrentTileDataBytesRead < expectedPaletteBytes) {
            return consumed;
        }
    }

    // at this point the palette has been fully read.
    const uint8_t* paletteStart = mCurrentTileDataBuffer;

    const uint8_t tileWidth = calculateTileWidth(mCurrentTileIndex, mExpectedTileColumns, mCurrentRect.mW);
    const uint8_t tileHeight = calculateTileHeight(mCurrentTileIndex, mExpectedTileColumns, mExpectedTileRows, mCurrentRect.mH);
    const uint32_t currentTilePixels = tileWidth * tileHeight;
    //ORV_DEBUG(mContext, "  Tile index %d with tileW=%d, tileH=%d in palette RLE encoding, paletteSize=%d...", (int)mCurrentTileIndex, (int)tileWidth, (int)tileHeight, (int)paletteSize);
    uint8_t color[4] = {};
    while (mCurrentTileRLEPixelsDone < currentTilePixels) {
        if (bufferSize < consumed + 1) {
            return consumed;
        }
        const uint8_t paletteIndexByte = buffer[consumed];
        const bool useRunLength = (paletteIndexByte & 0x80);
        const uint8_t paletteIndex = paletteIndexByte & (0xff-0x80);
        if (paletteIndex >= paletteSize) {
            orv_error_set(error, ORV_ERR_PROTOCOL_ERROR, 0, "Invalid palette index %d for pixel %d of %d in ZRLE encoding, palette size is %d", (int)paletteIndex, (int)mCurrentTileRLEPixelsDone, (int)currentTilePixels, (int)paletteSize);
            return 0;
        }
        const uint8_t* compressedColor = paletteStart + mZrleBytesPerPixel * paletteIndex;
        uint32_t runLength = 0;
        uint32_t byteCountOfRunLength = 0;
        if (useRunLength) {
            runLength = readRunLength((uint8_t*)buffer + consumed + 1, bufferSize - consumed - 1, error);
            if (error->mHasError) {
                return 0;
            }
            if (runLength == 0) {
                // need more data
                return consumed;
            }
            byteCountOfRunLength = (runLength - 1) / 255 + 1;
        }
        else {
            runLength = 1;
            byteCountOfRunLength = 0;
        }
        if (mCurrentTileRLEPixelsDone + runLength > currentTilePixels) {
            orv_error_set(error, ORV_ERR_PROTOCOL_ERROR, 0, "Run length in ZRLE encoding of %d yields total length of %d, which exceeds tile size of %d, garbage received.", (int)runLength, (int)(mCurrentTileRLEPixelsDone + runLength), (int)currentTilePixels);
            return 0;
        }

        makeUncompressedPixel(color, compressedColor, mZrleBytesPerPixel, mCurrentPixelFormat.mBitsPerPixel/8, mZrleByteOffsetOfUncompressedPixel);
        writeRunLengthColorToCurrentRect(color, mCurrentTileIndex, tileWidth, mCurrentTileRLEPixelsDone, runLength);

        consumed += 1 + byteCountOfRunLength;
        mCurrentTileRLEPixelsDone += runLength;
        //ORV_DEBUG(mContext, "    Tile index %d in palette RLE encoding: runLength=%d (useRunLength=%s), pixelsOfTileDone: %d", (int)mCurrentTileIndex, (int)runLength, useRunLength?"true":"false", (int)mCurrentTileRLEPixelsDone);

    }
    //ORV_DEBUG(mContext, "  Tile index %d in palette RLE encoding finished", (int)mCurrentTileIndex);
    mFinishedTile = true;
    return consumed;


}

uint8_t RectDataParserZRLE::calculateZrleBytesPerPixel(const orv_communication_pixel_format_t& pixelFormat, bool* omitLeastSignificantByte)
{
    *omitLeastSignificantByte = false;
    // ZRLE uses "compressed pixels", which are exactly like normal pixels, except in the special
    // case that ALL of the following hold for the pixels:
    // - trueColor is true
    // - bitsPerPixel is 32
    // - depth is 24 or less
    // - all bits of red/green/blue are either in the most-significant or the least-significant bits
    //   of the uint32.
    // in that case ZRLE uses 3 bytes per pixel (sidenote: even if depth is 16 or less, though a
    // client using depth <= 16 with bpp 32 is plain stupid).
    uint8_t zrleBpp = pixelFormat.mBitsPerPixel / 8;
    if (!pixelFormat.mTrueColor) {
        return zrleBpp;
    }
    if (pixelFormat.mBitsPerPixel == 32 && pixelFormat.mDepth <= 24) {
        uint32_t redBitmask = pixelFormat.mColorMax[0] << pixelFormat.mColorShift[0];
        uint32_t greenBitmask = pixelFormat.mColorMax[1] << pixelFormat.mColorShift[1];
        uint32_t blueBitmask = pixelFormat.mColorMax[2] << pixelFormat.mColorShift[2];
        uint32_t fullBitmask = redBitmask | greenBitmask | blueBitmask;
        // TODO: endianness?
        const bool usesMostSignificantBits = (fullBitmask & 0xff000000);
        const bool usesLeastSignificantBits = (fullBitmask & 0x000000ff);
        if (!usesMostSignificantBits || !usesLeastSignificantBits) {
            zrleBpp = 3;
            // NOTE: least significant bits are preferred over most-significant bytes in ZRLE if
            //       both are unused.
            if (!usesLeastSignificantBits) {
                *omitLeastSignificantByte = true;
            }
        }
    }
    return zrleBpp;
}

/**
 * @return The maximum size of the uncompressed data in ZRLE encoding for the specified parameters.
 **/
bool RectDataParserZRLE::calculateMaxUncompressedDataSize(uint32_t* maxSize, uint32_t totalNumberOfTiles, uint8_t bppForZrle)
{
    *maxSize = 0;
    if (totalNumberOfTiles == 0 || bppForZrle == 0) {
        return false;
    }
    const uint64_t bytes = ((uint64_t)totalNumberOfTiles) * ((uint64_t)maxBytesPerZRLETile());
    if (bytes > 0xffffffff) {
        // result exceeds 32 bit
        return false;
    }
    *maxSize = (uint32_t)bytes;
    return true;
}

bool RectDataParserZRLE::canFinishRect() const
{
    if (mZlibPlainParser.hasAllCompressedData()) {
        return true;
    }
    return false;
}

void RectDataParserZRLE::finishRect(orv_error_t* error)
{
    std::unique_lock<std::mutex> lock(mFramebufferMutex);
    if (!checkRectParametersForFramebufferMutexLocked(error)) {
        return;
    }
    ORV_DEBUG(mContext, "Performing framebuffer update for ZRLE data");
    if (mFramebuffer.mBytesPerPixel != 3) {
        orv_error_set(error, ORV_ERR_GENERIC, 0, "Internal error: %s assumes the internal framebuffer uses 3 bytes per pixel, have %d", __func__, (int)mFramebuffer.mBytesPerPixel);
        return;
    }

    // NOTE: mCurrentRectData uses mCurrentPixelFormat.mBitsPerPixel/8 (not mZrleBytesPerPixel)

    const int remoteBpp = mCurrentPixelFormat.mBitsPerPixel / 8;
    if (mCurrentPixelFormat.mBitsPerPixel == 8) {
        for (int srcY = 0; srcY < mCurrentRect.mH; srcY++) {
            const int dstY = mCurrentRect.mY + srcY;
            for (int srcX = 0; srcX < mCurrentRect.mW; srcX++) {
                const int dstX = mCurrentRect.mX + srcX;
                const uint8_t* pSrc = mCurrentRectData + (srcY * mCurrentRect.mW + srcX) * remoteBpp;
                uint8_t* pDst = mFramebuffer.mFramebuffer + (dstY * mFramebuffer.mWidth + dstX) * mFramebuffer.mBytesPerPixel;
                Reader::readPixel8Bit(pDst, pSrc, mCurrentPixelFormat);
            }
        }
    }
    else if (mCurrentPixelFormat.mBitsPerPixel == 16) {
        for (int srcY = 0; srcY < mCurrentRect.mH; srcY++) {
            const int dstY = mCurrentRect.mY + srcY;
            for (int srcX = 0; srcX < mCurrentRect.mW; srcX++) {
                const int dstX = mCurrentRect.mX + srcX;
                const uint8_t* pSrc = mCurrentRectData + (srcY * mCurrentRect.mW + srcX) * remoteBpp;
                uint8_t* pDst = mFramebuffer.mFramebuffer + (dstY * mFramebuffer.mWidth + dstX) * mFramebuffer.mBytesPerPixel;
                Reader::readPixel16Bit(pDst, pSrc, mCurrentPixelFormat);
            }
        }
    }
    else if (mCurrentPixelFormat.mBitsPerPixel == 32) {
        for (int srcY = 0; srcY < mCurrentRect.mH; srcY++) {
            const int dstY = mCurrentRect.mY + srcY;
            for (int srcX = 0; srcX < mCurrentRect.mW; srcX++) {
                const int dstX = mCurrentRect.mX + srcX;
                const uint8_t* pSrc = mCurrentRectData + (srcY * mCurrentRect.mW + srcX) * remoteBpp;
                uint8_t* pDst = mFramebuffer.mFramebuffer + (dstY * mFramebuffer.mWidth + dstX) * mFramebuffer.mBytesPerPixel;
                Reader::readPixel32Bit(pDst, pSrc, mCurrentPixelFormat);
            }
        }
    }
    else {
        orv_error_set(error, ORV_ERR_GENERIC, 0, "Invalid value for BitsPerPixel: %d", (int)mCurrentPixelFormat.mBitsPerPixel);
        return;
    }
    //ORV_DEBUG(mContext, "Finished performing framebuffer update for ZRLE data");
}

/**
 * Read a run length from @p buffer.
 *
 * The run length is encoded as the sum of individual bytes plus 1, until the first non-255 value is
 * reached. I.e. "41" encodes 42, "255,255,41" encodes 552, etc.
 *
 * If the @p buffer does not contain enough data for the full run (e.g. ends with 255), this
 * function returns 0 (but no error), indicating that no data was read.
 *
 * The maximal allowed value in this function is 64*64=4096, i.e. the run contains all pixels of a
 * full tile. If this value is exceeded, @p error will be set and this function returns 0.
 *
 * @return The length of the run, as read from @p buffer, or 0 if not enough data was available (or
 *         on error). The total number of bytes read from @p buffer is always (length-1)/255+1 (or 0
 *         if length is 0).
 **/
uint32_t RectDataParserZRLE::readRunLength(const uint8_t* buffer, uint32_t bufferSize, orv_error_t* error)
{
    uint32_t pos = 0;
    uint32_t length = 1;
    while (pos < bufferSize) {
        const uint8_t b = buffer[pos];
        length += b;
        if (length > mMaxTileWidth * mMaxTileHeight) {
            orv_error_set(error, 0, ORV_ERR_PROTOCOL_ERROR, "Run length in ZRLE encoding exceeds valid size: Have run of length %d, which exceeds %d.", (int)length, (int)(mMaxTileWidth * mMaxTileHeight));
            return 0;
        }
        if (b < 255) {
            return length;
        }
        pos++;
    }

    // Need more data.
    return 0;
}

void RectDataParserZRLE::writeRunLengthColorToCurrentRect(const uint8_t* uncompressedColor, uint16_t tileIndex, uint8_t tileWidth, uint32_t firstPixelIndex, uint32_t runLength)
{
    const uint16_t tileXInRect = (tileIndex % mExpectedTileColumns) * mMaxTileWidth;
    const uint16_t tileYInRect = (tileIndex / mExpectedTileColumns) * mMaxTileHeight;
    const int bpp = mCurrentPixelFormat.mBitsPerPixel / 8;
    for (uint32_t pixelIndex = firstPixelIndex; pixelIndex < firstPixelIndex + runLength; pixelIndex++) {
        const int pixelXInTile = pixelIndex % tileWidth;
        const int pixelYInTile = pixelIndex / tileWidth;
        const uint16_t rectY = tileYInRect + pixelYInTile;
        const uint16_t rectX = tileXInRect + pixelXInTile;
        uint8_t* dst = mCurrentRectData + (rectY * mCurrentRect.mW + rectX) * bpp;
        for (int i = 0; i < bpp; i++) {
            dst[i] = uncompressedColor[i];
        }
    }
}

} // namespace vnc
} // namespace openrv

