# https_client (+ wifi, net_config)

Blocking HTTPS GET over lwIP altcp + mbedtls with a persistent keep-alive TLS
connection, plus the Wi-Fi station helper and the lwIP/mbedtls configuration
they need. Pico W / Pico 2 W only.

| | |
|---|---|
| Targets / options | `pico_toolset_https_client` / `PICO_TOOLSET_BUILD_HTTPS_CLIENT` (**OFF**), `pico_toolset_wifi` / `PICO_TOOLSET_BUILD_WIFI` (**OFF**), `pico_toolset_net_config` (added by either) |
| Links | `pico_cyw43_arch_lwip_threadsafe_background`, `pico_lwip_mbedtls`, `pico_mbedtls` |
| Example | `components/https_client/example/https_client_example.cpp` (built when `-DPICO_TOOLSET_EXAMPLE_WIFI_SSID=... -DPICO_TOOLSET_EXAMPLE_WIFI_PASSWORD=...` is given) |
| Presets | none (no board wiring) |
| Reference consumer | PicoADSB (adsb.lol) |

## API

- `WifiConfig { ssid, password, auth = WPA2_AES_PSK, connect_timeout_ms = 30000, power_save = true }`;
  `bool wifi_connect(cfg)` (cyw43 init + STA + join + PM2 power saving).
- `HttpsClientConfig { ca_pem, ca_pem_len (incl. NUL), max_body = 16384, user_agent, port = 443 }`;
  `HttpsClient::init(cfg)`, `get(host, path, body)`, `last_status()`.
- `tls_roots.h`: `kIsrgRootYrPem` (api.adsb.lol) and `kIsrgRootX1Pem` (pass `sizeof()` as length).
  Pick the root that matches your server's chain.

## Build setup (one config per build)

- **mbedtls config.** pico-sdk reads `PICO_MBEDTLS_CONFIG_FILE` inside
  `pico_sdk_init()`, so it has to be set *before* it. `include()`
  `cmake/pico_toolset_tls_config.cmake` before `pico_sdk_init()` (the toolset's own
  top-level does), or point the variable at
  `components/https_client/config/mbedtls_config_wrapper.h`. With FetchContent that
  means `FetchContent_Populate` first. A project with its own mbedtls config must
  keep the shipped settings (TLS 1.2, ECDHE-RSA/ECDSA, GCM, `MBEDTLS_PLATFORM_MS_TIME_ALT`...).
- **lwipopts.h** comes from `pico_toolset_net_config`; do not add another one.
- `mbedtls_ms_time()` is provided here, and `psa_crypto_random.c` (mbedtls 3.6.6) is
  compiled in only while pico-sdk's own source list still lacks it.
- Both libraries are CMake INTERFACE libraries that add their sources to your
  executable: the SDK's lwIP/mbedtls targets also contribute INTERFACE sources, and a
  static library in between would compile them twice.

## Constraints

- Call `get()` from the main loop, never from an lwIP callback (the client's
  callbacks only set flags). Requires `threadsafe_background`, not the poll arch.
- **Certificate verification is not enforced.** `lwipopts.h` keeps
  `ALTCP_MBEDTLS_AUTHMODE = MBEDTLS_SSL_VERIFY_OPTIONAL` (overridable; it is
  `#ifndef`-guarded). With OPTIONAL, mbedtls evaluates the chain but a failure still
  completes the handshake, so a man-in-the-middle is not rejected. This is inherited from
  PicoADSB, where it was presumably chosen because there is no wall clock before the
  first response (validity periods cannot be checked); switching to REQUIRED is untested.
- The idle watchdog (~20 s) is armed only while a request is in flight; stale
  keep-alive connections are detected (2.5 s liveness) and transparently reopened once.
- Responses without `Content-Length`/chunking are read until close or 2 s idle.
- Messages go to `printf` (tls:, dns:, http:).
