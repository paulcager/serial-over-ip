/*
 * Copyright (c) 2014-2018 Cesanta Software Limited
 * All rights reserved
 *
 * Licensed under the Apache License, Version 2.0 (the ""License"");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an ""AS IS"" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>

#include "mgos.h"
#include "mgos_mongoose.h"
#include "mgos_app.h"
#include "mgos_gpio.h"
#include "mgos_timers.h"
#include "mgos_rpc_channel_uart.h"
#include "mgos_uart.h"
#include "mgos_http_server.h"
#include "mgos_wifi.h"

#define UART_NO mgos_sys_config_get_serial_uart()
#define STDERR_UART mgos_config_get_default_serial_debug_uart()
#define RESET_PIN mgos_sys_config_get_serial_reset_pin()
#define SHUTDOWN_PIN mgos_sys_config_get_serial_shutdown_pin()
#define SWITCH_MILLIS 120

static struct mg_connection *listener;
static int reset_pin = RESET_PIN;
static int shutdown_pin = SHUTDOWN_PIN;

static double last_wifi_ok;

/*
 * Dispatcher can be invoked with any amount of data (even none at all) and
 * at any time. Here we demonstrate how to process input line by line.
 */
static void uart_dispatcher(int uart_no, void *arg) {
    static struct mbuf lb = {0};
    assert(uart_no == UART_NO);
    size_t rx_av = mgos_uart_read_avail(uart_no);
    if (rx_av == 0) return;
    mgos_uart_read_mbuf(uart_no, &lb, rx_av);

    for (struct mg_connection *cc = mg_next(mgos_get_mgr(), NULL); cc != NULL; cc = mg_next(mgos_get_mgr(), cc)) {
        if (cc->listener == listener) {
            mg_send(cc, lb.buf, lb.len);
        }
    }

    mbuf_remove(&lb, lb.len);
    (void) arg;
}

static void http_pin_handler(struct mg_connection *c, int ev, void *p, void * user_data) {
    if (ev != MG_EV_HTTP_REQUEST) return;

    int pin = *(int*)user_data;

    mgos_gpio_write(pin, 0);
    mgos_msleep(SWITCH_MILLIS);
    mgos_gpio_write(pin, 1);

    //mg_http_reply(c, 204, "Content-Type: text/plain\r\nConnection: close\r\nContent-Length: 0\r\n", "");

    mg_send_response_line(c, 204, "Content-Type: text/plain\r\nConnection: close\r\nContent-Length: 0\r\n");
    c->flags |= (MG_F_SEND_AND_CLOSE);
}

static void http_reboot_handler(struct mg_connection *c, int ev, void *p, void * user_data) {
    if (ev != MG_EV_HTTP_REQUEST) return;

    mgos_system_restart();
}

// Event handler for an server (accepted) connection
static void connection_cb(struct mg_connection *c, int ev, void *ev_data, void *fn_data) {
    if (ev == MG_EV_POLL) return;

    if (ev == MG_EV_RECV) {
        mg_send(c, c->recv_mbuf.buf, c->recv_mbuf.len);
        mgos_uart_write(UART_NO, c->recv_mbuf.buf, c->recv_mbuf.len);
        mgos_uart_flush(UART_NO);
        mbuf_remove(&c->recv_mbuf, c->recv_mbuf.len);
    }
}

static void check_wifi_cb(void *arg) {
    enum mgos_wifi_status status = mgos_wifi_get_status();
    LOG(LL_DEBUG, ("uptime: %.2lf, last %.2lf %d", mgos_uptime(), last_wifi_ok, status));

    if (status == MGOS_WIFI_IP_ACQUIRED) {
        last_wifi_ok = mgos_uptime();
    } else {
        if (mgos_uptime() > last_wifi_ok + 30) {
            mgos_system_restart();
        }
    }

    (void) arg;
}

enum mgos_app_init_result mgos_app_init(void) {
    struct mgos_uart_config ucfg;
    mgos_uart_config_set_defaults(UART_NO, &ucfg);
    ucfg.baud_rate = 115200;
    ucfg.num_data_bits = 8;
    ucfg.parity = MGOS_UART_PARITY_NONE;
    ucfg.stop_bits = MGOS_UART_STOP_BITS_1;
    if (!mgos_uart_configure(UART_NO, &ucfg)) {
        return MGOS_APP_INIT_ERROR;
    }

    mgos_set_stderr_uart(STDERR_UART);
    mgos_set_stdout_uart(STDERR_UART);

    mgos_uart_set_dispatcher(UART_NO, uart_dispatcher, NULL /* arg */);
    mgos_uart_set_rx_enabled(UART_NO, true);

    listener = mg_bind(mgos_get_mgr(), "tcp://:8001", connection_cb, 0);

    if (listener == NULL) {
        return MGOS_APP_INIT_ERROR;
    }

    mgos_gpio_setup_output(RESET_PIN, 1);
    mgos_gpio_setup_output(SHUTDOWN_PIN, 1);

    mgos_register_http_endpoint("/reset", http_pin_handler, &reset_pin);
    mgos_register_http_endpoint("/shutdown", http_pin_handler, &shutdown_pin);
    mgos_register_http_endpoint("/wake-up", http_pin_handler, &shutdown_pin);
    mgos_register_http_endpoint("/reset-esp32", http_reboot_handler, NULL);

    mgos_http_server_set_document_root(NULL);

    last_wifi_ok = mgos_uptime();
    mgos_set_timer(2000, MGOS_TIMER_REPEAT, check_wifi_cb, NULL);

    return MGOS_APP_INIT_SUCCESS;
}
