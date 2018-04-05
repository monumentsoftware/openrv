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

#include <algorithm>
#include <string.h>

#define UNUSED(x) (void)x

namespace openrv {
namespace vnc {

MessageParserBase::MessageParserBase(orv_context_t* context)
    : mContext(context)
{
}

const char* MessageParserBase::messageTypeString() const
{
    return OrvVncClient::getServerMessageTypeString(messageType());
}

/**
 * Reset this object for the next message. This function is called after completely reading a
 * message. This function resets internal state variables.
 *
 * Derived classes must call the base implementation.
 **/
void MessageParserBase::reset()
{
    mIsFinished = false;
}

/**
 * Send @p event using the event callback.
 *
 * This function takes ownership of @p event.
 **/
void MessageParserBase::sendEvent(orv_event_t* event)
{
    mContext->mConfig.mEventCallback(mContext, event);
}

MessageParserFramebufferUpdate::MessageParserFramebufferUpdate(struct orv_context_t* ctx, std::mutex* framebufferMutex, std::mutex* cursorMutex, orv_framebuffer_t* framebuffer, orv_cursor_t* cursorData, const orv_communication_pixel_format_t* currentPixelFormat, const uint16_t* currentFramebufferWidth, const uint16_t* currentFramebufferHeight)
    : MessageParserBase(ctx),
      mFramebufferMutex(*framebufferMutex),
      mCursorMutex(*cursorMutex),
      mFramebuffer(*framebuffer),
      mCursorData(*cursorData),
      mCurrentPixelFormat(*currentPixelFormat),
      mCurrentFramebufferWidth(*currentFramebufferWidth),
      mCurrentFramebufferHeight(*currentFramebufferHeight)
{
    mAllRectDataParsers.reserve(10);
    mParserRawIndex = addRectDataParser(new RectDataParserRaw(mContext, &mFramebufferMutex, &mFramebuffer, &mCurrentPixelFormat, &mCurrentFramebufferWidth, &mCurrentFramebufferHeight));
    mParserCopyRectIndex = addRectDataParser(new RectDataParserCopyRect(mContext, &mFramebufferMutex, &mFramebuffer, &mCurrentPixelFormat, &mCurrentFramebufferWidth, &mCurrentFramebufferHeight));
    mParserRREIndex = addRectDataParser(new RectDataParserRRE(mContext, &mFramebufferMutex, &mFramebuffer, &mCurrentPixelFormat, &mCurrentFramebufferWidth, &mCurrentFramebufferHeight, false));
    mParserCoRREIndex = addRectDataParser(new RectDataParserRRE(mContext, &mFramebufferMutex, &mFramebuffer, &mCurrentPixelFormat, &mCurrentFramebufferWidth, &mCurrentFramebufferHeight, true));
    mParserCursorIndex = addRectDataParser(new RectDataParserCursor(mContext, &mCursorMutex, &mCursorData, &mCurrentPixelFormat, &mCurrentFramebufferWidth, &mCurrentFramebufferHeight));
    mParserZlibIndex = addRectDataParser(new RectDataParserZlib(mContext, &mFramebufferMutex, &mFramebuffer, &mCurrentPixelFormat, &mCurrentFramebufferWidth, &mCurrentFramebufferHeight));
    mParserHextileIndex = addRectDataParser(new RectDataParserHextile(mContext, &mFramebufferMutex, &mFramebuffer, &mCurrentPixelFormat, &mCurrentFramebufferWidth, &mCurrentFramebufferHeight));
    mParserZRLEIndex = addRectDataParser(new RectDataParserZRLE(mContext, &mFramebufferMutex, &mFramebuffer, &mCurrentPixelFormat, &mCurrentFramebufferWidth, &mCurrentFramebufferHeight));
}

MessageParserFramebufferUpdate::~MessageParserFramebufferUpdate()
{
    clearRectEvents();
    for (RectDataParserBase* parser : mAllRectDataParsers) {
        parser->reset();
        delete parser;
    }
}

int MessageParserFramebufferUpdate::addRectDataParser(RectDataParserBase* parser)
{
    if (!parser) {
        return -1;
    }
    int index = (int)mAllRectDataParsers.size();
    mAllRectDataParsers.push_back(parser);
    return index;
}

/**
 * @return The @ref RectDataParserBase subclass object for @p encodingType, or NULL if the specified
 *         encoding is not valid or not supported.
 *         If this function returns NULL, @p error is set accordingly, otherwise @p error remains
 *         unmodified.
 **/
RectDataParserBase* MessageParserFramebufferUpdate::findRectParserForEncoding(EncodingType encodingType, orv_error_t* error)
{
    enum class EncodingResult {
        Ok,
        UnsupportedEncoding,
        InvalidEncoding,
    };
    EncodingResult encodingResult = EncodingResult::Ok;
    int parserIndex = -1;
    switch (encodingType) {
        case EncodingType::Raw:
            parserIndex = mParserRawIndex;
            break;
        case EncodingType::CopyRect:
            parserIndex = mParserCopyRectIndex;
            break;
        case EncodingType::RRE:
            parserIndex = mParserRREIndex;
            break;
        case EncodingType::Hextile:
            parserIndex = mParserHextileIndex;
            break;
        case EncodingType::ZRLE:
            parserIndex = mParserZRLEIndex;
            break;
        case EncodingType::Cursor: // pseudo-encoding
            parserIndex = mParserCursorIndex;
            break;
        case EncodingType::DesktopSize: // pseudo-encoding
            encodingResult = EncodingResult::InvalidEncoding;
            break;
        case EncodingType::CoRRE:
            parserIndex = mParserCoRREIndex;
            break;
        case EncodingType::zlib:
            parserIndex = mParserZlibIndex;
            break;
        case EncodingType::CursorWithAlpha: // pseudo-encoding
        case EncodingType::ContinuousUpdates: // pseudo-encoding
        case EncodingType::tight:
        case EncodingType::zlibhex:
        case EncodingType::TRLE:
        case EncodingType::HitachiZYWRLE:
        case EncodingType::AdamWallingXZ:
        case EncodingType::AdamWallingXZYW:
        case EncodingType::JPEG:
        case EncodingType::JRLE:
        case EncodingType::TightJpegQualityLevel0: // pseudo-encoding
        case EncodingType::TightJpegQualityLevel1: // pseudo-encoding
        case EncodingType::TightJpegQualityLevel2: // pseudo-encoding
        case EncodingType::TightJpegQualityLevel3: // pseudo-encoding
        case EncodingType::TightJpegQualityLevel4: // pseudo-encoding
        case EncodingType::TightJpegQualityLevel5: // pseudo-encoding
        case EncodingType::TightJpegQualityLevel6: // pseudo-encoding
        case EncodingType::TightJpegQualityLevel7: // pseudo-encoding
        case EncodingType::TightJpegQualityLevel8: // pseudo-encoding
        case EncodingType::TightJpegQualityLevel9: // pseudo-encoding
        case EncodingType::LastRect:
        case EncodingType::PointerPosition:
        case EncodingType::XCursor:
        case EncodingType::TightCompressionLevel:
        case EncodingType::gii:
        case EncodingType::popa:
        case EncodingType::PeterAstrandDesktopName:
        case EncodingType::PierreOssmanExtendedDesktopSize:
        case EncodingType::ColinDeanxvp:
        case EncodingType::OLIVECallControl:
        case EncodingType::Fence: // pseudo-encoding
        case EncodingType::JpegFineGrainedQualityLevel:
        case EncodingType::JpegSubsamplingLevel:
            encodingResult = EncodingResult::UnsupportedEncoding;
            break;
    }
    switch (encodingResult) {
        case EncodingResult::Ok:
            if (parserIndex < 0) {
                orv_error_set(error, ORV_ERR_GENERIC, 0, "Internal error with encoding %d: Invalid parser index %d", (int)encodingType, (int)parserIndex);
                return nullptr;
            }
            break;
        case EncodingResult::InvalidEncoding:
            orv_error_set(error, ORV_ERR_INVALID_ENCODING, 0, "Encoding %d used by server but is not a valid encoding", (int)encodingType);
            return nullptr;
        case EncodingResult::UnsupportedEncoding:
            orv_error_set(error, ORV_ERR_UNSUPPORTED_ENCODING, 0, "Encoding %d used by server but not supported by this client", (int)encodingType);
            return nullptr;
    }
    if ((size_t)parserIndex >= mAllRectDataParsers.size()) {
        orv_error_set(error, ORV_ERR_GENERIC, 0, "Internal error with encoding %d: Invalid parser index %u, exceeds maximum value %d", (int)encodingType, (unsigned int)parserIndex, (int)mAllRectDataParsers.size());
        return nullptr;
    }
    return mAllRectDataParsers[parserIndex];
}


void MessageParserFramebufferUpdate::clearRectEvents()
{
    if (mRectEvent) {
        for (uint16_t i = 0; i < mNumberOfRectanglesSent; i++) {
            if (mRectEvent[i]) {
                orv_event_destroy(mRectEvent[i]);
            }
        }
        free(mRectEvent);
        mRectEvent = nullptr;
    }
}

void MessageParserFramebufferUpdate::reset()
{
    MessageParserBase::reset();
    mHasHeader = false;
    mNumberOfRectanglesSent = 0;
    mCurrentRectIndex = 0;
    mCurrentRectHeader = RectHeader();
    clearRectEvents();
    mSentRectEvents = 0;
}

/**
 * Called to indicate that the connection was closed and the parser should be prepared for a new
 * connection.
 *
 * This function makes sure that resources that persist over messages (see e.g. @ref
 * RectDataParserZlib) are freed or re-created for the new connection.
 **/
void MessageParserFramebufferUpdate::resetConnection()
{
    reset();
    for (RectDataParserBase* r : mAllRectDataParsers) {
        r->resetConnection();
    }
}

uint32_t MessageParserFramebufferUpdate::readData(const char* buffer, uint32_t bufferSize, orv_error_t* error)
{
    uint32_t consumed = 0;
    if (!mHasHeader) {
        // message has at least 4 bytes
        if (bufferSize < 4) {
            ORV_DEBUG(mContext, "MessageParserFramebufferUpdate::readData(): need more data");
            return 0;
        }
        mHasHeader = true;
        consumed += 4;
        mNumberOfRectanglesSent = Reader::readUInt16(buffer + 2);
        if (mNumberOfRectanglesSent == 0) {
            mIsFinished = true;
        }
        clearRectEvents();
        mRectEvent = (orv_event_t**)malloc(mNumberOfRectanglesSent * sizeof(orv_event_t*));
        memset(mRectEvent, 0, mNumberOfRectanglesSent * sizeof(orv_event_t*));
        mSentRectEvents = 0;
        ORV_DEBUG(mContext, "Received header of FramebufferUpdate message, numberOfRectangles: %d", (int)mNumberOfRectanglesSent);
    }

    bool needMoreData = false;
    while (!needMoreData && !mIsFinished) {
        uint32_t c = readRect(buffer + consumed, bufferSize - consumed, error);
        if (error->mHasError) {
            return 0;
        }
        consumed += c;
        if (mCurrentRectHeader.mRectFinished) {
            mCurrentRectIndex++;
            mCurrentRectHeader = RectHeader();
            if (mCurrentRectIndex >= mNumberOfRectanglesSent) {
                ORV_DEBUG(mContext, "All %d rectangles received and processed, message finished.", (int)mNumberOfRectanglesSent);
                mIsFinished = true;
            }
        }
        else {
            // NOTE: If readRect() consumed data but did not yet finish rect, we should call
            //       readRect() again, before assuming we need more data.
            //       -> some rect data parsers (especially for encodings that use sub-rects) may
            //          read partial data only and immediately continue on the next call.
            if (c == 0 || consumed >= bufferSize) {
                needMoreData = true;
            }
        }
    }
    if (!error->mHasError && mCurrentRectIndex == mNumberOfRectanglesSent + 1) {
        mIsFinished = true;
    }
    return consumed;
}

orv_event_t* MessageParserFramebufferUpdate::processFinishedMessage(orv_error_t* error)
{
    // TODO: support for partial messages:
    //       add processPartialMessage(), that sends events up to the mCurrentRectIndex and sets
    //       mSentRectEvents accordingly
    //       -> this way we can process data even before all rects have arrived
    for (uint16_t i = mSentRectEvents; i < mNumberOfRectanglesSent; i++) {
        orv_event_t* e = mRectEvent[i];
        if (!e) {
            // NULL rect events are generated for pseudo-encoding - this is valid.
            mSentRectEvents++; // not technically "sent", but fully processed.
            continue;
        }
        mRectEvent[i] = nullptr;

        // NOTE: ownership of event is passed
        sendEvent(e);
        mSentRectEvents++;
    }
    if (mSentRectEvents >= mNumberOfRectanglesSent) {
        return orv_event_init(ORV_EVENT_FRAMEBUFFER_UPDATE_REQUEST_FINISHED);
    }
    return nullptr;
}

/**
 * @param error Output parameter that receives errors encountered in this function.
 *        Must be initially reset (see @ref orv_error_reset()).
 **/
uint32_t MessageParserFramebufferUpdate::readRect(const char* buffer, uint32_t bufferSize, orv_error_t* error)
{
    if (error->mHasError) {
        return 0;
    }
    uint32_t consumed = 0;
    if (!mCurrentRectHeader.mHasHeader) {
        if (mCurrentRectParser) {
            mCurrentRectParser->reset();
            mCurrentRectParser = nullptr;
        }

        // each rect has 12 bytes header + n bytes encoding-specific data
        if (bufferSize < 12) {
            return 0;
        }
        mCurrentRectHeader.mX = Reader::readUInt16(buffer + 0);
        mCurrentRectHeader.mY = Reader::readUInt16(buffer + 2);
        mCurrentRectHeader.mW = Reader::readUInt16(buffer + 4);
        mCurrentRectHeader.mH = Reader::readUInt16(buffer + 6);
        mCurrentRectHeader.mEncodingType = Reader::readInt32(buffer + 8);
        mCurrentRectHeader.mHasHeader = true;
        consumed += 12;
        ORV_DEBUG(mContext, "Received header of rectangle %d (of %d): x=%d, y=%d, size: %dx%d, encoding: %d", (int)mCurrentRectIndex+1, (int)mNumberOfRectanglesSent, (int)mCurrentRectHeader.mX, (int)mCurrentRectHeader.mY, (int)mCurrentRectHeader.mW, (int)mCurrentRectHeader.mH, (int)mCurrentRectHeader.mEncodingType);

        // Ensure the received rect header fits into the framebuffer
        if ((uint32_t)mCurrentRectHeader.mX + (uint32_t)mCurrentRectHeader.mW > mCurrentFramebufferWidth ||
            (uint32_t)mCurrentRectHeader.mY + (uint32_t)mCurrentRectHeader.mH > mCurrentFramebufferHeight) {
            orv_error_set(error, ORV_ERR_PROTOCOL_ERROR, 20, "Invalid rect received, exceeds framebuffer dimensions. Rect: %dx%d at %dx%d, framebuffer: %ux%u", (int)mCurrentRectHeader.mW, (int)mCurrentRectHeader.mH, (int)mCurrentRectHeader.mX, (int)mCurrentRectHeader.mY, (unsigned int)mCurrentFramebufferWidth, (unsigned int)mCurrentFramebufferHeight);
            return 0;
        }

        mCurrentRectParser = findRectParserForEncoding((EncodingType)mCurrentRectHeader.mEncodingType, error);
        if (!mCurrentRectParser) {
            // error has been set
            return 0;
        }
        mCurrentRectParser->reset();
        mCurrentRectParser->setCurrentRect(mCurrentRectHeader.mX, mCurrentRectHeader.mY, mCurrentRectHeader.mW, mCurrentRectHeader.mH);
    }

    // sanity checks
    if (mCurrentRectIndex >= mNumberOfRectanglesSent) {
        orv_error_set(error, ORV_ERR_GENERIC, 0, "Invalid current rect number %d out of %d total rectangles", (int)mCurrentRectIndex+1, (int)mNumberOfRectanglesSent);
        return 0;
    }
    if (!mRectEvent) {
        orv_error_set(error, ORV_ERR_GENERIC, 0, "Internal error, mRectEvent not initialized");
        return 0;
    }
    if (mRectEvent[mCurrentRectIndex] != nullptr) {
        orv_error_set(error, ORV_ERR_GENERIC, 0, "Internal error, mRectEvent at index %d not NULL", (int)mCurrentRectIndex);
        return 0;
    }
    if (!mCurrentRectParser) {
        orv_error_set(error, ORV_ERR_GENERIC, 0, "Internal error with encoding %d: No parser object set", (int)mCurrentRectHeader.mEncodingType);
        return 0;
    }

    consumed += mCurrentRectParser->readRectData(buffer + consumed, bufferSize - consumed, error);
    if (error->mHasError) {
        mCurrentRectParser->reset();
        return 0;
    }
    // TODO: support for partial rects.
    //       probably rename canFinishRect() to canFinishPartialRect()
    if (mCurrentRectParser->canFinishRect()) {
        const bool isPseudoEncoding = mCurrentRectParser->isPseudoEncoding();
        mCurrentRectParser->finishRect(error);
        mCurrentRectParser->reset();
        if (error->mHasError) {
            return 0;
        }
        mCurrentRectHeader.mRectFinished = true;
        ORV_DEBUG(mContext, "Finished performing framebuffer update for %s data", OrvVncClient::getEncodingTypeString((EncodingType)mCurrentRectHeader.mEncodingType));
        if (mRectEvent[mCurrentRectIndex] != nullptr) {
            orv_event_destroy(mRectEvent[mCurrentRectIndex]);
            mRectEvent[mCurrentRectIndex] = nullptr;
        }
        if (!isPseudoEncoding) {
            mRectEvent[mCurrentRectIndex] = orv_event_framebuffer_init(mCurrentRectHeader.mX, mCurrentRectHeader.mY, mCurrentRectHeader.mW, mCurrentRectHeader.mH);
        }
        else {
            // pseudo-encodings.
            //
            // For pseudo-encodings we need special handling w.r.t. events. Contrary to rects, a
            // simple 1:1 relationship of rects to events may not be sufficient. For now we simply
            // do the special handling here, instead of providing a generic API in
            // RectDataParserBase.
            switch ((EncodingType)mCurrentRectHeader.mEncodingType) {
                default:
                    // no event generated
                    break;
                case EncodingType::Cursor:
                    // a single event is generated, we use mRectEvent for delivery (as convenience
                    // only, this is not actually a rect event).
                    mRectEvent[mCurrentRectIndex] = orv_event_init(ORV_EVENT_CURSOR_UPDATED);
                    break;
            }
        }
    }

    return consumed;
}


void MessageParserSetColourMapEntries::reset()
{
    MessageParserBase::reset();
    mHasHeader = false;
    free(mRed);
    mRed = nullptr;
    free(mGreen);
    mGreen = nullptr;
    free(mBlue);
    mBlue = nullptr;
    mFirstColour = 0;
    mNumberOfColours = 0;
    mReadColours = 0;
}

uint32_t MessageParserSetColourMapEntries::readData(const char* buffer, uint32_t bufferSize, orv_error_t* error)
{
    UNUSED(error);
    uint32_t consumed = 0;
    if (!mHasHeader) {
        // message has at least 6 bytes
        if (bufferSize < 6) {
            return 0;
        }
        mHasHeader = true;
        consumed += 6;
        mFirstColour = Reader::readUInt16(buffer + 2);
        mNumberOfColours = Reader::readUInt16(buffer + 4);
        ORV_DEBUG(mContext, "Have SetColourMapEntries header, first colour: %u, numberOfColours: %u", (unsigned int) mFirstColour, (unsigned int)mNumberOfColours);
        if (mNumberOfColours > 0) {
            free(mRed);
            free(mGreen);
            free(mBlue);
            mRed = (uint16_t*)malloc(mNumberOfColours * sizeof(uint16_t));
            mGreen = (uint16_t*)malloc(mNumberOfColours * sizeof(uint16_t));
            mBlue = (uint16_t*)malloc(mNumberOfColours * sizeof(uint16_t));
        }
    }
    while (mReadColours < mNumberOfColours && consumed + 6 <= bufferSize) {
        mRed[mReadColours] = Reader::readUInt16(buffer + consumed + 0);
        mGreen[mReadColours] = Reader::readUInt16(buffer + consumed + 2);
        mBlue[mReadColours] = Reader::readUInt16(buffer + consumed + 4);
        ORV_DEBUG(mContext, "Color %d: %d/%d/%d", (int)mReadColours, (int)mRed[mReadColours], (int)mGreen[mReadColours], (int)mBlue[mReadColours]);
        consumed += 6;
        mReadColours++;
    }
    if (mReadColours >= mNumberOfColours) {
        mIsFinished = true;
    }
    return consumed;
}

orv_event_t* MessageParserSetColourMapEntries::processFinishedMessage(orv_error_t* error)
{
    UNUSED(error);
    ORV_WARNING(mContext, "TODO: process finished SetColourMapEntries (send event)");
    return nullptr;
}

void MessageParserServerCutText::reset()
{
    MessageParserBase::reset();
    mHasHeader = false;
    mTextFullLength = 0;
    mTextCopiedLength = 0;
    free(mText);
    mText = nullptr;
    mTextOutputLength = 0;
    mTextConsumed = 0;
}

uint32_t MessageParserServerCutText::readData(const char* buffer, uint32_t bufferSize, orv_error_t* error)
{
    UNUSED(error);
    uint32_t consumed = 0;
    if (!mHasHeader) {
        // message has at least 8 bytes
        if (bufferSize < 8) {
            return 0;
        }
        // byte 0 is type, bytes 1..3 are padding.
        mTextFullLength = Reader::readUInt32(buffer + 4);
        mHasHeader = true;
        consumed += 8;

        uint32_t requiredCapacity = mTextFullLength;
        if (mTextFullLength > ORV_MAX_VNC_SERVER_CUT_TEXT_SIZE) {
            ORV_WARNING(mContext, "Have ServerCutText with %u bytes, truncating to %u bytes.", (unsigned int)mTextFullLength, (uint32_t)ORV_MAX_VNC_SERVER_CUT_TEXT_SIZE);
            requiredCapacity = ORV_MAX_VNC_SERVER_CUT_TEXT_SIZE;
        }
        free(mText);
        mText = (char*)malloc(requiredCapacity + 1);
        mTextOutputLength = requiredCapacity;
        mText[0] = '\0';
        mText[requiredCapacity] = '\0';
        if (mTextFullLength == 0) {
            mIsFinished = true;
        }
    }
    if (consumed >= bufferSize) {
        return consumed;
    }
    //
    // Copy text to output buffer if (and only if) we do not exceed mTextOutputLength.
    // Otherwise we consume remaining data without copying it.
    //
    if (mTextCopiedLength < mTextOutputLength) {
        uint32_t copy = std::min(bufferSize - consumed, mTextOutputLength - mTextCopiedLength);
        memcpy(mText + mTextCopiedLength, buffer + consumed, mTextOutputLength - mTextCopiedLength);
        mTextCopiedLength += copy;
        // TODO: check that all bytes are in valid latin-1 range
        //       (newlines are encoded with \n, no \r)
        if (mTextCopiedLength == mTextOutputLength) {
            mText[mTextCopiedLength] = '\0';
            //ORV_DEBUG(mContext, "ServerCutText: '%s'", mText);
            mIsFinished = true;
        }
    }
    uint32_t consumeLength = std::min(bufferSize - consumed, mTextFullLength - mTextConsumed);
    mTextConsumed += consumeLength;
    consumed += consumeLength;
    return consumed;
}

orv_event_t* MessageParserServerCutText::processFinishedMessage(orv_error_t* error)
{
    UNUSED(error);
    orv_event_t* event = orv_event_init(ORV_EVENT_CUT_TEXT);
    orv_cut_text_t* data = (orv_cut_text_t*)event->mEventData;
    data->mText = mText; // transfer ownership to the event
    mText = nullptr;
    return event;
}

} // namespace vnc
} // namespace openrv

