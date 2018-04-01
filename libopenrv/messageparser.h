#ifndef OPENRV_MESSAGEPARSER_H
#define OPENRV_MESSAGEPARSER_H

#include "orvvncclient.h"

#include <vector>

namespace openrv {
namespace vnc {

/**
 * Base class for classes that parse exactly one message of one type.
 *
 * A connection is meant to instantiate an object for each supported message type. The subclass
 * should implement reading partial messages (i.e. read 10 bytes of a 20 bytes message and continue
 * with the message later when the remaining data arrived).
 *
 * The data associated with the current message can be stored in the object, as only a single
 * message is parsed at a time and another message can only be started once the current message has
 * been flushed completely.
 *
 * NOTE: Arrays/buffers that are stored in objects of derived classes are normally intended to be
 *       @em moved to an @ref orv_event_t object once the message has been parsed completely.
 *
 * This class and its subclasses are used internally by the @ref openrv::vnc::ConnectionThread only.
 **/
class MessageParserBase
{
public:
    explicit MessageParserBase(struct orv_context_t* context);
    virtual ~MessageParserBase() = default;

    virtual ServerMessage messageType() const = 0;

    /**
     * @param buffer The buffer containing the message data. Initially pointing to the beginning of
     *        the message (the messageType byte).
     *        When multiple calls to this function are made for the same message, the buffer pointer
     *        must be moved to skip over the previously consumed bytes.
     * @param bufferSize The number of bytes available in @p buffer.
     * @param error An output error object (which must have been reset already). Set to an error
     *        value if something goes wrong, remains untouched otherwise.
     *        Must be initially reset (see @ref orv_error_reset()).
     * @return The number of consumed bytes, or 0 if more data is required before anything can be
     *         done.
     *         The caller must make sure only (exactly) the consumed bytes are removed from @p
     *         buffer in the next call. If the message was completed by this call, @ref isFinished()
     *         will return TRUE, otherwise FALSE.
     **/
    virtual uint32_t readData(const char* buffer, uint32_t bufferSize, orv_error_t* error) = 0;
    /**
     * @pre The current mesage is finished (@ref isFinished() is TRUE).
     * @return An allocated orv_error_t object that should be sent to the user, if any events should
     *         be generated, otherwise NULL.
     *         If this function returns non-NULL, the caller takes ownership of the returned pointer
     *         and is responsible for sending the event.
     **/
    virtual orv_event_t* processFinishedMessage(orv_error_t* error) = 0;
    virtual void reset();
    inline bool isFinished() const
    {
        return mIsFinished;
    }
    const char* messageTypeString() const;
protected:
    void sendEvent(orv_event_t* event);
protected:
    struct orv_context_t* mContext = nullptr;
    bool mIsFinished = false;
};

class RectDataParserBase;

class MessageParserFramebufferUpdate : public MessageParserBase
{
public:
    MessageParserFramebufferUpdate(struct orv_context_t* ctx, std::mutex* framebufferMutex, std::mutex* cursorMutex, orv_framebuffer_t* framebuffer, orv_cursor_t* cursorData, const orv_communication_pixel_format_t* currentPixelFormat, const uint16_t* currentFramebufferWidth, const uint16_t* currentFramebufferHeight);
    virtual ~MessageParserFramebufferUpdate();
    virtual void reset() override;
    virtual uint32_t readData(const char* buffer, uint32_t bufferSize, orv_error_t* error) override;
    virtual orv_event_t* processFinishedMessage(orv_error_t* error) override;
    virtual ServerMessage messageType() const override
    {
        return ServerMessage::FramebufferUpdate;
    }

