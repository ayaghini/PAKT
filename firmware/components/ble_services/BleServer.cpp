// BleServer – NimBLE GATT server implementation (FW-011, FW-012)
//
// Build environment: ESP-IDF v5.3.x + NimBLE (CONFIG_BT_NIMBLE_ENABLED=y).

#include "pakt/BleServer.h"
#include "pakt/BleChunker.h"
#include "pakt/BleUuids.h"

// NimBLE headers
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "esp_nimble_hci.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <cstring>

static const char *TAG = "ble_server";

// ── UUID declarations ─────────────────────────────────────────────────────────
// NimBLE requires ble_uuid128_t to be in static storage.

#define UUID128(arr) \
    { BLE_UUID_TYPE_128, { arr[0], arr[1], arr[2], arr[3], arr[4], arr[5], arr[6], arr[7], \
                           arr[8], arr[9], arr[10], arr[11], arr[12], arr[13], arr[14], arr[15] } }

static const ble_uuid128_t uuid_aprs_svc     = UUID128(pakt::uuids::kAprsService);
static const ble_uuid128_t uuid_telm_svc     = UUID128(pakt::uuids::kTelemetryService);
static const ble_uuid128_t uuid_dev_config   = UUID128(pakt::uuids::kDeviceConfig);
static const ble_uuid128_t uuid_dev_command  = UUID128(pakt::uuids::kDeviceCommand);
static const ble_uuid128_t uuid_dev_status   = UUID128(pakt::uuids::kDeviceStatus);
static const ble_uuid128_t uuid_dev_caps     = UUID128(pakt::uuids::kDeviceCapabilities);
static const ble_uuid128_t uuid_rx_packet    = UUID128(pakt::uuids::kRxPacketStream);
static const ble_uuid128_t uuid_tx_request   = UUID128(pakt::uuids::kTxRequest);
static const ble_uuid128_t uuid_tx_result    = UUID128(pakt::uuids::kTxResult);
static const ble_uuid128_t uuid_gps_telem    = UUID128(pakt::uuids::kGpsTelemetry);
static const ble_uuid128_t uuid_power_telem  = UUID128(pakt::uuids::kPowerTelemetry);
static const ble_uuid128_t uuid_system_telem = UUID128(pakt::uuids::kSystemTelemetry);

// ── GATT access callback declarations ────────────────────────────────────────

static int aprs_access_cb(uint16_t conn_h, uint16_t attr_h,
                           struct ble_gatt_access_ctxt *ctxt, void *arg);
static int dis_access_cb (uint16_t conn_h, uint16_t attr_h,
                           struct ble_gatt_access_ctxt *ctxt, void *arg);
static int telm_access_cb(uint16_t conn_h, uint16_t attr_h,
                           struct ble_gatt_access_ctxt *ctxt, void *arg);

// ── Value handle storage ──────────────────────────────────────────────────────
// Populated by ble_gatts_add_svcs() at startup.

static uint16_t g_h_dev_config   = 0;
static uint16_t g_h_dev_command  = 0;
static uint16_t g_h_dev_status   = 0;
static uint16_t g_h_dev_caps     = 0;
static uint16_t g_h_rx_packet    = 0;
static uint16_t g_h_tx_request   = 0;
static uint16_t g_h_tx_result    = 0;
static uint16_t g_h_gps_telem    = 0;
static uint16_t g_h_power_telem  = 0;
static uint16_t g_h_system_telem = 0;

// ── GATT service table ────────────────────────────────────────────────────────

