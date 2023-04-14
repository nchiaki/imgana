
#include <stdio.h>
#include <math.h>
#include <sys/time.h>
#include <string.h>
#include <stdlib.h>

int loopmax = 10000;
int examloop = 16;

int
fast_round(int x, int y)
{
    return (y+(x>>1))/x;
}

int
late_round(int x, int y)
{
    return (int)round((double)y/(double)x);
}

void
div_timeval(struct timeval *dst, struct timeval *src, int div)
{
    long long tmpusec;
    tmpusec = (long long)src->tv_usec + (long long)src->tv_sec * 1000000;
    tmpusec = tmpusec / div;
    dst->tv_sec = tmpusec / 1000000;
    dst->tv_usec = tmpusec % 1000000;
}

int
main (int ac, char *av[])
{
    int x, y, ans1, ans2, z;
    struct timeval strtv;
    struct timeval endtv;
    struct timeval fastrndtv[examloop];
    struct timeval laterndtv[examloop];
    struct timeval fastrndtvsum;
    struct timeval laterndtvsum;
    struct timeval fastrndtvave;
    struct timeval laterndtvave;

    for (x=1; x<loopmax; x++)
    {
        for (y=1; y<loopmax; y++)
        {
            ans1 = fast_round(x, y);

            ans2 = late_round(x, y);

            if (ans1 != ans2)
            {
                printf("x %d y %d ans1 %d ans2 %d", x, y, ans1, ans2);
            }
        }
    }
    memset(&fastrndtvsum, 0, sizeof(fastrndtvsum));
    for (z=0; z<examloop; z++)
    {
        gettimeofday(&strtv, NULL);
        for (x=1; x<loopmax; x++)
        {
            for (y=1; y<loopmax; y++)
            {
                ans1 = fast_round(x, y);
            }
        }
        gettimeofday(&endtv, NULL);
        fastrndtv[z].tv_sec = endtv.tv_sec - strtv.tv_sec;
        fastrndtv[z].tv_usec = endtv.tv_usec - strtv.tv_usec;
        timeradd(&fastrndtvsum, &fastrndtv[z], &fastrndtvsum);
    }
    div_timeval(&fastrndtvave, &fastrndtvsum, loopmax+examloop);
    printf("fast_round ave %ld.%06ld\n", fastrndtvave.tv_sec, fastrndtvave.tv_usec);

    memset(&laterndtvsum, 0, sizeof(laterndtvsum));
    for (z=0; z<examloop; z++)
    {
        gettimeofday(&strtv, NULL);
        for (x=1; x<loopmax; x++)
        {
            for (y=1; y<loopmax; y++)
            {
                ans2 = late_round(x, y);
            }
        }
        gettimeofday(&endtv, NULL);
        laterndtv[z].tv_sec = endtv.tv_sec - strtv.tv_sec;
        laterndtv[z].tv_usec = endtv.tv_usec - strtv.tv_usec;
        timeradd(&laterndtvsum, &laterndtv[z], &laterndtvsum);
    }
    div_timeval(&laterndtvave, &laterndtvsum, loopmax+examloop);
    printf("late_round ave %ld.%06ld\n", laterndtvave.tv_sec, laterndtvave.tv_usec);
}
