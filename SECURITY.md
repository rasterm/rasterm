# Security Policy

## Supported Versions

Security fixes target the latest 1.x minor release, currently `1.1.x`. Users of older
minor releases may be asked to upgrade before receiving a fix.

## Reporting a Vulnerability

Use GitHub's private security advisory reporting for
[`rasterm/rasterm`](https://github.com/rasterm/rasterm/security/advisories/new).
Include affected version, reproduction, impact, and any proposed mitigation. Do not
include secrets or copyrighted media. Please allow maintainers time to confirm and
prepare a coordinated fix before public disclosure.

rasterm processes untrusted dimensions, strides, palette indices, metadata, and output
failures. Reports involving checked arithmetic, terminal sequence injection/corruption,
state restoration failure, callback lifetime, ABI validation, or resource exhaustion are
in scope. Vulnerabilities in vendored apps such as RetroArch/SimpleNES code should also be reported
to their upstream projects when applicable.
