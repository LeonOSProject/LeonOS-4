# ReliefOS runtime ABI baseline

The legacy baseline was captured before task 5 changed the runtime API. The
shared object came from the pre-migration installer runtime at
`out/x86_64/release/installer/lib/libleonos.so.2` (2026-09-27 build). The
installer copy was still intact after the normal system runtime was rebuilt
with the ReliefNT UAPI headers.

`old-shared-symbols.tsv` records every defined dynamic symbol and its ELF
symbol type, with addresses omitted so the record stays stable across linker
layouts. `old-sdk-exports.tsv` is the subset of the published `leonos_*`
runtime API. `old-soname.txt` records its SONAME. The original library SHA-256
is retained as provenance in `old-library.sha256`.

`old-sdk/include/leonos/` contains the old public headers used by the legacy
source fixture. It is intentionally a small snapshot sufficient to compile
`old_api.c`; the rest of the system headers come from the current musl SDK.
The ABI remains frozen by the symbol and layout assertions in the fixtures.
