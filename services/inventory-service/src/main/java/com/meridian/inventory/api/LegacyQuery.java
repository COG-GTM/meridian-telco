package com.meridian.inventory.api;

import java.util.HashMap;
import java.util.Map;

/**
 * httpd.h parse_target: the raw (not URL-decoded) query string split on '&' then the first '=';
 * pairs without '=' are ignored and a repeated key keeps the last value.
 */
final class LegacyQuery {

  private LegacyQuery() {}

  static Map<String, String> parse(String queryString) {
    Map<String, String> q = new HashMap<>();
    if (queryString == null) return q;
    for (String kv : queryString.split("&", -1)) {
      int eq = kv.indexOf('=');
      if (eq >= 0) q.put(kv.substring(0, eq), kv.substring(eq + 1));
    }
    return q;
  }
}
