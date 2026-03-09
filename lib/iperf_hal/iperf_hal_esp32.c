#include "iperf.h"
#include "iperf_hal.h"
#include "network_hal.h"

static const char *TAG = "IPERF_HAL";

bool iperf_start_default_server(void) {
    if (!network_hal_wait_for_ip(30000)) {
        ESP_LOGE(TAG, "no ip after 30s, iperf not started");
        return false;
    }

    iperf_cfg_t cfg;
    memset(&cfg, 0, sizeof(cfg));

    cfg.flag = IPERF_FLAG_SERVER;
    int tries = 0;
    while (cfg.source_ip4 == 0 && tries++ < 5) {
        cfg.source_ip4 = network_hal_get_local_ip();
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    if (cfg.source_ip4 == 0) {
        ESP_LOGE(TAG, "no ip");
        return false;
    }

    cfg.len_send_buf = 0;
    cfg.flag |= IPERF_FLAG_TCP;
    cfg.sport = IPERF_DEFAULT_PORT;
    cfg.dport = IPERF_DEFAULT_PORT;

    cfg.interval = IPERF_DEFAULT_INTERVAL;
    cfg.time = IPERF_DEFAULT_TIME;

    ESP_LOGI(
        TAG,
        "mode=%s-%s sip=%" PRIu32 ".%" PRIu32 ".%" PRIu32 ".%" PRIu32 ":%d, \
             dip=%" PRIu32 ".%" PRIu32 ".%" PRIu32 ".%" PRIu32
        ":%d, interval=%" PRIu32 ", time=%" PRIu32,
        cfg.flag & IPERF_FLAG_TCP ? "tcp" : "udp",
        cfg.flag & IPERF_FLAG_SERVER ? "server" : "client",
        cfg.source_ip4 & 0xFF, (cfg.source_ip4 >> 8) & 0xFF,
        (cfg.source_ip4 >> 16) & 0xFF, (cfg.source_ip4 >> 24) & 0xFF, cfg.sport,
        cfg.destination_ip4 & 0xFF, (cfg.destination_ip4 >> 8) & 0xFF,
        (cfg.destination_ip4 >> 16) & 0xFF, (cfg.destination_ip4 >> 24) & 0xFF,
        cfg.dport, cfg.interval, cfg.time);

    iperf_start(&cfg);

    return true;
}