static const struct ble_gatt_chr_def aprs_chars[] = {
    {
        .uuid        = &uuid_dev_config.u,
        .access_cb   = aprs_access_cb,
        .val_handle  = &g_h_dev_config,
        .flags       = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
    },
    {
        .uuid        = &uuid_dev_command.u,
        .access_cb   = aprs_access_cb,
        .val_handle  = &g_h_dev_command,
        .flags       = BLE_GATT_CHR_F_WRITE_NO_RSP,
    },
    {
        .uuid        = &uuid_dev_status.u,
        .access_cb   = aprs_access_cb,
        .val_handle  = &g_h_dev_status,
        .flags       = BLE_GATT_CHR_F_NOTIFY,
    },
    {
        // Device Capabilities: read-only, no security restriction (INT-001).
        .uuid        = &uuid_dev_caps.u,
        .access_cb   = aprs_access_cb,
        .val_handle  = &g_h_dev_caps,
        .flags       = BLE_GATT_CHR_F_READ,
    },
    {
        .uuid        = &uuid_rx_packet.u,
        .access_cb   = aprs_access_cb,
        .val_handle  = &g_h_rx_packet,
        .flags       = BLE_GATT_CHR_F_NOTIFY,
    },
    {
        .uuid        = &uuid_tx_request.u,
        .access_cb   = aprs_access_cb,
        .val_handle  = &g_h_tx_request,
        .flags       = BLE_GATT_CHR_F_WRITE,
    },
    {
        .uuid        = &uuid_tx_result.u,
        .access_cb   = aprs_access_cb,
        .val_handle  = &g_h_tx_result,
        .flags       = BLE_GATT_CHR_F_NOTIFY,
    },
    { 0 } // terminator
};

static const struct ble_gatt_chr_def telm_chars[] = {
    {
        .uuid       = &uuid_gps_telem.u,
        .access_cb  = telm_access_cb,
        .val_handle = &g_h_gps_telem,
        .flags      = BLE_GATT_CHR_F_NOTIFY,
    },
    {
        .uuid       = &uuid_power_telem.u,
        .access_cb  = telm_access_cb,
        .val_handle = &g_h_power_telem,
        .flags      = BLE_GATT_CHR_F_NOTIFY,
    },
    {
        .uuid       = &uuid_system_telem.u,
        .access_cb  = telm_access_cb,
        .val_handle = &g_h_system_telem,
        .flags      = BLE_GATT_CHR_F_NOTIFY,
    },
    { 0 } // terminator
};

static const struct ble_gatt_chr_def dis_chars[] = {
    {
        .uuid      = BLE_UUID16_DECLARE(0x2A29), // Manufacturer Name
        .access_cb = dis_access_cb,
        .flags     = BLE_GATT_CHR_F_READ,
    },
    {
        .uuid      = BLE_UUID16_DECLARE(0x2A24), // Model Number
        .access_cb = dis_access_cb,
        .flags     = BLE_GATT_CHR_F_READ,
    },
    {
        .uuid      = BLE_UUID16_DECLARE(0x2A26), // Firmware Revision
        .access_cb = dis_access_cb,
        .flags     = BLE_GATT_CHR_F_READ,
    },
    { 0 }
};

static const struct ble_gatt_svc_def gatt_svcs[] = {
    { // Device Information Service
        .type            = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid            = BLE_UUID16_DECLARE(0x180A),
        .characteristics = dis_chars,
    },
    { // APRS Service
        .type            = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid            = &uuid_aprs_svc.u,
        .characteristics = aprs_chars,
    },
    { // Device Telemetry Service
        .type            = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid            = &uuid_telm_svc.u,
        .characteristics = telm_chars,
    },
    { 0 } // terminator
};

// ── Chunker instances (one per writable characteristic) ───────────────────────

// Forward declaration; callbacks defined after BleServer singleton is available.
static void on_config_chunk_complete(const uint8_t *data, size_t len);
static void on_tx_req_chunk_complete(const uint8_t *data, size_t len);

static pakt::BleChunker g_config_chunker(on_config_chunk_complete);
static pakt::BleChunker g_tx_req_chunker(on_tx_req_chunk_complete);

// ── Security helper ───────────────────────────────────────────────────────────

static bool conn_is_encrypted_and_bonded(uint16_t conn_handle)
{
    struct ble_gap_conn_desc desc{};
    if (ble_gap_conn_find(conn_handle, &desc) != 0) return false;
    return desc.sec_state.encrypted && desc.sec_state.bonded;
}

// ── GAP event handler ─────────────────────────────────────────────────────────

static int gap_event_cb(struct ble_gap_event *event, void *arg);

