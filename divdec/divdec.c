
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <sys/time.h>
#include <immintrin.h>

#define BITWIDTH 16

#if BITWIDTH == 16
#define NUM_BITS 5     // 分子のビット数 0-16
#define DEN_BITS 4     // 分母のビット数 0-15
#else
#define NUM_BITS 9     // 分子のビット数 0-256
#define DEN_BITS 8     // 分母のビット数 0-255
#endif

unsigned short int inv_table[1 << DEN_BITS];

void
init_inv_table(int numbits, int denbits)
{
    int i;
    for (i=1; i<(1 << denbits); ++i)
    {
        /** int q = n << 1;
        *** int r = (1 << q) / y;
        **/
        inv_table[i] = (1 << (numbits + denbits)) / i; // 0x10000 ～ 0x1ff00
    }
}

__m128i
div_int_sse2(__m128i mx, short *_y8, int numbits, int denbits)
{
    // 除数の逆数を128bitsデータに用意する
    __m128i mr = _mm_set_epi16(inv_table[_y8[7]], inv_table[_y8[6]],inv_table[_y8[5]], inv_table[_y8[4]], inv_table[_y8[3]], inv_table[_y8[2]],inv_table[_y8[1]], inv_table[_y8[0]]);
#if 0
    {
        unsigned short int *up;
        up = (unsigned short int *)&mx;
        int ix;
        for (ix=0; ix<8; ix++)
        {
            if (up[0] >= a->i[ix])
            {
                break;
            }
        }
    }
#endif
    // _mm_mullo_epi32 : 4つの32ビット整数の乗算を行う
    // int a = x * r /* + (1 << n) */;
    //         ^^^^^
    __m128i mc = _mm_mullo_epi16(mx, mr);
    // int a = /* x * r */ + (1 << n);
    //                     ^^^^^^^^^^
    __m128i md = _mm_add_epi16(mc, _mm_set1_epi16(1 << numbits));
    // a >> q;
    __m128i me = _mm_srli_epi16(md, numbits+denbits);

    return me;
}

void
round_short8(short *_rslt8, short *_x8, short *_y8)
{
    __m128i mx = _mm_set_epi16(_x8[7], _x8[6], _x8[5], _x8[4], _x8[3], _x8[2], _x8[1], _x8[0]);
    __m128i my = _mm_set_epi16(_y8[7], _y8[6], _y8[5], _y8[4], _y8[3], _y8[2], _y8[1], _y8[0]);

    __m128i ma = _mm_srli_epi16(my, 1);
    __m128i mb = _mm_add_epi16(mx, ma);
    __m128i mc = div_int_sse2(mb, _y8, NUM_BITS, DEN_BITS);

    _mm_storeu_si128((__m128i *)&_rslt8[0], mc);
}
int
fast_round(int x, int y)
{
    return (x+(y>>1))/y;
}

int
div_int(int x, int y)
{
    //int n = 8;  // 0x100(256)
    //int q = n << 1; // 0x200(512)
    //int r = (1 << q) / y;
    int r = (1 << (NUM_BITS + DEN_BITS)) / y;
    int a = x * r + (1 << NUM_BITS);
    return a >> (NUM_BITS + DEN_BITS);
}

