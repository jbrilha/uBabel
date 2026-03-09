#include "iperf_hal.h"
#include <string.h>

// from espressif/iperf
typedef struct {
    uint32_t flag; /**< iperf flag */
    union {
        uint32_t destination_ip4; /**< destination ipv4 */
        char *destination_ip6;    /**< destination ipv6 */
    };
    union {
        uint32_t source_ip4; /**< source ipv4 */
        char *source_ip6;    /**< source ipv6 */
    };
    uint8_t type;          /**< address type, ipv4 or ipv6 */
    uint16_t dport;        /**< destination port */
    uint16_t sport;        /**< source port */
    uint32_t interval;     /**< report interval in secs */
    uint32_t time;         /**< total send time in secs */
    uint16_t len_send_buf; /**< send buffer length in bytes */
    int32_t bw_lim;        /**< bandwidth limit in Mbits/s */
} iperf_cfg_t;


bool iperf_start_default_server(void) {
    iperf_cfg_t cfg;
    memset(&cfg, 0, sizeof(cfg));

    return true;
}
