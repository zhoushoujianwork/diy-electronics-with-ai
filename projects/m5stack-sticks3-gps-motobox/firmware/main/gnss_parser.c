#include "gnss_parser.h"

#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static bool checksum_valid(const char *sentence)
{
    if (!sentence || sentence[0] != '$') return false;
    const char *star = strchr(sentence, '*');
    if (!star || strlen(star + 1) != 2) return false;
    if (!isxdigit((unsigned char)star[1]) || !isxdigit((unsigned char)star[2])) return false;
    unsigned checksum = 0;
    for (const char *p = sentence + 1; p < star; ++p) checksum ^= (unsigned char)*p;
    char expected[3] = {star[1], star[2], 0};
    return checksum == strtoul(expected, NULL, 16);
}

static bool digits(const char *value, size_t count)
{
    if (!value) return false;
    for (size_t i = 0; i < count; ++i) {
        if (!isdigit((unsigned char)value[i])) return false;
    }
    return true;
}

static bool valid_date(int year, int month, int day)
{
    static const int days_by_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (year < 1980 || year > 2079 || month < 1 || month > 12 || day < 1) return false;
    int days = days_by_month[month - 1];
    bool leap = year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
    if (month == 2 && leap) days++;
    return day <= days;
}

static int split_fields(char *line, char **fields, int capacity)
{
    int count = 0;
    char *p = line;
    while (count < capacity) {
        fields[count++] = p;
        char *comma = strchr(p, ',');
        if (!comma) break;
        *comma = 0;
        p = comma + 1;
    }
    return count;
}

static bool sentence_type(const char *field, const char *suffix)
{
    const size_t length = strlen(field);
    return length >= 5 && strcmp(field + length - 3, suffix) == 0;
}

static bool parse_hhmmss(const char *field, int *second_of_day, int *hour, int *minute, int *second)
{
    if (!field || strlen(field) < 6 || !digits(field, 6)) return false;
    if (field[6] != 0) {
        if (field[6] != '.' || field[7] == 0) return false;
        for (const char *p = field + 7; *p; ++p) {
            if (!isdigit((unsigned char)*p)) return false;
        }
    }
    int hh = (field[0] - '0') * 10 + field[1] - '0';
    int mm = (field[2] - '0') * 10 + field[3] - '0';
    int ss = (field[4] - '0') * 10 + field[5] - '0';
    if (hh < 0 || hh > 23 || mm < 0 || mm > 59 || ss < 0 || ss > 60) return false;
    *second_of_day = hh * 3600 + mm * 60 + ss;
    if (hour) *hour = hh;
    if (minute) *minute = mm;
    if (second) *second = ss;
    return true;
}

static bool parse_number(const char *field, double *out)
{
    if (!field || !*field || !out) return false;
    char *end = NULL;
    double value = strtod(field, &end);
    if (!end || *end != 0 || !isfinite(value)) return false;
    *out = value;
    return true;
}

static bool parse_optional_number(const char *field, double *out)
{
    if (!field || !out) return false;
    if (*field == 0) {
        *out = 0;
        return true;
    }
    return parse_number(field, out);
}

static bool parse_nonnegative_int(const char *field, int *out)
{
    if (!field || !*field || !out) return false;
    char *end = NULL;
    long value = strtol(field, &end, 10);
    if (!end || *end != 0 || value < 0 || value > INT_MAX) return false;
    *out = (int)value;
    return true;
}

static bool parse_coordinate(const char *value, const char *hemisphere, bool latitude, double *out)
{
    if (!value || !*value || !hemisphere || !*hemisphere) return false;
    char *end = NULL;
    double raw = strtod(value, &end);
    if (!end || *end != 0 || !isfinite(raw)) return false;
    double degrees = floor(raw / 100.0);
    double minutes = raw - degrees * 100.0;
    double coordinate = degrees + minutes / 60.0;
    if (minutes < 0 || minutes >= 60) return false;
    if (*hemisphere == 'S' || *hemisphere == 'W') coordinate = -coordinate;
    else if (*hemisphere != 'N' && *hemisphere != 'E') return false;
    if ((latitude && fabs(coordinate) > 90) || (!latitude && fabs(coordinate) > 180)) return false;
    *out = coordinate;
    return true;
}

static int64_t utc_millis(int year, int month, int day, int hour, int minute, int second, int millis)
{
    struct tm value = {
        .tm_year = year - 1900,
        .tm_mon = month - 1,
        .tm_mday = day,
        .tm_hour = hour,
        .tm_min = minute,
        .tm_sec = second,
        .tm_isdst = 0,
    };
#if defined(__APPLE__) || defined(__linux__)
    time_t epoch = timegm(&value);
#else
    char *prior = getenv("TZ");
    char saved[64] = {0};
    if (prior) strncpy(saved, prior, sizeof(saved) - 1);
    setenv("TZ", "UTC0", 1);
    tzset();
    time_t epoch = mktime(&value);
    if (prior) setenv("TZ", saved, 1); else unsetenv("TZ");
    tzset();
#endif
    return epoch < 0 ? 0 : (int64_t)epoch * 1000 + millis;
}

static void emit_if_paired(gnss_parser_t *parser, gnss_fix_t *out)
{
    out->valid = false;
    if (!parser->rmc_valid || !parser->gga_valid || parser->rmc_second != parser->gga_second) return;
    if (fabs(parser->rmc_latitude - parser->gga_latitude) > 0.0002 ||
        fabs(parser->rmc_longitude - parser->gga_longitude) > 0.0002) return;
    *out = (gnss_fix_t){
        .valid = parser->satellites >= 4 && parser->hdop > 0 && parser->hdop < 20,
        .latitude = parser->rmc_latitude,
        .longitude = parser->rmc_longitude,
        .altitude_m = parser->altitude_m,
        .speed_kmh = parser->speed_kmh,
        .course_deg = parser->course_deg,
        .hdop = parser->hdop,
        .satellites = parser->satellites,
        .utc_ms = parser->utc_ms,
    };
}

