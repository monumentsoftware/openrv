#include "framebufferwidget.h"

#include <libopenrv/libopenrv.h>

#include <QtCore/QCoreApplication>
#include <QtGui/QPaintEvent>
#include <QtGui/QPainter>
#include <QtCore/QtDebug>

#define CHECK_GL_ERROR(x) { GLenum error = glGetError(); if (error != GL_NO_ERROR) { qCritical() << "GL error" << QString::number(error, 16) << "in" << __FILE__ << "at line" << __LINE__ << "additional info:" << (x); }}

static void printLog(FramebufferWidget* w, uint8_t compiler, GLuint object, const char* which)
{
    GLint logLength = 0;
    if (compiler == 1) {
        w->glGetShaderiv(object, GL_INFO_LOG_LENGTH, &logLength);
    }
    else {
        w->glGetProgramiv(object, GL_INFO_LOG_LENGTH, &logLength);
    }
    logLength = qMin(logLength, (GLint)1024*1024*10);
    char* log = (char*)malloc(logLength + 1);
    GLint length = 0;
    if (compiler == 1) {
        w->glGetShaderInfoLog(object, logLength, &length, log);
    }
    else {
        w->glGetProgramInfoLog(object, logLength, &length, log);
    }
    log[length] = '\0';
    if (compiler == 0) {
        qCritical() << "Failed to compile" << which << "shader. Log:";
    }
    else {
        qCritical() << "Failed to link shader program. Log:";
    }
    qCritical() << log;
    free(log);
}

// NOTE: The OpenGL framebuffer format is set before construction of th QApplication object due to
//       some platform limitations.
//       Note in particular: This widget does not require a depth buffer, only a RGB color buffer.
FramebufferWidget::FramebufferWidget(QWidget* parent)
    : QOpenGLWidget(parent)
{
}

FramebufferWidget::~FramebufferWidget()
{
    delete[] mDebugBitPlanesFramebuffer;
    if (mGLInitialized) {
        makeCurrent();
        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        glUseProgram(0);
        glDetachShader(mShaderProgram, mVertexShader);
        glDetachShader(mShaderProgram, mFragmentShader);
        glDeleteShader(mVertexShader);
        glDeleteShader(mFragmentShader);
        glDeleteProgram(mShaderProgram);
        glDeleteTextures(1, &mFramebufferTexture);
        glDeleteTextures(1, &mCursorTexture);
        glDeleteBuffers(1, &mBufferObject);
        glDeleteVertexArrays(1, &mVertexArrayObject);
        CHECK_GL_ERROR("destructor");
        doneCurrent();
    }
}

/**
 * Set whether the framebuffer should be scaled to fit the size of this widget. If TRUE, the widget
 * will always display the contents of the framebuffer, otherwise scrolling may be required (see
 * @ref setFramebufferOffset())
 **/
void FramebufferWidget::setScaleFramebuffer(bool scale)
{
    mScaleFramebuffer = scale;
}

/**
 * Set the offset of the displayed framebuffer to @p x and @p y. The values are clamped to the
 * maximum available range.
 *
 * If @ref scaleFramebuffer() is TRUE, this offset has no effect.
 **/
void FramebufferWidget::setFramebufferOffset(int x, int y)
{
    x = qMax(0, qMin(framebufferWidth() - width(), x));
    y = qMax(0, qMin(framebufferHeight() - height(), y));
    mFramebufferOffsetX = x;
    mFramebufferOffsetY = y;
}

/**
 * Set this widget to hold a framebuffer of the specified size. Any previous framebuffer is
 * discarded.
 *
 * The framebuffer of this widget will not contain any image data until @ref updateFramebuffer() is
 * called again.
 **/
void FramebufferWidget::resetFramebuffer(uint16_t width, uint16_t height)
{
    mFramebufferWidth = width;
    mFramebufferHeight = height;
    setFramebufferOffset(mFramebufferOffsetX, mFramebufferOffsetY); // apply/update clamping to valid range
    mFramebufferTextureInitialized = false;
}


/**
 * Make this widget copy the specified rect from @p framebuffer to the internal framebuffer texture.
 *
 * If this is the first call to this function since full OpenGL initialization of this widget (or
 * since the last framebuffer reset, see @ref resetFramebuffer()), the provided rectangle will be
 * ignored and the full framebuffer is copied.
 *
 * This function schedules a paint event on the updated region using @ref update().
 **/
