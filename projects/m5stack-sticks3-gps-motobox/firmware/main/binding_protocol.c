#include "binding_protocol.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *find_value(const char *json, const char *key)
{
    char needle[40];
    if (snprintf(needle, sizeof(needle), "\"%s\"", key) >= (int)sizeof(needle)) return NULL;
    const char *value = strstr(json, needle);
    if (!value) return NULL;
    value += strlen(needle);
    while (isspace((unsigned char)*value)) ++value;
    if (*value++ != ':') return NULL;
    while (isspace((unsigned char)*value)) ++value;
    return value;
}

static bool get_string(const char *json, const char *key, char *out, size_t capacity)
{
    const char *value = find_value(json, key);
    if (!value || *value++ != '"') return false;
    const char *end = strchr(value, '"');
    if (!end) return false;
    size_t length = (size_t)(end - value);
    if (length == 0 || length >= capacity) return false;
    memcpy(out, value, length);
    out[length] = '\0';
    return true;
}

static bool get_int64(const char *json, const char *key, int64_t *out)
{
    const char *value = find_value(json, key);
    if (!value) return false;
    char *end = NULL;
    long long parsed = strtoll(value, &end, 10);
    if (end == value) return false;
    *out = (int64_t)parsed;
    return true;
}

size_t binding_request_build(char *buffer, size_t capacity,
                             const char *device_id, const char *request_id)
{
    if (!buffer || capacity == 0 || !device_id || !request_id ||
        strlen(request_id) != BINDING_REQUEST_ID_LENGTH) return 0;
    int written = snprintf(buffer, capacity,
                           "{\"device_id\":\"%s\",\"request_id\":\"%s\"}",
                           device_id, request_id);
    return written > 0 && (size_t)written < capacity ? (size_t)written : 0;
}

bool binding_response_parse(const char *json, size_t length,
                            const char *expected_device_id,
                            const char *expected_request_id,
                            binding_response_t *out)
{
    if (!json || length == 0 || length >= 384 || !expected_device_id ||
        !expected_request_id || !out) return false;
    char copy[384];
    memcpy(copy, json, length);
    copy[length] = '\0';

    char device_id[32];
    char request_id[BINDING_REQUEST_ID_LENGTH + 1];
    char code[BINDING_CODE_LENGTH + 1];
    int64_t expires_ms = 0;
    if (!get_string(copy, "device_id", device_id, sizeof(device_id)) ||
        !get_string(copy, "request_id", request_id, sizeof(request_id)) ||
        !get_string(copy, "code", code, sizeof(code)) ||
        !get_int64(copy, "expires_ms", &expires_ms) ||
        strcmp(device_id, expected_device_id) != 0 ||
        strcmp(request_id, expected_request_id) != 0 ||
        strlen(code) != BINDING_CODE_LENGTH || expires_ms <= 0) return false;
    for (size_t i = 0; i < BINDING_CODE_LENGTH; ++i) {
        if (!isdigit((unsigned char)code[i])) return false;
    }
    memcpy(out->code, code, sizeof(out->code));
    out->expires_ms = expires_ms;
    return true;
}
