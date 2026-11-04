// Compares swrBlendRowNeon with the scalar blend on random pixels. Built with port/build.sh neon-test
// and run on the device, because it needs NEON.
#include <stdio.h>
#include <stdlib.h>
#include "sw_row_neon.h"

#define ROW 37

int main(void)
{
    static const int alphas[][2] = { {256, 0}, {255, 1}, {200, 56}, {128, 128}, {1, 255}, {100, 256}, {256, 256}, {-100, 256}, {-256, 256}, {0, 256} };
    long compared = 0, worst = 0, bad = 0;
    srand(1);
    for (int round = 0; round < 200000; round++)
    {
        uintpixel_t src[ROW], a[ROW], b[ROW];
        for (int i = 0; i < ROW; i++)
        {
            src[i] = ((uint32_t) rand() << 16) ^ (uint32_t) rand();
            // A mix of transparent, solid and partly transparent pixels, or groups of only the first two.
            int kind = rand() % 4;
            if (kind == 0) src[i] &= 0x00FFFFFF;
            else if (kind == 1 || (round & 1)) src[i] |= 0xFF000000;
            a[i] = b[i] = ((uint32_t) rand() << 16) ^ (uint32_t) rand();
        }
        uintpixel_t tintColor = (rand() % 2) ? 0xFFFFFFFF : (((uint32_t) rand() << 16) ^ (uint32_t) rand());
        const int* al = alphas[rand() % 10];
        int count = rand() % (ROW + 1);

        swrBlendRowNeon(a, src, count, tintColor, al[0], al[1]);
        for (int i = 0; i < count; i++)
        {
            swrBlendTexel(&b[i], src[i], tintColor, al[0], al[1]);
        }
        for (int i = 0; i < ROW; i++)
        {
            for (int c = 0; c < 32; c += 8)
            {
                long diff = labs((long) ((a[i] >> c) & 0xFF) - (long) ((b[i] >> c) & 0xFF));
                if (diff > worst) worst = diff;
                if (diff > 2 && bad++ < 8)
                    printf("diff %ld src=%08x tint=%08x alpha=%d,%d got=%08x want=%08x\n", diff, (unsigned) src[i], (unsigned) tintColor, al[0], al[1], (unsigned) a[i], (unsigned) b[i]);
            }
            compared++;
        }
    }
    printf("compared %ld pixels, worst channel difference %ld, over 2: %ld\n", compared, worst, bad);
    return bad != 0;
}