static void start_advertising_internal(const char *device_name)
{
    ble_addr_t addr{};
    if (ble_hs_id_gen_rnd(1, &addr) != 0) {
        ESP_LOGE(TAG, "ble_hs_id_gen_rnd failed");
        return;
    }
    ble_hs_id_set_rnd(addr.val);

    struct ble_gap_adv_params adv_params{};
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    struct ble_hs_adv_fields fields{};
    fields.flags                 = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.name                  = reinterpret_cast<const uint8_t *>(device_name);
    fields.name_len              = static_cast<uint8_t>(strlen(device_name));
    fields.name_is_complete      = 1;
    fields.uuids128              = &uuid_aprs_svc;
    fields.num_uuids128          = 1;
    fields.uuids128_is_complete  = 1;

    int rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gap_adv_set_fields: %d", rc);
        return;
    }

    rc = ble_gap_adv_start(BLE_OWN_ADDR_RANDOM, nullptr, BLE_HS_FOREVER,
                            &adv_params, gap_event_cb, nullptr);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gap_adv_start: %d", rc);
    } else {
        ESP_LOGI(TAG, "advertising started: %s", device_name);
    }
}

static int gap_event_cb(struct ble_gap_event *event, void *arg)
{
    pakt::BleServer &srv = pakt::BleServer::instance();

    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0) {
            ESP_LOGI(TAG, "connected: handle=%d", event->connect.conn_handle);
            srv.conn_handle_ = event->connect.conn_handle;
            srv.connected_   = true;
            srv.bonded_      = false;
        } else {
            ESP_LOGW(TAG, "connect failed: %d", event->connect.status);
            start_advertising_internal(srv.device_name_);
        }
        break;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "disconnected: reason=%d", event->disconnect.reason);
        srv.conn_handle_ = 0xFFFF;
        srv.connected_   = false;
        srv.bonded_      = false;
        g_config_chunker.reset();
        g_tx_req_chunker.reset();
        start_advertising_internal(srv.device_name_);
        break;

    case BLE_GAP_EVENT_ENCRYPT_CHANGE:
        if (event->enc_change.status == 0) {
            struct ble_gap_conn_desc desc{};
            if (ble_gap_conn_find(event->enc_change.conn_handle, &desc) == 0) {
                srv.bonded_ = desc.sec_state.bonded;
                ESP_LOGI(TAG, "encrypt change: encrypted=%d bonded=%d",
                         (int)desc.sec_state.encrypted, (int)desc.sec_state.bonded);
            }
        }
        break;

    case BLE_GAP_EVENT_REPEAT_PAIRING:
        // Accept re-pairing by deleting the old bond.
        ble_store_util_delete_peer(&event->repeat_pairing.conn_desc.peer_id_addr);
        return BLE_GAP_REPEAT_PAIRING_RETRY;

    case BLE_GAP_EVENT_SUBSCRIBE:
        ESP_LOGD(TAG, "subscribe: handle=%d cur_notify=%d",
                 event->subscribe.attr_handle, event->subscribe.cur_notify);
        break;

    default:
        break;
    }
    return 0;
}

// ── DIS access callback ───────────────────────────────────────────────────────

static int dis_access_cb(uint16_t /*conn_h*/, uint16_t /*attr_h*/,
                          struct ble_gatt_access_ctxt *ctxt, void * /*arg*/)
{
    if (ctxt->op != BLE_GATT_ACCESS_OP_READ_CHR) return BLE_ATT_ERR_UNLIKELY;

    const ble_uuid_t *uuid = ctxt->chr->uuid;
    const char *str = nullptr;

    if (ble_uuid_cmp(uuid, BLE_UUID16_DECLARE(0x2A29)) == 0) str = "PAKT";
    else if (ble_uuid_cmp(uuid, BLE_UUID16_DECLARE(0x2A24)) == 0) str = "APRS-TNC-1";
    else if (ble_uuid_cmp(uuid, BLE_UUID16_DECLARE(0x2A26)) == 0) str = "0.1.0";
    else return BLE_ATT_ERR_ATTR_NOT_FOUND;

    return os_mbuf_append(ctxt->om, str, strlen(str)) == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

// ── APRS access callback ──────────────────────────────────────────────────────

static int aprs_access_cb(uint16_t conn_h, uint16_t attr_h,
                           struct ble_gatt_access_ctxt *ctxt, void * /*arg*/)
{
    pakt::BleServer &srv = pakt::BleServer::instance();
    (void)attr_h;