void
loop_divint(int loops)
{
    int x, y, ans;
    for (x=0; x<loops; x++)
    {
        for (y=(x+1); 0<y; --y)
        {
            ans = div_int(x, y);
        }
    }
}
void
loop_div(int loops)
{
    int x, y, ans;
    for (x=0; x<loops; x++)
    {
        for (y=(x+1); 0<y; --y)
        {
            ans = x / y;
        }
    }
}
void
loop_div_simd(int loops)
{
    int x, y;
    short int simdanses[1<<DEN_BITS];

    for (x=0; x<loops; x+=8)
    {
        for (y=(x+8); 0<y; y -= 8)
        {
            __m128i mans;
            __m128i mx = _mm_set1_epi16(x);
            short int y8[8] = {y, y-1, y-2, y-3, y-4, y-5, y-6, y-7};
            mans = div_int_sse2(mx, y8, NUM_BITS, DEN_BITS);
            _mm_storeu_si128((__m128i *)&simdanses[0], mans);
        }
    }

}
void
loop_normal_round(int loops)
{
    int x, y, ans;
    for (x=0; x<loops; x++)
    {
        for (y=(x+1); 0<y; --y)
        {
            ans = (int)round((double)x/(double)y);
        }
    }
}
void
loop_fast_round(int loops)
{
    int x, y, ans;
    for (x=0; x<loops; x++)
    {
        for (y=(x+1); 0<y; --y)
        {
            ans = fast_round(x, y);
        }
    }
}
void
loop_short8_round(int loops)
{
    int x, y, ans;
    for (x=0; x<loops; x+=8)
    {
        for (y=(x+8); 0<y; y -= 8)
        {
            short int rslt8[8];
            short int x8[8] = {x, x, x, x, x, x, x, x};
            short int y8[8] = {y, y-1, y-2, y-3, y-4, y-5, y-6, y-7};
            round_short8(rslt8, x8, y8);
        }
    }
}
void
div_time(struct timeval *avetv, struct timeval *sumtv, int n)
{
    long long sumusec = sumtv->tv_sec * 1000000 + sumtv->tv_usec;
    long long aveusec = sumusec / n;
    avetv->tv_sec = aveusec / 1000000;
    avetv->tv_usec = aveusec % 1000000;
}

