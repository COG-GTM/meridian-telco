#ifndef MERIDIAN_HTTPD_H
#define MERIDIAN_HTTPD_H

/* minimal blocking http server. predates cpp-httplib being allowed through
   procurement. handles one request at a time, which has been fine.

   configuration comes from the environment, checked once at startup:

     MERIDIAN_API_TOKEN      required. bearer token every request must carry
                             (Authorization: Bearer <token>), 32+ chars.
     MERIDIAN_BIND_ADDR      ipv4 address to listen on. default 127.0.0.1.
     MERIDIAN_CORS_ORIGINS   comma separated browser origins allowed to read
                             responses. default is the dashboard on :8083.
     MERIDIAN_TLS_CERT       pem certificate chain. with MERIDIAN_TLS_KEY the
     MERIDIAN_TLS_KEY        server speaks https. required for any bind
                             address outside 127.0.0.0/8; plain http is only
                             served on loopback, behind a local tls proxy. */

#include <string>
#include <map>
#include <vector>
#include <cctype>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <openssl/crypto.h>
#include <openssl/err.h>
#include <openssl/ssl.h>

struct HttpRequest {
  std::string method;
  std::string path;
  std::map<std::string, std::string> query;
  std::map<std::string, std::string> headers;
};

typedef std::string (*HandlerFn)(const HttpRequest &, int *status);

struct Route {
  std::string prefix;
  HandlerFn fn;
};

struct ServerConfig {
  std::string bind_addr;
  std::string api_token;
  std::vector<std::string> cors_origins;
  std::string tls_cert;
  std::string tls_key;
};

static const size_t HTTPD_MIN_TOKEN_LEN = 32;
static const size_t HTTPD_MAX_REQUEST = 8192;
static const int HTTPD_IO_TIMEOUT_SEC = 5;

inline void parse_target(const std::string &target, HttpRequest *req) {
  size_t q = target.find('?');
  req->path = q == std::string::npos ? target : target.substr(0, q);
  if (q == std::string::npos) return;
  std::string qs = target.substr(q + 1);
  size_t pos = 0;
  while (pos < qs.size()) {
    size_t amp = qs.find('&', pos);
    std::string kv = qs.substr(pos, amp == std::string::npos ? std::string::npos : amp - pos);
    size_t eq = kv.find('=');
    if (eq != std::string::npos) req->query[kv.substr(0, eq)] = kv.substr(eq + 1);
    if (amp == std::string::npos) break;
    pos = amp + 1;
  }
}

inline std::string trim(const std::string &s) {
  size_t b = 0, e = s.size();
  while (b < e && isspace((unsigned char)s[b])) b++;
  while (e > b && isspace((unsigned char)s[e - 1])) e--;
  return s.substr(b, e - b);
}

inline std::string lower(std::string s) {
  for (size_t i = 0; i < s.size(); i++) s[i] = (char)tolower((unsigned char)s[i]);
  return s;
}

/* request line plus headers. header names are lowercased. */
inline bool parse_request(const std::string &raw, HttpRequest *req) {
  size_t eol = raw.find("\r\n");
  if (eol == std::string::npos) return false;
  char method[16] = {0}, target[2048] = {0};
  if (sscanf(raw.substr(0, eol).c_str(), "%15s %2047s", method, target) != 2) return false;
  req->method = method;
  parse_target(target, req);
  size_t pos = eol + 2;
  while (pos < raw.size()) {
    size_t end = raw.find("\r\n", pos);
    if (end == std::string::npos || end == pos) break;
    std::string line = raw.substr(pos, end - pos);
    size_t colon = line.find(':');
    if (colon != std::string::npos)
      req->headers[lower(trim(line.substr(0, colon)))] = trim(line.substr(colon + 1));
    pos = end + 2;
  }
  return true;
}

