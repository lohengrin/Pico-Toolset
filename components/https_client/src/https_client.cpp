#include "pico_toolset/https_client.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string_view>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/pbuf.h"
#include "lwip/altcp_tcp.h"
#include "lwip/altcp_tls.h"
#include "lwip/dns.h"
#include "mbedtls/ssl.h"
#include "mbedtls/x509_crt.h"
#include "mbedtls/error.h"


namespace pico_toolset {

// Persistent TLS connection, reused across httpsGet() calls (HTTP keep-alive).
// Callbacks run in lwIP/CYW43 context and only set flags; all blocking waits
// happen in httpsGet() from the main loop context.
struct Conn {
    struct altcp_pcb* pcb = nullptr;
    bool connected = false;      // TCP + TLS handshake done
    bool dead = true;            // pcb unusable (error/closed); may already be freed
    bool serverClosed = false;   // graceful FIN received
    bool gotData = false;        // payload bytes since last request
    bool responseDone = false;   // current response fully received
    int statusCode = 0;
    bool headersDone = false;
    size_t headerEnd = 0;
    long contentLength = -1;
    bool chunked = false;
    uint32_t lastActivityMs = 0;
    uint16_t port = 443;
    size_t maxRaw = 0;           // cap on buffered response bytes (headers + body)
    std::string raw;             // raw response buffer (headers + body)
};

struct HttpsClient::Impl {
    HttpsClientConfig cfg;
    struct altcp_tls_config* tlsConfig = nullptr;
    Conn conn;
};

static uint32_t nowMs() {
    return to_ms_since_boot(get_absolute_time());
}

// Tear down the pcb. Safe to call from callbacks too: the begin/end critical
// section is re-entrant.
static void closeConn(Conn& conn) {
    cyw43_arch_lwip_begin();
    if (conn.pcb) {
        altcp_arg(conn.pcb, nullptr);
        altcp_poll(conn.pcb, nullptr, 0);
        altcp_recv(conn.pcb, nullptr);
        altcp_err(conn.pcb, nullptr);
        if (altcp_close(conn.pcb) != ERR_OK) {
            altcp_abort(conn.pcb);
        }
        conn.pcb = nullptr;
    }
    cyw43_arch_lwip_end();
    conn.connected = false;
    conn.dead = true;
}

// Clear per-request state while keeping a healthy pcb for reuse.
static void resetResponse(Conn& conn) {
    conn.raw.clear();
    conn.raw.shrink_to_fit();
    conn.gotData = false;
    conn.responseDone = false;
    conn.serverClosed = false;
    conn.statusCode = 0;
    conn.headersDone = false;
    conn.headerEnd = 0;
    conn.contentLength = -1;
    conn.chunked = false;
}

static bool connUsable(Conn& conn) {
    return conn.pcb && conn.connected && !conn.dead && !conn.serverClosed;
}

static err_t connConnected(void* arg, struct altcp_pcb* pcb, err_t err) {
    Conn* c = static_cast<Conn*>(arg);
    if (err != ERR_OK) {
        c->dead = true;
        printf("tls: connect failed %d\n", err);
        return ERR_OK;
    }
    c->connected = true;
    return ERR_OK;
}

static err_t connPoll(void* arg, struct altcp_pcb* pcb) {
    // No traffic for the poll interval -> give up rather than hang forever.
    Conn* c = static_cast<Conn*>(arg);
    printf("tls: idle timeout\n");
    c->dead = true;
    return ERR_OK;
}

// Idle-watchdog for in-flight requests only. It is armed just before the
// request goes out and disarmed once the response is complete, so the idle
// gap between refreshes never trips it.
static void armPollWatchdog(Conn& conn) {
    cyw43_arch_lwip_begin();
    if (conn.pcb) altcp_poll(conn.pcb, connPoll, 40);  // ~20s (unit = 0.5s)
    cyw43_arch_lwip_end();
}

static void disarmPollWatchdog(Conn& conn) {
    cyw43_arch_lwip_begin();
    if (conn.pcb) altcp_poll(conn.pcb, nullptr, 0);
    cyw43_arch_lwip_end();
}

static void connErr(void* arg, err_t err) {
    Conn* c = static_cast<Conn*>(arg);
    // lwIP has already freed/aborted the pcb on error.
    c->pcb = nullptr;
    c->dead = true;
    printf("tls: error %d\n", err);
}

static err_t connRecv(void* arg, struct altcp_pcb* pcb, struct pbuf* p, err_t err) {
    Conn* c = static_cast<Conn*>(arg);
    if (!p) {  // server closed gracefully
        c->serverClosed = true;
        c->responseDone = true;  // nothing more is coming
        return ERR_OK;
    }
    if (p->tot_len > 0) {
        const size_t maxRaw = c->maxRaw;
        if (c->raw.size() < maxRaw) {
            size_t copyLen = p->tot_len;
            if (c->raw.size() + copyLen > maxRaw) copyLen = maxRaw - c->raw.size();
            size_t prev = c->raw.size();
            c->raw.resize(prev + copyLen);
            pbuf_copy_partial(p, &c->raw[prev], copyLen, 0);
        }
        altcp_recved(pcb, p->tot_len);
        c->gotData = true;
        c->lastActivityMs = nowMs();

        if (!c->headersDone) {
            size_t h = c->raw.find("\r\n\r\n");
            if (h != std::string::npos) {
                c->headersDone = true;
                c->headerEnd = h + 4;
                std::string_view headers(c->raw.c_str(), h);
                // Status code from "HTTP/1.x NNN ..."
                size_t sp1 = headers.find(' ');
                if (sp1 != std::string_view::npos) {
                    size_t sp2 = headers.find(' ', sp1 + 1);
                    if (sp2 != std::string_view::npos) {
                        c->statusCode =
                            atoi(std::string(headers.substr(sp1 + 1, sp2 - sp1 - 1)).c_str());
                    }
                }
                // Case-insensitive header scan on a lowercased copy.
                std::string hl(headers.substr(0, 512));
                std::transform(hl.begin(), hl.end(), hl.begin(), [](unsigned char c) {
                    return static_cast<char>(std::tolower(c));
                });
                size_t clPos = hl.find("content-length:");
                if (clPos != std::string::npos) {
                    c->contentLength = atol(hl.c_str() + clPos + 15);
                }
                c->chunked = hl.find("transfer-encoding:") != std::string::npos &&
                             hl.find("chunked", hl.find("transfer-encoding:")) !=
                                 std::string::npos;
            }
        }

        // Completion detection.
        if (c->headersDone && !c->responseDone) {
            if (c->chunked) {
                c->responseDone = c->raw.find("0\r\n\r\n", c->headerEnd) != std::string::npos;
            } else if (c->contentLength >= 0) {
                c->responseDone = c->raw.size() - c->headerEnd >= (size_t)c->contentLength;
            }
            // Without Content-Length/chunking we fall back to read-until-close
            // with an idle guard handled by the wait loop in httpsGet().
        }
    }
    pbuf_free(p);
    return ERR_OK;
}

static void doConnect(Conn& conn, const ip_addr_t* ipaddr) {
    printf("tls: connecting %s\n", ipaddr_ntoa(ipaddr));
    if (altcp_connect(conn.pcb, ipaddr, conn.port, connConnected) != ERR_OK) {
        conn.dead = true;
    }
}

static void dnsFound(const char* hostname, const ip_addr_t* ipaddr, void* arg) {
    Conn* c = static_cast<Conn*>(arg);
    if (!ipaddr) {
        printf("dns: failed for %s\n", hostname);
        c->dead = true;
        return;
    }
    doConnect(*c, ipaddr);
}

// Open a fresh TLS connection (DNS + TCP + handshake). Returns once the
// connection is usable or has definitively failed.
static bool openConn(HttpsClient::Impl& s, const char* host) {
    Conn& conn = s.conn;
    closeConn(conn);
    resetResponse(conn);
    // closeConn() marks the state dead; clear lifecycle flags for the new pcb.
    conn.connected = false;
    conn.dead = false;

    conn.pcb = altcp_tls_new(s.tlsConfig, IPADDR_TYPE_ANY);
    if (!conn.pcb) {
        printf("tls: pcb alloc failed\n");
        return false;
    }
    altcp_arg(conn.pcb, &conn);
    altcp_recv(conn.pcb, connRecv);
    altcp_err(conn.pcb, connErr);
    // Poll watchdog is armed per-request in httpsGet(), not here.

    mbedtls_ssl_set_hostname(
        reinterpret_cast<mbedtls_ssl_context*>(altcp_tls_context(conn.pcb)), host);

    ip_addr_t server_ip;
    err_t derr;
    cyw43_arch_lwip_begin();
    derr = dns_gethostbyname(host, &server_ip, dnsFound, &conn);
    if (derr == ERR_OK) {
        doConnect(conn, &server_ip);  // cached, connect immediately
    } else if (derr != ERR_INPROGRESS) {
        printf("dns: start failed %d\n", derr);
        conn.dead = true;
    }
    cyw43_arch_lwip_end();

    uint32_t t0 = nowMs();
    while (!conn.connected && !conn.dead && nowMs() - t0 < 15000) {
        sleep_ms(50);
    }
    if (!conn.connected || conn.dead) {
        printf("tls: handshake did not complete\n");
        closeConn(conn);
        return false;
    }
    return true;
}

static bool writeRequest(Conn& conn, const std::string& req) {
    err_t werr, oerr;
    cyw43_arch_lwip_begin();
    werr = altcp_write(conn.pcb, req.data(), req.size(), TCP_WRITE_FLAG_COPY);
    oerr = (werr == ERR_OK) ? altcp_output(conn.pcb) : ERR_OK;
    cyw43_arch_lwip_end();
    if (werr != ERR_OK || oerr != ERR_OK) {
        printf("tls: write failed %d/%d\n", werr, oerr);
        return false;
    }
    return true;
}

static void decodeChunked(const std::string& in, size_t start, std::string& out) {
    size_t pos = start;
    while (true) {
        size_t eol = in.find("\r\n", pos);
        if (eol == std::string::npos) break;
        char* endp = nullptr;
        unsigned long sz = strtoul(in.c_str() + pos, &endp, 16);
        if (endp == in.c_str() + pos) break;  // not a hex size line
        pos = eol + 2;
        if (sz == 0) break;                   // terminating chunk
        if (pos >= in.size()) break;
        if (pos + sz > in.size()) sz = in.size() - pos;
        out.append(in, pos, sz);
        pos += sz + 2;                        // chunk data + CRLF
    }
}

static void extractBody(Conn& conn, std::string& out) {
    if (!conn.headersDone || conn.raw.size() <= conn.headerEnd) return;
    if (conn.chunked) {
        decodeChunked(conn.raw, conn.headerEnd, out);
    } else if (conn.contentLength >= 0) {
        size_t avail = std::min(conn.raw.size() - conn.headerEnd, (size_t)conn.contentLength);
        out.assign(conn.raw, conn.headerEnd, avail);
    } else {
        out.assign(conn.raw, conn.headerEnd, std::string::npos);
    }
}

static bool ensureTlsConfig(HttpsClient::Impl& s) {
    if (s.tlsConfig) return true;
    const HttpsClientConfig& cfg = s.cfg;
    int ret = 0;
    {
        mbedtls_x509_crt crt;
        mbedtls_x509_crt_init(&crt);
        ret = mbedtls_x509_crt_parse(
            &crt, reinterpret_cast<const unsigned char*>(cfg.ca_pem), cfg.ca_pem_len);
        mbedtls_x509_crt_free(&crt);
    }
    if (ret != 0) {
        char buf[128];
        mbedtls_strerror(ret, buf, sizeof(buf));
        printf("tls: CA parse failed (%d): %s\n", ret, buf);
        return false;
    }
    s.tlsConfig = altcp_tls_create_config_client(
        reinterpret_cast<const uint8_t*>(cfg.ca_pem), cfg.ca_pem_len);
    if (!s.tlsConfig) {
        printf("tls: config creation failed\n");
        return false;
    }
    return true;
}

HttpsClient::HttpsClient() = default;

HttpsClient::~HttpsClient() {
    if (!m_impl) return;
    closeConn(m_impl->conn);
    if (m_impl->tlsConfig) altcp_tls_free_config(m_impl->tlsConfig);
}

bool HttpsClient::init(const HttpsClientConfig& config) {
    if (!config.ca_pem || config.ca_pem_len == 0) return false;
    m_impl = std::make_unique<Impl>();
    m_impl->cfg = config;
    m_impl->conn.port = config.port;
    m_impl->conn.maxRaw = config.max_body + kHeaderAllowance;
    return true;
}

int HttpsClient::last_status() const {
    return m_impl ? m_impl->conn.statusCode : 0;
}

bool HttpsClient::get(const char* host, const char* path, std::string& outBody) {
    outBody.clear();
    if (!m_impl) return false;
    Impl& s = *m_impl;
    Conn& conn = s.conn;

    if (!ensureTlsConfig(s)) return false;

    std::string req = "GET ";
    req += path;
    req += " HTTP/1.1\r\nHost: ";
    req += host;
    req += "\r\nConnection: keep-alive\r\nUser-Agent: ";
    req += s.cfg.user_agent;
    req += "\r\nAccept-Encoding: identity\r\n\r\n";

    for (int attempt = 0; attempt < 2; ++attempt) {
        resetResponse(conn);

        const bool reused = connUsable(conn);
        if (reused) {
            printf("tls: reusing connection\n");
        } else if (!openConn(s, host)) {
            return false;
        }

        armPollWatchdog(conn);
        if (!writeRequest(conn, req)) {
            conn.dead = true;  // force a fresh connection on retry
            continue;
        }

        // Liveness proof: some data or closure within ~2.5s catches silently
        // dropped requests on stale connections.
        uint32_t t0 = nowMs();
        while (!conn.gotData && !conn.responseDone && !conn.dead && nowMs() - t0 < 2500) {
            sleep_ms(50);
        }
        if (!conn.gotData && !conn.responseDone) {
            if (attempt == 0) {
                printf("tls: no response, reconnecting\n");
                closeConn(conn);
                continue;
            }
            closeConn(conn);
            return false;
        }

        // Wait for the rest of the response (max 20s). For responses without
        // Content-Length/chunking, stop after 2s without new data.
        t0 = nowMs();
        while (!conn.responseDone && !conn.dead && nowMs() - t0 < 20000) {
            if (conn.headersDone && conn.contentLength < 0 && !conn.chunked &&
                nowMs() - conn.lastActivityMs > 2000) {
                conn.responseDone = true;
                conn.serverClosed = true;  // can't trust this connection again
                break;
            }
            sleep_ms(50);
        }
        disarmPollWatchdog(conn);

        extractBody(conn, outBody);

        if (conn.serverClosed || conn.dead) closeConn(conn);

        if (outBody.empty() && attempt == 0) {
            printf("http: empty body, retrying\n");
            closeConn(conn);
            continue;
        }
        if (conn.statusCode != 200) {
            printf("http: status %d (bytes=%zu)\n", conn.statusCode, outBody.size());
            return outBody.empty() ? false : true;
        }
        return !outBody.empty();
    }
    return false;
}

}  // namespace pico_toolset
