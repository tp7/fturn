#include <future>
#include <Windows.h>
#pragma warning(disable: 4512 4244 4100)
#if defined(FILTER_AVS_25)
#include "avisynth-2_5.h"
#elif defined(FILTER_AVS_26)
#include <windows.h>
#include "avisynth-2_6.h"
#else
#error FILTER_AVS_2x not defined
#endif
#pragma warning(default: 4512 4244 4100)
#include <tmmintrin.h>
using namespace std;

enum class TurnDirection {
    LEFT,
    RIGHT,
    TURN180
};

bool isSupportedColorspace(int pixelType) {
    if (pixelType == VideoInfo::CS_YV12 || pixelType == VideoInfo::CS_I420) {
        return true;
    }
#if defined(FILTER_AVS_26) 
    if (pixelType == VideoInfo::CS_YV24 || pixelType == VideoInfo::CS_Y8) {
        return true;
    }
#endif
    return false;
}

const char* getUnsupportedColorspaceMessage() {
#if defined(FILTER_AVS_26) 
    return "Only YV12, YV24 and Y8 colorspaces are supported.";
#endif
    return "Only YV12 colorspace is supported.";
}

bool hasChroma(int pixelType) {
#if defined(FILTER_AVS_26) 
    return pixelType != VideoInfo::CS_Y8;
#endif
    return true;
}

#define FTURN_TRANSPOSE(src1, src2, src3, src4, src5, src6, src7, src8, mask) \
auto a = _mm_unpacklo_epi8((src1), (src1)); \
auto b = _mm_unpacklo_epi8((src2), (src2)); \
auto c = _mm_unpacklo_epi8((src3), (src3)); \
auto d = _mm_unpacklo_epi8((src4), (src4)); \
auto e = _mm_unpacklo_epi8((src5), (src5)); \
auto f = _mm_unpacklo_epi8((src6), (src6)); \
auto g = _mm_unpacklo_epi8((src7), (src7)); \
auto h = _mm_unpacklo_epi8((src8), (src8)); \
     \
auto a03b03 = _mm_unpacklo_epi16(a, b); \
auto c03d03 = _mm_unpacklo_epi16(c, d); \
auto e03f03 = _mm_unpacklo_epi16(e, f); \
auto g03h03 = _mm_unpacklo_epi16(g, h); \
auto a47b47 = _mm_unpackhi_epi16(a, b); \
auto c47d47 = _mm_unpackhi_epi16(c, d); \
auto e47f47 = _mm_unpackhi_epi16(e, f); \
auto g47h47 = _mm_unpackhi_epi16(g, h); \
    \
auto a01b01c01d01 = _mm_unpacklo_epi32(a03b03, c03d03); \
auto a23b23c23d23 = _mm_unpackhi_epi32(a03b03, c03d03); \
auto e01f01g01h01 = _mm_unpacklo_epi32(e03f03, g03h03); \
auto e23f23g23h23 = _mm_unpackhi_epi32(e03f03, g03h03); \
auto a45b45c45d45 = _mm_unpacklo_epi32(a47b47, c47d47); \
auto a67b67c67d67 = _mm_unpackhi_epi32(a47b47, c47d47); \
auto e45f45g45h45 = _mm_unpacklo_epi32(e47f47, g47h47); \
auto e67f67g67h67 = _mm_unpackhi_epi32(e47f47, g47h47); \
    \
