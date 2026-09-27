#include "telemetry_payload.h"

#include <inttypes.h>
#include <stdio.h>

size_t telemetry_payload_build(char *buffer, size_t capacity,
                               const telemetry_payload_input_t *input)
{
    if (!buffer || capacity == 0 || !input || !input->device_id || !input->firmware) return 0;

    char rmc_status = input->gnss_rmc_status == 'A' || input->gnss_rmc_status == 'V'
        ? input->gnss_rmc_status : '?';
    const char *caps = input->cellular_enabled ? "gps\",\"gsm" : "gps\",\"wifi";
    const char *model = input->cellular_model ? input->cellular_model : "";
    int csq = input->cellular_csq >= 0 && input->cellular_csq <= 31
        ? input->cellular_csq : 99;
    int signal = input->cellular_enabled ? csq : input->wifi_rssi;
    int written;
    if (input->has_fix) {
        written = snprintf(buffer, capacity,
            "{\"device_id\":\"%s\",\"timestamp\":%" PRId64 ",\"firmware\":\"%s\","
            "\"hardware\":\"m5stack-sticks3-k150\",\"model\":\"m5stack-sticks3-gps\","
            "\"caps\":[\"%s\"],\"location\":{\"lat\":%.7f,\"lng\":%.7f,"
            "\"alt\":%.1f,\"speed\":%.2f,\"course\":%.1f,\"satellites\":%d,\"hdop\":%.1f,"
            "\"nav_mode\":\"GNSS\",\"position_age_ms\":%" PRIu32 ",\"type\":\"GNSS\","
            "\"timestamp\":%" PRId64 "},\"system\":{\"uptime\":%" PRIu32
            ",\"free_heap\":%" PRIu32 ",\"signal\":%d},"
            "\"modules\":{\"wifi\":%s,\"gsm\":%s,\"gnss\":true,\"imu\":false,\"sd\":false},"
            "\"ext\":{\"seq\":%" PRIu64 ",\"ts_ms\":%" PRId64 ",\"speed_kmh\":%.2f,"
            "\"position_source\":\"GNSS\",\"gnss_online\":%s,\"gnss_rmc_status\":\"%c\","
            "\"gnss_gga_quality\":%d,\"gnss_satellites\":%d,\"gnss_hdop\":%.1f,"
            "\"gsv_seen\":%s,\"gsv_peak_in_view\":%d,\"gsv_peak_snr\":%d,"
            "\"cell_model\":\"%s\",\"cell_registered\":%s,\"cell_data\":%s,"
            "\"cell_reconnects\":%" PRIu32 ",\"cell_sampled_ms\":%" PRId64 "}}",
            input->device_id, input->now_ms / 1000, input->firmware, caps,
            input->fix.latitude, input->fix.longitude, input->fix.altitude_m,
            input->fix.speed_kmh, input->fix.course_deg, input->fix.satellites,
            input->fix.hdop, input->fix_age_ms, input->fix.utc_ms / 1000,
            input->uptime_s, input->free_heap, signal,
            input->wifi_connected ? "true" : "false",
            input->cellular_data_ready ? "true" : "false",
            input->sequence, input->fix.utc_ms, input->fix.speed_kmh,
            input->gnss_online ? "true" : "false",
            rmc_status,
            input->gnss_gga_quality, input->gnss_satellites, input->gnss_hdop,
            input->gsv_seen ? "true" : "false", input->gsv_peak_in_view, input->gsv_peak_snr,
            model, input->cellular_registered ? "true" : "false",
            input->cellular_data_ready ? "true" : "false",
            input->cellular_reconnects, input->cellular_sampled_ms);
    } else {
        written = snprintf(buffer, capacity,
            "{\"device_id\":\"%s\",\"timestamp\":%" PRId64 ",\"firmware\":\"%s\","
            "\"hardware\":\"m5stack-sticks3-k150\",\"model\":\"m5stack-sticks3-gps\","
            "\"caps\":[\"%s\"],\"system\":{\"uptime\":%" PRIu32
            ",\"free_heap\":%" PRIu32 ",\"signal\":%d},"
            "\"modules\":{\"wifi\":%s,\"gsm\":%s,\"gnss\":%s,\"imu\":false,\"sd\":false},"
            "\"ext\":{\"seq\":%" PRIu64 ",\"ts_ms\":%" PRId64 ",\"speed_kmh\":0,"
            "\"position_source\":\"NONE\",\"gnss_online\":%s,\"gnss_rmc_status\":\"%c\","
            "\"gnss_gga_quality\":%d,\"gnss_satellites\":%d,\"gnss_hdop\":%.1f,"
            "\"gsv_seen\":%s,\"gsv_peak_in_view\":%d,\"gsv_peak_snr\":%d,"
            "\"cell_model\":\"%s\",\"cell_registered\":%s,\"cell_data\":%s,"
            "\"cell_reconnects\":%" PRIu32 ",\"cell_sampled_ms\":%" PRId64 "}}",
            input->device_id, input->now_ms / 1000, input->firmware, caps,
            input->uptime_s, input->free_heap, signal,
            input->wifi_connected ? "true" : "false",
            input->cellular_data_ready ? "true" : "false",
            input->gnss_online ? "true" : "false",
            input->sequence, input->now_ms, input->gnss_online ? "true" : "false",
            rmc_status,
            input->gnss_gga_quality, input->gnss_satellites, input->gnss_hdop,
            input->gsv_seen ? "true" : "false", input->gsv_peak_in_view, input->gsv_peak_snr,
            model, input->cellular_registered ? "true" : "false",
            input->cellular_data_ready ? "true" : "false",
            input->cellular_reconnects, input->cellular_sampled_ms);
    }
    return written > 0 && (size_t)written < capacity ? (size_t)written : 0;
}