void FramebufferWidget::updateFramebuffer(const struct orv_framebuffer_t* framebuffer, uint16_t x, uint16_t y, uint16_t width, uint16_t height)
{
    if (!mGLInitialized) {
        // caller has to call again once GL was initialized.
        // no need to save location of rect, we'll have to fetch full framebuffer anyway.
        return;
    }
    if (framebuffer->mBytesPerPixel != 3) {
        // ERROR: cannot handle this
        qCritical("Unexpected mBytesPerPixel in framebuffer");
        return;
    }
    if (framebuffer->mWidth != mFramebufferWidth ||
        framebuffer->mHeight != mFramebufferHeight) {
        qCritical() << "ERROR: Framebuffer not properly initialized, remote framebuffer size: " << framebuffer->mWidth << "x" << framebuffer->mHeight << ", local framebuffer size: " << mFramebufferWidth << "x" << mFramebufferHeight;
        return;
    }

    if (mDebugBitPlanes > 0) {
        uint8_t bitMask = 0;
        for (int i = 0; i < mDebugBitPlanes; i++) {
            bitMask |= (0x1 << (7-i));
        }
        if (!mFramebufferTextureInitialized) {
            const uint32_t size = mFramebufferWidth * mFramebufferWidth * framebuffer->mBytesPerPixel;
            delete[] mDebugBitPlanesFramebuffer;
            mDebugBitPlanesFramebuffer = new uint8_t[size];
            memcpy(mDebugBitPlanesFramebuffer, framebuffer->mFramebuffer, size);
            for (uint32_t i = 0; i < size; i++) {
                // NOTE: Assuming RGB data (for RGBA we have to exclude alpha)
                mDebugBitPlanesFramebuffer[i] &= bitMask;
            }
        }
        else {
            for (uint32_t curY = y; curY < y + height; curY++) {
                uint8_t* dstRow = mDebugBitPlanesFramebuffer + curY * mFramebufferWidth * framebuffer->mBytesPerPixel;
                const uint8_t* srcRow = framebuffer->mFramebuffer + curY * mFramebufferWidth * framebuffer->mBytesPerPixel;
                const uint32_t rowSize = width * framebuffer->mBytesPerPixel;
                memcpy(dstRow, srcRow, rowSize);
                for (uint32_t i = 0; i < rowSize; i++) {
                    // NOTE: Assuming RGB data (for RGBA we have to exclude alpha)
                    dstRow[i] &= bitMask;
                }
            }
        }
    }

    makeCurrent();
    CHECK_GL_ERROR("before updating framebuffer texture");
    if (!mFramebufferTextureInitialized) {
        // updated rect is ignored - we copy the full data.
        CHECK_GL_ERROR("before initializing framebuffer texture");
        glBindTexture(GL_TEXTURE_2D, mFramebufferTexture);
        const GLint level = 0;
        const GLint internalFormat = GL_RGB;
        const GLint border = 0;
        const GLenum format = GL_RGB;
        /* NOTE: Our framebuffer texture matches the OpenRV framebuffer size exactly, which can easily be
         *       a completely random number (especially for virtual machines).
         *       In particular, the numbers may not be aligned properly to 4 bytes (GL default), so we
         *       have to either modify the data manually to proper alignment or request GL to use 1 byte
         *       alignment.
         */
        // NOTE: We set this value every time, in case some external code (e.g. Qt) reverts it to
        //       OpenGL default (4). However we do NOT revert it to its default after use!
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        const uint8_t* framebufferBase = framebuffer->mFramebuffer;
        if (mDebugBitPlanes > 0) {
            framebufferBase = mDebugBitPlanesFramebuffer;
        }
        glTexImage2D(GL_TEXTURE_2D, level, internalFormat, mFramebufferWidth, mFramebufferHeight, border, format, GL_UNSIGNED_BYTE, framebufferBase);
        CHECK_GL_ERROR("after initializing framebuffer texture");
        mFramebufferTextureInitialized = true;
    }
    else {
        if ((int)x + (int)width > mFramebufferWidth ||
            (int)y + (int)height > mFramebufferHeight) {
            qCritical() << "ERROR: Invalid rect received, exceeds valid framebuffer size";
            return;
        }
        glBindTexture(GL_TEXTURE_2D, mFramebufferTexture);
        const GLint level = 0;
        const GLenum format = GL_RGB;
        /* NOTE: Our framebuffer texture matches the OpenRV framebuffer size exactly, which can easily be
         *       a completely random number (especially for virtual machines).
         *       In particular, the numbers may not be aligned properly to 4 bytes (GL default), so we
         *       have to either modify the data manually to proper alignment or request GL to use 1 byte
         *       alignment.
         */
        // NOTE: We set this value every time, in case some external code (e.g. Qt) reverts it to
        //       OpenGL default (4). However we do NOT revert it to its default after use!
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, mFramebufferWidth);
        const uint8_t* framebufferBase = framebuffer->mFramebuffer;
        if (mDebugBitPlanes > 0) {
            framebufferBase = mDebugBitPlanesFramebuffer;
        }
        glTexSubImage2D(GL_TEXTURE_2D, level, x, y, width, height, format, GL_UNSIGNED_BYTE, framebufferBase + (x + y * mFramebufferWidth) * framebuffer->mBytesPerPixel);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    }
    // schedule a paint event
    update();
    CHECK_GL_ERROR("after updating framebuffer texture");
}

