#include "net.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "ip.h"
#include "net.h"
#include "platform.h"
#include "util.h"

/*
 * NOTE: if you want to add/delete the entries after net_run(),
 *       you need to protect these lists with a lock.
 */
static struct net_device* devices;

/**
 * スタックに登録されているプロトコル群。
 */
static struct net_protocol* protocols;

/**
 * ネットワークデバイスのオブジェクトを割り当てる。
 */
struct net_device* net_device_alloc(void) {
    struct net_device* dev;
    dev = memory_alloc(sizeof(*dev));
    if (!dev) {
        errorf("memory_alloc failure");
        return NULL;
    }

    return dev;
}

/*
 * ネットワークデバイスをプロトコルスタックへ登録する。
 *
 * NOTE: must not be call after net_run()
 */
int net_device_register(struct net_device* dev) {
    static unsigned int index = 0;
    // indexの採番とnameの生成
    dev->index = index++;
    snprintf(dev->name, sizeof(dev->name), "net%d", dev->index);

    dev->next = devices;
    devices = dev;
    infof("success dev=%s, type=0x%04x", dev->name, dev->type);
    return 0;
}

/**
 * ネットワークデバイスを起動する。
 */
static int net_device_open(struct net_device* dev) {
    infof("dev=%s", dev->name);
    // 既に起動済みの場合
    if (NET_DEVICE_IS_UP(dev)) {
        errorf("already opened, dev=%s", dev->name);
        return -1;
    }
    // デバイスドライバのopenルーチンを実行
    if (dev->ops->open) {
        if (dev->ops->open(dev) == -1) {
            errorf("failure, dev=%s", dev->name);
            return -1;
        }
    }

    dev->flags |= NET_DEVICE_FLAG_UP;
    return 0;
}

/**
 * ネットワークデバイスを停止する。
 */
static int net_device_close(struct net_device* dev) {
    infof("dev=%s", dev->name);
    // 既に停止済みの場合
    if (!NET_DEVICE_IS_UP(dev)) {
        errorf("not opened dev=%s", dev->name);
        return -1;
    }
    // デバイスドライバのcloseルーチンを実行
    if (dev->ops->close) {
        if (dev->ops->close(dev) == -1) {
            errorf("failure, dev=%s", dev->name);
            return -1;
        }
    }

    dev->flags &= ~NET_DEVICE_FLAG_UP;
    return 0;
}

/**
 * ネットワークデバイスに対してデータを出力する。
 */
int net_device_output(struct net_device* dev, uint16_t type,
                      const uint8_t* data, size_t len, const void* dst) {
    debugf("dev=%s, type=0x%04x, len=%zu", dev->name, type, len);
    debugdump(data, len);
    // デバイスが停止している場合
    if (!NET_DEVICE_IS_UP(dev)) {
        errorf("not opened, dev=%s", dev->name);
        return -1;
    }
    // MTUを超過している場合
    if (dev->mtu < len) {
        errorf("too long, dev=%s, mtu=%u, len=%zu", dev->name, dev->mtu, len);
        return -1;
    }

    // デバイスドライバのoutputルーチンを実行。
    // 未登録の場合はエラー
    if (!dev->ops->output) {
        errorf("output callback function is not set, dev=%s", dev->name);
        return -1;
    }
    if (dev->ops->output(dev, type, data, len, dst) == -1) {
        errorf("failure, dev=%s, len=%zu", dev->name, len);
        return -1;
    }

    return 0;
}

/*
 * プロトコルスタックに対してプロトコルを登録する。
 * NOTE: must not be call after net_run()
 */
int net_protocol_register(uint16_t type, net_protocol_handler_t handler) {
    struct net_protocol* proto;

    // 重複チェック。同一のプロトコル種別のハンドラが既に登録されていた場合はエラー。
    for (proto = protocols; proto; proto = proto->next) {
        if (proto->type == proto->type) {
            errorf("already registered, type=0x%04x", proto->type);
            return -1;
        }
    }

    proto = memory_alloc(sizeof(*proto));
    if (!proto) {
        errorf("memory_alloc() failure");
        return -1;
    }
    proto->type = type;
    proto->handler = handler;
    proto->next = protocols;
    protocols = proto;
    infof("success, type=0x%04x", type);
    return 0;
}

/**
 * デバイスドライバからプロトコルスタックへ入力パケットを渡す
 */
int net_input(uint16_t type, const uint8_t* data, size_t len,
              struct net_device* dev) {
    struct net_protocol* proto;

    debugf("dev=%s, type=0x%04x, len=%zu", dev->name, dev->type, len);
    debugdump(data, len);
    // プロトコルスタックを走査し、一致するプロトコル種別のハンドラを起動する。
    // 未対応のプロトコル種別を指定された際はパケットを破棄。
    for (proto = protocols; proto; proto = proto->next) {
        if (proto->type == type) {
            proto->handler(data, len, dev);
            return 0;
        }
    }

    return 0;
}

int net_init(void) {
    infof("initialize...");
    if (platform_init() == -1) {
        errorf("platform_init() failure");
        return -1;
    }

    // プロトコルスタックにIPを追加
    if (ip_init() == -1) {
        errorf("ip_init() failure");
        return -1;
    }

    infof("success");
    return 0;
}

int net_run(void) {
    infof("startup...");
    if (platform_run() == -1) {
        errorf("platform_run() failure");
        return -1;
    }

    // プロトコルの起動と共にネットワークデバイスの起動も行う
    for (struct net_device* dev = devices; dev; dev = dev->next) {
        net_device_open(dev);
    }

    infof("success");
    return 0;
}

int net_shutdown(void) {
    infof("shutting down...");
    if (platform_shutdown() == -1) {
        warnf("platform_shutdown() failure");
    }

    // ネットワークデバイスの停止も行う
    for (struct net_device* dev = devices; dev; dev = dev->next) {
        net_device_close(dev);
    }

    infof("success");
    return 0;
}
