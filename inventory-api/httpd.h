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
#include <cctype>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

struct HttpRequest {
  std::string method;
  std::string path;
  std::string target;
  std::string peer;
  std::map<std::string, std::string> query;
  std::map<std::string, std::string> headers; /* names lower cased */
};

typedef std::string (*HandlerFn)(const HttpRequest &, int *status);

struct Route {
  std::string prefix;
  HandlerFn fn;
};

/* allow_origin: value for Access-Control-Allow-Origin. "*" keeps the old
   behaviour; any other value is sent as is and OPTIONS preflights are
   answered for GET with an Authorization header. */
struct ServeOptions {
  std::string allow_origin;
  ServeOptions() : allow_origin("*") {}
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

inline void parse_headers(const char *buf, HttpRequest *req) {
  const char *p = strstr(buf, "\r\n");
  while (p) {
    p += 2;
    const char *end = strstr(p, "\r\n");
    if (!end || end == p) break;
    std::string line(p, end - p);
    size_t colon = line.find(':');
    if (colon != std::string::npos) {
      std::string name = line.substr(0, colon);
      for (size_t i = 0; i < name.size(); i++) name[i] = (char)tolower((unsigned char)name[i]);
      size_t v = colon + 1;
      while (v < line.size() && (line[v] == ' ' || line[v] == '\t')) v++;
      size_t e = line.size();
      while (e > v && (line[e - 1] == ' ' || line[e - 1] == '\t')) e--;
      req->headers[name] = line.substr(v, e - v);
    }
    p = end;
  }
}

inline void write_all(int c, const char *data, size_t len) {
  while (len > 0) {
    ssize_t w = write(c, data, len);
    if (w <= 0) return;
    data += w;
    len -= (size_t)w;
  }
}

inline int serve(int port, const std::vector<Route> &routes,
                 const ServeOptions &opts = ServeOptions()) {
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
    req.target = target;
    char peer_ip[INET_ADDRSTRLEN] = {0};
    inet_ntop(AF_INET, &peer.sin_addr, peer_ip, sizeof(peer_ip));
    req.peer = peer_ip;
    parse_target(target, &req);
    parse_headers(buf, &req);

    std::string cors = "Access-Control-Allow-Origin: " + opts.allow_origin + "\r\n";
    if (opts.allow_origin != "*") cors += "Vary: Origin\r\n";
    if (opts.allow_origin != "*" && req.method == "OPTIONS") {
      std::string pre = "HTTP/1.1 204 No Content\r\n" + cors +
                        "Access-Control-Allow-Methods: GET\r\n"
                        "Access-Control-Allow-Headers: Authorization\r\n"
                        "Access-Control-Max-Age: 600\r\n"
                        "Content-Length: 0\r\nConnection: close\r\n\r\n";
      write_all(c, pre.data(), pre.size());
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
    char head[1024];
    snprintf(head, sizeof(head),
             "HTTP/1.1 %d %s\r\nContent-Type: application/json\r\n"
             "%s%s"
             "Content-Length: %d\r\nConnection: close\r\n\r\n",
             status, status == 200 ? "OK" : "ERROR", cors.c_str(),
             status == 401 ? "WWW-Authenticate: Bearer\r\n" : "", (int)body.size());
    write_all(c, head, strlen(head));
    write_all(c, body.data(), body.size());
    close(c);
  }
}

inline std::string json_escape(const std::string &s) {
  std::string o;
  for (size_t i = 0; i < s.size(); i++) {
    unsigned char ch = (unsigned char)s[i];
    if (ch == '"' || ch == '\\') { o += '\\'; o += s[i]; }
    else if (ch < 0x20) {
      char u[8];
      snprintf(u, sizeof(u), "\\u%04x", ch);
      o += u;
    }
    else o += s[i];
  }
  return o;
}

#endif
