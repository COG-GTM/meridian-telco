# ADR 0001: Adopt vendored cpp-httplib

Status: Accepted

## Context

Inventory-api and invoice-api shared a hand-rolled blocking socket server in
`inventory-api/httpd.h`. It implemented request parsing and response framing
itself, including partial-I/O handling limitations. The C++20 modernization
needs a maintained HTTP implementation without changing existing consumers'
response bodies or wire output.

## Decision

Replace httpd.h with cpp-httplib v0.18.3, vendored as
`third_party/httplib.h`, for both APIs. cpp-httplib is a single-header,
MIT-licensed library. Keep the existing request dispatch and response formatting
semantics, including prefix routing and the legacy 404 reason phrase.

Configure one request per connection and explicit response header ordering to
keep captured wire output identical, including `Connection: close`. CMake links
Threads for the server implementation.

## Consequences

- HTTP parsing and transport move to a maintained library; the custom server is
  removed.
- Vendoring keeps builds independent of dependency downloads but makes version
  updates and license/security review our responsibility.
- Compatibility header writers depend on cpp-httplib's BufferStream behavior;
  dependency upgrades must rerun raw-body, status and header baseline captures.
- One-request connections intentionally give up keep-alive reuse. Existing
  manual JSON serialization and unauthenticated HTTP endpoints are unchanged.

## Alternatives

- Boost.Beast: capable, but requires more transport plumbing and Boost dependency
  integration than these small synchronous services need.
- Crow: provides a broader web framework and routing model than required, with
  additional integration surface.
- Keep httpd.h: smallest dependency footprint, but leaves HTTP parsing and socket
  correctness maintenance with this repository.
