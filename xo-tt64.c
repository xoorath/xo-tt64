/*
xo-tt64: xoorath's texture tool for the n64
Usage:
    xo-tt64.exe <full paths to image files>
    Will convert the input images to c files of the same name (in the same directory) for use with the n64.

Dev Notes:
    We work in bits not bytes because some formats have pixels that are half a byte in size.
*/
#define _CRT_SECURE_NO_WARNINGS 1
#include <assert.h>
#include <malloc.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_ONLY_BMP
#define STBI_ONLY_PSD
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#undef STBI_ONLY_JPEG
#undef STBI_ONLY_PNG
#undef STBI_ONLY_BMP
#undef STBI_ONLY_PSD
#undef STB_IMAGE_IMPLEMENTATION

#include "OutputFormat.h"

////////////////////////////////////////////////////////////////////////// application macros

#define LOG(fmt, ...) printf(fmt "\n", __VA_ARGS__ )
#define ELOG(fmt, ...) printf(fmt "\n", __VA_ARGS__ ); assert(false); __debugbreak()
#ifdef _DEBUG
#	define DLOG(fmt, ...) printf(fmt "\n", __VA_ARGS__ )
#else
#	define DLOG(fmt, ...)
#endif

#define TMEM_MAX_BITS 4096 * 8

////////////////////////////////////////////////////////////////////////// application types

typedef struct {
    uint16_t w, h;
} TextureChunkSettings_t;

typedef struct {
    // Should be set before ProcessTexture
    char* filepath;

    // Set by ProcessTexture
    size_t w, h;

    // The following chunk data is only to be used when the user provides them.
    // It's for setting up sprite sheets where you already know the dimensions we need to cut the sheet into.
    // if chunkCount is 0, we will try to slice the input texture for you based on the texture memory
    // required to draw the whole image.
    size_t chunkCount;
    TextureChunkSettings_t* chunks;
} InputTexture_t;

typedef struct {
    OutputFormat_t outputFormat;
    size_t chunkCount;
    TextureChunkSettings_t* chunks; // This pointer needs to be freed after ProcessTexture is called.
} ExportSettings_t;

////////////////////////////////////////////////////////////////////////// forward decl

// processing & printing
void   ProcessInput(char* path);
void   ProcessTexture(InputTexture_t* in, ExportSettings_t* settings);
void   BuildTLUT(char* buffer, size_t bufferSize, size_t* sizeOut);
void   PrintCodeFile(InputTexture_t* in, ExportSettings_t* settings, stbi_uc* data);
// helpers
size_t TLUT_GetSizeInBits(OutputFormat_t fmt, size_t tableCount);

////////////////////////////////////////////////////////////////////////// entry

int main(int argsc, char* argsv[]) {
    size_t i;

    if (argsc == 1) {
#ifdef _DEBUG
        ProcessInput("D:\\VM\\XP_Share\\Projects\\xo-64\\sprites\\test_joker.png");
#else
        ELOG("Must provide a texture path as an input.");
#endif
    }
    else {
        // skip argsv[0], the app path
        for (i = 1; i < (size_t)argsc; ++i) {
            ProcessInput(argsv[i]);
        }
    }

    return 0;
}

////////////////////////////////////////////////////////////////////////// processing

void ProcessInput(char* path) {
    ExportSettings_t defaultExport;
    InputTexture_t defaultInput;

    defaultExport.outputFormat = OUTFMT_RGBA_16;
    // chunks are determined during process texture
    defaultExport.chunkCount = 0;
    defaultExport.chunks = NULL;

    defaultInput.filepath = _strdup(path);
    defaultInput.chunkCount = 0;
    defaultInput.chunks = NULL;

    ProcessTexture(&defaultInput, &defaultExport);

    free((void*)defaultExport.chunks);
    free((void*)defaultInput.filepath);
}