inline std::vector<std::string> split_list(const std::string &s) {
  std::vector<std::string> out;
  size_t pos = 0;
  while (pos <= s.size()) {
    size_t comma = s.find(',', pos);
    std::string item = trim(s.substr(pos, comma == std::string::npos ? std::string::npos : comma - pos));
    if (!item.empty()) out.push_back(item);
    if (comma == std::string::npos) break;
    pos = comma + 1;
  }
  return out;
}

inline bool is_loopback_addr(const std::string &addr) {
  struct in_addr a;
  if (inet_pton(AF_INET, addr.c_str(), &a) != 1) return false;
  return (ntohl(a.s_addr) >> 24) == 127;
}

inline bool origin_allowed(const ServerConfig &cfg, const std::string &origin) {
  for (size_t i = 0; i < cfg.cors_origins.size(); i++)
    if (cfg.cors_origins[i] == origin) return true;
  return false;
}

inline bool token_matches(const std::string &expected, const std::string &authorization) {
  static const std::string scheme = "Bearer ";
  if (expected.empty() || authorization.compare(0, scheme.size(), scheme) != 0) return false;
  std::string given = authorization.substr(scheme.size());
  if (given.size() != expected.size()) return false;
  return CRYPTO_memcmp(given.data(), expected.data(), expected.size()) == 0;
}

inline bool validate_server_config(const ServerConfig &cfg, std::string *err) {
  struct in_addr a;
  if (inet_pton(AF_INET, cfg.bind_addr.c_str(), &a) != 1) {
    *err = "MERIDIAN_BIND_ADDR is not an ipv4 address: " + cfg.bind_addr;
    return false;
  }
  if (cfg.api_token.size() < HTTPD_MIN_TOKEN_LEN) {
    *err = "MERIDIAN_API_TOKEN must be set to at least 32 characters (try: openssl rand -hex 32)";
    return false;
  }
  for (size_t i = 0; i < cfg.cors_origins.size(); i++) {
    const std::string &o = cfg.cors_origins[i];
    if (o == "*" || o == "null" ||
        (o.compare(0, 7, "http://") != 0 && o.compare(0, 8, "https://") != 0)) {
      *err = "MERIDIAN_CORS_ORIGINS entries must be explicit http(s) origins, got: " + o;
      return false;
    }
  }
  if (cfg.tls_cert.empty() != cfg.tls_key.empty()) {
    *err = "MERIDIAN_TLS_CERT and MERIDIAN_TLS_KEY must be set together";
    return false;
  }
  if (cfg.tls_cert.empty() && !is_loopback_addr(cfg.bind_addr)) {
    *err = "refusing to serve plain http on " + cfg.bind_addr +
           ": set MERIDIAN_TLS_CERT/MERIDIAN_TLS_KEY or bind to loopback behind a tls proxy";
    return false;
  }
  return true;
}

inline std::string env_or(const char *name, const std::string &fallback) {
  const char *v = getenv(name);
  return v ? std::string(v) : fallback;
}

inline ServerConfig load_server_config() {
  ServerConfig cfg;
  cfg.bind_addr = env_or("MERIDIAN_BIND_ADDR", "127.0.0.1");
  cfg.api_token = env_or("MERIDIAN_API_TOKEN", "");
  cfg.cors_origins = split_list(env_or("MERIDIAN_CORS_ORIGINS",
                                       "http://localhost:8083,http://127.0.0.1:8083"));
  cfg.tls_cert = env_or("MERIDIAN_TLS_CERT", "");
  cfg.tls_key = env_or("MERIDIAN_TLS_KEY", "");
  return cfg;
}

inline const char *status_reason(int status) {
  switch (status) {
    case 200: return "OK";
    case 204: return "No Content";
    case 400: return "Bad Request";
    case 401: return "Unauthorized";
    case 403: return "Forbidden";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 431: return "Request Header Fields Too Large";
    default: return "Error";
  }
}

