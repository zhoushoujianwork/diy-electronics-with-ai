#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    bool valid;
    double latitude;
    double longitude;
    float altitude_m;
    float speed_kmh;
    float course_deg;
    float hdop;
    int satellites;
    int64_t utc_ms;
} gnss_fix_t;

typedef struct {
    char line[128];
    size_t line_length;
    bool rmc_valid;
    bool gga_valid;
    int rmc_second;
    int gga_second;
    double rmc_latitude;
    double rmc_longitude;
    float speed_kmh;
    float course_deg;
    int64_t utc_ms;
    double gga_latitude;
    double gga_longitude;
    float altitude_m;
    float hdop;
    int satellites;
} gnss_parser_t;

void gnss_parser_init(gnss_parser_t *parser);
bool gnss_parser_feed(gnss_parser_t *parser, char byte, gnss_fix_t *out);
bool gnss_parse_sentence(gnss_parser_t *parser, const char *sentence, gnss_fix_t *out);