auto a0b0c0d0e0f0g0h0 = _mm_unpacklo_epi64(a01b01c01d01, e01f01g01h01); \
auto a1b1c1d1e1f1g1h1 = _mm_unpackhi_epi64(a01b01c01d01, e01f01g01h01); \
auto a2b2c2d2e2f2g2h2 = _mm_unpacklo_epi64(a23b23c23d23, e23f23g23h23); \
auto a3b3c3d3e3f3g3h3 = _mm_unpackhi_epi64(a23b23c23d23, e23f23g23h23); \
auto a4b4c4d4e4f4g4h4 = _mm_unpacklo_epi64(a45b45c45d45, e45f45g45h45); \
auto a5b5c5d5e5f5g5h5 = _mm_unpackhi_epi64(a45b45c45d45, e45f45g45h45); \
auto a6b6c6d6e6f6g6h6 = _mm_unpacklo_epi64(a67b67c67d67, e67f67g67h67); \
auto a7b7c7d7e7f7g7h7 = _mm_unpackhi_epi64(a67b67c67d67, e67f67g67h67); \
    \
(src1) = _mm_shuffle_epi8(a0b0c0d0e0f0g0h0, (mask)); \
(src2) = _mm_shuffle_epi8(a1b1c1d1e1f1g1h1, (mask)); \
(src3) = _mm_shuffle_epi8(a2b2c2d2e2f2g2h2, (mask)); \
(src4) = _mm_shuffle_epi8(a3b3c3d3e3f3g3h3, (mask)); \
(src5) = _mm_shuffle_epi8(a4b4c4d4e4f4g4h4, (mask)); \
(src6) = _mm_shuffle_epi8(a5b5c5d5e5f5g5h5, (mask)); \
(src7) = _mm_shuffle_epi8(a6b6c6d6e6f6g6h6, (mask)); \
(src8) = _mm_shuffle_epi8(a7b7c7d7e7f7g7h7, (mask));


void turnPlaneRight(BYTE* pDst, const BYTE* pSrc, BYTE* buffer, int srcWidth, int srcHeight, int dstPitch, int srcPitch) {
    bool useBuffer = true;
    if (dstPitch == srcHeight) {
        buffer = pDst;
        useBuffer = false;
    }

    auto pDst2 = pDst;
    auto pSrc2 = pSrc;
    auto mask = _mm_set_epi8(0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 4, 6, 8, 10, 12, 14);
    int srcWidthMod8 = (srcWidth / 8) * 8;
    int srcHeightMod8 = (srcHeight / 8) * 8;
    for(int y=0; y<srcHeightMod8; y+=8)
    {
        int offset = srcHeight-8-y;
        for (int x=0; x<srcWidthMod8; x+=8)
        {
            auto src1 = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(pSrc+x+srcPitch*0));
            auto src2 = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(pSrc+x+srcPitch*1));
            auto src3 = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(pSrc+x+srcPitch*2));
            auto src4 = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(pSrc+x+srcPitch*3));
            auto src5 = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(pSrc+x+srcPitch*4));
            auto src6 = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(pSrc+x+srcPitch*5));
            auto src7 = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(pSrc+x+srcPitch*6));
            auto src8 = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(pSrc+x+srcPitch*7));

            FTURN_TRANSPOSE(src1, src2, src3, src4, src5, src6, src7, src8, mask);

            _mm_storel_epi64(reinterpret_cast<__m128i*>(buffer+offset+srcHeight*0), src1);
            _mm_storel_epi64(reinterpret_cast<__m128i*>(buffer+offset+srcHeight*1), src2);
            _mm_storel_epi64(reinterpret_cast<__m128i*>(buffer+offset+srcHeight*2), src3);
            _mm_storel_epi64(reinterpret_cast<__m128i*>(buffer+offset+srcHeight*3), src4);
            _mm_storel_epi64(reinterpret_cast<__m128i*>(buffer+offset+srcHeight*4), src5);
            _mm_storel_epi64(reinterpret_cast<__m128i*>(buffer+offset+srcHeight*5), src6);
            _mm_storel_epi64(reinterpret_cast<__m128i*>(buffer+offset+srcHeight*6), src7);
            _mm_storel_epi64(reinterpret_cast<__m128i*>(buffer+offset+srcHeight*7), src8);

            offset += srcHeight*8;
        }
        pSrc += srcPitch * 8;
    }

    if (srcHeightMod8 != srcHeight) {
        pSrc = pSrc2 + srcPitch*srcHeightMod8;
        for(int y=srcHeightMod8; y<srcHeight; ++y)
        {
            int offset = srcHeight-1-y;
            for (int x=0; x<srcWidth; ++x)
            {
                buffer[offset] = pSrc[x];
                offset += srcHeight;
            }
            pSrc += srcPitch;
        }
    }

    if (srcWidthMod8 != srcWidth) {
        pSrc = pSrc2;
        for(int y=0; y<srcHeight; ++y)
        {
            int offset = (srcWidthMod8+1)*srcHeight - 1 - y;
            for (int x=srcWidthMod8; x<srcWidth; ++x)
            {
                buffer[offset] = pSrc[x];
                offset += srcHeight;
            }
            pSrc += srcPitch;
        }
    }

    if (useBuffer) {
        for (int y = 0; y < srcWidth; ++y) {
            memcpy(pDst2, buffer, srcHeight);
            pDst2 += dstPitch;
            buffer += srcHeight;
        }
    }
}

