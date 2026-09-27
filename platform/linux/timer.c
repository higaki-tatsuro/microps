#include <errno.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "platform.h"
#include "util.h"

/**
 * ソフトウェアタイマーを管理する構造体。
 * 連結リスト構造を取る。
 */
struct timer {
    struct timer* next;
    struct timeval interval;
    // 最後に発火した時刻
    struct timeval last;
    void (*handler)(void);
};

static timer_t timerid;

/*
 * NOTE: if you want to add/delete the entries after timer_run(),
 *       you need to protect these lists with a mutex.
 */
static struct timer* timers;

/**
 * 間隔と実行関数を渡してタイマーを生成する。
 */
int timer_register(struct timeval interval, void (*handler)(void)) {
    struct timer* timer;

    timer = memory_alloc(sizeof(*timer));
    if (!timer) {
        errorf("memory_alloc() failure");
        return -1;
    }
    timer->interval = interval;
    gettimeofday(&timer->last, NULL);
    timer->handler = handler;
    timer->next = timers;
    timers = timer;
    infof("success, interval={%d, %d}", interval.tv_sec, interval.tv_usec);
    return 0;
}

static void timer_irq_handler(unsigned int irq, void* arg) {
    struct timer* timer;
    struct timeval now, diff;

    (void)irq;
    (void)arg;
    gettimeofday(&now, NULL);
    for (timer = timers; timer; timer = timer->next) {
        timersub(&now, &timer->last, &diff);
        if (timercmp(&timer->interval, &diff, <) !=
            0) { /* true (!0) or false (0) */
            timer->handler();
            timer->last = now;
        }
    }
}

/**
 * タイマー機構の初期化
 */
int timer_init(void) {
    struct sigevent sev;

    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = INTR_IRQ_TIMER;
    sev.sigev_value.sival_ptr = &timerid;
    if (timer_create(CLOCK_REALTIME, &sev, &timerid) == -1) {
        errorf("timer_create: %s", strerror(errno));
        return -1;
    }
    return intr_register(INTR_IRQ_TIMER, timer_irq_handler, 0, NULL);
}

int timer_run(void) {
    const struct timespec ts = {0, 1000000}; /* 1ms */
    struct itimerspec interval = {ts, ts};

    if (timer_settime(timerid, 0, &interval, NULL) == -1) {
        errorf("timer_settime: %s", strerror(errno));
        return -1;
    }
    infof("interval={%d, %d}, initial={%d, %d}", interval.it_interval.tv_sec,
          interval.it_interval.tv_nsec, interval.it_value.tv_sec,
          interval.it_value.tv_nsec);
    return 0;
}

int timer_shutdown(void) {
    if (timer_delete(timerid) == -1) {
        errorf("timer_delete: %s", strerror(errno));
        return -1;
    }
    return 0;
}
