// DeviceCapabilities.cpp – Device capability JSON serialiser (INT-001)

#include "pakt/DeviceCapabilities.h"

#include <cstdio>
#include <cstring>

namespace pakt {

// ── Feature name table ────────────────────────────────────────────────────────

struct FeatureEntry {
    Feature     flag;
    const char *name;
};

static constexpr FeatureEntry kFeatureNames[] = {
    { Feature::APRS_2M,      "aprs_2m"      },
    { Feature::BLE_CHUNKING, "ble_chunking"  },
    { Feature::TELEMETRY,    "telemetry"     },
    { Feature::MSG_ACK,      "msg_ack"       },
    { Feature::CONFIG_RW,    "config_rw"     },
    { Feature::GPS_ONBOARD,  "gps_onboard"   },
    { Feature::HF_AUDIO,     "hf_audio"      },
    { Feature::KISS_BLE,     "kiss_ble"      },
};

static constexpr size_t kFeatureCount =
    sizeof(kFeatureNames) / sizeof(kFeatureNames[0]);

// ── to_json ───────────────────────────────────────────────────────────────────

size_t DeviceCapabilities::to_json(char *buf, size_t buf_len) const {
    if (!buf || buf_len == 0) return 0;

    // Guard null pointers
    const char *fv = fw_ver  ? fw_ver  : "";
    const char *hr = hw_rev  ? hw_rev  : "";

    // Build features array string into a temporary buffer.
    char feat_buf[128];
    size_t fp = 0;
    feat_buf[fp++] = '[';
    bool first = true;

    for (size_t i = 0; i < kFeatureCount; ++i) {
        if ((features & static_cast<uint32_t>(kFeatureNames[i].flag)) == 0) continue;
        if (!first) {
            if (fp + 1 >= sizeof(feat_buf)) goto feat_overflow;
            feat_buf[fp++] = ',';
        }
        first = false;
        const char *nm = kFeatureNames[i].name;
        size_t nm_len = std::strlen(nm);
        if (fp + nm_len + 2 >= sizeof(feat_buf)) goto feat_overflow;
        feat_buf[fp++] = '"';
        std::memcpy(feat_buf + fp, nm, nm_len);
        fp += nm_len;
        feat_buf[fp++] = '"';
    }
    goto feat_done;

feat_overflow:
    // Truncate gracefully — add closing bracket without last entry
    ;

feat_done:
    if (fp + 1 >= sizeof(feat_buf)) fp = sizeof(feat_buf) - 2;
    feat_buf[fp++] = ']';
    feat_buf[fp]   = '\0';

    int n = std::snprintf(buf, buf_len,
        "{\"fw_ver\":\"%s\","
        "\"hw_rev\":\"%s\","
        "\"protocol\":%u,"
        "\"features\":%s}",
        fv, hr,
        static_cast<unsigned>(protocol),
        feat_buf
    );

    return (n > 0 && static_cast<size_t>(n) < buf_len) ? static_cast<size_t>(n) : 0;
}

// ── mvp_defaults ──────────────────────────────────────────────────────────────

DeviceCapabilities DeviceCapabilities::mvp_defaults() {
    return DeviceCapabilities{
        .fw_ver   = "0.1.0",
        .hw_rev   = "EVT-A",
        .protocol = 1,
        .features = kMvpFeatures,
    };
}

} // namespace pakt
