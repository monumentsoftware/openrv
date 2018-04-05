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

#ifndef OPENRV_RECTDATAPARSER_H
#define OPENRV_RECTDATAPARSER_H

struct orv_context_t;
struct z_stream_s;

namespace openrv {
namespace vnc {

/**
 * Base class for parsing rect data in a FramebufferUpdate message.
 *
 * The subclasses of this class implement the different encodings supported by this client library.
 **/
class RectDataParserBase
{
public:
    explicit RectDataParserBase(orv_context_t* ctx, const orv_communication_pixel_format_t* currentPixelFormat, const uint16_t* currentFramebufferWidth, const uint16_t* currentFramebufferHeight);
    virtual ~RectDataParserBase() = default;

    void setCurrentRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h);

    /**
     * @pre The rect size in @ref mCurrentRectHeader is initialized and valid.
     *
     * @param error Output parameter that receives errors encountered in this function.
     *        Must be initially reset (see @ref orv_error_reset()).
     *
     * Read data for the current rect from @p buffer (which may be empty).
     *
     * This function should read as much data for the current rect as possible from @p buffer. If
     * all data for the current rect has been rect, an internal flag should be set, so that @ref
     * canFinishRect() returns TRUE.
     *
     * @return The number of read/consumed bytes from @p buffer. 0 if additional data is required
     *         before data can be read. If an error occurs, @p error is set accordingly and the
     *         return value should be ignored (normally this function returns 0 then). The
     *         connection will then be unusable and should be closed.
     *         NOTE: This function returns 0 either if more data is required before reading can
     *               continue, or if no data is needed anymore (e.g. the rect header itself is
     *               sufficient, no data contained in rect). The caller must check using
     *               @ref canFinishRect() after calling this function.
     **/
    virtual uint32_t readRectData(const char* buffer, uint32_t bufferSize, orv_error_t* error) = 0;
    /**
     * @return TRUE if the current rect has been completely read (see @ref readRectData()),
     *         otherwise FALSE (more data required or this object is in an error state).
     **/
    virtual bool canFinishRect() const = 0;
    /**
     * Finalize the current rect, i.e. copy the data that was read using @ref readRectData() to the
     * framebuffer.
     *
     * This function is called once @ref canFinishRect() returns TRUE.
     *
     * If an error occurs, @p error must be set accordingly.
     **/
    virtual void finishRect(orv_error_t* error) = 0;

    /**
     * @return Whether this class represents an actual encoding, or a pseudo-encoding.
     **/
    virtual bool isPseudoEncoding() const = 0;

    virtual void reset();
    virtual void resetConnection();

protected:
    struct Rect
    {
        uint16_t mX = 0;
        uint16_t mY = 0;
        uint16_t mW = 0;
        uint16_t mH = 0;
    };
protected:
    bool checkRectParametersForFramebufferMutexLocked(orv_error_t* error);
protected:
    struct orv_context_t* mContext = nullptr;
    /**
     * Current pixel format of the communication with the server.
     *
     * Note that this is NOT (normally) the format of @ref mFramebuffer, but the format of the
     * @em communication with the server.
     **/
    const orv_communication_pixel_format_t& mCurrentPixelFormat;
    /**
     * Current width of the framebuffer.
     *
     * Identical to @ref orv_framebuffer_t::mWidth of @ref mFramebuffer, but no mutex locks are
     * required (lives in the connection thread).
     **/
    const uint16_t& mCurrentFramebufferWidth;
    /**
     * Current height of the framebuffer.
     *
     * Identical to @ref orv_framebuffer_t::mWidth of @ref mFramebuffer, but no mutex locks are
     * required (lives in the connection thread).
     **/
    const uint16_t& mCurrentFramebufferHeight;
    Rect mCurrentRect;
};

/**
 * Base class for parsing rect data in a FramebufferUpdate message. This class is the base class for
 * "real" rects, i.e. non-pseudo-encodings.
 *
 * The subclasses of this class implement the different "real" (non-pseudo) encodings supported by
 * this client library.
 **/
class RectDataParserRealRectBase : public RectDataParserBase
{
public:
    RectDataParserRealRectBase(orv_context_t* ctx, std::mutex* framebufferMutex, orv_framebuffer_t* framebuffer, const orv_communication_pixel_format_t* currentPixelFormat, const uint16_t* currentFramebufferWidth, const uint16_t* currentFramebufferHeight);
    virtual ~RectDataParserRealRectBase() = default;

