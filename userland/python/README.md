# Python Runtime

This external ABI test fixture uses the user-supplied CPython 3.14.7 build:
`cpython-3.14.7+20260901-x86_64-unknown-linux-musl-lto+static-full.tar.zst`.
SHA256: `e5a76e5893c39c89ed268ad71c3d6794c3be6b7236ec613449140c57686fc17f`.

The original static executable and complete `python/install` tree are packaged
in `/opt/python`; the image carries real relative symlinks for the aliases.
Archive licenses and `PYTHON.json` metadata are in `/usr/share/licenses/python`.
No interpreter or standard library source is patched. This does not certify
all Python modules against LeonOS's incomplete Linux ABI.

`/usr/bin/python3.14` is the static musl launcher; `/usr/bin/python` and
`/usr/bin/python3` are relative symlinks to it.
They locate `/opt/python` relative to the image root, set `PYTHONHOME` unless
the caller supplied it, and exec the original interpreter. No shell script
wrapper or `/install` alias is required. Bundled pip is available through
`python3 -m pip`; upstream script shebangs are preserved.

## Independent Fixture

Python has been removed from production builds and installer options. Future
system installation will use apk. The retained fixture can be prepared
explicitly for ABI regressions:

```sh
python3 build.py run musl
python3 tools/package_python.py --source /path/to/cpython-3.14.7+20260901-x86_64-unknown-linux-musl-lto+static-full.tar.zst --archive buildsystem/deps/python/cpython-3.14.7+20260901-x86_64-unknown-linux-musl-lto+static-full.tar.zst --musl build/musl/sysroot --out build/python/root
python3 tools/test_python_package.py --root build/python/root
```

Cache: `buildsystem/deps/python/` with the exact archive filename above.
This tree is not consumed by normal ISO, VMDK, installer or SDK builds.

Inside an explicitly prepared Python regression image:

```sh
python3 --version
python3 /usr/share/examples/python/hello.py
```