    void resetConnection();

protected:
    struct RectHeader
    {
        bool mHasHeader = false;
        uint16_t mX = 0;
        uint16_t mY = 0;
        uint16_t mW = 0;
        uint16_t mH = 0;
        int32_t mEncodingType = 0;
        bool mRectFinished = false;
    };
protected:
    uint32_t readRect(const char* buffer, uint32_t bufferSize, orv_error_t* error);
    void clearRectEvents();
    RectDataParserBase* findRectParserForEncoding(EncodingType encodingType, orv_error_t* error);
    int addRectDataParser(RectDataParserBase* parser);
protected:
    std::mutex& mFramebufferMutex;
    std::mutex& mCursorMutex;      // NOTE: May be the same mutex as mFramebufferMutex! Never lock both!
    /**
     * Reference to framebuffer of @ref openrv::vnc::ConnectionThread.
     *
     * Protected by @ref mFramebufferMutex, all accesses em MUST lock the mutex first.
     **/
    orv_framebuffer_t& mFramebuffer;
    /**
     * Reference to cursor data of @ref openrv::vnc::ConnectionThread.
     *
     * Protected by @ref mCursorMutex, all accesses em MUST lock the mutex first.
     **/
    orv_cursor_t& mCursorData;
private:
    const orv_communication_pixel_format_t& mCurrentPixelFormat;
    const uint16_t& mCurrentFramebufferWidth;
    const uint16_t& mCurrentFramebufferHeight;
    bool mHasHeader = false;
    uint16_t mNumberOfRectanglesSent = 0;
    uint16_t mCurrentRectIndex = 0;
    RectHeader mCurrentRectHeader;
    RectDataParserBase* mCurrentRectParser = nullptr;
    orv_event_t** mRectEvent = nullptr;
    uint16_t mSentRectEvents = 0;
    std::vector<RectDataParserBase*> mAllRectDataParsers;
    int mParserRawIndex = -1;
    int mParserCopyRectIndex = -1;
    int mParserRREIndex = -1;
    int mParserCoRREIndex = -1;
    int mParserCursorIndex = -1;
    int mParserZlibIndex = -1;
    int mParserHextileIndex = -1;
    int mParserZRLEIndex = -1;
};
class MessageParserSetColourMapEntries : public MessageParserBase
{
public:
    explicit MessageParserSetColourMapEntries(struct orv_context_t* ctx)
        : MessageParserBase(ctx)
    {
    }
    ~MessageParserSetColourMapEntries()
    {
    }
    virtual void reset() override;
    virtual uint32_t readData(const char* buffer, uint32_t bufferSize, orv_error_t* error) override;
    virtual orv_event_t* processFinishedMessage(orv_error_t* error) override;
    virtual ServerMessage messageType() const override
    {
        return ServerMessage::SetColourMapEntries;
    }
private:
    bool mHasHeader = false;
    uint16_t* mRed = nullptr;
    uint16_t* mGreen = nullptr;
    uint16_t* mBlue = nullptr;
    uint16_t mNumberOfColours = 0;
    uint16_t mFirstColour = 0;
    uint16_t mReadColours = 0;
};

/**
 * Parser for "ServerCutText" messages from the server. The server sends these messages whenever the
 * clipboard data has changed on the server.
 **/
class MessageParserServerCutText : public MessageParserBase
{
public:
    explicit MessageParserServerCutText(struct orv_context_t* ctx)
        : MessageParserBase(ctx)
    {
    }
    ~MessageParserServerCutText()
    {
        free(mText);
        mText = nullptr;
    }
    virtual void reset() override;
    virtual uint32_t readData(const char* buffer, uint32_t bufferSize, orv_error_t* error) override;
    virtual orv_event_t* processFinishedMessage(orv_error_t* error) override;
    virtual ServerMessage messageType() const override
    {
        return ServerMessage::ServerCutText;
    }
private:
    bool mHasHeader = false;
    char* mText = nullptr;
    /**
     * Number of bytes that will be (or have been) copied to @ref mText.
     * Usually matches @ref mTextFullLength, but may be smaller, if server sent too much data (we
     * truncate to a maximum size then and skip remaining data from server).
     *
     * This is the size of the @ref mText buffer, @em excluding an additional byte in @ref mText for
     * NUL-termination.
     **/
    uint32_t mTextOutputLength = 0;
    /**
     * Total size of the text as reported by server. Message is incomplete until all bytes have been
     * consumed.
     **/
    uint32_t mTextFullLength = 0;
    /**
     * Bytes copied to @ref mText so far.
     **/
    uint32_t mTextCopiedLength = 0;
    /**
     * Bytes of the text consumed so far. Usually matches @ref mTextCopiedLength, but may be larger,
     * if we truncate the text from the server.
     *
     * The message is complete once this value matches @ref mTextFullLength.
     **/
    uint32_t mTextConsumed = 0;
};

} // namespace vnc
} // namespace openrv

#endif