    virtual bool isPseudoEncoding() const override;

protected:
    bool checkRectParametersForFramebufferMutexLocked(orv_error_t* error);
    static bool calculateRectBufferSizeFor(uint32_t* bufferSize, uint16_t rectWidth, uint16_t rectHeight, uint8_t bitsPerPixel);
    static void fillSubrectInRect(uint8_t* rectData, uint16_t rectWidth, uint16_t subrectXInRect, uint16_t subrectYInRect, uint16_t subrectWidth, uint16_t subrectHeight, const uint8_t* color, uint8_t bpp);
protected:
    std::mutex& mFramebufferMutex;
    /**
     * Reference to framebuffer of @ref openrv::vnc::ConnectionThread.
     *
     * Protected by @ref mFramebufferMutex, all accesses em MUST lock the mutex first.
     **/
    orv_framebuffer_t& mFramebuffer;
};

/**
 * Implementation of the Raw encoding of the RFB protocol.
 **/
class RectDataParserRaw : public RectDataParserRealRectBase
{
public:
    RectDataParserRaw(struct orv_context_t* context, std::mutex* framebufferMutex, orv_framebuffer_t* framebuffer, const orv_communication_pixel_format_t* currentPixelFormat, const uint16_t* currentFramebufferWidth, const uint16_t* currentFramebufferHeight);
    virtual ~RectDataParserRaw();

    virtual uint32_t readRectData(const char* buffer, uint32_t bufferSize, orv_error_t* error) override;
    virtual bool canFinishRect() const override;
    virtual void finishRect(orv_error_t* error) override;
    virtual void reset() override;

protected:
    void clear();

private:
    uint32_t mExpectedBytes = 0;
    uint32_t mConsumed = 0;
    uint8_t* mCurrentRectData = nullptr;
    uint32_t mCurrentRectDataSize = 0;
};

/**
 * Implementation of the CopyRect encoding of the RFB protocol.
 **/
class RectDataParserCopyRect : public RectDataParserRealRectBase
{
public:
    RectDataParserCopyRect(struct orv_context_t* context, std::mutex* framebufferMutex, orv_framebuffer_t* framebuffer, const orv_communication_pixel_format_t* currentPixelFormat, const uint16_t* currentFramebufferWidth, const uint16_t* currentFramebufferHeight);
    virtual ~RectDataParserCopyRect();

    virtual uint32_t readRectData(const char* buffer, uint32_t bufferSize, orv_error_t* error) override;
    virtual bool canFinishRect() const override;
    virtual void finishRect(orv_error_t* error) override;
    virtual void reset() override;

private:
    bool mHasHeader = false;
    uint16_t mSrcX = 0;
    uint16_t mSrcY = 0;
};

/**
 * Implementation of the RRE and CoRRE encodings of the RFB protocol.
 **/
class RectDataParserRRE : public RectDataParserRealRectBase
{
public:
    RectDataParserRRE(struct orv_context_t* context, std::mutex* framebufferMutex, orv_framebuffer_t* framebuffer, const orv_communication_pixel_format_t* currentPixelFormat, const uint16_t* currentFramebufferWidth, const uint16_t* currentFramebufferHeight, bool isCompressedRRE);
    virtual ~RectDataParserRRE();

    virtual uint32_t readRectData(const char* buffer, uint32_t bufferSize, orv_error_t* error) override;
    virtual bool canFinishRect() const override;
    virtual void finishRect(orv_error_t* error) override;
    virtual void reset() override;

protected:
    struct SubRectangle
    {
        uint8_t mPixelValue[3] = {};
        uint16_t mX = 0;
        uint16_t mY = 0;
        uint16_t mW = 0;
        uint16_t mH = 0;
    };
protected:
    void clear();

private:
    /**
     * If TRUE, the CoRRE encoding is expected, otherwise normal RRE.
     **/
    const bool mIsCompressedRRE = false;

    uint32_t mTotalSubRectanglesCount = 0;
    uint32_t mFinishedSubRectanglesCount = 0;
    uint8_t mBackgroundPixelValue[3] = {};
    bool mHasRREHeader = false;
    SubRectangle* mSubRectangles = nullptr;
};

/**
 * Implementation of the Hextile encoding of the RFB protocol.
 **/
class RectDataParserHextile : public RectDataParserRealRectBase
{
public:
    RectDataParserHextile(struct orv_context_t* context, std::mutex* framebufferMutex, orv_framebuffer_t* framebuffer, const orv_communication_pixel_format_t* currentPixelFormat, const uint16_t* currentFramebufferWidth, const uint16_t* currentFramebufferHeight);
    virtual ~RectDataParserHextile();

