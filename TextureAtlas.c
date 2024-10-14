#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <stdint.h>

#include "TextureAtlas.h"



typedef enum InfoType {ATLAS, SPRITE} InfoType;

int StringView_compare(void* str1, void* str2)
{
    StringView s1 = *(StringView*)str1;
    StringView s2 = *(StringView*)str2;

    return !(s1.len == s2.len && s1.pos == s2.pos && !strncmp(s1.str+s1.pos, s2.str+s2.pos, s1.len));
}


// compares a StringView with a regular string
int StringView_strcmp(StringView* s1, const char* s2)
{
    return strncmp(s1->str + s1->pos, s2, s1->len);
}

// duplicates a StringView as a char*
char* StringView_dup(StringView* s)
{
    char* res = malloc(sizeof(char) * s->len+1);
    if(!res)
    {
        return NULL;
    }
    strncpy(res, s->str + s->pos, s->len);
    res[s->len] = '\0'; 

    return res;
}




void StringView_print(StringView* s)
{
    printf("%.*s\n", (int)s->len, (s->str + s->pos));
}


int str_cmp(void* str1, void* str2)
{
    char* s1 = *(char**)str1;
    char* s2 = *(char**)str2;

    return strcmp(s1,s2);
}


void TextureInfo_free(void* texinfo)
{
    TextureInfo info = *(TextureInfo*)texinfo;
    free(info.nameId);
}

void string_free(void* str)
{
    char* s = *(char**)str;
    free(s);
}

typedef struct Parser
{
    char* src;
    size_t len;
    size_t pos;
} Parser;


int Parser_new(Parser* p, const char* path)
{
    FILE* f = fopen(path, "rb");
    if(!f)
    {
        fprintf(stderr, "failed to open file %s", path);
        return -1;
    }
    p->pos = 0;
    // set pointer to end of file, then get location of pointer to get size
    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    fseek(f, 0, SEEK_SET);
    p->len = size;
    // allocate memory to read entire file
    p->src = malloc(sizeof(char)*size+1);
    if(!p->src)
    {
        fprintf(stderr, "failed to allocate memory for file: %s", strerror(errno));
        return -1;
    }

    fread(p->src, sizeof(char), size, f);

    p->src[size] = 0;

    

    fclose(f);

    return 0;
}


void Parser_free(Parser* p)
{
    free(p->src);
}




unsigned char Parser_atEnd(Parser* p)
{
    if(p->pos >= p->len - 1)
    {
        return 1;
    }
    return 0;
}

char Parser_peek(Parser* p)
{
    return p->src[p->pos];
}

// always check if at end before using advance
char Parser_advance(Parser* p)
{
    return p->src[++p->pos];
}


void Parser_skipWhitespace(Parser* p)
{
    while(!Parser_atEnd(p) && isspace(Parser_peek(p)) && Parser_peek(p) != '\n')
    {
        Parser_advance(p);
    }
}

void Parser_skipComments(Parser* p)
{
    while(!Parser_atEnd(p) && Parser_peek(p) == '#')
    {
        Parser_advance(p);
        while(!Parser_atEnd(p) && Parser_peek(p) != '\n')
        {
            Parser_advance(p);
        }
        Parser_advance(p);
    }
}

// skips both whitespace and comments
void Parser_skipWhitespaceComments(Parser* p)
{
    while(!Parser_atEnd(p) && (Parser_peek(p) == '#' || isspace(Parser_peek(p))))
    {
        Parser_skipWhitespace(p);
        Parser_skipComments(p);
    }
}

// allows for special characters '.' and '_'
int isStringChar(char c)
{
    return isalnum(c) || c == '.' || c == '_';
}

int Parser_expectString(Parser* p, StringView* s)
{
    s->pos = p->pos;
    int len = 0;
    while(!Parser_atEnd(p) && isStringChar(Parser_peek(p)))
    {

        Parser_advance(p);
        len++;
    }
    if(len == 0)
    {
        return -1;
    }
    s->str = p->src;
    s->len = len;

    return 0;
}


