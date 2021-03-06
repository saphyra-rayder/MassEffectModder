//===============================================================================
// Copyright (c) 2017-2019 Pawel Kolodziejski
// Copyright (c) 2007-2016  Advanced Micro Devices, Inc. All rights reserved.
// Copyright (c) 2004-2006 ATI Technologies Inc.
//===============================================================================
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
//////////////////////////////////////////////////////////////////////////////

#include "Common.h"
#include "CompressonatorXCodec.h"

#define DXTC_OFFSET_ALPHA 0
#define DXTC_OFFSET_RGB 2

void DxtcCompressExplicitAlphaBlock(CODEC_BYTE alphaBlock[BLOCK_SIZE_4X4], CODEC_DWORD compressedBlock[2]);
void DxtcDecompressExplicitAlphaBlock(CODEC_BYTE alphaBlock[BLOCK_SIZE_4X4], CODEC_DWORD compressedBlock[2]);

void DxtcCompressAlphaBlock(CODEC_BYTE alphaBlock[BLOCK_SIZE_4X4], CODEC_DWORD compressedBlock[2]);
void DxtcDecompressAlphaBlock(CODEC_BYTE alphaBlock[BLOCK_SIZE_4X4], CODEC_DWORD compressedBlock[2]);

#define ConstructColour(r, g, b)  (((r) << 11) | ((g) << 5) | (b))

/*
Channel Bits
*/
#define RG 5
#define GG 6
#define BG 5

void DxtcCompressRGBBlock(CODEC_BYTE rgbBlock[BLOCK_SIZE_4X4X4], CODEC_DWORD compressedBlock[2], bool bDXT1 = false, bool bDXT1UseAlpha = false, CODEC_BYTE nDXT1AlphaThreshold = 0)
{
    /*
    ARGB Channel indexes
    */

    int RC = 2, GC = 1, BC = 0;

    if (bDXT1)
    {
        CODEC_BYTE nEndpoints[2][3][2];
        CODEC_BYTE nIndices[2][BLOCK_SIZE_4X4];

        double fError3 = CompRGBBlock((CODEC_DWORD*)rgbBlock, BLOCK_SIZE_4X4, RG, GG, BG, nEndpoints[0], nIndices[0], 3, true, false, 1, NULL, bDXT1UseAlpha, nDXT1AlphaThreshold);
        double fError4 = (fError3 == 0.0) ? FLT_MAX : CompRGBBlock((CODEC_DWORD*)rgbBlock, BLOCK_SIZE_4X4, RG, GG, BG, nEndpoints[1], nIndices[1], 4, true, false, 1, NULL, bDXT1UseAlpha, nDXT1AlphaThreshold);

        unsigned int nMethod = (fError3 <= fError4) ? 0 : 1;
        unsigned int c0 = ConstructColour((nEndpoints[nMethod][RC][0] >> (8 - RG)), (nEndpoints[nMethod][GC][0] >> (8 - GG)), (nEndpoints[nMethod][BC][0] >> (8 - BG)));
        unsigned int c1 = ConstructColour((nEndpoints[nMethod][RC][1] >> (8 - RG)), (nEndpoints[nMethod][GC][1] >> (8 - GG)), (nEndpoints[nMethod][BC][1] >> (8 - BG)));
        if ((nMethod == 1 && c0 <= c1) || (nMethod == 0 && c0 > c1))
            compressedBlock[0] = c1 | (c0 << 16);
        else
            compressedBlock[0] = c0 | (c1 << 16);

        compressedBlock[1] = 0;
        for (int i = 0; i<16; i++)
            compressedBlock[1] |= (nIndices[nMethod][i] << (2 * i));
    }
    else
    {
        CODEC_BYTE nEndpoints[3][2];
        CODEC_BYTE nIndices[BLOCK_SIZE_4X4];

        CompRGBBlock((CODEC_DWORD*)rgbBlock, BLOCK_SIZE_4X4, RG, GG, BG, nEndpoints, nIndices, 4, true, false, 1, NULL, bDXT1UseAlpha, nDXT1AlphaThreshold);

        unsigned int c0 = ConstructColour((nEndpoints[RC][0] >> (8 - RG)), (nEndpoints[GC][0] >> (8 - GG)), (nEndpoints[BC][0] >> (8 - BG)));
        unsigned int c1 = ConstructColour((nEndpoints[RC][1] >> (8 - RG)), (nEndpoints[GC][1] >> (8 - GG)), (nEndpoints[BC][1] >> (8 - BG)));
        if (c0 <= c1)
            compressedBlock[0] = c1 | (c0 << 16);
        else
            compressedBlock[0] = c0 | (c1 << 16);

        compressedBlock[1] = 0;
        for (int i = 0; i<16; i++)
            compressedBlock[1] |= (nIndices[i] << (2 * i));
    }
}

