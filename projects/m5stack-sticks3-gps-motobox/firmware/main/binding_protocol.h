#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define BINDING_CODE_LENGTH 6
#define BINDING_REQUEST_ID_LENGTH 32

typedef struct {
    char code[BINDING_CODE_LENGTH + 1];
    int64_t expires_ms;
} binding_response_t;

size_t binding_request_build(char *buffer, size_t capacity,
                             const char *device_id, const char *request_id);

bool binding_response_parse(const char *json, size_t length,
                            const char *expected_device_id,
                            const char *expected_request_id,
                            binding_response_t *out);