void FramebufferWidget::updateCursor(const struct orv_cursor_t* cursor)
{
    if (!cursor || !cursor->mIsValid) {
        mCursorTextureInitialized = false;
        return;
    }
    if (!mGLInitialized) {
        // caller has to call again once GL was initialized.
        // no need to save location of rect, we'll have to fetch full cursor anyway.
        return;
    }
    if (cursor->mBytesPerPixel != 4) {
        // ERROR: cannot handle this
        qCritical("Unexpected mBytesPerPixel in cursor");
        return;
    }
    makeCurrent();
    CHECK_GL_ERROR("before updating cursor texture");

    glBindTexture(GL_TEXTURE_2D, mCursorTexture);
    const GLint level = 0;
    const GLint internalFormat = GL_RGBA;
    const GLint border = 0;
    const GLenum format = GL_RGBA;
    // NOTE: We set this value every time, in case some external code (e.g. Qt) reverts it to
    //       OpenGL default (4). However we do NOT revert it to its default after use!
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, level, internalFormat, cursor->mWidth, cursor->mHeight, border, format, GL_UNSIGNED_BYTE, cursor->mCursor);
    mCursorTextureInitialized = true;
    mCursorWidth = cursor->mWidth;
    mCursorHeight = cursor->mHeight;
    mCursorHotspotX = cursor->mHotspotX;
    mCursorHotspotY = cursor->mHotspotY;

    // schedule a paint event
    update();
    CHECK_GL_ERROR("after updating cursor texture");
}

/**
 * Convenience function that triggers an initial framebuffer update from @p framebuffer to the
 * internal framebuffer texture.
 *
 * This function can be used as a reaction to the @ref initializedOpenGL() signal, if OpenRV
 * framebuffer data is already available.
 *
 * Note that this function is completely optional: A call to @ref updateFramebuffer() will
 * initialize the data anyway, if required. This function is meant so that there is no need to wait
 * for the next framebuffer update after GL initialization.
 *
 * If the framebuffer texture was already initialized, this function does nothing.
 **/
void FramebufferWidget::initializeFramebuffer(const struct orv_framebuffer_t* framebuffer)
{
    if (!mGLInitialized) {
        return;
    }
    if (mFramebufferTextureInitialized) {
        return;
    }
    // dummy rectangle: will be ignored, full update is required anyway.
    updateFramebuffer(framebuffer, 0, 0, 1, 1);
}

void FramebufferWidget::initializeGL()
{
    if (mGLInitialized) {
        return;
    }
    initializeOpenGLFunctions(); // NOTE: gl*() function calls here are actually calls into Qt wrappers for the functions!
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    mShaderProgram = glCreateProgram();
    mVertexShader = glCreateShader(GL_VERTEX_SHADER);
    mFragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glAttachShader(mShaderProgram, mVertexShader);
    glAttachShader(mShaderProgram, mFragmentShader);
    // NOTE:
    // OpenGL 2.0 => GLSL 1.10
    // OpenGL 2.1 => GLSL 1.20
    // OpenGL 3.0 => GLSL 1.30
    // OpenGL 3.1 => GLSL 1.40
    // OpenGL 3.2 => GLSL 1.50
    // for OpenGL >= 3.3, the GLSL version matches the OpenGL version (OpenGL 3.3 uses GLSL 3.30)
    // note that GLSL for ES uses different versions.
    // NOTE: we use at least OpenGL 3.2, because we use a core profile.
    const char* vertexShaderString = R"raw_string(
#version 150 core
uniform vec2 framebufferOffset;
uniform vec2 framebufferScale;
uniform vec2 vertexOffset; // used to render the cursor
uniform vec2 vertexScale; // used to render the cursor
in vec2 in_vertex;
in vec2 in_textureCoordinates;
out vec2 frag_textureCoordinates;
void main()
{
    gl_Position = vec4(in_vertex * vertexScale + vertexOffset, 0, 1);
    frag_textureCoordinates = in_textureCoordinates * framebufferScale + framebufferOffset;
}
    )raw_string";
    const char* fragmentShaderString = R"raw_string(
