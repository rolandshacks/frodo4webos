/*
 *  renderer.cpp - OpenGL rendering
 *
 *  Frodo (C) 1994-1997,2002 Christian Bauer
 */

#include "sysdeps.h"
#if USE_OPENGL

#include "texture.h"
#include "font.h"
#include "renderer.h"

static void clearGlError()
{
    glGetError();
}

static void checkGlErrorImpl(const char* file, int line)
{
    GLenum errCode = glGetError();
    if (errCode != GL_NO_ERROR)
    {
        printf("%s(%d): opengl error 0x%04x\n", file, line, (int) errCode);
    }
}

#define checkGlError() checkGlErrorImpl(__FILE__, __LINE__)

static bool flipTextureHorizontally = false;
static bool flipTextureVertically = false;

float Renderer::quadCoords[8]    = { 0.0f };
float Renderer::quadTexCoords[8] = { 0.0f };
float Renderer::quadColors[16]   = { 0.0f };

Renderer* Renderer::__instance = NULL;

Renderer::Renderer()
{
    disposed = false;

    if (NULL == __instance)
    {
        __instance = this;
    }

    currentFont = NULL;
}

Renderer::~Renderer()
{
    if (this == __instance)
    {
        __instance = NULL;
    }

    disposed = true;
}

Renderer* Renderer::getInstance()
{
    return __instance;
}

void Renderer::init(int width, int height)
{
    this->width = width;
    this->height = height;

    clearGlError();

    // initialize opengl parameters
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE,        8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE,      8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE,       8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE,      8);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,      16);
    SDL_GL_SetAttribute(SDL_GL_BUFFER_SIZE,     32);

    SDL_GL_SetAttribute(SDL_GL_ACCUM_RED_SIZE,   8);
    SDL_GL_SetAttribute(SDL_GL_ACCUM_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ACCUM_BLUE_SIZE,  8);
    SDL_GL_SetAttribute(SDL_GL_ACCUM_ALPHA_SIZE, 8);

    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 2);

    glClearColor(0.0, 0.0, 0.0, 0.0);
    #if !defined(HAVE_GLES)
        glClearDepth(1.0f);
    #endif
    glViewport(0, 0, width, height);

    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_DEPTH_TEST);

    // initialize screen settings
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    #ifdef HAVE_GLES
        glOrthof(0, width, height, 0, 1, -1);
    #else
        glOrtho(0, width, height, 0, 1, -1);
    #endif

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    checkGlError();
}

void Renderer::shutdown()
{
    disposed = true;
}

void Renderer::resize(int width, int height)
{
    if (disposed) return;
    init(width, height);
}

void Renderer::beginDraw()
{
    if (disposed) return;

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // draw
    glPushMatrix();
}

void Renderer::endDraw()
{
    if (disposed) return;

    glPopMatrix();

    // swap opengl buffers
    SDL_GL_SwapBuffers();
}

void Renderer::fillRectangle(float x, float y, float width, float height)
{
    float texHorizontalMin = (!flipTextureHorizontally) ? 0.0f : 1.0f;
    float texHorizontalMax = 1.0f - texHorizontalMin;

    float texVerticalMin = (!flipTextureVertically) ? 0.0f : 1.0f;
    float texVerticalMax = 1.0f - texVerticalMin;

    drawQuad(GL_TRIANGLE_FAN,
             x, y,
             x+width, y,
             x+width, y+height,
             x, y+height,
             texHorizontalMin, texVerticalMin,
             texHorizontalMax, texVerticalMin,
             texHorizontalMax, texVerticalMax,
             texHorizontalMin, texVerticalMax);
}

void Renderer::draw(int mode, int count, float* coords, float* colors, float* texCoords)
{
    glEnableClientState(GL_VERTEX_ARRAY);

    if (NULL != colors)
    {
        glEnableClientState(GL_COLOR_ARRAY);
    }

    if (NULL != texCoords)
    {
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    }
 
    glVertexPointer(2, GL_FLOAT, 0, coords);

    if (NULL != colors)
    {
        glColorPointer(4, GL_FLOAT, 0, colors);
    }

    if (NULL != texCoords)
    {
        glTexCoordPointer(2, GL_FLOAT, 0, texCoords);
    }

    glDrawArrays(mode, 0, count);
 
    if (NULL != colors)
    {
        glDisableClientState(GL_COLOR_ARRAY);
    }

    if (NULL != texCoords)
    {
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    }

    glDisableClientState(GL_VERTEX_ARRAY);
}


