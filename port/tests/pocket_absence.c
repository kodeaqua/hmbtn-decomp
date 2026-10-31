#include "psx/card.h"
#include <libmcrd.h>
#include <libmcx.h>
#include <stdio.h>
#include <string.h>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "Failed %d: %s\n", __LINE__, #x);                                      \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
static int read_card(void *c, int slot, void *p, size_t n) {
    (void)c;
    (void)slot;
    (void)p;
    (void)n;
    return 0;
}
static int write_card(void *c, int slot, const void *p, size_t n) {
    (void)c;
    (void)slot;
    (void)p;
    (void)n;
    return 1;
}
static int complete(int expected) {
    long cmd, result;
    return McxSync(1, &cmd, &result) == 1 && result == expected;
}
int main(void) {
    u8 bytes[32];
    memset(bytes, 0xa5, sizeof bytes);
    hm_card_attach((hm_card_storage){NULL, read_card, write_card});
    MemCardInit(0);
    MemCardStart();
    McxStartCom();
    CHECK(McxAllInfo(0, bytes) == 1);
    CHECK(McxGetTime(0, bytes) == 0);
    CHECK(complete(McxErrInvalid));
    CHECK(McxGetApl(0, bytes) == 1 && complete(McxErrInvalid));
    CHECK(McxGetTime(0, bytes) == 1 && complete(McxErrInvalid));
    CHECK(McxGetSerial(0, bytes) == 1 && complete(McxErrInvalid));
    CHECK(McxGetMem(0, bytes, 0, sizeof bytes) == 1 && complete(McxErrInvalid));
    CHECK(McxSetMem(0, bytes, 0, sizeof bytes) == 1 && complete(McxErrInvalid));
    CHECK(McxExecFlag(0, 0, 0) == 1);
    long cmd, result;
    CHECK(McxSync(1, &cmd, &result) == 1 && cmd == McxFuncExecFlag && result == McxErrInvalid);
    CHECK(McxExecApl(0, 0, 0) == 1 && complete(McxErrInvalid));
    CHECK(McxCurrCtrl(0, 0, 0, 0) == 1 && complete(McxErrInvalid));
    CHECK(McxFlashAcs(0, 0) == 1 && complete(McxErrInvalid));
    CHECK(McxSetLED(0, 0) == 1 && complete(McxErrInvalid));
    CHECK(McxHideTrans(0) == 1 && complete(McxErrInvalid));
    CHECK(McxShowTrans(0, 0, 0) == 1 && complete(McxErrInvalid));
    CHECK(McxReadDev(0, 0, bytes, bytes) == 1 && complete(McxErrInvalid));
    CHECK(McxWriteDev(0, 0, bytes, bytes) == 1 && complete(McxErrInvalid));
    for (size_t i = 0; i < sizeof bytes; ++i)
        CHECK(bytes[i] == 0xa5);
    CHECK(McxGetTime(1, bytes) == 0);
    hm_card_detach();
    CHECK(McxGetTime(0, bytes) == 1 && complete(McxErrNoCard));
    puts("PocketStation probes return asynchronous no-device errors without touching buffers");
    return 0;
}