    virtual uint32_t readRectData(const char* buffer, uint32_t bufferSize, orv_error_t* error) override;
    virtual bool canFinishRect() const override;
    virtual void finishRect(orv_error_t* error) override;
    virtual void reset() override;

protected:
    enum SubencodingMaskFlags {
        SubencodingFlagRaw = 0x01,
        SubencodingFlagBackgroundSpecified = 0x02,
        SubencodingFlagForegroundSpecified = 0x04,
        SubencodingFlagAnySubrects = 0x08,
        SubencodingFlagSubrectsColoured = 0x10,
    };
protected:
    uint32_t readTileData(const char* buffer, uint32_t bufferSize, orv_error_t* error);
    void clear();
    void clearCurrentTile();
    static constexpr uint8_t calculateTileWidth(uint16_t tileIndex, uint16_t tileColumns, uint16_t rectWidth);
    static constexpr uint8_t calculateTileHeight(uint16_t tileIndex, uint16_t tileColumns, uint16_t tileRows, uint16_t rectHeight);
    //static void copyBackgroundColorForTileToRect(uint8_t* rectData, uint16_t rectWidth, uint16_t rectHeight, uint16_t tileIndex, uint16_t totalTileColumns, uint16_t totalTileRows, const uint8_t* color, uint8_t bpp);

private:
    static constexpr uint8_t mMaxTileWidth = 16;
    static constexpr uint8_t mMaxTileHeight = 16;
    static constexpr uint8_t mMaxBytesPerPixel = 4; // max bytes per pixel in pixel encoding
    static constexpr uint8_t mMaxBytesPerSubrect = mMaxBytesPerPixel + 2;
private:
    bool mIsInitialized = false;
    uint8_t* mCurrentRectData = nullptr;
    uint32_t mCurrentRectDataSize = 0;
    uint16_t mCurrentTileIndex = 0;
    uint16_t mExpectedTileRows = 0;
    uint16_t mExpectedTileColumns = 0;
    uint32_t mExpectedTotalTiles = 0;
    uint8_t mCurrentTileSubencodingMask = 0;
    bool mCurrentTileSubencodingMaskRead = false;
    bool mCurrentTileDidReadBackgroundColor = false;
    bool mCurrentTileDidReadForegroundColor = false;
    bool mCurrentTileDidReadAnySubrects = false;
    uint8_t mCurrentTileSubrects = 0;
    bool mFinishedTile = false;

    uint8_t mCurrentBackgroundColor[mMaxBytesPerPixel] = {};
    uint8_t mCurrentForegroundColor[mMaxBytesPerPixel] = {};
    uint8_t mCurrentTileDataBuffer[mMaxTileWidth * mMaxTileHeight * mMaxBytesPerSubrect] = {};
    uint32_t mCurrentTileDataBytesRead = 0; // for Raw tiles: pixels*bpp bytes. otherwise: subrects data
};

/**
 * Implementation of the Cursor pseudo-encoding of the RFB protocol.
 **/
class RectDataParserCursor : public RectDataParserBase
{
public:
    RectDataParserCursor(struct orv_context_t* context, std::mutex* cursorMutex, orv_cursor_t* cursorData, const orv_communication_pixel_format_t* currentPixelFormat, const uint16_t* currentFramebufferWidth, const uint16_t* currentFramebufferHeight);
    virtual ~RectDataParserCursor();

    virtual uint32_t readRectData(const char* buffer, uint32_t bufferSize, orv_error_t* error) override;
    virtual bool canFinishRect() const override;
    virtual void finishRect(orv_error_t* error) override;
    virtual void reset() override;
    virtual bool isPseudoEncoding() const override;

protected:
    void clear();

private:
    std::mutex& mCursorMutex;
    /**
     * Reference to cursor data of @ref openrv::vnc::ConnectionThread.
     *
     * Protected by @ref mCursorMutex, all accesses em MUST lock the mutex first.
     **/
    orv_cursor_t& mCursorData;
    bool mIsInitialized = false;
    uint8_t* mCursor = nullptr;
    uint8_t* mCursorMask = nullptr;
    uint32_t mExpectedCursorBytes = 0;
    uint32_t mExpectedCursorBitmaskBytes = 0;
    uint32_t mCursorBytesRead = 0;
    uint32_t mCursorBitmaskBytesRead = 0;
};

/**
 * Helper class for other @ref RectDataParserRealRectBase subclasses.
 *
 * This class simply reads zlib compressed data from a buffer and provides the uncompressed data. No
 * actual rect is read.
 *
 * NOTE: This class assumes the zlib data is prefixed by a 4 byte header, providing a uint32 with
 *       the size of the compressed data.
 **/
class RectDataParserZlibPlain : public RectDataParserRealRectBase
{
public:
    RectDataParserZlibPlain(struct orv_context_t* context, std::mutex* framebufferMutex, orv_framebuffer_t* framebuffer, const orv_communication_pixel_format_t* currentPixelFormat, const uint16_t* currentFramebufferWidth, const uint16_t* currentFramebufferHeight, const char* owningEncodingString);
    virtual ~RectDataParserZlibPlain();

