#ifndef TEXTUREATLAS_H
#define TEXTUREATLAS_H
#include "stdint.h"
#include "hash_table.h"


// represents the string str[pos:pos+len)
typedef struct StringView
{
    char* str;
    size_t pos;
    size_t len;
} StringView;





typedef struct TextureInfo
{
    char* nameId;
    size_t originX;
    size_t originY;
    // where the texture is located in the atlas
    size_t positionX;
    size_t positionY;
    // the dimensions of the texture originally
    size_t sourceWidth;
    size_t sourceHeight;
    size_t padding;
    // whether the texture is the entire original image, or if the empty parts have been trimmed
    unsigned char trimmed;
    // if trimmed, this describes the region of space in the original image which contains the trimmed texture
    size_t trimRecX;
    size_t trimRecY;
    size_t trimRecWidth;
    size_t trimRecHeight;
} TextureInfo;


typedef struct AtlasInfo
{
    char* imagePath;
    size_t width;
    size_t height;
    size_t spriteCount;
    size_t isFont;
    size_t fontSize;
} AtlasInfo;


typedef struct TextureAtlas
{
    // key: string
    // value: TextureInfo
    hash_table* textures;

    AtlasInfo info;
    const char* config_file;
} TextureAtlas;



TextureAtlas* TextureAtlas_parse(const char* path);
void TextureAtlas_free(TextureAtlas* atlas);


#endif