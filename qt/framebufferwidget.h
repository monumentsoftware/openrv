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

#ifndef FRAMEBUFFERWIDGET_H
#define FRAMEBUFFERWIDGET_H

#include <QtWidgets/QOpenGLWidget>
#include <QtGui/QOpenGLFunctions_3_2_Core>

class FramebufferWidget : public QOpenGLWidget, public  QOpenGLFunctions_3_2_Core
{
    Q_OBJECT
public:
    FramebufferWidget(QWidget* parent);
    virtual ~FramebufferWidget();

    bool isGLInitialized() const;

    void resetFramebuffer(uint16_t width, uint16_t height);
    void updateFramebuffer(const struct orv_framebuffer_t* framebuffer, uint16_t x, uint16_t y, uint16_t width, uint16_t height);
    void updateCursor(const struct orv_cursor_t* cursor);
    void initializeFramebuffer(const struct orv_framebuffer_t* framebuffer);
    void setDebugBitPlanes(int planes);

    uint16_t framebufferWidth() const;
    uint16_t framebufferHeight() const;

    void setScaleFramebuffer(bool scale);
    bool scaleFramebuffer() const;
    void setFramebufferOffset(int x, int y);
    int framebufferOffsetX() const;
    int framebufferOffsetY() const;

signals:
    /**
     * Emitted when @ref initializeGL() has finished.
     *
     * The widget required that @ref initializeFramebuffer() or @ref updateFramebuffer() is called
     * once afterwards, otherwise no data can be displayed.
     *
     * Also note: If no OpenRV framebuffer data is available yet, this signal can safely be ignored.
     **/
    void initializedOpenGL();

protected:
    virtual void initializeGL() override;
    virtual void paintGL() override;
    virtual void resizeGL(int w, int h) override;
private:
    bool mGLInitialized = false;

    GLuint mShaderProgram = 0;
    GLuint mVertexShader = 0;
    GLuint mFragmentShader = 0;
    GLuint mFramebufferTexture = 0;
    GLuint mCursorTexture = 0;
    GLuint mVertexAttribLocation = 0;
    GLuint mTextureAttribCoordinatesLocation = 0;
    GLuint mBufferObject = 0;
    GLint mBufferObjectVertexCount = 0;
    GLuint mVertexArrayObject = 0;
    GLint mFramebufferOffsetLocation = 0;
    GLint mFramebufferScaleLocation = 0;
    GLint mVertexOffsetLocation = 0;
    GLint mVertexScaleLocation = 0;
    int mFramebufferWidth = 0;
    int mFramebufferHeight = 0;
    bool mFramebufferTextureInitialized = false;
    bool mScaleFramebuffer = true;
    int mFramebufferOffsetX = 0;
    int mFramebufferOffsetY = 0;
    int mCursorHotspotX = 0;
    int mCursorHotspotY = 0;
    int mCursorWidth = 0;
    int mCursorHeight = 0;
    bool mCursorTextureInitialized = false;
    int mDebugBitPlanes = -1;
    uint8_t* mDebugBitPlanesFramebuffer = nullptr;
};

inline uint16_t FramebufferWidget::framebufferWidth() const
{
    return (uint16_t)mFramebufferWidth;
}
inline uint16_t FramebufferWidget::framebufferHeight() const
{
    return (uint16_t)mFramebufferHeight;
}

/**
 * @return Whether the framebuffer is scaled, see @ref setScaleFramebuffer()
 **/
inline bool FramebufferWidget::scaleFramebuffer() const
{
    return mScaleFramebuffer;
}
/**
 * @return The x-offset of the displayed framebuffer area. This value is only meaningful if @ref
 *         scaleFramebuffer() is FALSE.
 **/
inline int FramebufferWidget::framebufferOffsetX() const
{
    return mFramebufferOffsetX;
}
/**
 * @return The y-offset of the displayed framebuffer area. This value is only meaningful if @ref
 *         scaleFramebuffer() is FALSE.
 **/
inline int FramebufferWidget::framebufferOffsetY() const
{
    return mFramebufferOffsetY;
}

#endif

