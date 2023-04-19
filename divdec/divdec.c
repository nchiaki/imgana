
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
    __m128i mx = _mm_loadu_si128((__m128i *)&_x8[0]);
    __m128i my = _mm_loadu_si128((__m128i *)&_y8[0]);

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

unsigned char uchar_tableX[256];
unsigned char uchar_tableY[256];

void
init_uchar_table(void)
{
    int i;
    unsigned char *_uc;
    struct timeval tv;

    gettimeofday(&tv, NULL);

    srand((unsigned int)(tv.tv_sec + tv.tv_usec));
    
    _uc = uchar_tableX;
    for (i=0; i<sizeof(uchar_tableX); ++i)
    {*(_uc++) = rand() & 0xff;}
    _uc = uchar_tableY;
    for (i=0; i<sizeof(uchar_tableY); ++i)
    {*(_uc++) = rand() & 0xff;}
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
    int lpcnt = 0;
    for (x=0; x<loops; x++)
    {
        for (y=(x+1); 0<y; --y)
        {
            ans = div_int(x, y);
            if (loops <= lpcnt)
            {return;}
            lpcnt++;
        }
    }
}
void
loop_div(int loops)
{
    int x, y, ans;
    int lpcnt = 0;
    for (x=0; x<loops; x++)
    {
        for (y=(x+1); 0<y; --y)
        {
            ans = x / y;
            if (loops <= lpcnt)
            {return;}
            lpcnt++;
        }
    }
}
void
loop_div_simd(int loops)
{
    int x, y;
    short int simdanses[1<<DEN_BITS];
    int lpcnt = 0;

    //short int y8[8] = {1, 2, 3, 4, 5, 6, 7, 8};

    for (x=0; x<loops; x+=8)
    {
        //y = x + 8;
        //short int y8[8] = {y, y-1, y-2, y-3, y-4, y-5, y-6, y-7};
        for (y=(x+8); 0<y; y -= 8)
        {
            __m128i mans;
            __m128i mx = _mm_set1_epi16(x);
            short int y8[8] = {y, y-1, y-2, y-3, y-4, y-5, y-6, y-7};
            mans = div_int_sse2(mx, y8, NUM_BITS, DEN_BITS);
            _mm_storeu_si128((__m128i *)&simdanses[0], mans);
            if (loops <= lpcnt)
            {return;}
            lpcnt += 8;
        }
    }

}
void
loop_normal_round(int loops)
{
    int x, y, ans;
    int lpcnt = 0;
    for (x=0; x<loops; x++)
    {
        for (y=(x+1); 0<y; --y)
        {
            ans = (int)round((double)x/(double)y);
            if (loops <= lpcnt)
            {return;}
            lpcnt++;
        }
    }
}
void
loop_fast_round(int loops)
{
    int x, y, ans;
    int lpcnt = 0;
    for (x=0; x<loops; x++)
    {
        for (y=(x+1); 0<y; --y)
        {
            ans = fast_round(x, y);
            if (loops <= lpcnt)
            {return;}
            lpcnt++;
        }
    }
}
void
loop_short8_round(int loops)
{
    int x, y, ans;
    int lpcnt = 0;

    short int x8[8] = {4, 5, 4, 5, 4, 5, 4, 5};
    short int y8[8] = {1, 2, 3, 4, 5, 6, 7, 8};

    for (x=0; x<loops; x+=8)
    {
        for (y=(x+8); 0<y; y -= 8)
        {
            short int rslt8[8];
            //short int x8[8] = {x, x, x, x, x, x, x, x};
            //short int y8[8] = {y, y-1, y-2, y-3, y-4, y-5, y-6, y-7};
            round_short8(rslt8, x8, y8);
            if (loops <= lpcnt)
            {return;}
            lpcnt += 8;
        }
    }
}

