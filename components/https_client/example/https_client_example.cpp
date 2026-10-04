#include <cstdio>
#include <string>

#include "pico/stdlib.h"
#include "pico_toolset/https_client.h"
#include "pico_toolset/tls_roots.h"
#include "pico_toolset/wifi.h"

using namespace pico_toolset;

int main() {
    stdio_init_all();
    sleep_ms(3000);

    WifiConfig wifi;
    wifi.ssid = EXAMPLE_WIFI_SSID;
    wifi.password = EXAMPLE_WIFI_PASSWORD;
    if (!wifi_connect(wifi)) {
        for (;;) tight_loop_contents();
    }

    HttpsClientConfig cfg;
    cfg.ca_pem = tls_roots::kIsrgRootYrPem;
    cfg.ca_pem_len = sizeof(tls_roots::kIsrgRootYrPem);
    cfg.user_agent = "pico-toolset-example/1.0";

    HttpsClient client;
    client.init(cfg);

    for (;;) {
        std::string body;
        const bool ok = client.get("api.adsb.lol", "/api/0/route/AFR1/48.8/2.3", body);
        std::printf("ok=%d status=%d bytes=%zu\n", ok, client.last_status(), body.size());
        sleep_ms(10000);
    }
}