void turnPlaneLeft(BYTE* pDst, const BYTE* pSrc, BYTE* buffer, int srcWidth, int srcHeight, int dstPitch, int srcPitch) {
    bool useBuffer = true;
    if (dstPitch == srcHeight) {
        buffer = pDst;
        useBuffer = false;
    }

    auto pDst2 = pDst;
    auto pSrc2 = pSrc;
    int srcWidthMod8 = (srcWidth / 8) * 8;
    int srcHeightMod8 = (srcHeight / 8) * 8;

    pSrc += srcWidth-8;
    auto mask = _mm_set_epi8(0, 0, 0, 0, 0, 0, 0, 0, 14, 12, 10, 8, 6, 4, 2, 0);
    for(int y=0; y<srcHeightMod8; y+=8)
    {
        int offset = y;
        for (int x=0; x<srcWidthMod8; x+=8)
        {
            auto src1 = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(pSrc-x+srcPitch*0));
            auto src2 = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(pSrc-x+srcPitch*1));
            auto src3 = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(pSrc-x+srcPitch*2));
            auto src4 = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(pSrc-x+srcPitch*3));
            auto src5 = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(pSrc-x+srcPitch*4));
            auto src6 = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(pSrc-x+srcPitch*5));
            auto src7 = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(pSrc-x+srcPitch*6));
            auto src8 = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(pSrc-x+srcPitch*7));

            FTURN_TRANSPOSE(src1, src2, src3, src4, src5, src6, src7, src8, mask);

            _mm_storel_epi64(reinterpret_cast<__m128i*>(buffer+offset+srcHeight*0), src8);
            _mm_storel_epi64(reinterpret_cast<__m128i*>(buffer+offset+srcHeight*1), src7);
            _mm_storel_epi64(reinterpret_cast<__m128i*>(buffer+offset+srcHeight*2), src6);
            _mm_storel_epi64(reinterpret_cast<__m128i*>(buffer+offset+srcHeight*3), src5);
            _mm_storel_epi64(reinterpret_cast<__m128i*>(buffer+offset+srcHeight*4), src4);
            _mm_storel_epi64(reinterpret_cast<__m128i*>(buffer+offset+srcHeight*5), src3);
            _mm_storel_epi64(reinterpret_cast<__m128i*>(buffer+offset+srcHeight*6), src2);
            _mm_storel_epi64(reinterpret_cast<__m128i*>(buffer+offset+srcHeight*7), src1);

            offset += srcHeight*8;
        }
        pSrc += srcPitch * 8;
    }

    if (srcHeightMod8 != srcHeight) {
        pSrc = pSrc2;

        pSrc += srcWidth-1 + srcPitch*srcHeightMod8;
        for(int y=srcHeightMod8; y<srcHeight; ++y)
        {
            int offset = y;
            for (int x=0; x<srcWidth; ++x)
            {
                buffer[offset] = pSrc[-x];
                offset += srcHeight;
            }
            pSrc += srcPitch;
        }
    }

    if (srcWidthMod8 != srcWidth) {
        pSrc = pSrc2;

        pSrc += srcWidth-1;
        for(int y=0; y<srcHeight; ++y)
        {
            int offset = y+srcHeight*srcWidthMod8;
            for (int x=srcWidthMod8; x<srcWidth; ++x)
            {
                buffer[offset] = pSrc[-x];
                offset += srcHeight;
            }
            pSrc += srcPitch;
        }
    }

    if (useBuffer) {
        for (int y = 0; y < srcWidth; ++y) {
            memcpy(pDst2, buffer, srcHeight);
            pDst2 += dstPitch;
            buffer += srcHeight;
        }
    }
}

