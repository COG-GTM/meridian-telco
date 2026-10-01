#include <cstdio>
#include <string>
#include "httpd.h"

/* transport checks for httpd.h: auth, origin allow-list, bind/tls policy. */

static int g_fail = 0;

static void check(bool ok, const char *name) {
  printf("%s %s\n", ok ? "ok  " : "FAIL", name);
  if (!ok) g_fail++;
}

static std::string ok_handler(const HttpRequest &, int *) { return "{\"ok\":true}"; }

static ServerConfig base_config() {
  ServerConfig c;
  c.bind_addr = "127.0.0.1";
  c.api_token = std::string(64, 'a');
  c.cors_origins = split_list("http://localhost:8083, http://127.0.0.1:8083");
  return c;
}

static HttpRequest request(const std::string &raw) {
  HttpRequest r;
  parse_request(raw, &r);
  return r;
}

static int run(const ServerConfig &cfg, const std::string &raw, std::string *body,
               std::string *cors, bool *preflight) {
  std::vector<Route> routes;
  Route r; r.prefix = "/sites"; r.fn = ok_handler; routes.push_back(r);
  std::string auth;
  cors->clear();
  return dispatch(cfg, routes, request(raw), body, cors, preflight, &auth);
}

int main() {
  ServerConfig cfg = base_config();
  std::string err, body, cors;
  bool pre = false;
  std::string good = "Authorization: Bearer " + cfg.api_token + "\r\n";

  check(validate_server_config(cfg, &err), "loopback without tls is accepted");

  ServerConfig c = cfg; c.bind_addr = "0.0.0.0";
  check(!validate_server_config(c, &err), "all-interfaces bind without tls is refused");
  c.tls_cert = "cert.pem"; c.tls_key = "key.pem";
  check(validate_server_config(c, &err), "all-interfaces bind with tls is accepted");
  c.tls_key = "";
  check(!validate_server_config(c, &err), "tls cert without key is refused");

  c = cfg; c.api_token = "";
  check(!validate_server_config(c, &err), "missing token is refused");
  c.api_token = "short-token";
  check(!validate_server_config(c, &err), "short token is refused");

  c = cfg; c.cors_origins = split_list("*");
  check(!validate_server_config(c, &err), "wildcard cors origin is refused");
  c.cors_origins = split_list("null");
  check(!validate_server_config(c, &err), "null cors origin is refused");
  c.bind_addr = "localhost";
  check(!validate_server_config(c, &err), "non numeric bind address is refused");

  check(is_loopback_addr("127.0.0.1") && is_loopback_addr("127.8.9.10"), "127/8 is loopback");
  check(!is_loopback_addr("0.0.0.0") && !is_loopback_addr("10.20.0.5"), "other addresses are not loopback");

  int st = run(cfg, "GET /sites HTTP/1.1\r\nHost: x\r\n\r\n", &body, &cors, &pre);
  check(st == 401, "request without token gets 401");
  st = run(cfg, "GET /sites HTTP/1.1\r\nAuthorization: Bearer " + std::string(64, 'b') + "\r\n\r\n",
           &body, &cors, &pre);
  check(st == 401, "request with wrong token gets 401");
  st = run(cfg, "GET /sites HTTP/1.1\r\nAuthorization: Basic " + cfg.api_token + "\r\n\r\n",
           &body, &cors, &pre);
  check(st == 401, "non bearer scheme gets 401");
  st = run(cfg, "GET /sites HTTP/1.1\r\n" + good + "\r\n", &body, &cors, &pre);
  check(st == 200 && body == "{\"ok\":true}" && cors.empty(), "request with token reaches handler");
  st = run(cfg, "GET /nope HTTP/1.1\r\n" + good + "\r\n", &body, &cors, &pre);
  check(st == 404, "unknown path with token gets 404");
  st = run(cfg, "POST /sites HTTP/1.1\r\n" + good + "\r\n", &body, &cors, &pre);
  check(st == 405, "non GET method gets 405");

  st = run(cfg, "GET /sites HTTP/1.1\r\nOrigin: http://localhost:8083\r\n" + good + "\r\n",
           &body, &cors, &pre);
  check(st == 200 && cors == "http://localhost:8083", "allowed origin is echoed back");
  st = run(cfg, "GET /sites HTTP/1.1\r\nOrigin: https://evil.example\r\n" + good + "\r\n",
           &body, &cors, &pre);
  check(st == 403 && cors.empty(), "disallowed origin gets 403 and no cors header");
  st = run(cfg, "OPTIONS /sites HTTP/1.1\r\nOrigin: http://127.0.0.1:8083\r\n\r\n",
           &body, &cors, &pre);
  check(st == 204 && pre && cors == "http://127.0.0.1:8083", "preflight from allowed origin gets 204");
  st = run(cfg, "OPTIONS /sites HTTP/1.1\r\nOrigin: https://evil.example\r\n\r\n",
           &body, &cors, &pre);
  check(st == 403, "preflight from disallowed origin gets 403");

  std::string resp = build_response(200, "{}", "", false, false);
  check(resp.find("Access-Control-Allow-Origin") == std::string::npos, "no cors header without allowed origin");
  check(resp.find("*") == std::string::npos, "response never carries a wildcard");
  resp = build_response(204, "", "http://localhost:8083", true, true);
  check(resp.find("Access-Control-Allow-Origin: http://localhost:8083\r\n") != std::string::npos &&
        resp.find("Access-Control-Allow-Headers: Authorization\r\n") != std::string::npos &&
        resp.find("Strict-Transport-Security") != std::string::npos, "preflight response headers");
  resp = build_response(401, "{}", "", false, false);
  check(resp.find("WWW-Authenticate: Bearer\r\n") != std::string::npos, "401 carries WWW-Authenticate");

  HttpRequest h = request("GET /x?a=1 HTTP/1.1\r\nAUTHORIZATION:  Bearer t \r\n\r\n");
  check(h.headers["authorization"] == "Bearer t" && h.query["a"] == "1", "headers are parsed case insensitively");
  check(log_safe("/a b\r\nfake") == "/a?b??fake", "log lines are sanitised");

  if (g_fail) {
    printf("\n%d check(s) failed\n", g_fail);
    return 1;
  }
  printf("\nall checks passed\n");
  return 0;
}