void ProcessTexture(InputTexture_t* in, ExportSettings_t* settings) {
    stbi_uc* data;
    char tlutBuffer[1024];
    int
        w,
        h,
        components;
    size_t
        i,
        pixels,
        bpp,
        maxImgBitsPerDrawCall,
        chunkHeight,
        tlutSize;
    bool partialChunk;

    LOG("processing %s", in->filepath);

    if (!OutputFormat_GetIsSupported(settings->outputFormat)) {
        ELOG("Invalid output format.");
        return;
    }

    components = (int)OutputFormat_GetColorComponents(settings->outputFormat);

    data = stbi_load(in->filepath, &w, &h, &components, components);
    if (data == NULL) {
        ELOG("Couldn't load file: %s.", in->filepath);
        return;
    }
    in->w = (size_t)w;
    in->h = (size_t)h;

    DLOG("w:%d, h:%d, c:%d", w, h, components);

    BuildTLUT(tlutBuffer, sizeof(tlutBuffer), &tlutSize);
    DLOG("TLUT count: %Iu", tlutSize);

    tlutSize = TLUT_GetSizeInBits(settings->outputFormat, tlutSize);
    DLOG("TLUT size: %Iu", tlutSize);

    if (tlutSize >= TMEM_MAX_BITS) {
        ELOG("TLUT is too large to use.");
        stbi_image_free(data);
        return;
    }

    maxImgBitsPerDrawCall = TMEM_MAX_BITS - tlutSize;

    bpp = OutputFormat_GetBPP(settings->outputFormat);
    if (bpp <= 0) {
        ELOG("bpp for this format is 0. That makes no sense.");
        stbi_image_free(data);
        return;
    }

    if (in->chunkCount > 0 && in->chunks != NULL) {
        settings->chunkCount = in->chunkCount;
        settings->chunks = calloc(in->chunkCount, sizeof(TextureChunkSettings_t));
        memcpy(settings->chunks, in->chunks, in->chunkCount * sizeof(TextureChunkSettings_t));
    }
    else {
        // get max pixels per draw call
        pixels = maxImgBitsPerDrawCall / bpp;

        // now get max image rows per draw call
        chunkHeight = min(in->h, maxImgBitsPerDrawCall / (in->w * bpp));

        // if you can't finish the last row while drawing, exclude it
        if (maxImgBitsPerDrawCall % (in->w * bpp) != 0) {
            chunkHeight--;
        }
        // get the chunk count
        settings->chunkCount = in->h / chunkHeight;

        // if there's a remaining not-full chunk, append it here.
        partialChunk = (in->h % chunkHeight) != 0;
        if (partialChunk) {
            settings->chunkCount++;
        }

        settings->chunks = calloc(settings->chunkCount, sizeof(TextureChunkSettings_t));
        for (i = 0; i < settings->chunkCount; ++i) {
            // todo: range check before or during cast
            settings->chunks[i].w = (uint16_t)w;
            settings->chunks[i].h = (uint16_t)chunkHeight;
            // special case the last chunk if it's a partial chunk. Include only the remaining rows.
            if (i == settings->chunkCount - 1 && partialChunk) {
                settings->chunks[i].h = (uint16_t)(in->h % chunkHeight);
            }
        }
    }

    for (i = 0; i < settings->chunkCount; ++i) {
        pixels = settings->chunks[i].w * settings->chunks[i].h;
        if (pixels * bpp > maxImgBitsPerDrawCall) {
            ELOG("Chunk %Iu is too large.\nRequesting chunk of size %Iu x %Iu.\nRequested pixels:%Iu Max pixels:%Iu", i, settings->chunks[i].w, settings->chunks[i].h, pixels, maxImgBitsPerDrawCall / bpp);
        }
    }

    LOG("done processing %s.\nPrinting code file.", in->filepath);
    PrintCodeFile(in, settings, data);
    stbi_image_free(data);
}

void BuildTLUT(char* buffer, size_t bufferSize, size_t* sizeOut)
{
    (void)buffer;
    (void)bufferSize;
    *sizeOut = 0;
}

