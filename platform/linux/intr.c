#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>

#include "platform.h"
#include "util.h"

/**
 * 割り込み番号と対応するハンドラの情報を格納する構造体。
 * 連結リスト構造を取る。
 */
struct irq_entry {
    struct irq_entry* next;
    unsigned int irq;  // 割り込み番号(IRQ番号)
    intr_isr_t isr;    // ハンドラ
    int flags;         // 割り込み登録時の動作をカスタマイズするためのフラグ
    void* arg;
};

/*
 * NOTE: if you want to add/delete the entries after intr_run(),
 *       you need to protect these lists with a mutex.
 */
static struct irq_entry* irqs;

static pthread_t tid;
static pthread_barrier_t barrier;
static sigset_t sigmask;

/**
 * 割り込みを登録する
 */
int intr_register(unsigned int irq, intr_isr_t isr, int flags, void* arg) {
    struct irq_entry* entry;

    for (entry = irqs; entry; entry = entry->next) {
        if (entry->irq == irq) {
            if (entry->flags ^ INTR_IRQ_SHARED || flags ^ INTR_IRQ_SHARED) {
                errorf("conflicts with already registered IRQs, irq=%u", irq);
                return -1;
            }
        }
    }
    entry = memory_alloc(sizeof(*entry));
    if (!entry) {
        errorf("memory_alloc() failure");
        return -1;
    }
    entry->irq = irq;
    entry->isr = isr;
    entry->flags = flags;
    entry->arg = arg;
    entry->next = irqs;
    irqs = entry;
    sigaddset(&sigmask, irq);
    infof("success, irq=%u", irq);
    return 0;
}

int intr_raise(unsigned int irq) { return pthread_kill(tid, (int)irq); }

/**
 * シグナル処理スレッドのエントリーポイント。
 * 補足対象シグナルの発生を待機し、発生したシグナルの番号に応じた処理を実行する。
 */
static void* intr_main(void* arg) {
    int terminate = 0, sig, err;
    struct irq_entry* entry;

    infof("start...");
    pthread_barrier_wait(&barrier);
    while (!terminate) {
        err = sigwait(&sigmask, &sig);
        if (err) {
            errorf("sigwait() %s", strerror(err));
            break;
        }
        switch (sig) {
            case SIGHUP:
                terminate = 1;
                break;
            default:
                if (sig != INTR_IRQ_TIMER) {
                    debugf("IRQ <%d> occurred", sig);
                }
                for (entry = irqs; entry; entry = entry->next) {
                    if (entry->irq == (unsigned int)sig) {
                        entry->isr(entry->irq, entry->arg);
                        if (entry->flags ^ INTR_IRQ_SHARED) {
                            break;
                        }
                    }
                }
                break;
        }
    }
    infof("terminated");
    return NULL;
}

/**
 * 割り込み機構の初期化関数
 * グローバル変数の初期化と設定を行う
 */
int intr_init(void) {
    tid = pthread_self();
    pthread_barrier_init(&barrier, NULL, 2);
    sigemptyset(&sigmask);
    sigaddset(&sigmask, SIGHUP);
    return 0;
}

/**
 * 割り込み機構の起動。
 * シグナル処理スレッドを起動する。
 */
int intr_run(void) {
    int err;

    // メインスレッドがシグナルを受け取らないようにブロックする。
    err = pthread_sigmask(SIG_BLOCK, &sigmask, NULL);
    if (err) {
        errorf("pthread_sigmask() %s", strerror(err));
        return -1;
    }
    err = pthread_create(&tid, NULL, intr_main, NULL);
    if (err) {
        errorf("pthread_create() %s", strerror(err));
        return -1;
    }
    pthread_barrier_wait(&barrier);
    return 0;
}

/**
 * 割り込み機構の停止。
 */
int intr_shutdown(void) {
    if (pthread_equal(tid, pthread_self()) != 0) {
        /* Thread not created. */
        return -1;
    }
    pthread_kill(tid, SIGHUP);
    pthread_join(tid, NULL);
    return 0;
}
