# Fastfetch for ReliefOS

The image ships Fastfetch 2.68.1 as a prebuilt native x86-64 static musl
executable, with the ReliefOS logo included. Its Linux detection code uses
`uname`, `/etc/os-release`, `/proc`, and `/sys`; it does not link an OS-specific adapter. The former submodule and adapter build have been removed.

Explicit `make fetch` downloads the binary from
`https://github.com/VasilyZa/fastfetch/releases/download/2.68.1/fastfetch`
using the host's normal network and optional proxy environment. It pins SHA-256
`25107efd56d0286059487bab17d964a6ec72275263de2ab09095a46637b06be1`
and caches it in
`$(RELIEFOS_CACHE)/fastfetch` (by default `cache/downloads/fastfetch`).
The retained Python reference packager uses the historical
`buildsystem/deps/fastfetch/fastfetch-2.68.1-leonos-x86_64-linux-musl` cache.
Subsequent builds use the validated cache without downloading again.
Downloads are verified before atomic publication; partial downloads and hash
mismatches are errors. The build never falls back to an older implementation.
For an offline build, provision the same hash-verified file in the configured
`RELIEFOS_CACHE`; the Make adapter reads only that explicit cache.
Updating the release requires explicitly updating the URL and pinned hash.

```sh
make fetch
make installer
make test
python3 tools/test_linux_inventory.py --guest
```

`/usr/bin/fastfetch` resolves to the packaged executable at
`/usr/lib/reliefos/apps/fastfetch/fastfetch.elf`. Both normal and installer image
staging use this same payload. `/etc/fastfetch/config.jsonc` selects the built-in
`LeonOS` logo alias for the ReliefOS artwork by default. Users can override the logo with
`fastfetch --logo LeonOS`, another upstream logo, or their own configuration.
The component's MIT license is installed in `/usr/share/licenses/fastfetch`.
Its `package.json` records the release URL, version, and binary hash.

The Kernel row reports the ReliefNT name and release supplied by `uname`; the
OS row reads the ReliefOS identity from `/etc/os-release`. No application-side
output substitution is used.

HyFetch supplies its own ASCII art to Fastfetch, so Fastfetch's default logo
does not select HyFetch's logo. The package also ships the ReliefOS art in
`/usr/share/fastfetch/leonos-ascii.txt` and a HyFetch configuration template in
`/etc/skel/.config/hyfetch.json`. New standalone-image and installer accounts
receive this template, which selects RGB rainbow coloring and the Fastfetch
backend. HyFetch itself remains the unmodified Alpine package, installed with
`apk add hyfetch@testing`; it is not bundled by this component. Disabling the
Fastfetch component also removes these defaults from staging.

Existing user configurations are preserved. For an existing configuration
with no custom ASCII path, use:

```sh
hyfetch --ascii-file /usr/share/fastfetch/leonos-ascii.txt
```

To persist the selection while choosing a palette, run `hyfetch --config` and
select that file when asked for custom ASCII art. Users can also set the
`custom_ascii_path` field in their existing `~/.config/hyfetch.json` to that
absolute path without changing the other settings.

The guest inventory test checks the binary actually staged at
the supplied guest image, including Kernel, hardware and resource JSON,
and execution through Desktop Terminal. Passing this subset does not certify
every upstream module or VMware behavior.

Validation on 2026-09-11 (kernel build 3523):

- A fresh cache downloaded and verified the pinned GitHub release through the
  proxy. All four packaging tests and the component configuration checks passed.
- The installer build completed with zero errors. Its installed payload and
  normal image staging both contain the pinned binary, configuration, license,
  and release manifest.
- Host ASan/UBSan inventory checks and QEMU/KVM inventory checks passed. Default
  output includes the LeonOS logo, OS and Memory rows, and
  `Kernel: ntclks 4.6.2-3523`. Console and Desktop Terminal JSON agree with
  `uname` and procfs; the guest reported `[inventory] DONE failures=0`.
  QEMU configured two CPUs, but the kernel enabled only one; this is not an SMP
  validation. Evidence is in `build/linux-inventory/`.
- Fastfetch license packaging checks passed for both staging trees. The full
  repository license check still reports three unrelated failures: Lua's SDK
  upstream license lookup and the two PortableGL SDK library notices.
- VMware was not tested.