void PrintCodeFile(InputTexture_t* in, ExportSettings_t* settings, stbi_uc* data) {
    char buffer[102400];
    char outCFilePath[260];
    char outHFilePath[260];
    char simpleName[260];
    char const* hexSpec;
    char const* hexType;
    char* bufferPtr;
    // segment offset from segment rom start/end.
    int segOff;
    // vertex x,y,u,v, and an offset per chunk
    int vtxX, vtxY, vtxTCU, vtxTCV, vtxOffY;
    size_t i, count, x, y, comp;
    FILE* f;

    count = strlen(in->filepath);
    for (i = count - 1; i > 1; --i) {
        if (in->filepath[i] == '.') break;
    }
    if (count - i < 3) {
        ELOG("file extension doesn't appear to be usable");
    }
    memcpy(outCFilePath, in->filepath, count + 1); // get the null term
    memcpy(outHFilePath, in->filepath, count + 1);

    memcpy(&outCFilePath[i], ".c", 3);
    memcpy(&outHFilePath[i], ".h", 3);

    for (i = count - 1; i > 1; --i) {
        if (in->filepath[i] == '/' || in->filepath[i] == '\\') {
            break;
        }
    }
    memcpy(simpleName, &in->filepath[i + 1], count - i + 1);
    count = strlen(simpleName);
    for (i = count - 1; i > 1; --i) {
        if (simpleName[i] == '.') {
            simpleName[i] = '\0';
            break;
        }
    }

    count = strlen(simpleName);
    for (i = 0; i < count; ++i) {
        simpleName[i] = (char)tolower((int)simpleName[i]);
    }

    f = fopen(outCFilePath, "w+");
    if (f == NULL) {
        ELOG("Couldn't open code file for writing. %s", outCFilePath);
        return;
    }
    memset(buffer, 0, sizeof(buffer));

#define buffSprintf(fmt, ...) bufferPtr += sprintf_s(bufferPtr, sizeof(buffer) - ((ptrdiff_t)bufferPtr- (ptrdiff_t)buffer), fmt, __VA_ARGS__ )

    bufferPtr = buffer;
    buffSprintf("// Generated by xo-tt64.\n");
    buffSprintf("// A tool by xoorath.\n//\n");
    buffSprintf("// Name: %s\n", simpleName);
    buffSprintf("// Size: %Iu x %Iu\n", in->w, in->h);
    buffSprintf("// Type: %s\n\n", OutputFormat_GetName(settings->outputFormat));
    buffSprintf("#include <ultra64.h>\n\n");

    switch (OutputFormat_GetBPP(settings->outputFormat)) {
    case 32:
        hexSpec = "0x%08x";
        hexType = "unsigned int";
        break;
    default:
        // fallthrough
    case 16:
        hexSpec = "0x%04x";
        hexType = "unsigned short";
        break;
    case 8:
        hexSpec = "0x%02x";
        hexType = "unsigned char";
        break;
    case 4:
        hexSpec = "0x%01x";
        hexType = "unsigned char";
        ELOG("hex type of unsigned char being used, despite single digit hex output... fix me!");
        break;
    }
    buffSprintf("%s %s_data[] = {\n", hexType, simpleName);

    comp = OutputFormat_GetColorComponents(settings->outputFormat);
    for (y = 0; y < in->h; ++y) {
        buffSprintf("    ");
        for (x = 0; x < in->w; ++x) {
            i = ((y * in->w) + x) * comp;
            buffSprintf(hexSpec, OutputFormat_ConvertColor(settings->outputFormat, data[i + 0], data[i + 1], data[i + 2], comp == 4 ? data[i + 3] : 0xff));
            buffSprintf(", ");
        }
        buffSprintf("\n");
    }
    buffSprintf("};\n");
    buffSprintf("\n");
    fwrite(buffer, 1, strlen(buffer), f);
    fclose(f);

    f = fopen(outHFilePath, "w+");
    if (f == NULL) {
        ELOG("Couldn't open header file for writing. %s", outHFilePath);
        return;
    }
    memset(buffer, 0, sizeof(buffer));
    bufferPtr = buffer;
    buffSprintf("// Generated by xo-tt64.\n");
    buffSprintf("// A tool by xoorath.\n//\n");
    buffSprintf("// Name: %s\n", simpleName);
    buffSprintf("// Size: %Iu x %Iu\n", in->w, in->h);
    buffSprintf("// Type: %s\n\n", OutputFormat_GetName(settings->outputFormat));
    buffSprintf("#ifndef _%s_H_\n", simpleName);
    buffSprintf("#define _%s_H_\n\n", simpleName);
    buffSprintf("#include <PR/ultratypes.h>\n");
    buffSprintf("#include <xo-img.h>\n\n");
    buffSprintf("#include \"xo-spec.h\"\n\n");
    buffSprintf("xo_spec_DeclareRomSegment(%s)\n\n", simpleName);
    buffSprintf("#ifndef GENERATED_IMAGE_DECLARATIONS\n");
    buffSprintf("extern Img_t %s;\n", simpleName);
    buffSprintf("#else // GENERATED_IMAGE_DECLARATIONS\n");
    buffSprintf("#include \"xo-render.h\"\n");
    buffSprintf("#include <PR/gbi.h>\n\n");

    vtxOffY = 0;
    for (i = 0; i < settings->chunkCount; ++i) {
#define VTX_FMT "{.v = {.ob = {%*3d, %*3d, 0}, .flag = 0, .tc = {%*3d<<6, %*3d<<6}, .cn = {0xff, 0xff, 0xff, 0xff}}}"
        buffSprintf("Vtx %s_mesh_%Iu[] = {\n", simpleName, i);
        vtxX = 0;
        vtxY = vtxOffY;
        vtxTCU = 0;
        vtxTCV = 0;
        buffSprintf("    " VTX_FMT ",\n", " ", vtxX, " ", vtxY, " ", vtxTCU, " ", vtxTCV);

        vtxX = settings->chunks[i].w;
        vtxY = vtxOffY;
        vtxTCU = settings->chunks[i].w;
        vtxTCV = 0;
        buffSprintf("    " VTX_FMT ",\n", " ", vtxX, " ", vtxY, " ", vtxTCU, " ", vtxTCV);

        vtxX = settings->chunks[i].w;
        vtxY = -settings->chunks[i].h + vtxOffY;
        vtxTCU = settings->chunks[i].w;
        vtxTCV = settings->chunks[i].h;
        buffSprintf("    " VTX_FMT ",\n", " ", vtxX, " ", vtxY, " ", vtxTCU, " ", vtxTCV);

        vtxX = 0;
        vtxY = -settings->chunks[i].h + vtxOffY;
        vtxTCU = 0;
        vtxTCV = settings->chunks[i].h;
        buffSprintf("    " VTX_FMT "\n", " ", vtxX, " ", vtxY, " ", vtxTCU, " ", vtxTCV);

        buffSprintf("}; // %s_mesh_%Iu\n\n", simpleName, i);

        vtxOffY -= settings->chunks[i].h;
#undef VTX_FMT
    }

    segOff = 0;
    buffSprintf("ImgSeg_t %s_components[] = {\n", simpleName);
    for (i = 0; i < settings->chunkCount; ++i) {
        buffSprintf("  {\n");
        buffSprintf("    _%sSegmentRomStart + %d,\n", simpleName, segOff);
        // TODO: "/8" wont work for bits per pixel
        segOff += settings->chunks[i].w * settings->chunks[i].h * (OutputFormat_GetBPP(settings->outputFormat)/8);
        buffSprintf("    _%sSegmentRomStart + %d,\n", simpleName, segOff);

        buffSprintf("    %hu, %hu,\n", settings->chunks[i].w, settings->chunks[i].h);
        buffSprintf("    G_TX_CLAMP | G_TX_NOMIRROR, G_TX_CLAMP | G_TX_NOMIRROR,\n");
        buffSprintf("    NULL,\n");
        buffSprintf("    %s_mesh_%Iu\n", simpleName, i);

        buffSprintf("  }");
        if (i != settings->chunkCount - 1) {
            buffSprintf(",");
        }
        buffSprintf("\n");
    }
    buffSprintf("}; // %s_components\n\n", simpleName);

    buffSprintf("Img_t %s = {\n", simpleName);
    buffSprintf("  sizeof(%s_components)/sizeof(%s_components[0]),\n", simpleName, simpleName);
    buffSprintf("  %s_components,\n", simpleName);
    buffSprintf("  %Iu, %Iu,\n\n", in->w, in->h);
    buffSprintf("  G_TF_BILERP\n");
    buffSprintf("}; // %s\n\n", simpleName);

    buffSprintf("#endif // GENERATED_IMAGE_DECLARATIONS\n\n");

    buffSprintf("#endif // _%s_H_\n", simpleName);

    fwrite(buffer, 1, strlen(buffer), f);
    fclose(f);
}

////////////////////////////////////////////////////////////////////////// helpers
size_t TLUT_GetSizeInBits(OutputFormat_t fmt, size_t tableCount) {
    switch (fmt) {
    case OUTFMT_RGBA_32:
    case OUTFMT_RGBA_16:
    case OUTFMT_YUV_16:
    case OUTFMT_IA_4:
    case OUTFMT_IA_8:
    case OUTFMT_IA_16:
    case OUTFMT_I_8:
    case OUTFMT_I_4:
    default:
    case OUTFMT_COUNT:
        return 0;
    case OUTFMT_CI_4:
    case OUTFMT_CI_8:
        return 16 * tableCount;
    }
}