void turnPlane180(BYTE* pDst, const BYTE* pSrc, BYTE*, int srcWidth, int srcHeight, int dstPitch, int srcPitch) {
    BYTE* pDst2 = pDst;
    const BYTE* pSrc2 = pSrc;
    int srcWidthMod16 = (srcWidth / 16) * 16;

    auto mask = _mm_set_epi8(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
    pDst += dstPitch * (srcHeight-1) + srcWidth - 16;
    for(int y = 0; y < srcHeight; ++y)
    {
        for (int x = 0; x < srcWidthMod16; x+=16)
        {
            auto src = _mm_loadu_si128(reinterpret_cast<const __m128i*>(pSrc+x));

            auto result = _mm_shuffle_epi8(src, mask);

            _mm_storeu_si128(reinterpret_cast<__m128i*>(pDst-x), result);
        }
        pSrc += srcPitch;
        pDst -= dstPitch;
    }
    if (srcWidthMod16 != srcWidth) {
        pSrc = pSrc2;
        pDst = pDst2 + dstPitch * (srcHeight-1) + srcWidth - 1;

        for (int y = 0; y < srcHeight; ++y) {
            for (int x = srcWidthMod16; x < srcWidth; ++x) {
                pDst[-x] = pSrc[x];
            }
            pSrc += srcPitch;
            pDst -= dstPitch;
        }
    }
}

class FTurn : public GenericVideoFilter {
public:
    FTurn(PClip child, TurnDirection direction, bool chroma, bool mt, IScriptEnvironment* env);
    PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env);

    ~FTurn() {
        delete [] buffer;
    }

private:
    bool chroma_;
    bool mt_;
    decltype(&turnPlaneLeft) turnFunction_;
    BYTE *buffer;
    BYTE *bufferUV;
};

FTurn::FTurn(PClip child, TurnDirection direction, bool chroma, bool mt, IScriptEnvironment* env) 
    : GenericVideoFilter(child), chroma_(chroma), mt_(mt), buffer(nullptr), bufferUV(nullptr) {
    if (!isSupportedColorspace(vi.pixel_type)) {
        env->ThrowError(getUnsupportedColorspaceMessage());
    }

    int CPUInfo[4]; //eax, ebx, ecx, edx
    __cpuid(CPUInfo, 1);

    if (!(CPUInfo[2] & 0x00000200)) {
        env->ThrowError("Sorry, SSSE3 is required");
    }

    if (direction == TurnDirection::RIGHT || direction == TurnDirection::LEFT) {
        vi.width = child->GetVideoInfo().height;
        vi.height = child->GetVideoInfo().width;

        turnFunction_ = direction == TurnDirection::RIGHT ? turnPlaneRight : turnPlaneLeft;

        buffer = new BYTE[vi.width*vi.height];

        if (mt_) {
            bufferUV = new BYTE[vi.width*vi.height];
        }
    } else {
        turnFunction_ = turnPlane180;
    }
}

