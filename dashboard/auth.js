/* the inventory and invoice apis require a bearer token. it is asked for once
   per tab and kept in sessionStorage, never in the page or the url. */
var MERIDIAN_TOKEN_KEY = "meridian.apiToken";

function meridianToken() {
  var t = sessionStorage.getItem(MERIDIAN_TOKEN_KEY);
  if (!t) {
    t = (window.prompt("Meridian API token (MERIDIAN_API_TOKEN)") || "").trim();
    if (t) sessionStorage.setItem(MERIDIAN_TOKEN_KEY, t);
  }
  return t;
}

function apiFetch(url) {
  return fetch(url, { headers: { Authorization: "Bearer " + meridianToken() } }).then(function (r) {
    if (r.status === 401) {
      sessionStorage.removeItem(MERIDIAN_TOKEN_KEY);
      throw new Error("API token rejected, reload to enter it again");
    }
    if (r.status === 403) throw new Error("this page's origin is not in MERIDIAN_CORS_ORIGINS");
    return r;
  });
}