    // ── Read ──────────────────────────────────────────────────────────────────
    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        if (ble_uuid_cmp(ctxt->chr->uuid, &uuid_dev_config.u) == 0) {
            if (!srv.handlers_.on_config_read) return BLE_ATT_ERR_UNLIKELY;
            uint8_t buf[256];
            size_t n = srv.handlers_.on_config_read(buf, sizeof(buf));
            if (n == 0) return BLE_ATT_ERR_UNLIKELY;
            return os_mbuf_append(ctxt->om, buf, n) == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        }
        if (ble_uuid_cmp(ctxt->chr->uuid, &uuid_dev_caps.u) == 0) {
            uint8_t buf[256];
            size_t n = 0;
            if (srv.handlers_.on_caps_read) {
                n = srv.handlers_.on_caps_read(buf, sizeof(buf));
            }
            if (n == 0) {
                // Fallback: empty JSON object if handler not wired.
                buf[0] = '{'; buf[1] = '}';
                n = 2;
            }
            return os_mbuf_append(ctxt->om, buf, n) == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        }
        return BLE_ATT_ERR_READ_NOT_PERMITTED;
    }

    // ── Write (all write types) ───────────────────────────────────────────────
    if (ctxt->op != BLE_GATT_ACCESS_OP_WRITE_CHR) return BLE_ATT_ERR_UNLIKELY;

    // Security check for config, command, and TX request.
    if (!conn_is_encrypted_and_bonded(conn_h)) {
        ESP_LOGW(TAG, "write rejected: link not encrypted+bonded");
        return BLE_ATT_ERR_INSUFFICIENT_AUTHEN;
    }

    // Flatten mbuf chain into a local buffer.
    uint8_t buf[512];
    uint16_t buf_len = sizeof(buf);
    int rc = ble_hs_mbuf_to_flat(ctxt->om, buf, sizeof(buf), &buf_len);
    if (rc != 0) return BLE_ATT_ERR_UNLIKELY;

    uint32_t now_ms = static_cast<uint32_t>(esp_timer_get_time() / 1000);

    if (ble_uuid_cmp(ctxt->chr->uuid, &uuid_dev_config.u) == 0) {
        if (!g_config_chunker.feed(buf, buf_len, now_ms)) return BLE_ATT_ERR_UNLIKELY;
        // Reassembled payload delivered via on_config_chunk_complete callback.
        // Direct (single-chunk) case: feed returns true and callback fires inline.
        return 0;
    }

    if (ble_uuid_cmp(ctxt->chr->uuid, &uuid_dev_command.u) == 0) {
        // Device Command is small (≤64 B) – not chunked, delivered directly.
        if (srv.handlers_.on_command && !srv.handlers_.on_command(buf, buf_len)) {
            return BLE_ATT_ERR_UNLIKELY;
        }
        return 0;
    }

    if (ble_uuid_cmp(ctxt->chr->uuid, &uuid_tx_request.u) == 0) {
        if (!g_tx_req_chunker.feed(buf, buf_len, now_ms)) return BLE_ATT_ERR_UNLIKELY;
        return 0;
    }

    return BLE_ATT_ERR_WRITE_NOT_PERMITTED;
}

// ── Telemetry access callback (notify-only; no read/write) ────────────────────

static int telm_access_cb(uint16_t /*conn_h*/, uint16_t /*attr_h*/,
                           struct ble_gatt_access_ctxt * /*ctxt*/, void * /*arg*/)
{
    return BLE_ATT_ERR_READ_NOT_PERMITTED;
}

// ── NimBLE host sync/reset callbacks ─────────────────────────────────────────

static void on_ble_sync()
{
    pakt::BleServer &srv = pakt::BleServer::instance();
    start_advertising_internal(srv.device_name_);
}

static void on_ble_reset(int reason)
{
    ESP_LOGE(TAG, "NimBLE host reset: reason=%d", reason);
}

static void ble_host_task(void * /*arg*/)
{
    nimble_port_run();          // blocks until nimble_port_stop() is called
    nimble_port_freertos_deinit();
}

// ── Chunker callbacks (now have access to Handlers) ──────────────────────────
// Re-define to forward to actual handlers after instance is accessible.

static void on_config_chunk_complete(const uint8_t *data, size_t len)
{
    pakt::BleServer &srv = pakt::BleServer::instance();
    if (srv.handlers_.on_config_write) {
        srv.handlers_.on_config_write(data, len);
    }
}

static void on_tx_req_chunk_complete(const uint8_t *data, size_t len)
{
    pakt::BleServer &srv = pakt::BleServer::instance();
    if (srv.handlers_.on_tx_request) {
        srv.handlers_.on_tx_request(data, len);
    }
}

