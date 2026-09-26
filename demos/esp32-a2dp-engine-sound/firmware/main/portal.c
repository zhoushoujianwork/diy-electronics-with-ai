#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "portal.h"
#include "cJSON.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "nvs.h"
#include "portal_dns.h"

static const char *TAG = "a2dp_portal";
static const char *SSID = "EV-Engine-Setup";
static portMUX_TYPE portal_lock = portMUX_INITIALIZER_UNLOCKED;
static portal_connect_fn connect_device;
static bool scan_active, link_connected, audio_streaming;
static unsigned ap_clients;
static int64_t last_join_us, last_dhcp_us;
typedef struct { bool joined, has_ip; uint8_t mac[6]; } ap_station_t;
static ap_station_t ap_stations[2];
enum { MAX_DEVICES = 20, DEVICE_AGE_US = 30000000 };
typedef struct {
    bool used;
    uint8_t address[6];
    char name[64];
    int rssi;
    int64_t seen_us;
} device_t;
static device_t devices[MAX_DEVICES];

static void wifi_event(void *arg, esp_event_base_t base, int32_t id, void *data) {
    (void)arg; (void)base;
    unsigned count;
    portENTER_CRITICAL(&portal_lock);
    if(id==WIFI_EVENT_AP_STACONNECTED) {
        const wifi_event_ap_staconnected_t *event=data;
        for(size_t i=0;i<2;i++) if(!ap_stations[i].joined) {
            ap_stations[i].joined=true; ap_stations[i].has_ip=false;
            memcpy(ap_stations[i].mac,event->mac,6);
            last_join_us=esp_timer_get_time();
            ++ap_clients; break;
        }
    } else if(id==WIFI_EVENT_AP_STADISCONNECTED) {
        const wifi_event_ap_stadisconnected_t *event=data;
        for(size_t i=0;i<2;i++) if(ap_stations[i].joined && !memcmp(ap_stations[i].mac,event->mac,6)) {
            ap_stations[i].joined=false; ap_stations[i].has_ip=false;
            --ap_clients; break;
        }
    }
    count=ap_clients;
    portEXIT_CRITICAL(&portal_lock);
    if(id==WIFI_EVENT_AP_STACONNECTED) ESP_LOGI(TAG,"AP_CLIENT joined count=%u",count);
    else if(id==WIFI_EVENT_AP_STADISCONNECTED) ESP_LOGI(TAG,"AP_CLIENT left count=%u",count);
}

static void ip_event(void *arg, esp_event_base_t base, int32_t id, void *data) {
    (void)arg; (void)base; (void)id;
    const ip_event_ap_staipassigned_t *event=data;
    unsigned count=0,joined;
    portENTER_CRITICAL(&portal_lock);
    for(size_t i=0;i<2;i++) if(ap_stations[i].joined && !memcmp(ap_stations[i].mac,event->mac,6)) {
        ap_stations[i].has_ip=true; last_dhcp_us=esp_timer_get_time(); break;
    }
    for(size_t i=0;i<2;i++) if(ap_stations[i].joined && ap_stations[i].has_ip) ++count;
    joined=ap_clients;
    portEXIT_CRITICAL(&portal_lock);
    ESP_LOGI(TAG,"AP_CLIENT dhcp_ready=%u joined=%u",count,joined);
}

bool portal_scan_ready(void) {
    bool ready=false;
    portENTER_CRITICAL(&portal_lock);
    for(size_t i=0;i<2;i++) if(ap_stations[i].joined && ap_stations[i].has_ip) ready=true;
    int64_t joined=last_join_us, assigned=last_dhcp_us;
    portEXIT_CRITICAL(&portal_lock);
    int64_t now=esp_timer_get_time();
    return ready && now-joined>=5000000 && now-assigned>=2000000;
}

static bool valid_ap_pin(const char *pin) {
    if(strlen(pin)!=8) return false;
    for(int i=0;i<8;i++) if(pin[i]<'0' || pin[i]>'9') return false;
    return true;
}

