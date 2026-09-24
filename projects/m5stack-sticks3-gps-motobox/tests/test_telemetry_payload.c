#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "telemetry_payload.h"
#include "telemetry_queue.h"

static void contains(const char *payload, const char *expected)
{
    assert(strstr(payload, expected) != NULL);
}

int main(void)
{
    char payload[TELEMETRY_PAYLOAD_MAX];
    telemetry_payload_input_t status = {
        .device_id = "BOX-001122334455",
        .firmware = "test",
        .now_ms = 1790017179265LL,
        .sequence = 42,
        .uptime_s = 12,
        .free_heap = 345678,
        .wifi_connected = true,
        .gnss_online = true,
        .gnss_rmc_status = 'V',
        .gnss_gga_quality = 0,
        .gnss_satellites = 0,
        .gnss_hdop = 25.5f,
    };
    assert(telemetry_payload_build(payload, sizeof(payload), &status) > 0);
    contains(payload, "\"timestamp\":1790017179");
    contains(payload, "\"model\":\"m5stack-sticks3-gps\"");
    contains(payload, "\"caps\":[\"gps\",\"wifi\"]");
    contains(payload, "\"wifi\":true");
    contains(payload, "\"gnss\":true");
    contains(payload, "\"position_source\":\"NONE\"");
    contains(payload, "\"gnss_online\":true");
    contains(payload, "\"gnss_rmc_status\":\"V\"");
    contains(payload, "\"gnss_gga_quality\":0");
    contains(payload, "\"gnss_satellites\":0");
    contains(payload, "\"gnss_hdop\":25.5");
    contains(payload, "\"seq\":42");
    contains(payload, "\"ts_ms\":1790017179265");
    assert(strstr(payload, "\"location\"") == NULL);

    telemetry_payload_input_t fix = status;
    fix.sequence = 43;
    fix.has_fix = true;
    fix.fix_age_ms = 321;
    fix.gnss_rmc_status = 'A';
    fix.gnss_gga_quality = 1;
    fix.gnss_satellites = 8;
    fix.gnss_hdop = 0.9f;
    fix.fix = (gnss_fix_t){
        .valid = true,
        .latitude = 31.230416,
        .longitude = 121.473701,
        .altitude_m = 8.5f,
        .speed_kmh = 18.52f,
        .course_deg = 84.4f,
        .hdop = 0.9f,
        .satellites = 8,
        .utc_ms = 1790017179000LL,
    };
    assert(telemetry_payload_build(payload, sizeof(payload), &fix) > 0);
    contains(payload, "\"lat\":31.2304160");
    contains(payload, "\"lng\":121.4737010");
    contains(payload, "\"speed\":18.52");
    contains(payload, "\"speed_kmh\":18.52");
    contains(payload, "\"position_age_ms\":321");
    contains(payload, "\"satellites\":8");
    contains(payload, "\"nav_mode\":\"GNSS\"");
    contains(payload, "\"position_source\":\"GNSS\"");
    contains(payload, "\"gnss_rmc_status\":\"A\"");
    contains(payload, "\"ts_ms\":1790017179000");

    assert(telemetry_payload_build(payload, 32, &fix) == 0);
    puts("telemetry payload tests passed");
    return 0;
}
