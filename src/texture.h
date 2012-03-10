/*
 *  texture.cpp - OpenGL texture
 *
 *  Frodo (C) 1994-1997,2002 Christian Bauer
 */
#if USE_OPENGL
#ifndef _TEXTURE_H
#define _TEXTURE_H

// Class for rendering OpenGL graphics
class Texture
{

    private:
        bool valid;

        GLuint textureId;
        int width;
        int height;
        int bitsPerPixel;
        int pitch;
        int size;

        void* buffer;
        int bufferSize;

        int format;
        int internalFormat;

        bool filtering;

    private:
        int paletteSize;
        float* paletteRed;
        float* paletteGreen;
        float* paletteBlue;


    public:
        Texture();
        Texture(const char* name);
        Texture(int width, int height, int bitsPerPixel);
        ~Texture();

    public:
        bool load(const char* name);
        bool create(int width, int height, int bitsPerPixel, const void* pixels=NULL, SDL_Color* palette=NULL, int paletteSize=0);
        void free();
        void* getBuffer();
        void updateData(const void* pixels);
        void updateData(const void* pixels, int bitsPerPixel, uint32* palette);
        void setAntialias(bool enabled=true);
        bool isAntialiased() const;

    public:
        int getWidth() const;
        int getHeight() const;
        int getBitsPerPixel() const;

    public:
        void bind();
        void unbind();

    private:
        void init();
        void storePixelData(const void* pixels);
        bool hasPalette() const;
        void enablePalette();
        void disablePalette();
        void freePalette();

};

#endif /* _TEXTURE_H */
#endif /* USE_OPENGL */
