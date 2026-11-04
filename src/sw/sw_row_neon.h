#ifndef _SW_ROW_NEON_H
#define _SW_ROW_NEON_H

#include "sw_renderer_private.h"

#if defined(__ARM_NEON) && PIXEL_SIZE == 32

#include <arm_neon.h>

#define SW_NEON_ROWS
#define SW_NEON_ROW_MAX 4096

// tint() and alphaBlend() for a row of pixels, 8 at a time. Results can differ from the scalar code
// by a level or two out of 255, because the tint divides by 256 instead of 255.
static void swrBlendRowNeon(uintpixel_t* dst, const uintpixel_t* src, int count, uintpixel_t tintColor, int srcalpha, int invalpha)
{
    if (srcalpha == 0) return;

    Pixel32ARGB t;
    t.l = tintColor;
    const bool tinted = (tintColor & 0xFFFFFF) != 0xFFFFFF;
    const bool opaqueCopy = invalpha < 3 && srcalpha > 253; // alphaBlend keeps the source's alpha for these
    const bool plain = !tinted && opaqueCopy;
    const bool subtract = srcalpha < 0;
    const bool normal = srcalpha + invalpha == 256; // add and subtract keep all of what's underneath
    const int drawAlpha = subtract ? -srcalpha : srcalpha;
    const uint8x8_t tintChannels[3] = { vdup_n_u8(t.p.b), vdup_n_u8(t.p.g), vdup_n_u8(t.p.r) };
    const uint16x8_t srcFactor = vdupq_n_u16((uint16_t) (subtract ? -srcalpha : srcalpha));
    const uint16x8_t dstFactor = vdupq_n_u16((uint16_t) invalpha);
    const uint16x8_t round255 = vdupq_n_u16(255);

    int x = 0;
    for (; x + 8 <= count; x += 8)
    {
        uint8x8x4_t s = vld4_u8((const uint8_t*) (src + x));
        uint8x8x4_t d = vld4_u8((const uint8_t*) (dst + x));
        uint8x8_t mask = vtst_u8(s.val[3], s.val[3]);

        // Few sprites have partly transparent pixels, so test for them once per group.
        uint8x8_t partial = vbic_u8(mask, vceq_u8(s.val[3], vdup_n_u8(0xFF)));
        if (UNLIKELY(vget_lane_u64(vreinterpret_u64_u8(partial), 0) != 0))
        {
            // The same sums as swrTexelAlpha and swrBlendTexel, with a factor per pixel.
            uint16x8_t texelAlpha = vmovl_u8(s.val[3]);
            uint16x8_t texel;
            if (drawAlpha == 256)
            {
                texel = vaddq_u16(texelAlpha, vshrq_n_u16(texelAlpha, 7));
            }
            else
            {
                uint16x8_t product = vmulq_n_u16(texelAlpha, (uint16_t) drawAlpha);
                texel = vshrq_n_u16(vaddq_u16(vaddq_u16(product, vshrq_n_u16(product, 8)), vdupq_n_u16(1)), 8);
            }
            uint16x8_t uncovered = vsubq_u16(vdupq_n_u16(256), texel);
            uint16x8_t texelDstFactor = normal ? uncovered : dstFactor;

            for (int c = 0; c < 3; c++)
            {
                uint8x8_t color = s.val[c];
                if (tinted)
                    color = vshrn_n_u16(vaddq_u16(vmull_u8(color, tintChannels[c]), round255), 8);

                uint16x8_t dstPart = vmulq_u16(vmovl_u8(d.val[c]), texelDstFactor);
                uint16x8_t srcPart = vmulq_u16(vmovl_u8(color), texel);
                uint16x8_t sum = subtract ? vqsubq_u16(dstPart, srcPart) : vqaddq_u16(dstPart, srcPart);
                d.val[c] = vbsl_u8(mask, vshrn_n_u16(sum, 8), d.val[c]);
            }
            uint16x8_t coverage = vaddq_u16(vshrq_n_u16(vmulq_u16(vmovl_u8(d.val[3]), uncovered), 8), texel);
            // Solid pixels in the group end up solid, as they do outside this path.
            uint8x8_t resultAlpha = vorr_u8(vqmovn_u16(coverage), vceq_u8(s.val[3], vdup_n_u8(0xFF)));
            d.val[3] = vbsl_u8(mask, resultAlpha, d.val[3]);

            vst4_u8((uint8_t*) (dst + x), d);
            continue;
        }

        if (plain)
        {
            for (int c = 0; c < 4; c++)
                d.val[c] = vbsl_u8(mask, s.val[c], d.val[c]);
        }
        else
        {
            for (int c = 0; c < 3; c++)
            {
                uint8x8_t color = s.val[c];
                if (tinted)
                    color = vshrn_n_u16(vaddq_u16(vmull_u8(color, tintChannels[c]), round255), 8);

                uint16x8_t dstPart = vmulq_u16(vmovl_u8(d.val[c]), dstFactor);
                uint16x8_t srcPart = vmulq_u16(vmovl_u8(color), srcFactor);
                // Saturating, which gives the same clamping as alphaBlend.
                uint16x8_t sum = subtract ? vqsubq_u16(dstPart, srcPart) : vqaddq_u16(dstPart, srcPart);
                d.val[c] = vbsl_u8(mask, vshrn_n_u16(sum, 8), d.val[c]);
            }
            d.val[3] = vbsl_u8(mask, opaqueCopy ? s.val[3] : vdup_n_u8(0xFF), d.val[3]);
        }

        vst4_u8((uint8_t*) (dst + x), d);
    }

    for (; x < count; x++)
    {
        swrBlendTexel(&dst[x], src[x], tintColor, srcalpha, invalpha);
    }
}

#endif

#endif//_SW_ROW_NEON_H
