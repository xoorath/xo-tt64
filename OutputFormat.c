#include "OutputFormat.h"

stbi_uc To5Bit(stbi_uc in) {
	return (stbi_uc)(((double)in / 255.0) * 31.0);
}

stbi_uc To1Bit(stbi_uc in) {
	return (double)in / 255.0 > 0.5 ? 1 : 0;
}

////////////////////////////////////////////////////////////////////////////////////////// helpers

bool OutputFormat_GetIsSupported(OutputFormat_t fmt) {
	switch (fmt) {
	case OUTFMT_RGBA_16:
		return true;
	case OUTFMT_RGBA_32:
	case OUTFMT_YUV_16:
	case OUTFMT_IA_16:
	case OUTFMT_IA_8:
	case OUTFMT_IA_4:
	case OUTFMT_I_8:
	case OUTFMT_I_4:
	case OUTFMT_CI_8:
	case OUTFMT_CI_4:
	default:
	case OUTFMT_COUNT:
		return false;
	}
}

size_t OutputFormat_GetColorComponents(OutputFormat_t fmt) {
	switch (fmt) {
	case OUTFMT_YUV_16:
	case OUTFMT_I_8:
	case OUTFMT_I_4:
		return 3;
	case OUTFMT_RGBA_32:
	case OUTFMT_RGBA_16:
	case OUTFMT_CI_8:
	case OUTFMT_CI_4:
	case OUTFMT_IA_16:
	case OUTFMT_IA_8:
	case OUTFMT_IA_4:
		return 4;
	default:
	case OUTFMT_COUNT:
		return 0;
	}
}

size_t OutputFormat_GetBPP(OutputFormat_t fmt)
{
	switch (fmt) {
	case OUTFMT_IA_4:
	case OUTFMT_CI_4:
	case OUTFMT_I_4:
		return 4;
	case OUTFMT_IA_8:
	case OUTFMT_CI_8:
	case OUTFMT_I_8:
		return 8;
	case OUTFMT_YUV_16:
	case OUTFMT_RGBA_16:
	case OUTFMT_IA_16:
		return 16;
	case OUTFMT_RGBA_32:
		return 32;
	default:
	case OUTFMT_COUNT:
		return 0;
	}
}

char const* OutputFormat_GetName(OutputFormat_t fmt) {
	switch (fmt) {
	case OUTFMT_RGBA_32:
		return "32-Bit RGBA (OUTFMT_RGBA_32)";
	case OUTFMT_RGBA_16:
		return "16-Bit RGBA (OUTFMT_RGBA_16)";
	case OUTFMT_YUV_16:
		return "16-Bit YUV (OUTFMT_YUV_16)";
	case OUTFMT_IA_16:
		return "16-bit Intensity Alpha (OUTFMT_IA_16)";
	case OUTFMT_IA_8:
		return "8-bit Intensity Alpha (OUTFMT_IA_8)";
	case OUTFMT_IA_4:
		return "4-bit Intensity Alpha (OUTFMT_IA_4)";
	case OUTFMT_I_8:
		return "8-bit Intensity (OUTFMT_IA_8)";
	case OUTFMT_I_4:
		return "4-bit Intensity (OUTFMT_IA_4)";
	case OUTFMT_CI_8:
		return "8-bit Color Indexed (OUTFMT_CI_8)";
	case OUTFMT_CI_4:
		return "4-bit Color Indexed (OUTFMT_CI_4)";
	default:
		return "INVALID";
	}
}

uint32_t OutputFormat_ConvertColor(OutputFormat_t fmt, stbi_uc r, stbi_uc g, stbi_uc b, stbi_uc a) {
	switch (fmt) {
	case OUTFMT_RGBA_32:
		return (uint32_t)(r << 24) | (uint32_t)(g << 16) | (uint32_t)(b << 8) | (uint32_t)a;
	case OUTFMT_RGBA_16:
		return (To5Bit(r) << 11) | (To5Bit(g) << 6) | (To5Bit(b) << 1) | To1Bit(a);
	case OUTFMT_YUV_16:
		return 0;
	case OUTFMT_IA_16:
		return 0;
	case OUTFMT_IA_8:
		return 0;
	case OUTFMT_IA_4:
		return 0;
	case OUTFMT_I_8:
		return 0;
	case OUTFMT_I_4:
		return 0;
	case OUTFMT_CI_8:
		return 0;
	case OUTFMT_CI_4:
		return 0;
	default:
		return 0;
	}
}