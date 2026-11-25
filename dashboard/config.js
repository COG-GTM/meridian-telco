/* API base URLs for every dashboard page. Change them here, or define window.MERIDIAN_CONFIG
   before this script loads to override individual entries. */
window.MERIDIAN_CONFIG = Object.assign({
  inventoryApi: "http://localhost:8081",   // inventory-service: /sites /circuits /capacity
  billingApi: "http://localhost:8082",     // billing-service: /invoices
  networkApi: "http://localhost:8084"      // network-service: /ipam/report /devices ...
}, window.MERIDIAN_CONFIG || {});

/* HTML-escape API values before they are put into innerHTML. */
window.esc = window.esc || function (v) {
  return String(v == null ? "" : v).replace(/[&<>"']/g, function (c) {
    return { "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" }[c];
  });
};
