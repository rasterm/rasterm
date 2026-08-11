# Windows Terminal Compatibility

rasterm targets Windows Terminal's SIXEL implementation. Windows Terminal 1.22 is
the minimum stable line with SIXEL, synchronized output became available in stable
1.23. The recommended rasterm 1.2 baseline is therefore Windows Terminal 1.23.20211.0 or
newer. The latest stable release recorded for this matrix is 1.24.11321.0 (May 13,
2026).

| Terminal | SIXEL | synchronized output | rasterm 1.2 status |
|---|---:|---:|---|
| Windows Terminal 1.22.10352 | supported | not assumed | minimum SIXEL only (set `useSynchronizedOutput=false`) |
| Windows Terminal 1.23.20211 | supported | supported | minimum complete feature baseline |
| Windows Terminal 1.24.11321 | supported | supported | recommended current stable baseline |
| Preview/Canary | expected | expected | development only (not a release baseline) |
| legacy conhost or redirected stdout | unsupported/unknown | unsupported/unknown | use a custom sink or fail capability requirements |

The version entries record Microsoft release behavior. Live minimize/restore,
DPI/font change, and long running validation remain release candidate manual tests.
The automated suite exercises equivalent geometry changes and exact parser output.

## Capability detection decision

rasterm uses environment and console checks instead of asking the terminal directly.
This is conservative, but it avoids a reply blocking the program, being mistaken for
user input, or leaking into the graphics stream.

- `WT_SESSION` or `TERM_PROGRAM=Windows_Terminal`: SIXEL and synchronized output are
  reported supported. The environment does not expose a reliable version, so apps
  supporting Windows Terminal 1.22 must disable synchronized output themselves.
- a `TERM` value containing `sixel`: SIXEL is supported and synchronized output is
  `Unknown`.
- an ordinary console without either marker: both capabilities are `Unknown`.
- an invalid or non console default output: both are unsupported.
- a custom `OutputSink`: capabilities are `Unknown`; rasterm never requires or mutates
  a Win32 console handle.

`requireSixelSupport=true` rejects `Unknown`. Without it, applications may attempt
rendering and handle structured output errors.

Microsoft references: the [Windows Terminal 1.22 release](https://devblogs.microsoft.com/commandline/windows-terminal-preview-1-22-release/)
introduced SIXEL, the [official release history](https://github.com/microsoft/terminal/releases)
records the 1.23.20211 DECSET 2026 backport and current stable build.
