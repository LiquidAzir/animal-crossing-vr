#include "quest_eye_size.h"
#include <stdio.h>

static int checks, failures;
#define CHECK(c) do { ++checks; if (!(c)) { ++failures; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static void expected(uint32_t rw, uint32_t rh, uint32_t mw, uint32_t mh,
                     uint32_t cap, int ew, int eh) {
    int w = -1, h = -1;
    CHECK(quest_select_eye_size(rw,rh,mw,mh,cap,&w,&h));
    CHECK(w == ew); CHECK(h == eh);
}

static void bounds(uint32_t rw, uint32_t rh, uint32_t mw, uint32_t mh, uint32_t cap) {
    int w = -1, h = -1;
    CHECK(quest_select_eye_size(rw,rh,mw,mh,cap,&w,&h));
    CHECK(w > 0 && h > 0);
    CHECK((uint32_t)w <= rw && (uint32_t)h <= rh);
    CHECK((uint32_t)w <= mw && (uint32_t)h <= mh);
    CHECK((uint32_t)w <= cap && (uint32_t)h <= cap);
    uint64_t a = (uint64_t)w * rh, b = (uint64_t)h * rw;
    uint64_t error = a > b ? a-b : b-a;
    CHECK(error <= (rw > rh ? rw : rh)); /* At most one pixel of aspect rounding. */
}

int main(void) {
    expected(1680,1760,4096,4096,1760,1680,1760);
    expected(2800,2933,4096,4096,1760,1680,1760);
    expected(2933,2800,4096,4096,1760,1760,1680);
    expected(640,480,4096,4096,1760,640,480); /* Never upscale. */
    expected(2048,2048,4096,4096,1760,1760,1760);
    expected(2800,2933,1000,4096,1760,1000,1048); /* Runtime width limits scale both. */
    expected(2800,2933,4096,1000,1760,955,1000);
    expected(2000,1000,800,300,1760,600,300); /* More restrictive runtime axis wins. */
    expected(1,UINT32_MAX,UINT32_MAX,UINT32_MAX,1760,1,1760);
    expected(UINT32_MAX,1,UINT32_MAX,UINT32_MAX,1760,1760,1);
    expected(UINT32_MAX,UINT32_MAX,UINT32_MAX,UINT32_MAX,1760,1760,1760);
    expected(UINT32_MAX,UINT32_MAX,UINT32_MAX,UINT32_MAX,UINT32_MAX,INT_MAX,INT_MAX);
    expected(2,3,2,3,1,1,1);
    for (int invalid = 0; invalid < 5; ++invalid) {
        uint32_t v[] = {1680,1760,4096,4096,1760}; v[invalid] = 0;
        int w = -1, h = -1;
        CHECK(!quest_select_eye_size(v[0],v[1],v[2],v[3],v[4],&w,&h));
        CHECK(w == 0 && h == 0);
    }
    int value = -1;
    CHECK(!quest_select_eye_size(1,1,1,1,1,NULL,&value)); CHECK(value == 0);
    CHECK(!quest_select_eye_size(1,1,1,1,1,&value,NULL)); CHECK(value == 0);
    static const uint32_t values[] = {1,2,3,17,480,1760,2933,65535,INT_MAX,UINT32_MAX};
    for (unsigned w=0;w<sizeof(values)/sizeof(values[0]);++w)
        for (unsigned h=0;h<sizeof(values)/sizeof(values[0]);++h)
            for (unsigned m=0;m<sizeof(values)/sizeof(values[0]);++m) {
                bounds(values[w],values[h],values[m],values[9-m],1760);
                bounds(values[w],values[h],UINT32_MAX,UINT32_MAX,values[m]);
            }
    printf("Quest eye dimensions: %d checks, %d failures\n",checks,failures);
    return failures != 0;
}