int Parser_expectNumber(Parser* p, uint64_t* num)
{
    *num = 0;
    if(!isdigit(Parser_peek(p)))
    {
        return -1;
    }
    while(!Parser_atEnd(p) && isdigit(Parser_peek(p)))
    {
        *num = *num * 10 + (Parser_peek(p) - '0');
        Parser_advance(p);
    }
    // we encountered some non-digit after the number
    if(Parser_peek(p) != ' ' && Parser_peek(p) != '\n')
    {
        return -1;
    }

    return 0;
    
}



TextureAtlas* TextureAtlas_parse(const char* path)
{
    TextureAtlas* atlas = malloc(sizeof(TextureAtlas));
    if(!atlas)
    {
        goto cleanup;
    }
    atlas->config_file = path;
    atlas->textures = hash_table_new_ex(sizeof(char*), sizeof(TextureInfo), str_cmp, string_hash, 32, NULL, NULL, string_free, TextureInfo_free);
    
    Parser p;
    if(Parser_new(&p, path) == -1)
    {

        goto cleanup;
    }

    StringView s;

    InfoType t;
    uint64_t num;
    TextureInfo texInfo;
    char* key;
    while(!Parser_atEnd(&p))
    {
        Parser_skipWhitespaceComments(&p);

        // type of information
        if(Parser_expectString(&p, &s) == -1)
        {
            fprintf(stderr, "Error: either 'a' for atlas or 's' for sprite\n");
            goto cleanup;
        }
        if(!StringView_strcmp(&s, "a"))
        {
            t = ATLAS;
        }
        else if(!StringView_strcmp(&s, "s"))
        {
            t = SPRITE;
        }
        else
        {
            fprintf(stderr, "Error: either 'a' for atlas or 's' for sprite\n");
            goto cleanup;
        }

        Parser_skipWhitespace(&p);

        switch(t)
        {
            case ATLAS:
                if(Parser_expectString(&p, &s) == -1)
                {
                    fprintf(stderr, "Error: Expected atlas file path\n");
                }

                atlas->info.imagePath = StringView_dup(&s);
                if(!atlas->info.imagePath)
                {
                    fprintf(stderr, "Error: Failed to duplicate string\n");
                    goto cleanup;
                }

                Parser_skipWhitespace(&p);

                if(Parser_expectNumber(&p, &num) == -1)
                {
                    fprintf(stderr, "Error: Expected number: Width\n");
                    goto cleanup;
                }
                atlas->info.width = num;
                
                
                Parser_skipWhitespace(&p);

                if(Parser_expectNumber(&p, &num) == -1)
                {
                    fprintf(stderr, "Error: Expected number: Height\n");
                    goto cleanup;

                }
                atlas->info.height = num;
                
                Parser_skipWhitespace(&p);

                if(Parser_expectNumber(&p, &num) == -1)
                {
                    fprintf(stderr, "Error: Expected number: SpriteCount\n");
                    goto cleanup;
                }
                atlas->info.spriteCount = num;
                

                Parser_skipWhitespace(&p);

                if(Parser_expectNumber(&p, &num) == -1)
                {
                    fprintf(stderr, "Error: Expected number: isFont\n");
                    goto cleanup;
                }
                atlas->info.isFont = num;
                
                
                Parser_skipWhitespace(&p);

                if(Parser_expectNumber(&p, &num) == -1)
                {
                    fprintf(stderr, "Error: Expected number: FontSize\n");
                    goto cleanup;
                }
                atlas->info.fontSize = num;
                
                
                
                Parser_skipWhitespace(&p);
                if(Parser_peek(&p) == '#')
                {
                    Parser_skipComments(&p);
                }
                else if(Parser_peek(&p) == '\n')
                {
                    Parser_advance(&p);
                }
                else
                {
                    fprintf(stderr, "Error: expected newline");
                    goto cleanup;
                }

                //Parser_advance(&p);
                break;
            case SPRITE:
                Parser_skipWhitespaceComments(&p);

                if(Parser_expectString(&p, &s) == -1)
                {
                    fprintf(stderr, "Error: Expected nameId\n");
                }

                StringView nameId = s;
                
    
                

                Parser_skipWhitespace(&p);

                if(Parser_expectNumber(&p, &num) == -1)
                {
                    fprintf(stderr, "Error: Expected number: originX\n");
                    goto cleanup;
                }
                texInfo.originX = num;
                

                Parser_skipWhitespace(&p);

                if(Parser_expectNumber(&p, &num) == -1)
                {
                    fprintf(stderr, "Error: Expected number: originY\n");
                    goto cleanup;
                }
                texInfo.originY = num;
                

                Parser_skipWhitespace(&p);

                if(Parser_expectNumber(&p, &num) == -1)
                {
                    fprintf(stderr, "Error: Expected number: positionX\n");
                    goto cleanup;
                }
                texInfo.positionX = num;
                

                Parser_skipWhitespace(&p);

                if(Parser_expectNumber(&p, &num) == -1)
                {
                    fprintf(stderr, "Error: Expected number: positionY\n");
                    goto cleanup;
                }
                texInfo.positionY = num;
                

                Parser_skipWhitespace(&p);

                if(Parser_expectNumber(&p, &num) == -1)
                {
                    fprintf(stderr, "Error: Expected number: sourceWidth\n");
                    goto cleanup;
                }
                texInfo.sourceWidth = num;
                

                Parser_skipWhitespace(&p);

                if(Parser_expectNumber(&p, &num) == -1)
                {
                    fprintf(stderr, "Error: Expected number: sourceHeight\n");
                    goto cleanup;
                }
                texInfo.sourceHeight = num;
                
                
                Parser_skipWhitespace(&p);

                if(Parser_expectNumber(&p, &num) == -1)
                {
                    fprintf(stderr, "Error: Expected number: padding\n");
                    goto cleanup;
                }
                texInfo.padding = num;
                

                Parser_skipWhitespace(&p);

                if(Parser_expectNumber(&p, &num) == -1)
                {
                    fprintf(stderr, "Error: Expected number: trimed\n");
                    goto cleanup;
                }
                texInfo.trimmed = num;
                


                Parser_skipWhitespace(&p);

                if(Parser_expectNumber(&p, &num) == -1)
                {
                    fprintf(stderr, "Error: Expected number: trimRecX\n");
                    goto cleanup;
                }
                texInfo.trimRecX = num;
                

                Parser_skipWhitespace(&p);

                if(Parser_expectNumber(&p, &num) == -1)
                {
                    fprintf(stderr, "Error: Expected number: trimRecY\n");
                    goto cleanup;
                }
                texInfo.trimRecY = num;
                


                Parser_skipWhitespace(&p);

                if(Parser_expectNumber(&p, &num) == -1)
                {
                    fprintf(stderr, "Error: Expected number: trimRecWidth\n");
                    goto cleanup;
                }
                texInfo.trimRecWidth = num;
                

                Parser_skipWhitespace(&p);

                if(Parser_expectNumber(&p, &num) == -1)
                {
                    fprintf(stderr, "Error: Expected number: trimRecHeight\n");
                    goto cleanup;
                }
                texInfo.trimRecHeight = num;
                

                Parser_skipWhitespace(&p);
                if(Parser_peek(&p) == '#')
                {
                    Parser_skipComments(&p);
                }
                else if(Parser_peek(&p) == '\n')
                {
                    Parser_advance(&p);
                }
                else
                {
                    fprintf(stderr, "Error: expected newline");
                    goto cleanup;
                }
                
                texInfo.nameId = StringView_dup(&nameId);
                key = StringView_dup(&nameId);

                hash_table_set(atlas->textures, &key, &texInfo);
                printf("%s\n", key);
                break;
            
        }
        
    }


    return atlas;

cleanup:
    Parser_free(&p);
    TextureAtlas_free(atlas);
    return NULL;

}


void TextureAtlas_free(TextureAtlas* atlas)
{
    if(!atlas)
    {
        return;
    }
    if(atlas->textures)
    {
        hash_table_free(atlas->textures);
    }
    if(atlas->info.imagePath)
    {
        free(atlas->info.imagePath);
    }
    


    free(atlas);
}