/* The page contains no external assets and is available only on the local AP. */
static const char PAGE[] =
"<!doctype html><html lang=zh><meta charset=utf-8><meta name=viewport content='width=device-width,initial-scale=1'>"
"<title>EV Engine 蓝牙配对</title><style>body{font:16px system-ui;margin:24px auto;max-width:560px;padding:0 16px;background:#101826;color:#f5f7fa}"
"h1{font-size:24px}p{color:#b8c9dc}button{font:inherit;padding:10px 14px;border:0;border-radius:8px;background:#57c7dc;color:#09232c}"
"li{display:flex;justify-content:space-between;align-items:center;gap:12px;padding:12px 0;border-bottom:1px solid #385067}"
"small{display:block;color:#b8c9dc}#status{padding:12px;background:#203045;border-radius:8px}</style>"
"<h1>EV Engine 蓝牙配对</h1><p>先让耳机指示灯闪烁，再点选刚出现的设备。列表显示最近 30 秒内发现的设备。</p>"
"<div id=status>读取中…</div><h2>扫描结果</h2><ul id=list></ul><p id=message></p>"
"<script>async function refresh(){try{let s=await(await fetch('/api/status',{cache:'no-store'})).json();"
"document.getElementById('status').textContent=`扫描 ${s.scanning?'进行中':'间歇中'} · 蓝牙 ${s.connected?'已连接':'未连接'} · 音频 ${s.streaming?'传输中':'未传输'}`;"
"let a=await(await fetch('/api/devices',{cache:'no-store'})).json(),l=document.getElementById('list');l.replaceChildren();"
"if(!a.length){let x=document.createElement('li');x.textContent='尚未发现设备';l.append(x)}"
"for(let d of a){let li=document.createElement('li'),t=document.createElement('span'),b=document.createElement('button');"
"t.textContent=d.name||('未报告名称的蓝牙设备 #'+d.id);let small=document.createElement('small');small.textContent=`信号 ${d.rssi} dBm · ${d.age}s 前`;t.append(small);"
"b.textContent='连接';b.onclick=async()=>{b.disabled=true;try{let r=await fetch('/api/connect?id='+d.id,{method:'POST'});"
"document.getElementById('message').textContent=await r.text()}catch(e){document.getElementById('message').textContent=String(e)};b.disabled=false};"
"li.append(t,b);l.append(li)}}catch(e){document.getElementById('message').textContent='页面与开发板通信失败：'+e}}"
"refresh();setInterval(refresh,1500)</script></html>";

void portal_record_device(const uint8_t address[6], const char *name, int rssi) {
    int64_t now=esp_timer_get_time();
    portENTER_CRITICAL(&portal_lock);
    int slot=-1,oldest=0;
    for(int i=0;i<MAX_DEVICES;i++) {
        if(devices[i].used && !memcmp(devices[i].address,address,6)) { slot=i; break; }
        if(!devices[i].used && slot<0) slot=i;
        if(devices[i].seen_us<devices[oldest].seen_us) oldest=i;
    }
    if(slot<0) slot=oldest;
    device_t *d=&devices[slot];
    if(!d->used || memcmp(d->address,address,6)) d->name[0]=0;
    d->used=true; memcpy(d->address,address,6); d->rssi=rssi; d->seen_us=now;
    if(name && name[0]) { strncpy(d->name,name,sizeof(d->name)-1); d->name[sizeof(d->name)-1]=0; }
    portEXIT_CRITICAL(&portal_lock);
}

void portal_set_state(bool scanning, bool connected, bool streaming) {
    portENTER_CRITICAL(&portal_lock);
    scan_active=scanning; link_connected=connected; audio_streaming=streaming;
    portEXIT_CRITICAL(&portal_lock);
}

static esp_err_t page_get(httpd_req_t *req) {
    ESP_LOGI(TAG,"HTTP_PORTAL page");
    httpd_resp_set_type(req,"text/html; charset=utf-8");
    httpd_resp_set_hdr(req,"Cache-Control","no-store");
    return httpd_resp_send(req,PAGE,HTTPD_RESP_USE_STRLEN);
}