inline std::string build_response(int status, const std::string &body,
                                  const std::string &cors_origin, bool tls, bool preflight) {
  char line[64];
  snprintf(line, sizeof(line), "HTTP/1.1 %d %s\r\n", status, status_reason(status));
  std::string h = line;
  if (!preflight) h += "Content-Type: application/json\r\n";
  h += "Cache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\nVary: Origin\r\n";
  if (tls) h += "Strict-Transport-Security: max-age=31536000\r\n";
  if (status == 401) h += "WWW-Authenticate: Bearer\r\n";
  if (!cors_origin.empty()) {
    h += "Access-Control-Allow-Origin: " + cors_origin + "\r\n";
    if (preflight)
      h += "Access-Control-Allow-Methods: GET\r\n"
           "Access-Control-Allow-Headers: Authorization\r\n"
           "Access-Control-Max-Age: 600\r\n";
  }
  char len[64];
  snprintf(len, sizeof(len), "Content-Length: %d\r\nConnection: close\r\n\r\n", (int)body.size());
  return h + len + body;
}

/* auth, origin and method checks ahead of the route handlers. */
inline int dispatch(const ServerConfig &cfg, const std::vector<Route> &routes,
                    const HttpRequest &req, std::string *body, std::string *cors_origin,
                    bool *preflight, std::string *auth_result) {
  std::map<std::string, std::string>::const_iterator o = req.headers.find("origin");
  bool has_origin = o != req.headers.end();
  *preflight = false;
  *auth_result = "-";
  if (has_origin) {
    if (!origin_allowed(cfg, o->second)) {
      *body = "{\"error\":\"origin not allowed\"}";
      return 403;
    }
    *cors_origin = o->second;
  }
  if (req.method == "OPTIONS" && has_origin) {
    *preflight = true;
    body->clear();
    return 204;
  }
  if (req.method != "GET") {
    *body = "{\"error\":\"method not allowed\"}";
    return 405;
  }
  std::map<std::string, std::string>::const_iterator a = req.headers.find("authorization");
  if (a == req.headers.end() || !token_matches(cfg.api_token, a->second)) {
    *auth_result = "denied";
    *body = "{\"error\":\"unauthorized\"}";
    return 401;
  }
  *auth_result = "ok";
  for (size_t i = 0; i < routes.size(); i++) {
    if (req.path.compare(0, routes[i].prefix.size(), routes[i].prefix) == 0) {
      int status = 200;
      *body = routes[i].fn(req, &status);
      return status;
    }
  }
  *body = "{\"error\":\"not found\"}";
  return 404;
}

inline std::string log_safe(const std::string &s) {
  std::string o = s.substr(0, 256);
  for (size_t i = 0; i < o.size(); i++)
    if (!isprint((unsigned char)o[i]) || o[i] == ' ') o[i] = '?';
  return o;
}

struct HttpConn {
  int fd;
  SSL *ssl;
};

inline int conn_read(HttpConn &c, char *buf, int len) {
  return c.ssl ? SSL_read(c.ssl, buf, len) : (int)read(c.fd, buf, len);
}

inline void conn_write(HttpConn &c, const std::string &data) {
  size_t off = 0;
  while (off < data.size()) {
    int n = c.ssl ? SSL_write(c.ssl, data.data() + off, (int)(data.size() - off))
                  : (int)write(c.fd, data.data() + off, data.size() - off);
    if (n <= 0) return;
    off += n;
  }
}

/* reads up to the end of the headers. 0 ok, -1 connection error, -2 too large */
inline int read_request(HttpConn &c, std::string *raw) {
  char buf[2048];
  while (raw->find("\r\n\r\n") == std::string::npos) {
    if (raw->size() >= HTTPD_MAX_REQUEST) return -2;
    int n = conn_read(c, buf, sizeof(buf));
    if (n <= 0) return -1;
    raw->append(buf, n);
  }
  return 0;
}

