#ifndef MERIDIAN_HTTPD_H
#define MERIDIAN_HTTPD_H

/* minimal blocking http server. predates cpp-httplib being allowed through
   procurement. handles one request at a time, which has been fine. */

#include <string>
#include <map>
#include <vector>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <cctype>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>

struct HttpRequest {
  std::string method;
  std::string path;
  std::map<std::string, std::string> query;
  std::map<std::string, std::string> headers; /* keys lowercased */
  std::string peer;                           /* client ip */
};

typedef std::string (*HandlerFn)(const HttpRequest &, int *status);

struct Route {
  std::string prefix;
  HandlerFn fn;
};

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

inline void parse_headers(const char *raw, HttpRequest *req) {
  const char *p = strstr(raw, "\r\n");
  while (p) {
    p += 2;
    const char *eol = strstr(p, "\r\n");
    if (!eol || eol == p) break;
    std::string line(p, eol - p);
    size_t colon = line.find(':');
    if (colon != std::string::npos) {
      std::string k = line.substr(0, colon);
      for (size_t i = 0; i < k.size(); i++) k[i] = (char)tolower((unsigned char)k[i]);
      size_t vs = line.find_first_not_of(" \t", colon + 1);
      size_t ve = line.find_last_not_of(" \t");
      req->headers[k] = vs == std::string::npos ? "" : line.substr(vs, ve - vs + 1);
    }
    p = eol;
  }
}

inline int serve(int port, const std::vector<Route> &routes) {
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  int one = 1;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(port);
  if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    fprintf(stderr, "bind failed on port %d\n", port);
    return 1;
  }
  listen(fd, 16);
  fprintf(stderr, "listening on :%d\n", port);
  for (;;) {
    struct sockaddr_in peer;
    socklen_t peer_len = sizeof(peer);
    int c = accept(fd, (struct sockaddr *)&peer, &peer_len);
    if (c < 0) continue;
    char buf[8192];
    int n = read(c, buf, sizeof(buf) - 1);
    if (n <= 0) { close(c); continue; }
    buf[n] = 0;
    HttpRequest req;
    char method[16] = {0}, target[2048] = {0};
    sscanf(buf, "%15s %2047s", method, target);
    req.method = method;
    parse_target(target, &req);
    parse_headers(buf, &req);
    char ip[INET_ADDRSTRLEN] = {0};
    inet_ntop(AF_INET, &peer.sin_addr, ip, sizeof(ip));
    req.peer = ip;

    if (req.method == "OPTIONS") {
      const char *pre =
          "HTTP/1.1 204 No Content\r\n"
          "Access-Control-Allow-Origin: *\r\n"
          "Access-Control-Allow-Methods: GET\r\n"
          "Access-Control-Allow-Headers: Authorization\r\n"
          "Content-Length: 0\r\nConnection: close\r\n\r\n";
      write(c, pre, strlen(pre));
      close(c);
      continue;
    }

    std::string body = "{\"error\":\"not found\"}";
    int status = 404;
    for (size_t i = 0; i < routes.size(); i++) {
      if (req.path.compare(0, routes[i].prefix.size(), routes[i].prefix) == 0) {
        status = 200;
        body = routes[i].fn(req, &status);
        break;
      }
    }
    char head[512];
    snprintf(head, sizeof(head),
             "HTTP/1.1 %d %s\r\nContent-Type: application/json\r\n"
             "Access-Control-Allow-Origin: *\r\n"
             "Content-Length: %d\r\nConnection: close\r\n\r\n",
             status, status == 200 ? "OK" : "ERROR", (int)body.size());
    write(c, head, strlen(head));
    write(c, body.data(), body.size());
    close(c);
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
