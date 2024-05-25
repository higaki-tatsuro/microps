#ifndef NET_H
#define NET_H

#include <stddef.h>
#include <stdint.h>

#ifndef IFNAMSIZ
#define IFNAMSIZ 16
#endif

#define NET_DEVICE_TYPE_DUMMY 0x0000
#define NET_DEVICE_TYPE_LOOPBACK 0x0001
#define NET_DEVICE_TYPE_ETHERNET 0x0002

#define NET_DEVICE_FLAG_UP 0x0001
#define NET_DEVICE_FLAG_LOOPBACK 0x0010
#define NET_DEVICE_FLAG_BROADCAST 0x0020
#define NET_DEVICE_FLAG_P2P 0x0040
#define NET_DEVICE_FLAG_NEED_ARP 0x0100

#define NET_DEVICE_ADDR_LEN 16

#define NET_DEVICE_IS_UP(x) ((x)->flags & NET_DEVICE_FLAG_UP)
#define NET_DEVICE_STATE(x) (NET_DEVICE_IS_UP(x) ? "UP" : "DOWN")

/*
 * NOTE: use same value as the Ethernet types
 */
#define NET_PROTOCOL_TYPE_IP 0x0800
#define NET_PROTOCOL_TYPE_ARP 0x0806
#define NTT_PROTOCOL_TYPE_IPV6 0x86dd

/**
 * ネットワークデバイスを管理する構造体。
 * 連結リスト構造を取る。
 */
struct net_device {
    struct net_device* next;
    unsigned int index;   // デバイスを一意に識別するインデックス番号
    char name[IFNAMSIZ];  // デバイス名
    uint16_t type;        // デバイスタイプ
    uint16_t mtu;         // 最大伝送単位「MTU」の定義
    uint16_t flags;       // デバイスの特性と状態を示すフラグ
    uint16_t hlen;        // データリンクのヘッダ長
    uint16_t alen;        // データリンクのアドレス長
    uint8_t addr
        [NET_DEVICE_ADDR_LEN];  // データリンク層で利用するデバイスの物理アドレス
    uint8_t broadcast
        [NET_DEVICE_ADDR_LEN];  // データリンク層におけるブロードキャストアドレス
    struct net_device_ops* ops;  // デバイス固有の制御ルーチンへのポインタ
    void*
        priv;  // デバイス固有のパラメータを保持するための領域。デバイスドライバが内部で利用する。
};

/**
 * デバイスドライバに実装されている各種制御ルーチンへのアドレスを格納する構造体
 */
struct net_device_ops {
    int (*open)(struct net_device* dev);
    int (*close)(struct net_device* dev);
    int (*output)(struct net_device* dev, uint16_t type, const uint8_t* data,
                  size_t len, const void* dst);
};

typedef void (*net_protocol_handler_t)(const uint8_t* data, size_t len,
                                       struct net_device* dev);

/**
 * プロトコルを管理するための構造体。
 * プロトコルスタックにこれら構造体のオブジェクトを追加することで、任意のプロトコルでスタックを構成できる。
 */
struct net_protocol {
    struct net_protocol* next;
    uint16_t type;  // プロトコル種別を表す値
    net_protocol_handler_t
        handler;  // プロトコルのパケットを処理する入力ハンドラの関数ポインタ
};

extern struct net_device* net_device_alloc(void);
extern int net_device_register(struct net_device* dev);
extern int net_device_output(struct net_device* dev, uint16_t type,
                             const uint8_t* data, size_t len, const void* dst);

extern int net_init(void);
extern int net_run(void);
extern int net_shutdown(void);
extern int net_input(uint16_t type, const uint8_t* data, size_t len,
                     struct net_device* dev);
extern int net_protocol_register(uint16_t type, net_protocol_handler_t handler);

#endif
