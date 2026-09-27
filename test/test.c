#include "test.h"

#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "net.h"
#include "util.h"

static volatile sig_atomic_t terminate;

static void on_signal(int signum) {
    (void)signum;
    terminate = 1;
}

static int setup(void) {
    struct sigaction sa = {0};

    sa.sa_handler = on_signal;
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        errorf("sigaction() %s", strerror(errno));
        return -1;
    }
    infof("setup protocol stack...");
    if (net_init() == -1) {
        errorf("net_init() failure");
        return -1;
    }
    if (net_run() == -1) {
        errorf("net_run() failure");
        return -1;
    }
    return 0;
}

static int cleanup(void) {
    infof("cleanup protocol stack...");
    if (net_shutdown() == -1) {
        errorf("net_shutdown() failure");
        return -1;
    }
    return 0;
}

static int app_main(void) {
    debugf("precc Ctrl+C terminate");
    while (!terminate) {
        sleep(1);
    }
    debugf("terminate");
    return 0;
}

/**
 * テストプログラムのメイン関数
 */
int main(void) {
    int ret;

    if (setup() == -1) {
        errorf("setup() failure");
        return -1;
    }
    ret = app_main();
    if (cleanup() == -1) {
        errorf("cleanup() failure");
        return -1;
    }
    return ret;
}
