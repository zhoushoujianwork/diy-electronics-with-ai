#pragma once

#include "esp_err.h"
#include "esp_netif.h"

/* Answer standard IPv4 DNS queries with the local SoftAP address. */
esp_err_t portal_dns_start(esp_netif_t *ap_netif);
