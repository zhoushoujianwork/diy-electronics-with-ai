#include <errno.h>
#include <string.h>
#include "esp_log.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include "portal_dns.h"

static const char *TAG = "a2dp_portal_dns";
static uint32_t ap_address;

static uint16_t read16(const uint8_t *p) { return ((uint16_t)p[0] << 8) | p[1]; }
static void write16(uint8_t *p, uint16_t v) { p[0] = v >> 8; p[1] = v; }

/* 256-byte question plus one 16-byte A answer; reject compressed or malformed questions. */
static size_t answer_dns(uint8_t *packet, size_t length) {
    if(length < 17 || (packet[2] & 0xf8) || read16(packet + 4) != 1) return 0;
    size_t at = 12;
    while(at < length && packet[at]) {
        uint8_t label = packet[at++];
        if(label > 63 || at + label >= length) return 0;
        at += label;
    }
    if(at + 5 > length) return 0;
    ++at;
    if(read16(packet + at) != 1 || read16(packet + at + 2) != 1) return 0;
    size_t question_end=at + 4; /* Ignore optional EDNS records in the request. */
    packet[2] = 0x84; packet[3] = 0; /* authoritative response, no recursion */
    write16(packet + 6, 1);
    memset(packet + 8, 0, 4);
    static const uint8_t answer_prefix[] = {0xc0,0x0c,0,1,0,1,0,0,0,10,0,4};
    memcpy(packet + question_end, answer_prefix, sizeof(answer_prefix));
    memcpy(packet + question_end + sizeof(answer_prefix), &ap_address, sizeof(ap_address));
    return question_end + sizeof(answer_prefix) + sizeof(ap_address);
}

static void dns_task(void *unused) {
    (void)unused;
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(sock < 0) { ESP_LOGE(TAG,"DNS socket errno=%d",errno); vTaskDelete(NULL); return; }
    struct sockaddr_in local = {.sin_family=AF_INET,.sin_port=htons(53),.sin_addr.s_addr=htonl(INADDR_ANY)};
    if(bind(sock,(struct sockaddr *)&local,sizeof(local)) < 0) {
        ESP_LOGE(TAG,"DNS bind errno=%d",errno); close(sock); vTaskDelete(NULL); return;
    }
    ESP_LOGI(TAG,"DNS_READY port=53");
    unsigned replied = 0;
    for(;;) {
        uint8_t packet[272];
        struct sockaddr_in peer;
        socklen_t peer_len=sizeof(peer);
        ssize_t n=recvfrom(sock,packet,256,0,(struct sockaddr *)&peer,&peer_len);
        if(n < 0) {
            if(errno==EINTR) continue;
            ESP_LOGE(TAG,"DNS receive errno=%d",errno); break;
        }
        size_t answer_len=answer_dns(packet,(size_t)n);
        if(!answer_len) continue;
        if(sendto(sock,packet,answer_len,0,(struct sockaddr *)&peer,peer_len)<0) {
            ESP_LOGE(TAG,"DNS send errno=%d",errno); continue;
        }
        if(++replied == 1 || replied % 32 == 0)
            ESP_LOGI(TAG,"DNS_REPLY count=%u stack_free=%u",replied,
                     (unsigned)uxTaskGetStackHighWaterMark(NULL));
    }
    close(sock);
    vTaskDelete(NULL);
}

esp_err_t portal_dns_start(esp_netif_t *ap_netif) {
    esp_netif_ip_info_t ip;
    esp_err_t rc=esp_netif_get_ip_info(ap_netif,&ip);
    if(rc!=ESP_OK) return rc;
    ap_address=ip.ip.addr;
    /* 4096 B covers socket/recvfrom plus a 272 B packet and sockaddr state. */
    return xTaskCreate(dns_task,"portal_dns",4096,NULL,5,NULL)==pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}
