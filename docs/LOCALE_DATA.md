# Locale data

LeonOS uses the standard musl locale-map search path:

```text
MUSL_LOCPATH=/usr/share/musl/locales
```

The path is exported by `/etc/leonos/locale.conf`, while message catalogs remain
under `/usr/share/locale/<locale>/LC_MESSAGES/`. Locale maps must be named with
the complete locale value, for example `zh_CN.UTF-8`.

The musl locale loader deliberately ignores `MUSL_LOCPATH` for secure (setuid or
setgid) processes. This is a musl security property: privileged programs use
the built-in C locale for numeric, date, and collation categories. Message
translation through the `leonos` gettext catalog remains available.

The `musl-locales` source is maintained as an explicit locked dependency. A
fetch records both its upstream commit and archive SHA256 before locale maps
are published into a rootfs; ordinary builds never download it. The Chinese
category definitions are derived from the pinned glibc `zh_CN` definition and
kept in `configs/locale/zh_CN`, because the locked musl-locales archive does
not yet contain a Chinese source file.
