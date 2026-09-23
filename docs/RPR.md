# LeonOS Remote Package Repository

LeonOS RPR is a static, HTTPS-only repository published by GitHub Pages. It has
two independent channels:

- `/apk` contains signed LeonOS APK packages, including the optional official
  applications, the signed APK v3 index, and the corresponding public key.
- `/kernel` contains the latest matching `kernel.sys` and `loader.elf` pair plus
  their version and SHA-256 metadata.

The default base URL is `https://leonosproject.github.io/LeonOS-4`. It can be
changed under **Build > LeonOS remote package repository URL** in `menuconfig`.
The selected value is installed as `/etc/leonos/rpr.conf`.

## One-time signing-key provisioning

The APK identity must be generated with Alpine `abuild-keygen`. Run the helper
from an Alpine host with `alpine-sdk`, OpenSSL, and an authenticated GitHub CLI:

```sh
./tools/provision_rpr_signing_key.sh LeonOSProject/LeonOS-4
```

The helper runs `abuild-keygen` in a private temporary home, stores the private
key as the base64-encoded GitHub Actions secret
`LEONOS_APK_SIGNING_KEY_B64`, prints the public-key SHA-256 fingerprint, and
removes the temporary keypair. It does not write either key into this checkout.
The workflow derives the public key from the private key and publishes it as
`/apk/leonos-rpr.rsa.pub`; only the public key is part of the Pages artifact.

After provisioning, the private key must exist only in the GitHub Secret.
Losing the Secret requires a signing-key rotation. A rotation replaces the
Secret and the next `leonos-rpr-apkcheck` run replaces the client's fixed-name
public key.

## Publishing

Enable GitHub Pages with **Source: GitHub Actions**, then manually run the
**Publish LeonOS RPR** workflow. The workflow has no push or schedule trigger.
It:

1. checks out the complete source tree and installs the normal LeonOS toolchain;
2. decodes the signing key only under `$RUNNER_TEMP` with mode `0600`;
3. builds the kernel, base APK repository, and optional official
   application APKs through `make rpr-pages`;
4. merges both APK inputs and signs `/apk/packages.adb` from only
   `leonos-*.apk` packages;
5. assembles `/kernel` from artifacts produced in the same build;
6. rejects a Pages tree containing any PEM private-key marker, removes the
   temporary key, and deploys with GitHub's OIDC Pages action.

The default images contain neither these optional applications nor their APK
files. They are available only from RPR as `leonos-helloworld`, `leonos-doom`,
and `leonos-oschinpt`. `leonos-doom` includes the launcher, engine, Freedoom
IWAD, and license notice. The legacy `.api` installer remains available for
third-party packages, but LeonOS no longer publishes these three applications
as `.api` files.

The published tree is:

```text
/
├── health.txt
├── manifest.json
├── apk/
│   ├── leonos-rpr.rsa.pub
│   ├── packages.adb
│   ├── leonos-*.apk
│   ├── repository.json
│   └── SHA256SUMS
└── kernel/
    ├── kernel.sys
    ├── loader.elf
    ├── release.txt
    ├── release.json
    └── SHA256SUMS
```

`release.txt` is the strict line-oriented client protocol. Its
`image_version` and `version` are both `major.minor.patch`; no build counter or
numeric build suffix is published. `format_version` is `2` and the record lists
`kernel_file`/`kernel_sha256` together with `loader_file`/`loader_sha256`.

The loader ships with the kernel because the boot handoff layout is compiled
into both images: `kernel.sys` rejects any `loader.elf` that publishes a
different `LEONOS_BOOT_HANDOFF_VERSION`, so replacing one without the other
could leave the machine unable to boot. Format 1 advertised a paired
`middlelayer.sys`, a boot payload this system no longer loads, so
`leonos-kernel-update` refuses it instead of installing a half update. A client
that predates format 2 refuses the format-2 manifest for the same reason, which
means an installed system first has to receive the newer `leonos-kernel-update`
through `apk upgrade` before it can take a kernel release; that ordering is
deliberate, because a refused manifest leaves the working payload untouched.
The JSON file is informational and intended for external tools.

## Client commands

All network access uses the LeonOS TLS client and rejects non-HTTPS repository
URLs.

- `leonos-rpr-ping` checks `/health.txt`. It does not need root.
- `leonos-rpr-apkcheck` requires root. It downloads the public key into
  `/etc/apk/keys`, adds `ndx BASE_URL/apk/packages.adb` to
  `/etc/apk/repositories`, and runs `apk update`. It never disables APK
  signature verification.
- `leonos-kernel-update --check` strips the build suffix from `uname -r` and
  compares only the three-part `major.minor.patch` image version. A newer build
  of the same image version does not trigger an update. Check mode does not
  modify the system.
- `leonos-kernel-update` requires root. It downloads `kernel.sys` and
  `loader.elf`, verifies both SHA-256 values, then swaps
  `/boot/leonos/kernel.sys` and `/boot/loader.elf` inside a single commit
  window, also moving aside any stale `/boot/leonos/middlelayer.sys` left by an
  older release. A failed commit restores the previous kernel, loader and
  retired payload; only a completed commit deletes the backups. Reboot to
  activate the release. Use
  `leonos-kernel-update -f` (or `--force`) to repeat the replacement even when
  the local and remote image versions match; the same format, checksum and
  rollback protections still apply.
- `leonos-check-update` requires root because it refreshes APK indexes. It only
  checks the kernel release and installed `leonos-*` APK packages; it
  does not install updates. APK availability is determined by `apk` itself.

Initial setup on an installed system is:

```sh
leonos-rpr-ping
sudo leonos-rpr-apkcheck
sudo leonos-check-update
sudo apk add leonos-helloworld leonos-doom leonos-oschinpt
```

Run `sudo leonos-kernel-update` and/or `sudo apk upgrade` only after reviewing
the reported updates.
