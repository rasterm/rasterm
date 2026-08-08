# Contributing to rasterm

Thanks for taking the time to contribute! We welcome contributions that improve correctness, boost performance, update documentation, or add tests.

Since `rasterm` is intentionally focused strictly on frame presentation, we try to keep the scope tight. Here is what you need to know before working on a pull request.

---

## 1. Before You Code

* **Discuss big changes first:** Open an issue before starting work on public API/ABI changes, new protocol features, or major architectural shifts. Small bug fixes and docs improvements don't need prior discussion.
* **Respect boundary rules:** Terminal protocols must remain hidden behind the private engine layer. Public applications should only interact with frame submission.
* **Protect performance:** Don't break DCS/output transactions or producer thread timing guarantees.

---

## 2. PR Requirements

Before submitting your pull request, make sure you've covered these bases:

1. **Tests:** Include golden correctness tests for any new features or bug fixes. If you're submitting a performance optimization, include benchmark results backing up the speedup claims.
2. **Clean Builds:** Build with warnings treated as errors. Check [`docs/BUILD.md`](docs/BUILD.md) for the build and test commands.
3. **Clean Diffs:** Match the existing code style and formatting in nearby files. Avoid sweeping reformatting or unrelated changes in your PR.
4. **Third Party Code:** Do not add third party code, assets, or fixtures without explicitly documenting their origin and license.

---

## 3. Licensing

By contributing, you agree that your submissions are licensed under the **Apache License 2.0** (unless explicitly marked otherwise).

* Do not alter existing SPDX license headers or copyright notices.
* If you add or modify dependencies, make sure `NOTICE` and `THIRD_PARTY_NOTICES.md` are updated accordingly.

---

## 4. Security Vulnerabilities

**Please do not report security vulnerabilities in public GitHub issues.**

If you discover a security issue, refer to our [Security Policy (`SECURITY.md`)](SECURITY.md) for instructions on how to report it privately.