#version 150 core
uniform sampler2D framebufferTexture;
in vec2 frag_textureCoordinates;
out vec4 out_fragment;
void main()
{
    if (frag_textureCoordinates.x > 1.0 || frag_textureCoordinates.y > 1.0) {
        out_fragment = vec4(0, 0, 0, 1);
    }
    else {
        out_fragment = texture(framebufferTexture, frag_textureCoordinates);
        if (out_fragment.a < 0.5) {
            discard;
        }
    }
}
    )raw_string";

    glShaderSource(mVertexShader, 1, &vertexShaderString, nullptr);
    glShaderSource(mFragmentShader, 1, &fragmentShaderString, nullptr);
    CHECK_GL_ERROR("initializeGL: glShaderSource");
    glCompileShader(mVertexShader);
    CHECK_GL_ERROR("initializeGL: compile vertex shader");
    glCompileShader(mFragmentShader);
    CHECK_GL_ERROR("initializeGL: compile fragment shader");
    int compileStatusVertex = GL_FALSE;
    int compileStatusFragment = GL_FALSE;
    glGetShaderiv(mVertexShader, GL_COMPILE_STATUS, &compileStatusVertex);
    glGetShaderiv(mFragmentShader, GL_COMPILE_STATUS, &compileStatusFragment);
    if (compileStatusVertex != GL_TRUE || compileStatusFragment != GL_TRUE) {
        if (compileStatusVertex != GL_TRUE) {
            printLog(this, 1, mVertexShader, "vertex");
        }
        if (compileStatusFragment != GL_TRUE) {
            printLog(this, 1, mFragmentShader, "fragment");
        }
        QCoreApplication::exit(1);
        return;
    }
    glBindAttribLocation(mShaderProgram, 0, "in_vertex");
    CHECK_GL_ERROR("initializeGL: glBindAttribLocation()");
    glLinkProgram(mShaderProgram);
    CHECK_GL_ERROR("initializeGL: linking shader");
    int linkStatus = GL_FALSE;
    glGetProgramiv(mShaderProgram, GL_LINK_STATUS, &linkStatus);
    if (linkStatus != GL_TRUE) {
        printLog(this, 0, mShaderProgram, "");
        QCoreApplication::exit(1);
    }
    glUseProgram(mShaderProgram);
    glGenTextures(1, &mFramebufferTexture);
    glBindTexture(GL_TEXTURE_2D, mFramebufferTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    mFramebufferTextureInitialized = false;
    GLint textureLocation = glGetUniformLocation(mShaderProgram, "framebufferTexture");
    glUniform1i(textureLocation, 0);
    mFramebufferOffsetLocation = glGetUniformLocation(mShaderProgram, "framebufferOffset");
    glUniform2f(mFramebufferOffsetLocation, 0.0f, 0.f);
    mFramebufferScaleLocation = glGetUniformLocation(mShaderProgram, "framebufferScale");
    glUniform2f(mFramebufferScaleLocation, 1.0f, 1.f);
    mVertexOffsetLocation = glGetUniformLocation(mShaderProgram, "vertexOffset");
    glUniform2f(mVertexOffsetLocation, 0.0f, 0.f);
    mVertexScaleLocation = glGetUniformLocation(mShaderProgram, "vertexScale");
    glUniform2f(mVertexScaleLocation, 1.0f, 1.f);

    mVertexAttribLocation = glGetAttribLocation(mShaderProgram, "in_vertex");
    mTextureAttribCoordinatesLocation = glGetAttribLocation(mShaderProgram, "in_textureCoordinates");

    glGenBuffers(1, &mBufferObject);
    glBindBuffer(GL_ARRAY_BUFFER, mBufferObject);
    const GLfloat vertexAndTextureData[] = {
        // vertices (x,y)
        -1.0, -1.0,
        1.0, -1.0,
        -1.0, 1.0,
        1.0, -1.0,
        1.0, 1.0,
        -1.0, 1.0,

        // texture coordinates
        0.0, 1.0,
        1.0, 1.0,
        0.0, 0.0,
        1.0, 1.0,
        1.0, 0.0,
        0.0, 0.0
    };
    mBufferObjectVertexCount = 6;
    const GLint vertexComponentCount = 2;
    const GLint textureComponentCount = 2;
    glBufferData(GL_ARRAY_BUFFER, mBufferObjectVertexCount * (vertexComponentCount + textureComponentCount) * sizeof(GL_FLOAT), vertexAndTextureData, GL_STATIC_DRAW);

    glGenVertexArrays(1, &mVertexArrayObject);
    glBindVertexArray(mVertexArrayObject);
    const GLsizei stride = 0;
    const GLboolean normalized = GL_FALSE;
    glEnableVertexAttribArray(mVertexAttribLocation);
    glVertexAttribPointer(mVertexAttribLocation, vertexComponentCount, GL_FLOAT, normalized, stride, (void*)(intptr_t)0);
    glEnableVertexAttribArray(mTextureAttribCoordinatesLocation);
    glVertexAttribPointer(mTextureAttribCoordinatesLocation, textureComponentCount, GL_FLOAT, normalized, stride, (void*)(intptr_t)(vertexComponentCount * mBufferObjectVertexCount * sizeof(GL_FLOAT)));

    glGenTextures(1, &mCursorTexture);
    glBindTexture(GL_TEXTURE_2D, mCursorTexture);
    mCursorTextureInitialized = false;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);


    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glUseProgram(0);

    CHECK_GL_ERROR("after initializeGL()");

    // NOTE: No GL functions may be called before initializeGL() was called, as the GL context may
    //       not yet be set up.
    mGLInitialized = true;

    emit initializedOpenGL();
}