static esp_err_t portal_redirect(httpd_req_t *req, httpd_err_code_t error) {
    (void)error;
    ESP_LOGI(TAG,"HTTP_PORTAL probe redirected");
    httpd_resp_set_status(req,"302 Found");
    httpd_resp_set_hdr(req,"Location","http://192.168.4.1/");
    httpd_resp_set_type(req,"text/html; charset=utf-8");
    httpd_resp_set_hdr(req,"Cache-Control","no-store");
    return httpd_resp_sendstr(req,"<html><body><a href='http://192.168.4.1/'>EV Engine 蓝牙配对</a></body></html>");
}

static esp_err_t status_get(httpd_req_t *req) {
    bool scanning,connected,streaming;
    portENTER_CRITICAL(&portal_lock);
    scanning=scan_active; connected=link_connected; streaming=audio_streaming;
    portEXIT_CRITICAL(&portal_lock);
    char body[100];
    snprintf(body,sizeof(body),"{\"scanning\":%s,\"connected\":%s,\"streaming\":%s}",
             scanning?"true":"false",connected?"true":"false",streaming?"true":"false");
    httpd_resp_set_type(req,"application/json");
    httpd_resp_set_hdr(req,"Cache-Control","no-store");
    return httpd_resp_sendstr(req,body);
}

static esp_err_t devices_get(httpd_req_t *req) {
    cJSON *list=cJSON_CreateArray();
    if(!list) return httpd_resp_send_err(req,HTTPD_500_INTERNAL_SERVER_ERROR,"out of memory");
    int64_t now=esp_timer_get_time();
    for(int i=0;i<MAX_DEVICES;i++) {
        device_t d;
        portENTER_CRITICAL(&portal_lock); d=devices[i]; portEXIT_CRITICAL(&portal_lock);
        if(!d.used || now-d.seen_us>DEVICE_AGE_US) continue;
        cJSON *item=cJSON_CreateObject();
        if(!item) break;
        cJSON_AddNumberToObject(item,"id",i);
        cJSON_AddStringToObject(item,"name",d.name);
        cJSON_AddNumberToObject(item,"rssi",d.rssi);
        cJSON_AddNumberToObject(item,"age",(now-d.seen_us)/1000000);
        cJSON_AddItemToArray(list,item);
    }
    char *body=cJSON_PrintUnformatted(list);
    cJSON_Delete(list);
    if(!body) return httpd_resp_send_err(req,HTTPD_500_INTERNAL_SERVER_ERROR,"out of memory");
    httpd_resp_set_type(req,"application/json");
    httpd_resp_set_hdr(req,"Cache-Control","no-store");
    esp_err_t rc=httpd_resp_sendstr(req,body);
    free(body);
    return rc;
}

static esp_err_t connect_post(httpd_req_t *req) {
    char query[32],value[12],tail;
    unsigned id;
    if(httpd_req_get_url_query_str(req,query,sizeof(query))!=ESP_OK ||
       httpd_query_key_value(query,"id",value,sizeof(value))!=ESP_OK ||
       sscanf(value,"%u%c",&id,&tail)!=1) {
        return httpd_resp_send_err(req,HTTPD_400_BAD_REQUEST,"invalid device id");
    }
    if(id>=MAX_DEVICES) return httpd_resp_send_err(req,HTTPD_400_BAD_REQUEST,"invalid device id");
    uint8_t address[6]; bool valid;
    int64_t now=esp_timer_get_time();
    portENTER_CRITICAL(&portal_lock);
    valid=devices[id].used && now-devices[id].seen_us<=DEVICE_AGE_US && !link_connected;
    if(valid) memcpy(address,devices[id].address,6);
    portEXIT_CRITICAL(&portal_lock);
    if(!valid || !connect_device || !connect_device(address))
        return httpd_resp_send_err(req,HTTPD_400_BAD_REQUEST,"device expired or connection busy");
    ESP_LOGI(TAG,"USER_SELECT device_id=%u result=queued",id);
    return httpd_resp_sendstr(req,"已请求连接，请观察状态变化");
}