PVideoFrame FTurn::GetFrame(int n, IScriptEnvironment* env) {
    PVideoFrame src = child->GetFrame(n,env);
    auto dst = env->NewVideoFrame(vi);
    auto pSrcY = src->GetReadPtr(PLANAR_Y);
    auto pDstY = dst->GetWritePtr(PLANAR_Y);
    int srcPitchY = src->GetPitch(PLANAR_Y);
    int dstPitchY = dst->GetPitch(PLANAR_Y);
    int srcWidthY = src->GetRowSize(PLANAR_Y);
    int srcHeightY = src->GetHeight(PLANAR_Y);

    if (!(chroma_ && hasChroma(vi.pixel_type))) {
        turnFunction_(pDstY, pSrcY, buffer, srcWidthY, srcHeightY, dstPitchY, srcPitchY);
    } else {
        auto pDstU = dst->GetWritePtr(PLANAR_U);
        auto pDstV = dst->GetWritePtr(PLANAR_V);
        auto pSrcU = src->GetReadPtr(PLANAR_U);
        auto pSrcV = src->GetReadPtr(PLANAR_V);
        int srcPitchUV = src->GetPitch(PLANAR_U);
        int dstPitchUV = dst->GetPitch(PLANAR_U);
        int srcWidthUV = src->GetRowSize(PLANAR_U);
        int srcHeightUV = src->GetHeight(PLANAR_V);
        
        if (mt_) {
            auto thread2 = std::async(launch::async, [=] { 
                turnFunction_(pDstU, pSrcU, bufferUV, srcWidthUV, srcHeightUV, dstPitchUV, srcPitchUV);
                turnFunction_(pDstV, pSrcV, bufferUV, srcWidthUV, srcHeightUV, dstPitchUV, srcPitchUV);
            });
            turnFunction_(pDstY, pSrcY, buffer, srcWidthY, srcHeightY, dstPitchY, srcPitchY);
            thread2.wait();
        } else {
            turnFunction_(pDstU, pSrcU, buffer, srcWidthUV, srcHeightUV, dstPitchUV, srcPitchUV);
            turnFunction_(pDstV, pSrcV, buffer, srcWidthUV, srcHeightUV, dstPitchUV, srcPitchUV);
            turnFunction_(pDstY, pSrcY, buffer, srcWidthY, srcHeightY, dstPitchY, srcPitchY);
        }
    }
    return dst;
}

AVSValue __cdecl CreateFTurnLeft(AVSValue args, void*, IScriptEnvironment* env) {
    enum { CLIP, CHROMA, MT };
    return new FTurn(args[CLIP].AsClip(), TurnDirection::LEFT, args[CHROMA].AsBool(true), args[MT].AsBool(true), env);
}

AVSValue __cdecl CreateFTurnRight(AVSValue args, void*, IScriptEnvironment* env) {
    enum { CLIP, CHROMA, MT };
    return new FTurn(args[CLIP].AsClip(), TurnDirection::RIGHT, args[CHROMA].AsBool(true), args[MT].AsBool(true), env);
}

AVSValue __cdecl CreateFTurn180(AVSValue args, void*, IScriptEnvironment* env) {
    enum { CLIP, CHROMA };
    return new FTurn(args[CLIP].AsClip(), TurnDirection::TURN180, args[CHROMA].AsBool(true), false, env);
}

#ifdef FILTER_AVS_25
extern "C" __declspec(dllexport) const char* __stdcall AvisynthPluginInit2(IScriptEnvironment* env) {
#else
const AVS_Linkage *AVS_linkage = nullptr;

extern "C" __declspec(dllexport) const char* __stdcall AvisynthPluginInit3(IScriptEnvironment* env, const AVS_Linkage* const vectors) {

        AVS_linkage = vectors;
#endif
        env->AddFunction("fturnleft", "c[chroma]b[mt]b", CreateFTurnLeft, 0);
        env->AddFunction("fturnright", "c[chroma]b[mt]b", CreateFTurnRight, 0);
        env->AddFunction("fturn180", "c[chroma]b", CreateFTurn180, 0);
        return "Why are you looking at this?";
}