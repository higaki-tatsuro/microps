#include "net.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "platform.h"
#include "util.h"

/*
 * NOTE: if you want to add/delete the entries after net_run(),
 *       you need to protect these lists with a lock.
 */
static struct net_device* devices;

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

    return 0;
}

int net_init(void) {
    infof("initialize...");
    if (platform_init() == -1) {
        errorf("platform_init() failure");
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