    virtual uint32_t readRectData(const char* buffer, uint32_t bufferSize, orv_error_t* error) override;
    virtual bool canFinishRect() const override;
    virtual void finishRect(orv_error_t* error) override;
    virtual void reset() override;
    virtual void resetConnection() override;

    uint32_t totalExpectedCompressedBytes() const;
    bool hasAllCompressedData() const;
    bool hasUncompressibleData() const;
    uint32_t uncompressTo(uint8_t* buffer, uint32_t bufferSize, uint32_t* uncompressedSize, orv_error_t* error);

protected:
    void clear();
    void destroyZStream();

private:
    char* mOwningEncodingString = nullptr;
    bool mHasZlibHeader = false;
    uint32_t mExpectedCompressedDataLength = 0;
    uint32_t mCompressedDataReceived = 0;
    uint8_t* mCompressedData = nullptr;
    uint32_t mCompressedDataUncompressedLength = 0;
    struct z_stream_s* mZStream = nullptr;
};


/**
 * Implementation of the Zlib encoding of the RFB protocol.
 **/
class RectDataParserZlib : public RectDataParserRaw
{
public:
    RectDataParserZlib(struct orv_context_t* context, std::mutex* framebufferMutex, orv_framebuffer_t* framebuffer, const orv_communication_pixel_format_t* currentPixelFormat, const uint16_t* currentFramebufferWidth, const uint16_t* currentFramebufferHeight);
    virtual ~RectDataParserZlib();

    virtual uint32_t readRectData(const char* buffer, uint32_t bufferSize, orv_error_t* error) override;
    //virtual bool canFinishRect() const override;
    //virtual void finishRect(orv_error_t* error) override;
    virtual void reset() override;
    virtual void resetConnection() override;

protected:
    void clear();

private:
    RectDataParserZlibPlain mZlibPlainParser;
    uint8_t* mUncompressedData = nullptr;
    uint32_t mUncompressedDataSize = 0;
    uint32_t mUncompressedDataOffset = 0;
};


/**
 * Implementation of the ZRLE encoding of the RFB protocol.
 **/
class RectDataParserZRLE : public RectDataParserRealRectBase
{
public:
    RectDataParserZRLE(struct orv_context_t* context, std::mutex* framebufferMutex, orv_framebuffer_t* framebuffer, const orv_communication_pixel_format_t* currentPixelFormat, const uint16_t* currentFramebufferWidth, const uint16_t* currentFramebufferHeight);
    virtual ~RectDataParserZRLE();

    virtual uint32_t readRectData(const char* buffer, uint32_t bufferSize, orv_error_t* error) override;
    virtual bool canFinishRect() const override;
    virtual void finishRect(orv_error_t* error) override;
    virtual void reset() override;
    virtual void resetConnection() override;

protected:
    void clear();
    void clearCurrentTile();
    static constexpr uint8_t calculateTileWidth(uint16_t tileIndex, uint16_t tileColumns, uint16_t rectWidth);
    static constexpr uint8_t calculateTileHeight(uint16_t tileIndex, uint16_t tileColumns, uint16_t tileRows, uint16_t rectHeight);
    uint32_t readTileData(const char* buffer, uint32_t bufferSize, orv_error_t* error);
    uint32_t readTileDataRaw(const char* buffer, uint32_t bufferSize, orv_error_t* error);
    uint32_t readTileDataSolidColor(const char* buffer, uint32_t bufferSize, orv_error_t* error);
    uint32_t readTileDataPackedPaletteTypes(const char* buffer, uint32_t bufferSize, orv_error_t* error);
    uint32_t readTileDataPlainRLE(const char* buffer, uint32_t bufferSize, orv_error_t* error);
    uint32_t readTileDataPaletteRLE(const char* buffer, uint32_t bufferSize, orv_error_t* error);
    static bool calculateMaxUncompressedDataSize(uint32_t* maxSize, uint32_t totalNumberOfTiles, uint8_t zrleBpp);
    static uint8_t calculateZrleBytesPerPixel(const orv_communication_pixel_format_t& pixelFormat, bool* omitLeastSignificantByte);
    static uint32_t maxBytesPerZRLETile();
    static uint32_t readRunLength(const uint8_t* buffer, uint32_t bufferSize, orv_error_t* error);

