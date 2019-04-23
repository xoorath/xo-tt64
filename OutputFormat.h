#pragma once
#include <stdbool.h>
#include <stdint.h>

#include "stb_image.h"

typedef enum {
	OUTFMT_RGBA_32 = 0,
	OUTFMT_RGBA_16,
	OUTFMT_YUV_16,
	OUTFMT_IA_16,
	OUTFMT_IA_8,
	OUTFMT_IA_4,
	OUTFMT_I_8,
	OUTFMT_I_4,
	OUTFMT_CI_8,
	OUTFMT_CI_4,

	OUTFMT_COUNT
} OutputFormat_t;


bool           OutputFormat_GetIsSupported(OutputFormat_t fmt);
size_t         OutputFormat_GetColorComponents(OutputFormat_t fmt);
size_t         OutputFormat_GetBPP(OutputFormat_t fmt);
char const*    OutputFormat_GetName(OutputFormat_t fmt);
uint32_t       OutputFormat_ConvertColor(OutputFormat_t fmt, stbi_uc r, stbi_uc g, stbi_uc b, stbi_uc a);