void FramebufferWidget::paintGL()
{
    CHECK_GL_ERROR("before paintGL()");
    glClear(GL_COLOR_BUFFER_BIT);
    if (mFramebufferTextureInitialized) {
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
        glUseProgram(mShaderProgram);
        float offsetX = 0.0f;
        float offsetY = 0.0f;
        float scaleX = 1.0f;
        float scaleY = 1.0f;
        if (!mScaleFramebuffer) {
            if (width() > 0 && framebufferWidth() > 0) {
                scaleX = (float)width() / (float)framebufferWidth();
                offsetX = (float)mFramebufferOffsetX / (float)framebufferWidth();
            }
            if (height() > 0 && framebufferHeight() > 0) {
                scaleY = (float)height() / (float)framebufferHeight();
                offsetY = (float)mFramebufferOffsetY / (float)framebufferHeight();
            }
        }
        glUniform2f(mFramebufferOffsetLocation, offsetX, offsetY);
        glUniform2f(mFramebufferScaleLocation, scaleX, scaleY);
        glUniform2f(mVertexOffsetLocation, 0.0f, 0.0f);
        glUniform2f(mVertexScaleLocation, 1.0f, 1.0f);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, mFramebufferTexture);
        glBindVertexArray(mVertexArrayObject);
        glDrawArrays(GL_TRIANGLES, 0, mBufferObjectVertexCount);

        if (mCursorTextureInitialized) {
            // we re-use the framebuffer shader to render the cursor texture.
            glBindTexture(GL_TEXTURE_2D, mCursorTexture);
            const QPoint pos = mapFromGlobal(QCursor::pos());
            float sx = 1.0f;
            float sy = 1.f;
            if (mScaleFramebuffer) {
                sx = mCursorWidth / (float)framebufferWidth();
                sy = mCursorHeight / (float)framebufferHeight();
            }
            else {
                sx = mCursorWidth / (float)width();
                sy = mCursorHeight / (float)height();
            }
            float ox = (float)(pos.x() + mCursorWidth/2 - mCursorHotspotX) * 2.0 / (float)width() - 1.0;
            float oy = (float)(pos.y() + mCursorHeight/2 - mCursorHotspotY) * -2.0 / (float)height() + 1.0;
            glUniform2f(mFramebufferOffsetLocation, 0.0f, 0.0f);
            glUniform2f(mFramebufferScaleLocation, 1.0f, 1.0f);
            glUniform2f(mVertexOffsetLocation, ox, oy);
            glUniform2f(mVertexScaleLocation, sx, sy);
            glDrawArrays(GL_TRIANGLES, 0, mBufferObjectVertexCount);
        }
    }
    CHECK_GL_ERROR("after paintGL()");
}

void FramebufferWidget::resizeGL(int w, int h)
{
    // TODO: keep aspect ratio? i.e. add black frame left/right/top/bottom?
    glViewport(0, 0, w, h);
}

/**
 * @return Whether the GL context has been initialized. Once this function returns TRUE, this widget
 *         can be used - otherwise this widget is NOT yet usable.
 **/
bool FramebufferWidget::isGLInitialized() const
{
    return mGLInitialized;
}

void FramebufferWidget::setDebugBitPlanes(int planes)
{
    if (planes <= 0 || planes >= 8) {
        planes = -1;
    }
    mDebugBitPlanes = planes;
    if (planes != -1) {
        qDebug() << "Displaying" << mDebugBitPlanes << "most significant bit planes";
    }
    mFramebufferTextureInitialized = false;
    // schedule a paint event
    update();
}


