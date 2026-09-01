#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winhttp.h>

#include <cstdlib>
#include <cstring>
#include <string>

#include "core/constants.h"
#include "core/types.h"

#ifndef WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_1
#define WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_1 0x00000200
#endif
#ifndef WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2
#define WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2 0x00000800
#endif
#ifndef WINHTTP_OPTION_SECURE_PROTOCOLS
#define WINHTTP_OPTION_SECURE_PROTOCOLS 84
#endif

namespace adhan {
namespace {

std::wstring utf8_to_wide(const std::string& s) {
  if (s.empty()) return std::wstring();
  int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, 0, 0);
  std::wstring w(n ? n - 1 : 0, 0);
  if (n > 1) MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], n);
  return w;
}

bool parse_url(const std::string& url, bool* https, std::wstring* host, INTERNET_PORT* port,
               std::wstring* path) {
  std::string u = url;
  *https = true;
  *port = 443;
  const char* rest = u.c_str();
  if (u.compare(0, 8, "https://") == 0) {
    *https = true;
    *port = 443;
    rest = u.c_str() + 8;
  } else if (u.compare(0, 7, "http://") == 0) {
    *https = false;
    *port = 80;
    rest = u.c_str() + 7;
  } else {
    return false;
  }
  const char* slash = std::strchr(rest, '/');
  const char* colon = std::strchr(rest, ':');
  std::string host_a;
  if (colon && (!slash || colon < slash)) {
    host_a.assign(rest, colon);
    *port = static_cast<INTERNET_PORT>(std::atoi(colon + 1));
    rest = slash ? slash : colon + std::strlen(colon);
  } else if (slash) {
    host_a.assign(rest, slash);
    rest = slash;
  } else {
    host_a = rest;
    rest = "/";
  }
  if (host_a.empty()) return false;
  *host = utf8_to_wide(host_a);
  std::string path_a = (rest && *rest) ? rest : "/";
  *path = utf8_to_wide(path_a);
  return true;
}

class WinHttpClient : public HttpClient {
 public:
  HttpResult get(const std::string& url, int timeout_ms) override {
    HttpResult r;
    r.ok = false;
    r.status = 0;
    if (url.compare(0, 8, "https://") != 0) {
      r.error = "HTTPS required";
      return r;
    }
    bool https = true;
    std::wstring host, path;
    INTERNET_PORT port = 443;
    if (!parse_url(url, &https, &host, &port, &path)) {
      r.error = "invalid URL";
      return r;
    }

    HINTERNET session = WinHttpOpen(utf8_to_wide(std::string(kHttpUserAgentPrefix) + kVersion).c_str(),
                                    WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) {
      r.error = "WinHttpOpen failed";
      return r;
    }
    DWORD proto = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_1 |
                  WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;
    WinHttpSetOption(session, WINHTTP_OPTION_SECURE_PROTOCOLS, &proto, sizeof(proto));
    if (timeout_ms < 1000) timeout_ms = 1000;
    WinHttpSetTimeouts(session, timeout_ms, timeout_ms, timeout_ms, timeout_ms);

    HINTERNET conn = WinHttpConnect(session, host.c_str(), port, 0);
    if (!conn) {
      r.error = "WinHttpConnect failed";
      WinHttpCloseHandle(session);
      return r;
    }

    DWORD flags = https ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET req = WinHttpOpenRequest(conn, L"GET", path.c_str(), NULL, WINHTTP_NO_REFERER,
                                       WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!req) {
      r.error = "WinHttpOpenRequest failed";
      WinHttpCloseHandle(conn);
      WinHttpCloseHandle(session);
      return r;
    }

    BOOL sent = WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0,
                                   0, 0);
    if (!sent || !WinHttpReceiveResponse(req, 0)) {
      r.error = "request failed (network/TLS)";
      WinHttpCloseHandle(req);
      WinHttpCloseHandle(conn);
      WinHttpCloseHandle(session);
      return r;
    }

    DWORD status = 0;
    DWORD size = sizeof(status);
    WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX);
    r.status = static_cast<int>(status);

    std::string body;
    for (;;) {
      DWORD avail = 0;
      if (!WinHttpQueryDataAvailable(req, &avail)) break;
      if (avail == 0) break;
      if (avail > 1024 * 1024) avail = 1024 * 1024;
      std::string chunk(avail, 0);
      DWORD read = 0;
      if (!WinHttpReadData(req, &chunk[0], avail, &read)) break;
      body.append(chunk.data(), read);
      if (body.size() > 2 * 1024 * 1024) break;
    }
    r.body = body;
    r.ok = true;
    WinHttpCloseHandle(req);
    WinHttpCloseHandle(conn);
    WinHttpCloseHandle(session);
    return r;
  }
};

}  // namespace

HttpClient* create_win_http_client() { return new WinHttpClient(); }

}  // namespace adhan
