//
//  RemoteView.m
//  opervclient
//
//  Created by christoph on 03.04.18.
//  Copyright © 2018 Christoph Möde. All rights reserved.
//

#import "RemoteView.h"
#import <OpenGLES/es3/gl.h>

const char* vertexShaderSource = ""
        "uniform vec2 framebufferOffset;\n"
        "uniform vec2 framebufferScale;\n"
        "attribute vec2 in_vertex;\n"
        "attribute vec2 in_textureCoordinates;\n"
        "varying vec2 frag_textureCoordinates;\n"
        "void main()\n"
        "{\n"
        "    gl_Position = vec4(in_vertex, 0, 1);\n"
        "    frag_textureCoordinates = in_textureCoordinates * framebufferScale + framebufferOffset;\n"
        "}\n";

const char* fragmentShaderSource = ""
    "precision mediump float;"
    "uniform sampler2D framebufferTexture;\n"
    "varying vec2 frag_textureCoordinates;\n"
    "void main()\n"
    "{\n"
    "    if (frag_textureCoordinates.x > 1.0 || frag_textureCoordinates.y > 1.0) {\n"
    "        gl_FragColor = vec4(0, 0, 0, 1);\n"
    "    }\n"
    "    else {\n"
    "        gl_FragColor = texture2D(framebufferTexture, frag_textureCoordinates);\n"
    "    }\n"
    "}\n";