int
main(int ac, char *av[])
{
    int x, y, ans;
    int ans2;
    short int simdanses[1<<DEN_BITS];
    int z;
    struct timeval strtv, endtv, intrtv, acttv;
#define MAXLOOPS 1000
#if BITWIDTH == 16
    int divmax = 16;
#else
    int divmax = 256;
#endif
    //_allow_cpu_features (_FEATURE_SSE);

    init_inv_table(NUM_BITS, DEN_BITS);

    for (x=1; x<divmax; x++)
    {
        for (y=divmax-1; 0<y; --y)
        {
            ans = div_int(x, y);
            ans2 = x / y;
            if (ans != ans2)
            {printf("%d / %d = %d, %d\n", x, y, ans, ans2);}
        }
    }
    
    for (x=1; x<divmax; x++)
    {
        memset(simdanses, 0, sizeof(simdanses));
        for (y=divmax-1; 0<y; y-=8)
        {
            __m128i mans;
            __m128i mx = _mm_set1_epi16(x);
            //__m128i my = _mm_set_epi16(y-7, y-6, y-5, y-4, y-3, y-2, y-1, y);
            short int y8[8] = {y, y-1, y-2, y-3, y-4, y-5, y-6, y-7};

#if 0
            {
                short int *up;
                up = (short int *)&mx;
                printf("mx       :%2d %2d %2d %2d %2d %2d %2d %2d\n", up[0], up[1], up[2], up[3], up[4], up[5], up[6], up[7]);
                up = (short int *)&my;
                printf("my       :%2d %2d %2d %2d %2d %2d %2d %2d\n", up[0], up[1], up[2], up[3], up[4], up[5], up[6], up[7]);
            }
#endif
            mans = div_int_sse2(mx, y8, NUM_BITS, DEN_BITS);
            _mm_storeu_si128((__m128i *)&simdanses[0], mans);
#if 0
            {
                short int *up;
                up = (short int *)&mans;
                printf("mans     :%2d %2d %2d %2d %2d %2d %2d %2d\n", up[0], up[1], up[2], up[3], up[4], up[5], up[6], up[7]);
                printf("simdanses:%2d %2d %2d %2d %2d %2d %2d %2d\n", simdanses[7], simdanses[6], simdanses[5], simdanses[4], simdanses[3], simdanses[2], simdanses[1], simdanses[0]);
            }
#endif
            {
                int i,j;
                for (i=0; i<8; i++)
                {
                    j = y-i;
                    if (j <= 0)
                        continue;
                    ans = div_int(x, y-i);
                    if (ans != simdanses[i])
                    {printf("div_int/div_simd: %d / %d = %d, %d\n", x, y-i, ans, simdanses[i]);}
                }
            }
#if 0
            if (_may_i_use_cpu_feature(_FEATURE_SSE))
            {
                mans = _mm_div_epi16(mx, my);
                _mm_storeu_si128((__m128i *)&simdanses[0], mans);
                {
                    int i,j;
                    for (i=0; i<8; i++)
                    {
                        j = y-i;
                        if (j <= 0)
                            continue;
                        ans = div_int(x, y-i);
                        if (ans != simdanses[i])
                        {printf("div_int/mm_div: %d / %d = %d, %d\n", x, y-i, ans, simdanses[i]);}
                    }
                }
            }
#endif
        }
    }

    for (x=1; x<divmax; x++)
    {
        for (y=divmax-1; 0<y; --y)
        {
            ans = fast_round(x, y);
            ans2 = (int)round((double)x / (double)y);
            if (ans != ans2)
            {printf("Round: x %d  y %d : Fast %d Round %d\n", x, y, ans, ans2);}
        }
    }
    for (x=1; x<divmax; x++)
    {
        memset(simdanses, 0, sizeof(simdanses));
        for (y=divmax-1; 0<y; y-=8)
        {
            short x8[8] = {x, x, x, x, x, x, x, x};
            short y8[8] = {y, y-1, y-2, y-3, y-4, y-5, y-6, y-7};
            round_short8(simdanses, x8, y8);
        }
        {
            int i,j;
            for (i=0; i<8; i++)
            {
                j = y-i;
                if (j <= 0)
                    continue;
                ans = fast_round(x, y-i);
                if (ans != simdanses[i])
                {printf("Round: x %d y %d :  Fast %d Short8%d\n", x, y-i, ans, simdanses[i]);}
            }
        }
    }

    gettimeofday(&strtv, NULL);
    for (z=0; z<MAXLOOPS; z++)
    {
        loop_divint(MAXLOOPS);
    }
    gettimeofday(&endtv, NULL);
    timersub(&endtv, &strtv, &intrtv);
    div_time(&acttv, &intrtv, MAXLOOPS);
    printf("div_int: %ld.%06ld\n", acttv.tv_sec, acttv.tv_usec);

    gettimeofday(&strtv, NULL);
    for (z=0; z<MAXLOOPS; z++)
    {
        loop_div(MAXLOOPS);
    }
    gettimeofday(&endtv, NULL);
    timersub(&endtv, &strtv, &intrtv);
    div_time(&acttv, &intrtv, MAXLOOPS);
    printf("div: %ld.%06ld\n", acttv.tv_sec, acttv.tv_usec);

    gettimeofday(&strtv, NULL);
    for (z=0; z<MAXLOOPS; z++)
    {
        loop_div_simd(MAXLOOPS);
    }
    gettimeofday(&endtv, NULL);
    timersub(&endtv, &strtv, &intrtv);
    div_time(&acttv, &intrtv, MAXLOOPS);
    printf("div_simd: %ld.%06ld\n", acttv.tv_sec, acttv.tv_usec);

    gettimeofday(&strtv, NULL);
    for (z=0; z<MAXLOOPS; z++)
    {
        loop_normal_round(MAXLOOPS);
    }
    gettimeofday(&endtv, NULL);
    timersub(&endtv, &strtv, &intrtv);
    div_time(&acttv, &intrtv, MAXLOOPS);
    printf("normal_round: %ld.%06ld\n", acttv.tv_sec, acttv.tv_usec);

    gettimeofday(&strtv, NULL);
    for (z=0; z<MAXLOOPS; z++)
    {
        loop_fast_round(MAXLOOPS);
    }
    gettimeofday(&endtv, NULL);
    timersub(&endtv, &strtv, &intrtv);
    div_time(&acttv, &intrtv, MAXLOOPS);
    printf("fast_round: %ld.%06ld\n", acttv.tv_sec, acttv.tv_usec);

    gettimeofday(&strtv, NULL);
    for (z=0; z<MAXLOOPS; z++)
    {
        loop_short8_round(MAXLOOPS);
    }
    gettimeofday(&endtv, NULL);
    timersub(&endtv, &strtv, &intrtv);
    div_time(&acttv, &intrtv, MAXLOOPS);
    printf("short8_round: %ld.%06ld\n", acttv.tv_sec, acttv.tv_usec);

}

