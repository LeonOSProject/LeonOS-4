# LeonOS Remote Package Repository

LeonOS RPR is a static, HTTPS-only repository published by GitHub Pages. It has
two independent channels:

- `/apk` contains signed LeonOS APK packages, the signed APK v3 index, and the
  corresponding public key.
- `/kernel` contains the latest matching `kernel.sys` and `middlelayer.sys`
  pair plus version and SHA-256 metadata.

The default base URL is `https://leonmmcoset.github.io/LeonOS-4`. It can be
changed under **Build > LeonOS remote package repository URL** in `menuconfig`.
The selected value is installed as `/etc/leonos/rpr.conf`.

## One-time signing-key provisioning

The APK identity must be generated with Alpine `abuild-keygen`. Run the helper
from an Alpine host with `alpine-sdk`, OpenSSL, and an authenticated GitHub CLI:

```sh
./tools/provision_rpr_signing_key.sh Leonmmcoset/LeonOS-4
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
3. builds the kernel, middle layer, rootfs, and signed LeonOS APKs through
   `python3 build.py run rpr-pages`;
4. rebuilds and signs `/apk/packages.adb` from only `leonos-*.apk` packages;
5. assembles `/kernel` from artifacts produced in the same build;
6. rejects a Pages tree containing any PEM private-key marker, removes the
   temporary key, and deploys with GitHub's OIDC Pages action.

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
    ├── middlelayer.sys
    ├── release.txt
    ├── release.json
    └── SHA256SUMS
```

`release.txt` is the strict line-oriented client protocol. Its `version` is
`major.minor.patch-build`; the final number is the LeonOS build number exposed
by `uname -r`. The JSON file is informational and intended for external tools.

## Client commands

All network access uses the LeonOS TLS client and rejects non-HTTPS repository
URLs.

- `leonos-rpr-ping` checks `/health.txt`. It does not need root.
- `leonos-rpr-apkcheck` requires root. It downloads the public key into
  `/etc/apk/keys`, adds `ndx BASE_URL/apk/packages.adb` to
  `/etc/apk/repositories`, and runs `apk update`. It never disables APK
  signature verification.
- `leonos-kernel-update --check` compares all four numeric version fields with
  `uname -r` and does not modify the system.
- `leonos-kernel-update` requires root. It downloads both boot files, verifies
  each SHA-256, then replaces `/boot/leonos/kernel.sys` and
  `/boot/leonos/middlelayer.sys` with rollback on a failed replacement. Reboot
  to activate the release.
- `leonos-check-update` requires root because it refreshes APK indexes. It only
  checks the kernel/middle-layer pair and installed `leonos-*` APK packages; it
  does not install updates. APK availability is determined by `apk` itself.

Initial setup on an installed system is:

```sh
leonos-rpr-ping
sudo leonos-rpr-apkcheck
sudo leonos-check-update
```

Run `sudo leonos-kernel-update` and/or `sudo apk upgrade` only after reviewing
the reported updates.
