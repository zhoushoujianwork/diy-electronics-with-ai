#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "binding_protocol.h"

int main(void)
{
    const char *device = "BOX-001122334455";
    const char *request = "00112233445566778899aabbccddeeff";
    char payload[160];
    size_t length = binding_request_build(payload, sizeof(payload), device, request);
    assert(length == strlen(payload));
    assert(strstr(payload, device));
    assert(strstr(payload, request));

    length = binding_status_request_build(payload, sizeof(payload), device, request);
    assert(length == strlen(payload));
    assert(strstr(payload, "\"op\":\"status\""));

    const char *response =
        "{\"device_id\":\"BOX-001122334455\",\"request_id\":"
        "\"00112233445566778899aabbccddeeff\",\"code\":\"042731\","
        "\"expires_ms\":1790064000000}";
    binding_response_t parsed;
    assert(binding_response_parse(response, strlen(response), device, request, &parsed));
    assert(strcmp(parsed.code, "042731") == 0);
    assert(parsed.expires_ms == 1790064000000LL);

    bool bound = false;
    const char *status_response =
        "{\"device_id\":\"BOX-001122334455\",\"request_id\":"
        "\"00112233445566778899aabbccddeeff\",\"bound\":true}";
    assert(binding_status_parse(status_response, strlen(status_response), device, request, &bound));
    assert(bound);
    const char *unbound_response =
        "{\"device_id\":\"BOX-001122334455\",\"request_id\":"
        "\"00112233445566778899aabbccddeeff\",\"bound\":false}";
    assert(binding_status_parse(unbound_response, strlen(unbound_response), device, request, &bound));
    assert(!bound);
    assert(!binding_status_parse(response, strlen(response), device, request, &bound));
    assert(!binding_status_parse(status_response, strlen(status_response), "BOX-OTHER", request, &bound));

    assert(!binding_response_parse(response, strlen(response), "BOX-OTHER", request, &parsed));
    assert(!binding_response_parse(response, strlen(response), device,
                                   "ffffffffffffffffffffffffffffffff", &parsed));
    const char *bad_code =
        "{\"device_id\":\"BOX-001122334455\",\"request_id\":"
        "\"00112233445566778899aabbccddeeff\",\"code\":\"ABC123\","
        "\"expires_ms\":1790064000000}";
    assert(!binding_response_parse(bad_code, strlen(bad_code), device, request, &parsed));
    puts("binding protocol tests passed");
    return 0;
}