    static void makeUncompressedPixel(uint8_t* pixelColor, const uint8_t* compressedPixelColor, uint8_t zrleBytesPerPixel, uint8_t pixelFormatBytesPerPixel, uint8_t zrleByteOffsetOfUncompressedPixel);

    void writeRunLengthColorToCurrentRect(const uint8_t* uncompressedColor, uint16_t tileIndex, uint8_t tileWidth, uint32_t firstPixelIndex, uint32_t runLength);

private:
    static const uint8_t mMaxTileWidth = 64;
    static const uint8_t mMaxTileHeight = 64;
    RectDataParserZlibPlain mZlibPlainParser;
    uint8_t* mUncompressedData = nullptr;
    uint32_t mUncompressedDataMaxSize = 0;
    uint32_t mUncompressedDataOffset = 0;  // # of bytes successfully uncompressed
    uint32_t mUncompressedConsumedOffset;  // # of uncompressed bytes successfully parsed
    uint8_t mZrleBytesPerPixel = 0; // BPP for the ZRLE encoding, also known as bytesPerCPixel
    uint8_t mZrleByteOffsetOfUncompressedPixel = 0; // always 0 or 1.
    uint8_t* mCurrentRectData = nullptr;
    uint32_t mCurrentRectDataSize = 0;
    uint16_t mCurrentTileIndex = 0;
    uint16_t mExpectedTileRows = 0;
    uint16_t mExpectedTileColumns = 0;
    uint32_t mExpectedTotalTiles = 0;
    uint8_t mCurrentTileSubencodingType = 0;
    bool mCurrentTileSubencodingTypeRead = false;
    uint32_t mCurrentTileRLEPixelsDone = 0;
    bool mFinishedTile = false;

    uint8_t* mCurrentTileDataBuffer = nullptr; // Lazy initialized to maxBytesPerZRLETile(), i.e. can always hold a full tile
    uint32_t mCurrentTileDataBytesRead = 0;
};



inline bool RectDataParserRealRectBase::isPseudoEncoding() const
{
    return false;
}
inline bool RectDataParserCursor::isPseudoEncoding() const
{
    return true;
}

inline constexpr uint8_t RectDataParserHextile::calculateTileWidth(uint16_t tileIndex, uint16_t tileColumns, uint16_t rectWidth)
{
    // tiles have width 16, except last column, which may have less.
    return (((tileIndex % tileColumns) == tileColumns - 1) && ((rectWidth % 16) != 0))
        ?  (rectWidth % 16) : 16;
}

inline constexpr uint8_t RectDataParserHextile::calculateTileHeight(uint16_t tileIndex, uint16_t tileColumns, uint16_t tileRows, uint16_t rectHeight)
{
    // tiles have height  16, except last row, which may have less.
    return (((tileIndex / tileColumns) == tileRows - 1) && ((rectHeight % 16) != 0))
        ? (rectHeight % 16) : 16;
}

inline constexpr uint8_t RectDataParserZRLE::calculateTileWidth(uint16_t tileIndex, uint16_t tileColumns, uint16_t rectWidth)
{
    // tiles have width mMaxTileWidth, except last column, which may have less.
    return (((tileIndex % tileColumns) == tileColumns - 1) && ((rectWidth % mMaxTileWidth) != 0))
        ?  (rectWidth % mMaxTileWidth) : mMaxTileWidth;
}

inline constexpr uint8_t RectDataParserZRLE::calculateTileHeight(uint16_t tileIndex, uint16_t tileColumns, uint16_t tileRows, uint16_t rectHeight)
{
    // tiles have height mMaxTileHeight, except last row, which may have less.
    return (((tileIndex / tileColumns) == tileRows - 1) && ((rectHeight % mMaxTileHeight) != 0))
        ? (rectHeight % mMaxTileHeight) : mMaxTileHeight;
}

inline void RectDataParserZRLE::makeUncompressedPixel(uint8_t* pixelColor, const uint8_t* compressedPixelColor, uint8_t zrleBytesPerPixel, uint8_t pixelFormatBytesPerPixel, uint8_t zrleByteOffsetOfUncompressedPixel)
{
    // depending on zrleByteOffsetOfUncompressedPixel, we either have to set the first or the last
    // byte to 0
    pixelColor[0] = 0;
    pixelColor[pixelFormatBytesPerPixel - 1] = 0;
    uint8_t* p = pixelColor + zrleByteOffsetOfUncompressedPixel;
    for (int i = 0; i < zrleBytesPerPixel; i++) {
        p[i] = compressedPixelColor[i];
    }
}

} // namespace vnc
} // namespace openrv

#endif