void gnss_parser_init(gnss_parser_t *parser)
{
    memset(parser, 0, sizeof(*parser));
    parser->rmc_status = '?';
    parser->gga_quality = -1;
    parser->reported_satellites = -1;
    parser->reported_hdop = -1;
}

bool gnss_parse_sentence(gnss_parser_t *parser, const char *sentence, gnss_fix_t *out)
{
    if (!parser || !out || !checksum_valid(sentence)) return false;
    char copy[128];
    strncpy(copy, sentence + 1, sizeof(copy) - 1);
    copy[sizeof(copy) - 1] = 0;
    char *star = strchr(copy, '*');
    if (star) *star = 0;
    char *fields[20];
    int count = split_fields(copy, fields, 20);
    bool recognized = false;

    if (count >= 10 && sentence_type(fields[0], "RMC")) {
        recognized = true;
        parser->rmc_seen = true;
        parser->rmc_status = fields[2][0] ? fields[2][0] : '?';
        int sod = 0, hour = 0, minute = 0, second = 0;
        double lat = 0, lon = 0, speed_knots = 0, course = 0;
        bool date_format = strlen(fields[9]) == 6 && digits(fields[9], 6);
        int day = date_format ? (fields[9][0] - '0') * 10 + fields[9][1] - '0' : 0;
        int month = date_format ? (fields[9][2] - '0') * 10 + fields[9][3] - '0' : 0;
        int yy = date_format ? (fields[9][4] - '0') * 10 + fields[9][5] - '0' : 0;
        int year = yy >= 80 ? 1900 + yy : 2000 + yy;
        parser->rmc_valid = fields[2][0] == 'A' &&
            parse_hhmmss(fields[1], &sod, &hour, &minute, &second) &&
            parse_coordinate(fields[3], fields[4], true, &lat) &&
            parse_coordinate(fields[5], fields[6], false, &lon) &&
            parse_optional_number(fields[7], &speed_knots) && speed_knots >= 0 &&
            parse_optional_number(fields[8], &course) && course >= 0 && course <= 360 && date_format &&
            valid_date(year, month, day);
        if (parser->rmc_valid) {
            int millis = 0;
            const char *dot = strchr(fields[1], '.');
            if (dot) millis = (int)(strtod(dot, NULL) * 1000.0);
            parser->rmc_second = sod;
            parser->rmc_latitude = lat;
            parser->rmc_longitude = lon;
            parser->speed_kmh = (float)(speed_knots * 1.852);
            parser->course_deg = (float)course;
            parser->utc_ms = utc_millis(year, month, day, hour, minute, second, millis);
        }
    } else if (count >= 10 && sentence_type(fields[0], "GGA")) {
        recognized = true;
        parser->gga_seen = true;
        int sod = 0, quality = 0, satellites = 0;
        double lat = 0, lon = 0, hdop = 0, altitude = 0;
        parser->gga_quality = parse_nonnegative_int(fields[6], &quality) ? quality : -1;
        parser->reported_satellites = parse_nonnegative_int(fields[7], &satellites) ? satellites : -1;
        parser->reported_hdop = parse_number(fields[8], &hdop) && hdop >= 0 ? (float)hdop : -1;
        parser->gga_valid = parse_nonnegative_int(fields[6], &quality) && quality > 0 &&
            parse_hhmmss(fields[1], &sod, NULL, NULL, NULL) &&
            parse_coordinate(fields[2], fields[3], true, &lat) &&
            parse_coordinate(fields[4], fields[5], false, &lon) &&
            parse_nonnegative_int(fields[7], &satellites) &&
            parse_number(fields[8], &hdop) && hdop >= 0 &&
            parse_optional_number(fields[9], &altitude);
        if (parser->gga_valid) {
            parser->gga_second = sod;
            parser->gga_latitude = lat;
            parser->gga_longitude = lon;
            parser->satellites = satellites;
            parser->hdop = (float)hdop;
            parser->altitude_m = (float)altitude;
        }
    } else if (count >= 4 && sentence_type(fields[0], "GSV")) {
        recognized = true;
        parser->gsv_seen = true;
        int in_view = 0;
        if (parse_nonnegative_int(fields[3], &in_view) && in_view <= 64 &&
            in_view > parser->gsv_peak_in_view) {
            parser->gsv_peak_in_view = in_view;
        }
        for (int index = 7; index < count; index += 4) {
            int snr = 0;
            if (parse_nonnegative_int(fields[index], &snr) && snr <= 99 &&
                snr > parser->gsv_peak_snr) {
                parser->gsv_peak_snr = snr;
            }
        }
    }
    if (recognized) parser->nmea_sentence_count++;
    emit_if_paired(parser, out);
    return recognized && out->valid;
}

bool gnss_parser_feed(gnss_parser_t *parser, char byte, gnss_fix_t *out)
{
    if (!parser || !out) return false;
    if (byte == '\r') return false;
    if (byte == '\n') {
        parser->line[parser->line_length] = 0;
        bool result = parser->line_length > 0 && gnss_parse_sentence(parser, parser->line, out);
        parser->line_length = 0;
        return result;
    }
    if (parser->line_length + 1 >= sizeof(parser->line)) {
        parser->line_length = 0;
        return false;
    }
    if (byte == '$') parser->line_length = 0;
    parser->line[parser->line_length++] = byte;
    return false;
}
