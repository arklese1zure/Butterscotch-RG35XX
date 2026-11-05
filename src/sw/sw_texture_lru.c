#include <stdlib.h>
#include <string.h>
#include "sw_renderer_private.h"
#include "image/image_decoder.h"

static size_t swrTextureBytes(int width, int height)
{
    return (size_t) width * (size_t) height * sizeof(uintpixel_t);
}

void swrEvictTextureFromCache(SWRenderer* swr, int textureIndex)
{
    SWTexture* texture = swr->textures[textureIndex];
    swr->textures[textureIndex] = NULL;
    swr->textureBytesLoaded -= swrTextureBytes(texture->width, texture->height);
    
    swrFreeTexture(texture);
}

// Frees the TXTR page that was drawn from longest ago, other than `keepPageId`.
// A page drawn from this frame is likely to be needed again right away, so it's only
// freed if `evenIfInUse` is set. Returns false if nothing was freed.
static bool swrEvictOldestTexture(SWRenderer* swr, uint32_t keepPageId, bool evenIfInUse)
{
    int oldest = -1;
    for (uint32_t i = 0; i < swr->textureCount; i++)
    {
        if (i == keepPageId || !swr->textures[i])
            continue;
        if (oldest == -1 || swr->textureLastUsedFrame[i] < swr->textureLastUsedFrame[oldest])
            oldest = (int) i;
    }
    
    if (oldest == -1)
        return false;
    if (!evenIfInUse && swr->textureLastUsedFrame[oldest] == swr->textureFrame)
        return false;
    
    swrEvictTextureFromCache(swr, oldest);
    logInfo("SWR: Evicted TXTR page %d\n", oldest);
    return true;
}

// Makes room for a page within the budget. The budget can be exceeded if one frame draws from more than fits.
static void swrMakeRoomForTexture(SWRenderer* swr, uint32_t pageId, size_t bytes)
{
    if (swr->textureBudgetBytes == 0)
        return;
    while (swr->textureBytesLoaded + bytes > swr->textureBudgetBytes)
    {
        if (!swrEvictOldestTexture(swr, pageId, false))
            return;
    }
}

// The page's size from its PNG header, or 0 if it isn't a PNG.
static size_t swrPngBytes(const uint8_t* data, size_t size)
{
    static const uint8_t signature[8] = { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n' };
    if (size < 24 || memcmp(data, signature, sizeof(signature)) != 0)
        return 0;
    uint32_t width = (uint32_t) data[16] << 24 | (uint32_t) data[17] << 16 | (uint32_t) data[18] << 8 | data[19];
    uint32_t height = (uint32_t) data[20] << 24 | (uint32_t) data[21] << 16 | (uint32_t) data[22] << 8 | data[23];
    return swrTextureBytes((int) width, (int) height);
}

// Lazily decodes and uploads a TXTR page on first access.
// Returns true if the texture is ready, false if it failed to decode.
bool swrEnsureTextureIsLoaded(SWRenderer* swr, uint32_t pageId)
{
    swr->textureLastUsedFrame[pageId] = swr->textureFrame;
    
    if (swr->textures[pageId])
        return true;

    DataWin* dw = swr->base.dataWin;
    Texture* txtr = &dw->txtr.textures[pageId];

    int w, h;
    bool gm2022_5 = DataWin_isVersionAtLeast(dw, 2022, 5, 0, 0);
    
    uint8_t* pixels = NULL;
    
    do
    {
        if (!txtr->blobData) {
            DataWin_loadTxtrIfNeeded(dw, pageId);
        }
        
        if (txtr->blobData) {
            // Before decoding, since that briefly needs the page's size twice over.
            swrMakeRoomForTexture(swr, pageId, swrPngBytes(txtr->blobData, (size_t) txtr->blobSize));
            pixels = ImageDecoder_decodeToRgba(txtr->blobData, (size_t) txtr->blobSize, gm2022_5, &w, &h);
            if (pixels) {
                if (!txtr->mapped) {
                    free(txtr->blobData);
                    txtr->blobData = NULL;
                } else
                    dropMappedRange(txtr->blobData, 0, txtr->blobSize);
                break;
            }
            
            logWarn("swr: Failed to decode TXTR page %u.  This is likely because we're out of memory, so evicting a texture.\n", pageId);
        } else {
            logWarn("swr: Failed to load TXTR page %u.  This is likely because we're out of memory, so evicting a texture.\n", pageId);
        }
        
        if (!swrEvictOldestTexture(swr, pageId, true)) {
            logError("swr: Looks like we can't fit this texture in memory at all. Bummer.\n");
            break;
        }
    }
    while (!pixels);
    
    if (pixels == nullptr) {
        logError("swr: Failed to decode TXTR page %u.\n", pageId);
        return false;
    }

    // For pages that aren't PNGs, whose size is only known now.
    swrMakeRoomForTexture(swr, pageId, swrTextureBytes(w, h));
    swr->textures[pageId] = swrCreateTexture(pixels, w, h);
    swr->textureBytesLoaded += swrTextureBytes(w, h);
    free(pixels);
    
    logInfo("SWR: Loaded TXTR page %u (%dx%d)\n", pageId, w, h);
#ifdef TARGET_RG35XX
    {
        extern void rg35xxLogMemory(const char* where);
        rg35xxLogMemory("texture page");
    }
#endif
    
    return true;
}
