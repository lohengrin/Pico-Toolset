#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

namespace pico_toolset {

struct HttpsClientConfig {
    // PEM-encoded CA certificate(s) the server chain is checked against, with the
    // NUL terminator counted in ca_pem_len (see tls_roots.h).
    const char* ca_pem = nullptr;
    size_t ca_pem_len = 0;
    // Cap on the response body kept in RAM. Headers get kHeaderAllowance on top.
    size_t max_body = 16384;
    const char* user_agent = "pico-toolset";
    uint16_t port = 443;
};

// Blocking HTTPS GET client over lwIP altcp + mbedtls, with one persistent
// keep-alive TLS connection reused across calls (a reconnect is transparent).
//
// Needs Wi-Fi up (see pico_toolset_wifi), pico_cyw43_arch_lwip_threadsafe_background,
// and the build's mbedtls/lwIP configuration from this component's config/ dir:
// see docs/components/https-client.md. Call get() from the main loop context,
// never from an lwIP callback.
class HttpsClient {
public:
    static constexpr size_t kHeaderAllowance = 8192;

    HttpsClient();
    ~HttpsClient();
    HttpsClient(const HttpsClient&) = delete;
    HttpsClient& operator=(const HttpsClient&) = delete;

    bool init(const HttpsClientConfig& config);

    // On success outBody holds the response body (without headers). Returns
    // false on any error; a non-200 status with a body still returns true, so
    // check last_status() when it matters.
    bool get(const char* host, const char* path, std::string& outBody);

    int last_status() const;

    struct Impl;

private:
    std::unique_ptr<Impl> m_impl;
};

} // namespace pico_toolset
