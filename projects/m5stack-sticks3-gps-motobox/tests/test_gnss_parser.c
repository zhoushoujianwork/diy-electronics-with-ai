#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "gnss_parser.h"

static void sentence(char *out, size_t capacity, const char *payload)
{
    unsigned checksum = 0;
    for (const char *p = payload; *p; ++p) checksum ^= (unsigned char)*p;
    snprintf(out, capacity, "$%s*%02X", payload, checksum);
}

static bool parse(gnss_parser_t *parser, const char *payload, gnss_fix_t *fix)
{
    char value[160];
    sentence(value, sizeof(value), payload);
    return gnss_parse_sentence(parser, value, fix);
}

int main(void)
{
    gnss_parser_t parser;
    gnss_fix_t fix;
    gnss_parser_init(&parser);

    assert(!parse(&parser, "GNGGA,123519.00,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,", &fix));
    assert(parse(&parser, "GNRMC,123519.00,A,4807.038,N,01131.000,E,10.0,84.4,220926,,,A", &fix));
    assert(fix.valid);
    assert(fabs(fix.latitude - 48.1173) < 0.00001);
    assert(fabs(fix.longitude - 11.5166667) < 0.00001);
    assert(fabs(fix.speed_kmh - 18.52) < 0.01);
    assert(fix.satellites == 8);
    assert(fix.utc_ms == 1790080519000LL);

    gnss_parser_init(&parser);
    assert(!parse(&parser, "GNGGA,123519.00,4807.038,N,01131.000,E,1,08,0.9,,M,46.9,M,,", &fix));
    assert(parse(&parser, "GNRMC,123519.00,A,4807.038,N,01131.000,E,,,220926,,,A", &fix));
    assert(fix.valid && fix.speed_kmh == 0 && fix.course_deg == 0 && fix.altitude_m == 0);

    gnss_parser_init(&parser);
    assert(!parse(&parser, "GPGGA,235959.00,3456.000,S,05822.000,W,1,03,1.2,5.0,M,0.0,M,,", &fix));
    assert(!parse(&parser, "GPRMC,235959.00,A,3456.000,S,05822.000,W,1.0,270.0,220926,,,A", &fix));

    gnss_parser_init(&parser);
    assert(!parse(&parser, "GNGGA,000000.00,3456.000,S,05822.000,W,1,07,1.1,5.0,M,0.0,M,,", &fix));
    assert(parse(&parser, "GNRMC,000000.00,A,3456.000,S,05822.000,W,2.0,270.0,230926,,,A", &fix));
    assert(fix.latitude < 0 && fix.longitude < 0);

    char bad[160];
    sentence(bad, sizeof(bad), "GNRMC,123519.00,A,4807.038,N,01131.000,E,10.0,84.4,220926,,,A");
    bad[strlen(bad) - 1] = bad[strlen(bad) - 1] == '0' ? '1' : '0';
    gnss_parser_init(&parser);
    assert(!gnss_parse_sentence(&parser, bad, &fix));
    assert(!gnss_parse_sentence(&parser,
        "$GNRMC,123519.00,A,4807.038,N,01131.000,E,10.0,84.4,220926,,,A*ZZ", &fix));
    strcat(bad, "00");
    assert(!gnss_parse_sentence(&parser, bad, &fix));

    gnss_parser_init(&parser);
    assert(!parse(&parser, "GNGGA,123518.00,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,", &fix));
    assert(!parse(&parser, "GNRMC,123519.00,A,4807.038,N,01131.000,E,10.0,84.4,220926,,,A", &fix));

    gnss_parser_init(&parser);
    assert(!parse(&parser, "GNGGA,123519.00,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,", &fix));
    assert(!parse(&parser, "GNRMC,123519.00,A,4807.038,N,01131.000,E,10.0,84.4,320926,,,A", &fix));

    gnss_parser_init(&parser);
    assert(!parse(&parser, "GNGGA,123519.BAD,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,", &fix));
    assert(!parse(&parser, "GNGGA,123519.00,4807.038,N,01131.000,E,1,08,0.9,nan,M,46.9,M,,", &fix));
    assert(!parse(&parser, "GNRMC,123519.00,A,4807.038,N,01131.000,E,-1.0,84.4,220926,,,A", &fix));

    puts("gnss parser tests passed");
    return 0;
}