void Renderer::drawQuad(int mode,
                          float x1, float y1,
                          float x2, float y2,
                          float x3, float y3,
                          float x4, float y4,
                          float u1, float v1,
                          float u2, float v2,
                          float u3, float v3,
                          float u4, float v4)
{
    quadCoords[0] = x1; quadCoords[1] = y1;
    quadCoords[2] = x2; quadCoords[3] = y2;
    quadCoords[4] = x3; quadCoords[5] = y3;
    quadCoords[6] = x4; quadCoords[7] = y4;

    quadTexCoords[0] = u1; quadTexCoords[1] = v1;
    quadTexCoords[2] = u2; quadTexCoords[3] = v2;
    quadTexCoords[4] = u3; quadTexCoords[5] = v3;
    quadTexCoords[6] = u4; quadTexCoords[7] = v4;

    draw(mode, 4, quadCoords, NULL, quadTexCoords);
}

void Renderer::drawTexture(Texture* texture, float x, float y, float width, float height)
{
    if (width < 0.0f) width = texture->getWidth();
    if (height < 0.0f) height = texture->getHeight();

    texture->bind();
    fillRectangle(x, y, width, height);
    texture->unbind();
}

void Renderer::drawTiledTexture(Texture* texture,
                                float x, float y,
                                float width, float height,
                                bool horizontal, bool vertical)
{
    texture->bind();

    float w1 = texture->getWidth()/2.0f;
    float h1 = texture->getHeight()/2.0f;

    float w2 = width - texture->getWidth()/2.0f;
    float h2 = height - texture->getHeight()/2.0f;

    if (horizontal && vertical)
    {
        drawQuad(GL_TRIANGLE_FAN,
                 x, y,
                 x+w1, y,
                 x+w1, y+h1,
                 x, y+h1,
                 0.0f, 0.0f,
                 0.5f, 0.0f,
                 0.5f, 0.5f,
                 0.0f, 0.5f);

        drawQuad(GL_TRIANGLE_FAN,
                 x+w1, y,
                 x+w2, y,
                 x+w2, y+h1,
                 x+w1, y+h1,
                 0.5f, 0.0f,
                 0.5f, 0.0f,
                 0.5f, 0.5f,
                 0.5f, 0.5f);

        drawQuad(GL_TRIANGLE_FAN,
                 x+w2, y,
                 x+width, y,
                 x+width, y+h1,
                 x+w2, y+h1,
                 0.5f, 0.0f,
                 1.0f, 0.0f,
                 1.0f, 0.5f,
                 0.5f, 0.5f);

        /////////////////////////////

        drawQuad(GL_TRIANGLE_FAN,
                 x, y+h1,
                 x+w1, y+h1,
                 x+w1, y+h2,
                 x, y+h2,
                 0.0f, 0.5f,
                 0.5f, 0.5f,
                 0.5f, 0.5f,
                 0.0f, 0.5f);

        drawQuad(GL_TRIANGLE_FAN,
                 x+w1, y+h1,
                 x+w2, y+h1,
                 x+w2, y+h2,
                 x+w1, y+h2,
                 0.5f, 0.5f,
                 0.5f, 0.5f,
                 0.5f, 0.5f,
                 0.5f, 0.5f);

        drawQuad(GL_TRIANGLE_FAN,
                 x+w2, y+h1,
                 x+width, y+h1,
                 x+width, y+h2,
                 x+w2, y+h2,
                 0.5f, 0.5f,
                 1.0f, 0.5f,
                 1.0f, 0.5f,
                 0.5f, 0.5f);

        /////////////////////////////

        drawQuad(GL_TRIANGLE_FAN,
                 x, y+h2,
                 x+w1, y+h2,
                 x+w1, y+height,
                 x, y+height,
                 0.0f, 0.5f,
                 0.5f, 0.5f,
                 0.5f, 1.0f,
                 0.0f, 1.0f);

        drawQuad(GL_TRIANGLE_FAN,
                 x+w1, y+h2,
                 x+w2, y+h2,
                 x+w2, y+height,
                 x+w1, y+height,
                 0.5f, 0.5f,
                 0.5f, 0.5f,
                 0.5f, 1.0f,
                 0.5f, 1.0f);

        drawQuad(GL_TRIANGLE_FAN,
                 x+w2, y+h2,
                 x+width, y+h2,
                 x+width, y+height,
                 x+w2, y+height,
                 0.5f, 0.5f,
                 1.0f, 0.5f,
                 1.0f, 1.0f,
                 0.5f, 1.0f);
    }
    else if (horizontal)
    {
        drawQuad(GL_TRIANGLE_FAN,
                 x, y,
                 x+w1, y,
                 x+w1, y+height,
                 x, y+height,
                 0.0f, 0.0f,
                 0.5f, 0.0f,
                 0.5f, 1.0f,
                 0.0f, 1.0f);

        drawQuad(GL_TRIANGLE_FAN,
                 x+w1, y,
                 x+w2, y,
                 x+w2, y+height,
                 x+w1, y+height,
                 0.5f, 0.0f,
                 0.5f, 0.0f,
                 0.5f, 1.0f,
                 0.5f, 1.0f);

        drawQuad(GL_TRIANGLE_FAN,
                 x+w2, y,
                 x+width, y,
                 x+width, y+height,
                 x+w2, y+height,
                 0.5f, 0.0f,
                 1.0f, 0.0f,
                 1.0f, 1.0f,
                 0.5f, 1.0f);
    }
    else if (vertical)
    {
        drawQuad(GL_TRIANGLE_FAN,
                 x, y,
                 x+width, y,
                 x+width, y+h1,
                 x, y+h1,
                 0.0f, 0.0f,
                 1.0f, 0.0f,
                 1.0f, 0.5f,
                 0.0f, 0.5f);

        drawQuad(GL_TRIANGLE_FAN,
                 x, y+h1,
                 x+width, y+h1,
                 x+width, y+h2,
                 x, y+h2,
                 0.0f, 0.5f,
                 1.0f, 0.5f,
                 1.0f, 0.5f,
                 0.0f, 0.5f);

        drawQuad(GL_TRIANGLE_FAN,
                 x, y+h2,
                 x+width, y+h2,
                 x+width, y+height,
                 x, y+height,
                 0.0f, 0.5f,
                 1.0f, 0.5f,
                 1.0f, 1.0f,
                 0.0f, 1.0f);
    }

    texture->unbind();
}