inline SSL_CTX *make_tls_ctx(const ServerConfig &cfg) {
  SSL_CTX *ctx = SSL_CTX_new(TLS_server_method());
  if (!ctx) return 0;
  SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);
  if (SSL_CTX_use_certificate_chain_file(ctx, cfg.tls_cert.c_str()) != 1 ||
      SSL_CTX_use_PrivateKey_file(ctx, cfg.tls_key.c_str(), SSL_FILETYPE_PEM) != 1 ||
      SSL_CTX_check_private_key(ctx) != 1) {
    ERR_print_errors_fp(stderr);
    SSL_CTX_free(ctx);
    return 0;
  }
  return ctx;
}

inline int serve(int port, const std::vector<Route> &routes) {
  ServerConfig cfg = load_server_config();
  std::string err;
  if (!validate_server_config(cfg, &err)) {
    fprintf(stderr, "%s\n", err.c_str());
    return 1;
  }
  SSL_CTX *tls = 0;
  if (!cfg.tls_cert.empty()) {
    tls = make_tls_ctx(cfg);
    if (!tls) {
      fprintf(stderr, "could not load tls certificate/key\n");
      return 1;
    }
  }
  signal(SIGPIPE, SIG_IGN);

  int fd = socket(AF_INET, SOCK_STREAM, 0);
  int one = 1;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  inet_pton(AF_INET, cfg.bind_addr.c_str(), &addr.sin_addr);
  addr.sin_port = htons(port);
  if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    fprintf(stderr, "bind failed on %s:%d\n", cfg.bind_addr.c_str(), port);
    return 1;
  }
  listen(fd, 16);
  fprintf(stderr, "listening on %s://%s:%d\n", tls ? "https" : "http", cfg.bind_addr.c_str(), port);
  for (;;) {
    struct sockaddr_in peer;
    socklen_t peer_len = sizeof(peer);
    int s = accept(fd, (struct sockaddr *)&peer, &peer_len);
    if (s < 0) continue;
    char peer_ip[INET_ADDRSTRLEN] = "?";
    inet_ntop(AF_INET, &peer.sin_addr, peer_ip, sizeof(peer_ip));
    struct timeval tv;
    tv.tv_sec = HTTPD_IO_TIMEOUT_SEC;
    tv.tv_usec = 0;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    HttpConn c;
    c.fd = s;
    c.ssl = 0;
    if (tls) {
      c.ssl = SSL_new(tls);
      SSL_set_fd(c.ssl, s);
      if (SSL_accept(c.ssl) != 1) {
        fprintf(stderr, "access peer=%s tls handshake failed\n", peer_ip);
        SSL_free(c.ssl);
        close(s);
        continue;
      }
    }

    std::string raw;
    int rr = read_request(c, &raw);
    HttpRequest req;
    std::string body, cors_origin, auth_result = "-";
    bool preflight = false;
    int status;
    if (rr == -1) {
      status = 0;
    } else if (rr == -2) {
      status = 431;
      body = "{\"error\":\"request too large\"}";
    } else if (!parse_request(raw, &req)) {
      status = 400;
      body = "{\"error\":\"bad request\"}";
    } else {
      status = dispatch(cfg, routes, req, &body, &cors_origin, &preflight, &auth_result);
    }
    if (status) {
      conn_write(c, build_response(status, body, cors_origin, tls != 0, preflight));
      fprintf(stderr, "access peer=%s method=%s path=%s status=%d auth=%s\n", peer_ip,
              log_safe(req.method).c_str(), log_safe(req.path).c_str(), status,
              auth_result.c_str());
    }
    if (c.ssl) {
      SSL_shutdown(c.ssl);
      SSL_free(c.ssl);
    }
    close(s);
  }
}

inline std::string json_escape(const std::string &s) {
  std::string o;
  for (size_t i = 0; i < s.size(); i++) {
    if (s[i] == '"' || s[i] == '\\') { o += '\\'; o += s[i]; }
    else o += s[i];
  }
  return o;
}

#endif