esp_err_t portal_start(portal_connect_fn fn) {
    connect_device=fn;
    esp_err_t rc=esp_netif_init(); if(rc!=ESP_OK) return rc;
    rc=esp_event_loop_create_default(); if(rc!=ESP_OK) return rc;
    esp_netif_t *ap_netif=esp_netif_create_default_wifi_ap();
    if(!ap_netif) return ESP_FAIL;
    wifi_init_config_t init=WIFI_INIT_CONFIG_DEFAULT();
    rc=esp_wifi_init(&init); if(rc!=ESP_OK) return rc;
    rc=esp_wifi_set_mode(WIFI_MODE_AP); if(rc!=ESP_OK) return rc;
    char password[16]; bool saved_pin=false;
    nvs_handle_t nvs;
    if(nvs_open("a2dp_demo",NVS_READONLY,&nvs)==ESP_OK) {
        size_t length=sizeof(password);
        saved_pin=nvs_get_str(nvs,"ap_pin",password,&length)==ESP_OK && valid_ap_pin(password);
        nvs_close(nvs);
    }
    if(!saved_pin) snprintf(password,sizeof(password),"EV%08" PRIX32,esp_random());
    wifi_config_t config={0};
    memcpy(config.ap.ssid,SSID,strlen(SSID));
    config.ap.ssid_len=strlen(SSID);
    memcpy(config.ap.password,password,strlen(password));
    config.ap.channel=1;
    config.ap.max_connection=2;
    config.ap.authmode=WIFI_AUTH_WPA2_PSK;
    rc=esp_wifi_set_config(WIFI_IF_AP,&config); if(rc!=ESP_OK) return rc;
    rc=esp_event_handler_register(WIFI_EVENT,WIFI_EVENT_AP_STACONNECTED,wifi_event,NULL);
    if(rc!=ESP_OK) return rc;
    rc=esp_event_handler_register(WIFI_EVENT,WIFI_EVENT_AP_STADISCONNECTED,wifi_event,NULL);
    if(rc!=ESP_OK) return rc;
    rc=esp_event_handler_register(IP_EVENT,IP_EVENT_AP_STAIPASSIGNED,ip_event,NULL);
    if(rc!=ESP_OK) return rc;
    rc=esp_wifi_start(); if(rc!=ESP_OK) return rc;
    /* DHCP option 114 advertises the page even on clients that skip DNS probes. */
    static char captive_url[]="http://192.168.4.1/";
    rc=esp_netif_dhcps_stop(ap_netif); if(rc!=ESP_OK) return rc;
    rc=esp_netif_dhcps_option(ap_netif,ESP_NETIF_OP_SET,ESP_NETIF_CAPTIVEPORTAL_URI,
                              captive_url,strlen(captive_url));
    if(rc!=ESP_OK) return rc;
    rc=esp_netif_dhcps_start(ap_netif); if(rc!=ESP_OK) return rc;
    httpd_config_t server_config=HTTPD_DEFAULT_CONFIG();
    server_config.stack_size=8192; /* cJSON/list handler + HTTP parser + status helpers. */
    server_config.max_open_sockets=4;
    httpd_handle_t server=NULL;
    rc=httpd_start(&server,&server_config); if(rc!=ESP_OK) return rc;
    httpd_uri_t routes[]={
        {.uri="/",.method=HTTP_GET,.handler=page_get},
        {.uri="/api/status",.method=HTTP_GET,.handler=status_get},
        {.uri="/api/devices",.method=HTTP_GET,.handler=devices_get},
        {.uri="/api/connect",.method=HTTP_POST,.handler=connect_post},
    };
    for(size_t i=0;i<sizeof(routes)/sizeof(routes[0]);i++) {
        rc=httpd_register_uri_handler(server,&routes[i]); if(rc!=ESP_OK) return rc;
    }
    rc=httpd_register_err_handler(server,HTTPD_404_NOT_FOUND,portal_redirect);
    if(rc!=ESP_OK) return rc;
    rc=portal_dns_start(ap_netif); if(rc!=ESP_OK) return rc;
    ESP_LOGI(TAG,"CAPTIVE_PORTAL_READY dhcp_option=114 dns=53 http=80");
    if(saved_pin) ESP_LOGI(TAG,"AP_READY ssid=%s password_source=NVS url=http://192.168.4.1/",SSID);
    else ESP_LOGI(TAG,"AP_READY ssid=%s password=%s url=http://192.168.4.1/",SSID,password);
    return ESP_OK;
}
