#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <immintrin.h>
#include <sys/time.h>

#define MAPDATAZ (16*256)
char mapdata[MAPDATAZ];
char mapdata2[MAPDATAZ];
char target[MAPDATAZ];

void
do_dump(char *title, unsigned char *p, int n)
{
    int i;
    printf("%s: ", title);
    for (i = 0; i < n; i++)
    {
        printf("%02x ", p[i]);
    }
    printf("\n");
}

void
init(void)
{
    int i;
    for (i = 0; i < MAPDATAZ; i++)
    {
        mapdata[i] = rand() & 0xff;
        mapdata2[i] = rand() & 0xff;
    }
    memset(target, 0, MAPDATAZ);
#if 0
    int ix;
    for (ix=0; ix<MAPDATAZ; ix+=sizeof(__m128i))
    {
        do_dump("mapdata\n", &mapdata[ix], sizeof(__m128i));
    }
    for (ix=0; ix<MAPDATAZ; ix+=sizeof(__m128i))
    {
        do_dump("mapdata2\n", &mapdata2[ix], sizeof(__m128i));
    }
    for (ix=0; ix<MAPDATAZ; ix+=sizeof(__m128i))
    {
        do_dump("target\n", &target[ix], sizeof(__m128i));
    }
#endif
}

void
div_time(struct timeval *avetv, struct timeval *sumtv, int n)
{
    long long sumusec = sumtv->tv_sec * 1000000 + sumtv->tv_usec;
    long long aveusec = sumusec / n;
    avetv->tv_sec = aveusec / 1000000;
    avetv->tv_usec = aveusec % 1000000;
}


void
do_normalproc(void)
{
    int i;
    for (i = 0; i < MAPDATAZ; i++)
    {
        target[i] = (mapdata[i] + mapdata2[i]) / 2;
    }
}


void
do_simd(void)
{
    int i;
    __m128i *p1 = (__m128i *)mapdata;
    __m128i *p2 = (__m128i *)mapdata2;
    __m128i *p3 = (__m128i *)target;
    __m128i d1, d2, sum, ave;

   // printf("__m128i size %ld, sum size %ld, ave size %ld\n", sizeof(__m128i), sizeof(sum), sizeof(ave));

    for (i = 0; i < MAPDATAZ/16; i++)
    {
        d1 = _mm_loadu_si128(p1);
        d2 = _mm_loadu_si128(p2);
        ave = _mm_avg_epu8(d1, d2);
        _mm_storeu_si128(p3, ave);
#if 0
        do_dump("p1", (unsigned char *)p1, sizeof(__m128i));
        do_dump("p2", (unsigned char *)p2, sizeof(__m128i));
        do_dump("d1", (unsigned char *)&d1, sizeof(__m128i));
        do_dump("d2", (unsigned char *)&d2, sizeof(__m128i));
        do_dump("ave", (unsigned char *)&ave, sizeof(__m128i));
        do_dump("p3", (unsigned char *)p3, sizeof(__m128i));
        printf("================\n");
#endif
        p1++;
        p2++;
        p3++;
    }

    return;

}

int
main(int ac, char *av[])
{
    int ix;
    struct timeval strttv, endtv, intrtv, sumtv, avetv;

    init();

    memset(&sumtv, 0, sizeof(sumtv));
    for (ix=0; ix<MAPDATAZ; ix++)
    {
        gettimeofday(&strttv, NULL);
        do_normalproc();
        gettimeofday(&endtv, NULL);
        timersub(&endtv, &strttv, &intrtv);
        timeradd(&intrtv, &sumtv, &sumtv);
    }
    div_time(&avetv, &sumtv, MAPDATAZ);
    printf("normalproc: %ld.%06ld\n", avetv.tv_sec, avetv.tv_usec);

    memset(&sumtv, 0, sizeof(sumtv));
    for (ix=0; ix<MAPDATAZ; ix++)
    {
        gettimeofday(&strttv, NULL);
        do_simd();
        gettimeofday(&endtv, NULL);
        timersub(&endtv, &strttv, &intrtv);
        timeradd(&intrtv, &sumtv, &sumtv);
    }
    div_time(&avetv, &sumtv, MAPDATAZ);
    printf("simd: %ld.%06ld\n", avetv.tv_sec, avetv.tv_usec);
}