// This function decompresses a DXT colour block
// The block is decompressed to 8 bits per channel
void DxtcDecompressRGBBlock(CODEC_BYTE rgbBlock[BLOCK_SIZE_4X4X4], CODEC_DWORD compressedBlock[2], bool bDXT1)
{
    CODEC_DWORD n0 = compressedBlock[0] & 0xffff;
    CODEC_DWORD n1 = compressedBlock[0] >> 16;
    CODEC_DWORD r0;
    CODEC_DWORD g0;
    CODEC_DWORD b0;
    CODEC_DWORD r1;
    CODEC_DWORD g1;
    CODEC_DWORD b1;

    r0 = ((n0 & 0xf800) >> 8);
    g0 = ((n0 & 0x07e0) >> 3);
    b0 = ((n0 & 0x001f) << 3);

    r1 = ((n1 & 0xf800) >> 8);
    g1 = ((n1 & 0x07e0) >> 3);
    b1 = ((n1 & 0x001f) << 3);

    // Apply the lower bit replication to give full dynamic range
    r0 += (r0 >> 5); r1 += (r1 >> 5);
    g0 += (g0 >> 6); g1 += (g1 >> 6);
    b0 += (b0 >> 5); b1 += (b1 >> 5);

    CODEC_DWORD c0 = 0xff000000 | (r0 << 16) | (g0 << 8) | b0;
    CODEC_DWORD c1 = 0xff000000 | (r1 << 16) | (g1 << 8) | b1;

    if (!bDXT1 || n0 > n1)
    {
        CODEC_DWORD c2 = 0xff000000 | (((2 * r0 + r1 + 1) / 3) << 16) | (((2 * g0 + g1 + 1) / 3) << 8) | (((2 * b0 + b1 + 1) / 3));
        CODEC_DWORD c3 = 0xff000000 | (((2 * r1 + r0 + 1) / 3) << 16) | (((2 * g1 + g0 + 1) / 3) << 8) | (((2 * b1 + b0 + 1) / 3));

        for (int i = 0; i<16; i++)
        {
            switch ((compressedBlock[1] >> (2 * i)) & 3)
            {
                case 0:
                    ((CODEC_DWORD*)rgbBlock)[i] = c0;
                    break;
                case 1:
                    ((CODEC_DWORD*)rgbBlock)[i] = c1;
                    break;
                case 2:
                    ((CODEC_DWORD*)rgbBlock)[i] = c2;
                    break;
                case 3:
                    ((CODEC_DWORD*)rgbBlock)[i] = c3;
                    break;
            }
        }
    }
    else
    {
        // Transparent decode
        CODEC_DWORD c2 = 0xff000000 | (((r0 + r1) / 2) << 16) | (((g0 + g1) / 2) << 8) | (((b0 + b1) / 2));

        for (int i = 0; i<16; i++)
        {
            switch ((compressedBlock[1] >> (2 * i)) & 3)
            {
                case 0:
                    ((CODEC_DWORD*)rgbBlock)[i] = c0;
                    break;
                case 1:
                    ((CODEC_DWORD*)rgbBlock)[i] = c1;
                    break;
                case 2:
                    ((CODEC_DWORD*)rgbBlock)[i] = c2;
                    break;
                case 3:
                    ((CODEC_DWORD*)rgbBlock)[i] = 0x00000000;
                    break;
            }
        }
    }
}

void DxtcCompressRGBABlock(CODEC_BYTE rgbaBlock[BLOCK_SIZE_4X4X4], CODEC_DWORD compressedBlock[4])
{
    CODEC_BYTE alphaBlock[BLOCK_SIZE_4X4];
    for(CODEC_DWORD i = 0; i < 16; i++)
        alphaBlock[i] = static_cast<CODEC_BYTE>(((CODEC_DWORD*)rgbaBlock)[i] >> RGBA8888_OFFSET_A);

    DxtcCompressAlphaBlock(alphaBlock, &compressedBlock[DXTC_OFFSET_ALPHA]);

    DxtcCompressRGBBlock(rgbaBlock, &compressedBlock[DXTC_OFFSET_RGB], false, false);
}

void DxtcDecompressRGBABlock(CODEC_BYTE rgbaBlock[BLOCK_SIZE_4X4X4], CODEC_DWORD compressedBlock[4])
{
    CODEC_BYTE alphaBlock[BLOCK_SIZE_4X4];

    DxtcDecompressAlphaBlock(alphaBlock, &compressedBlock[DXTC_OFFSET_ALPHA]);
    DxtcDecompressRGBBlock(rgbaBlock, &compressedBlock[DXTC_OFFSET_RGB], false);

    for(CODEC_DWORD i = 0; i < 16; i++)
        ((CODEC_DWORD*)rgbaBlock)[i] = (alphaBlock[i] << RGBA8888_OFFSET_A) | (((CODEC_DWORD*)rgbaBlock)[i] & ~(BYTE_MASK << RGBA8888_OFFSET_A));
}

void DxtcCompressRGBABlock_ExplicitAlpha(CODEC_BYTE rgbaBlock[BLOCK_SIZE_4X4X4], CODEC_DWORD compressedBlock[4])
{
    CODEC_BYTE alphaBlock[BLOCK_SIZE_4X4];
    for(CODEC_DWORD i = 0; i < 16; i++)
        alphaBlock[i] = static_cast<CODEC_BYTE>(((CODEC_DWORD*)rgbaBlock)[i] >> RGBA8888_OFFSET_A);

    DxtcCompressExplicitAlphaBlock(alphaBlock, &compressedBlock[DXTC_OFFSET_ALPHA]);

    DxtcCompressRGBBlock(rgbaBlock, &compressedBlock[DXTC_OFFSET_RGB], false, false);
}

void DxtcDecompressRGBABlock_ExplicitAlpha(CODEC_BYTE rgbaBlock[BLOCK_SIZE_4X4X4], CODEC_DWORD compressedBlock[4])
{
    CODEC_BYTE alphaBlock[BLOCK_SIZE_4X4];

    DxtcDecompressExplicitAlphaBlock(alphaBlock, &compressedBlock[DXTC_OFFSET_ALPHA]);
    DxtcDecompressRGBBlock(rgbaBlock, &compressedBlock[DXTC_OFFSET_RGB], false);

    for(CODEC_DWORD i = 0; i < 16; i++)
        ((CODEC_DWORD*)rgbaBlock)[i] = (alphaBlock[i] << RGBA8888_OFFSET_A) | (((CODEC_DWORD*)rgbaBlock)[i] & ~(BYTE_MASK << RGBA8888_OFFSET_A));
}