void
loop_roundavg_ceil(int loops)
{
    int x, y, ans;
    int lpcnt = 0;
    for (x=0; x<loops; x++)
    {
        for (y=(x+1); 0<y; --y)
        {
            ans = (unsigned char)ceil( (double)(x + y)/2.0);
            if (loops <= lpcnt)
            {return;}
            lpcnt++;
        }
    }
}
void
loop_roundavg_inline(int loops)
{
    int x, y, ans;
    int lpcnt = 0;
    for (x=0; x<loops; x++)
    {
        for (y=(x+1); 0<y; --y)
        {
            ans = (x + y + 1) >> 1;
            if (loops <= lpcnt)
            {return;}
            lpcnt++;
        }
    }
}
void
loop_roundavg_simd(int loops)
{
    int i, x, y;
    int lpcnt = 0;

    for (i=0; i<loops; i+=(x+y))
    {
        for (x=0; x<sizeof(uchar_tableX); x+=16)
        {
            unsigned char avet[16];
            __m128i mx = _mm_loadu_si128((__m128i *)&uchar_tableX[x]);
            for (y=0; y<sizeof(uchar_tableY); y+=16)
            {
                __m128i my = _mm_loadu_si128((__m128i *)&uchar_tableY[y]);
                __m128i mave = _mm_avg_epu8(mx, my);
                _mm_storeu_si128((__m128i *)&avet[0], mave);
                if (loops <= lpcnt)
                {return;}
                lpcnt += 16;
            }
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

#define MAXLOOPS 10000

void
do_perf(char *submsg, void (*perffunc)(int), int loops)
{
    int z;
    struct timeval strtv, endtv, intrtv, acttv;

    gettimeofday(&strtv, NULL);
    for (z=0; z<loops; z++)
    {
        (*perffunc)(loops);
    }
    gettimeofday(&endtv, NULL);
    timersub(&endtv, &strtv, &intrtv);
    div_time(&acttv, &intrtv, loops);
    printf("%s: %d times %ld.%06ld sec\n", submsg, loops, acttv.tv_sec, acttv.tv_usec);
}

void
div_perf(void)
{
    do_perf("div", loop_div, MAXLOOPS);
    do_perf("div_int", loop_divint, MAXLOOPS);
    do_perf("div_simd", loop_div_simd, MAXLOOPS);
    _mm_prefetch((const char *)&inv_table[0], _MM_HINT_T0);
    do_perf("div_simd 2nd", loop_div_simd, MAXLOOPS);
}

void
round_perf(void)
{
    do_perf("normal_round", loop_normal_round, MAXLOOPS);
    do_perf("fast_round", loop_fast_round, MAXLOOPS);
    do_perf("short8_round", loop_short8_round, MAXLOOPS);
}

void
roundave_perf(void)
{
    do_perf("roundavg_ceil", loop_roundavg_ceil, MAXLOOPS);
    do_perf("roundavg_inline", loop_roundavg_inline, MAXLOOPS);
    do_perf("roundavg_simd", loop_roundavg_simd, MAXLOOPS);
}

int
main(int ac, char *av[])
{
    int x, y, ans;
    int ans2;
    short int simdanses[1<<DEN_BITS];
#if BITWIDTH == 16
    int divmax = 16;
#else
    int divmax = 256;
#endif
    //_allow_cpu_features (_FEATURE_SSE);

    init_inv_table(NUM_BITS, DEN_BITS);
    init_uchar_table();

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

    for (x=0; x<sizeof(uchar_tableX); x++)
    {
        for (y=0; y<sizeof(uchar_tableY); y++)
        {
            unsigned char ave0, ave1;
            ave0 = (unsigned char)ceil( (double)(uchar_tableX[x] + uchar_tableY[y])/2.0);
            ave1 = (uchar_tableX[x] + uchar_tableY[y] + 1) >> 1;
            if (ave0 != ave1)
            {printf("Ave: x %d y %d :  Ave0 %d Ave1 %d\n", uchar_tableX[x], uchar_tableY[y], ave0, ave1);}
        }
        
    }
    for (x=0; x<sizeof(uchar_tableX); x+=16)
    {
        unsigned char avet[16];
        __m128i mx = _mm_loadu_si128((__m128i *)&uchar_tableX[x]);
        for (y=0; y<sizeof(uchar_tableY); y+=16)
        {
            __m128i my = _mm_loadu_si128((__m128i *)&uchar_tableY[y]);
            __m128i mave = _mm_avg_epu8(mx, my);
            _mm_storeu_si128((__m128i *)&avet[0], mave);
            {
                int i,j;
                for (i=0; i<16; i++)
                {
                    unsigned char ave0, ave1;
                    ave0 = (unsigned char)ceil( (double)(uchar_tableX[x+i] + uchar_tableY[y+i])/2.0);
                    if (ave0 != avet[i])
                    {printf("Ave: x %d y %d :  Ave0 %d mmAve %d\n", uchar_tableX[x+i], uchar_tableY[y-i], ave0, avet[i]);}
                    ave1 = (uchar_tableX[x+i] + uchar_tableY[y+i] + 1) >> 1;
                    if (ave1 != avet[i])
                    {printf("Ave: x %d y %d :  Ave1 %d mmAve %d\n", uchar_tableX[x+i], uchar_tableY[y-i], ave1, avet[i]);}
                }
            }
        }
    }

    div_perf();
    putchar('\n');

    round_perf();
    putchar('\n');

    roundave_perf();
}