void Renderer::setFont(Font* font)
{
    currentFont = font;
}

Font* Renderer::getFont()
{
    return currentFont;
}

void Renderer::drawText(float x, float y, const char* text, int flags)
{
    float scaleX = 1.0f;
    float scaleY = 1.0f;
    float rotation = 0.0f;

    TTF_Font* font = (TTF_Font*) currentFont->getInternal();
    if (NULL == font)
    {
        return;
    }

    SDL_Color Color = {0xff, 0xff, 0xff};

    SDL_Surface* textSurface = TTF_RenderText_Blended(font, text, Color);
    if (NULL == textSurface)
    {
        return;
    }

    GLuint textTexture = 0;
    glGenTextures(1, &textTexture);
    glBindTexture(GL_TEXTURE_2D, textTexture);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, textSurface->w, textSurface->h, 0, GL_BGRA, GL_UNSIGNED_BYTE, textSurface->pixels);
    glBindTexture(GL_TEXTURE_2D, 0);

    float xPos = x;
    float translateX = 0.0f;

    if ((flags & ALIGN_CENTER) != 0)
    {
        translateX -= scaleX * (float) (textSurface->w/2);
    }
    else if ((flags & ALIGN_RIGHT) != 0)
    {
        translateX -= scaleX * (float) (textSurface->w - 1);
    }

    float yPos = y;
    float translateY = 0.0f;

    float fontHeight = currentFont->getHeight();
    if ((flags & ALIGN_MIDDLE) != 0)
    {
        translateY -= scaleY * fontHeight/2.0f;
    }
    else if ((flags & ALIGN_BOTTOM) != 0)
    {
        translateY -= scaleY * fontHeight - 1.0f;
    }

    float screenScaleX = 1.0f; // getWidth() / (float) getPixelWidth();
    float screenScaleY = 1.0f; // getHeight() / (float) getPixelHeight();

    float w = (float) (textSurface->w) * screenScaleX;
    float h = (float) (textSurface->h) * screenScaleY;

    w *= scaleX;
    h *= scaleY;

    glPushMatrix();
        glTranslatef(xPos, yPos, 0.0f);
        glRotatef(rotation, 0.0f, 0.0f, 1.0f);
        glTranslatef(translateX, translateY, 0.0f);

        /*
        if ((flags & TEXTMODE_FILL) != 0 || (textMode & TEXTMODE_FILL) != 0)
        {
            fillRectangle(0.0f, 0.0f, w, h, gradientBackground);
        }
        */

        glBindTexture(GL_TEXTURE_2D, textTexture);
        fillRectangle(0.0f, 0.0f, w, h);
        glBindTexture(GL_TEXTURE_2D, 0);

        /*
        if ((flags & TEXTMODE_BORDER) != 0 || (textMode & TEXTMODE_BORDER) != 0)
        {
            drawRectangle(0.0f, 0.0f, w, h, gradientForeground);
        }
        */

    glPopMatrix();

    glDeleteTextures(1, &textTexture);

    SDL_FreeSurface(textSurface);
}

int Renderer::getWidth() const
{
    return width;
}

int Renderer::getHeight() const
{
    return height;
}

void Renderer::enableClipping(int x, int y, int w, int h)
{
    glEnable(GL_SCISSOR_TEST);
    glScissor(x, height-y-h, w, h);
}

void Renderer::disableClipping()
{
    glDisable(GL_SCISSOR_TEST);
}

#endif /* USE_OPENGL */