@implementation RemoteView
{
    // shader
    GLuint mShaderProgram;
    GLuint mVertexShader;
    GLuint mFragmentShader;

    // texture
    BOOL mTextureInitialized;
    GLuint mTexture;
    CGSize mTextureSize;
    CGPoint mTextureOffset;
    GLenum mTextureFormat;

    // uniform locations
    GLint mOffsetUniformLocation;
    GLint mScaleUniformLocation;

    // vertex attribute locations
    GLuint mVertexAttribLocation;
    GLuint mTextureAttribCoordinatesLocation;

    // primitive buffer
    GLuint mBufferObject;
    GLint mBufferObjectVertexCount;
    GLuint mVertexArrayObject;

    GLsizei mViewportWidth;
    GLsizei mViewportHeight;
}
-(instancetype) initWithFrame:(CGRect)frame
{
    self = [super initWithFrame:frame];
    if (self) {
        mTextureFormat = GL_RGB;
        self.context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES3];
        self.drawableColorFormat = GLKViewDrawableColorFormatRGBA8888;
        self.drawableDepthFormat = GLKViewDrawableDepthFormat24;
        [EAGLContext setCurrentContext:self.context];

        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        [self createShaders];
        [self createTexture];
        [self createPrimitives];

    }
    [self setUserInteractionEnabled:true];
    return self;
}
-(void) checkGLError
{
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        NSLog(@"GLError!!!");
    }
}
-(void) compileShader:(GLuint)shader source:(const char*)source
{
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    GLint status;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (status != GL_TRUE) {
        NSLog(@"failed to compile shader");
    }
}
-(void) createShaders
{
    // create vertex and fragment shader
    mShaderProgram = glCreateProgram();
    mVertexShader = glCreateShader(GL_VERTEX_SHADER);
    mFragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glAttachShader(mShaderProgram, mVertexShader);
    glAttachShader(mShaderProgram, mFragmentShader);
    [self compileShader:mVertexShader source:vertexShaderSource];
    [self compileShader:mFragmentShader source:fragmentShaderSource];

    glLinkProgram(mShaderProgram);
    int linkStatus = GL_FALSE;
    glGetProgramiv(mShaderProgram, GL_LINK_STATUS, &linkStatus);
    if (linkStatus != GL_TRUE) {
        NSLog(@"Failed to link shader");
    }
    glUseProgram(mShaderProgram);

    // get uniform locations
    GLint textureLocation = glGetUniformLocation(mShaderProgram, "framebufferTexture");
    glUniform1i(textureLocation, 0);
    mOffsetUniformLocation = glGetUniformLocation(mShaderProgram, "framebufferOffset");
    glUniform2f(mOffsetUniformLocation, 0.0f, 0.f);
    mScaleUniformLocation = glGetUniformLocation(mShaderProgram, "framebufferScale");
    glUniform2f(mScaleUniformLocation, 1.0f, 1.f);
    mVertexAttribLocation = glGetAttribLocation(mShaderProgram, "in_vertex");
    mTextureAttribCoordinatesLocation = glGetAttribLocation(mShaderProgram, "in_textureCoordinates");
    [self checkGLError];

}
-(void) createTexture
{
    glGenTextures(1, &mTexture);
    glBindTexture(GL_TEXTURE_2D, mTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    [self checkGLError];
}
-(void) createPrimitives
{
    glGenBuffers(1, &mBufferObject);
    glBindBuffer(GL_ARRAY_BUFFER, mBufferObject);
    const GLfloat vertexAndTextureData[] = {
        /* vertices (x,y) */
        -1.0, -1.0,
        1.0, -1.0,
        -1.0, 1.0,
        1.0, -1.0,
        1.0, 1.0,
        -1.0, 1.0,

        /* texture coordinates */
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
    [self checkGLError];

    // vertex array object
    glGenVertexArrays(1, &mVertexArrayObject);
    glBindVertexArray(mVertexArrayObject);
    glBindBuffer(GL_ARRAY_BUFFER, mBufferObject);
    const GLsizei stride = 0;
    const GLboolean normalized = GL_FALSE;
    glEnableVertexAttribArray(mVertexAttribLocation);
    glVertexAttribPointer(mVertexAttribLocation, vertexComponentCount, GL_FLOAT, normalized, stride, (void*)(intptr_t)0);
    glEnableVertexAttribArray(mTextureAttribCoordinatesLocation);
    glVertexAttribPointer(mTextureAttribCoordinatesLocation, textureComponentCount, GL_FLOAT, normalized, stride, (void*)(intptr_t)(vertexComponentCount * mBufferObjectVertexCount * sizeof(GL_FLOAT)));
    glBindVertexArray(0);
}

-(void) drawRect:(CGRect)rect
{
    glClearColor(1, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    if (!mTextureInitialized) {
        return;
    }

    // setup viewport
    glViewport(0, 0, mViewportWidth, mViewportHeight);


    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glUseProgram(mShaderProgram);


    float offsetX = 0.0f;
    float offsetY = 0.0f;
    float scaleX = 1.0f;
    float scaleY = 1.0f;
#if 0
    if (!mScaleFramebuffer) {
        if (mView > 0 && mRemoteBufferSize.width > 0) {
            scaleX = self.bounds.size.width / mRemoteBufferSize.width;
            offsetX = mRemoteBufferOffset.x / mRemoteBufferSize.width;
        }
        if (self.bounds.size.height > 0 && mRemoteBufferSize.height > 0) {
            scaleY = self.bounds.size.height / mRemoteBufferSize.height;
            offsetY = mRemoteBufferOffset.y / mRemoteBufferSize.height;
        }
    }
#endif

    glUniform2f(mOffsetUniformLocation, offsetX, offsetY);
    glUniform2f(mScaleUniformLocation, scaleX, scaleY);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mTexture);


    glBindVertexArray(mVertexArrayObject);
    glDrawArrays(GL_TRIANGLES, 0, mBufferObjectVertexCount);
    glBindVertexArray(0);
#if 0 /* USE_VERTEX_ARRAY_OBJECT */
        glBindBuffer(GL_ARRAY_BUFFER, mBufferObject);
        const GLsizei stride = 0;
        const GLboolean normalized = GL_FALSE;
        const GLint vertexComponentCount = 2;
        const GLint textureComponentCount = 2;
        glEnableVertexAttribArray(mVertexAttribLocation);
        glVertexAttribPointer(mVertexAttribLocation, vertexComponentCount, GL_FLOAT, normalized, stride, (void*)(intptr_t)0);
        glEnableVertexAttribArray(mTextureAttribCoordinatesLocation);
        glVertexAttribPointer(mTextureAttribCoordinatesLocation, textureComponentCount, GL_FLOAT, normalized, stride, (void*)(intptr_t)(vertexComponentCount * mBufferObjectVertexCount * sizeof(GL_FLOAT)));
#endif /* USE_VERTEX_ARRAY_OBJECT */

}
-(void) setRemoteBufferOffset:(CGPoint)offset
{
    if (offset.x > mTextureSize.width - self.bounds.size.width) {
        offset.x = mTextureSize.width - self.bounds.size.width;
    }
    if (offset.y > mTextureSize.height - self.bounds.size.height) {
        offset.y = mTextureSize.height - self.bounds.size.height;
    }
    if (offset.x < 0) {
        offset.x = 0;
    }
    if (offset.y < 0) {
        offset.y = 0;
    }
    mTextureOffset = offset;


}
-(void) setRemoteBufferSize:(CGSize)size
{
    const GLint level = 0;
    const GLenum format = mTextureFormat;
    mTextureSize = size;
    const GLint internalFormat = mTextureFormat;
    const GLint border = 0;

    size_t dataSize = mTextureSize.height * mTextureSize.width * 3;
    uint8_t* data = malloc(dataSize);
    memset(data, 0xff, dataSize);
    glTexImage2D(GL_TEXTURE_2D, level, internalFormat, mTextureSize.width, mTextureSize.height, border, format, GL_UNSIGNED_BYTE, data);
    free(data);
    mTextureInitialized = YES;
}
-(void) fillRemoteBuffer:(const uint8_t*)data frame:(CGRect)frame
{
    if (!mTextureInitialized) {
        NSLog(@"texture not initialized");
        return;
    }
    if (frame.origin.x < 0 || frame.origin.y < 0) {
        NSLog(@"frame out of bounds");
        return;
    }
    if (frame.origin.x + frame.size.width > mTextureSize.width ||
        frame.origin.y + frame.size.height > mTextureSize.height) {
        return;
    }
    glBindTexture(GL_TEXTURE_2D, mTexture);
    const GLint level = 0;
    const GLenum format = mTextureFormat;
    glPixelStorei(GL_UNPACK_ROW_LENGTH, mTextureSize.width);
    size_t ofs = (frame.origin.y * 3 * mTextureSize.width) + frame.origin.x * 3;
    glTexSubImage2D(GL_TEXTURE_2D, level, frame.origin.x, frame.origin.y, frame.size.width, frame.size.height, format, GL_UNSIGNED_BYTE, data + ofs);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    [self checkGLError];
}
-(void) layoutSubviews
{
    [super layoutSubviews];
    mViewportWidth = self.bounds.size.width * [UIScreen mainScreen].scale;
    mViewportHeight = self.bounds.size.height * [UIScreen mainScreen].scale;
}

-(void) touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event{
    [_viewController sendTouchEvents:touches click:true];
}

-(void) touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event{
    [_viewController sendTouchEvents:touches click:false];
}

@end