// ── BleServer public interface ────────────────────────────────────────────────

namespace pakt {

BleServer &BleServer::instance()
{
    static BleServer s_instance;
    return s_instance;
}

bool BleServer::init(const Handlers &handlers, const char *device_name)
{
    handlers_ = handlers;
    strncpy(device_name_, device_name, sizeof(device_name_) - 1);
    device_name_[sizeof(device_name_) - 1] = '\0';

    esp_nimble_hci_init();
    nimble_port_init();

    // Security configuration: No I/O, bonding, LE Secure Connections.
    ble_hs_cfg.sm_io_cap        = BLE_SM_IO_CAP_NO_IO;
    ble_hs_cfg.sm_bonding       = 1;
    ble_hs_cfg.sm_sc            = 1;
    ble_hs_cfg.sm_our_key_dist  = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.sm_their_key_dist= BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.sync_cb          = on_ble_sync;
    ble_hs_cfg.reset_cb         = on_ble_reset;

    ble_svc_gap_init();
    ble_svc_gatt_init();

    if (int rc = ble_gatts_count_cfg(gatt_svcs); rc != 0) {
        ESP_LOGE(TAG, "ble_gatts_count_cfg: %d", rc);
        return false;
    }
    if (int rc = ble_gatts_add_svcs(gatt_svcs); rc != 0) {
        ESP_LOGE(TAG, "ble_gatts_add_svcs: %d", rc);
        return false;
    }
    if (int rc = ble_svc_gap_device_name_set(device_name_); rc != 0) {
        ESP_LOGE(TAG, "ble_svc_gap_device_name_set: %d", rc);
        return false;
    }

    // Copy value handles to the BleServer instance after registration.
    h_dev_config_   = g_h_dev_config;
    h_dev_command_  = g_h_dev_command;
    h_dev_status_   = g_h_dev_status;
    h_dev_caps_     = g_h_dev_caps;
    h_rx_packet_    = g_h_rx_packet;
    h_tx_request_   = g_h_tx_request;
    h_tx_result_    = g_h_tx_result;
    h_gps_telem_    = g_h_gps_telem;
    h_power_telem_  = g_h_power_telem;
    h_system_telem_ = g_h_system_telem;

    initialized_ = true;
    ESP_LOGI(TAG, "BleServer initialized");
    return true;
}

void BleServer::start()
{
    nimble_port_freertos_init(ble_host_task);
    // Advertising starts when NimBLE fires on_ble_sync after stack init.
}

// ── Notify helpers ────────────────────────────────────────────────────────────

bool BleServer::send_notify_(uint16_t val_handle, int64_t &last_us,
                              const uint8_t *data, size_t len)
{
    if (!connected_ || val_handle == 0) return false;

    // Rate-limit: drop if notified too recently.
    const int64_t now_us = esp_timer_get_time();
    if ((now_us - last_us) < static_cast<int64_t>(kNotifyMinIntervalMs) * 1000) {
        return false;
    }

    struct os_mbuf *om = ble_hs_mbuf_from_flat(data, len);
    if (!om) return false;

    int rc = ble_gatts_notify_custom(conn_handle_, val_handle, om);
    if (rc == 0) {
        last_us = now_us;
        return true;
    }
    return false;
}

bool BleServer::notify_status   (const uint8_t *d, size_t n) { return send_notify_(h_dev_status_,   last_notify_status_, d, n); }
bool BleServer::notify_rx_packet(const uint8_t *d, size_t n) { return send_notify_(h_rx_packet_,    last_notify_rx_,     d, n); }
bool BleServer::notify_tx_result(const uint8_t *d, size_t n) { return send_notify_(h_tx_result_,    last_notify_tx_res_, d, n); }
bool BleServer::notify_gps      (const uint8_t *d, size_t n) { return send_notify_(h_gps_telem_,   last_notify_gps_,    d, n); }
bool BleServer::notify_power    (const uint8_t *d, size_t n) { return send_notify_(h_power_telem_, last_notify_power_,  d, n); }
bool BleServer::notify_system   (const uint8_t *d, size_t n) { return send_notify_(h_system_telem_,last_notify_system_, d, n); }

bool BleServer::is_connected() const { return connected_; }
bool BleServer::is_bonded()    const { return bonded_;    }

} // namespace pakt
