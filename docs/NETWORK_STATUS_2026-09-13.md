# IPv4 networking repair, 2026-09-13

## APK download throughput follow-up

The VMware log `/home/xiaobai/installer-serial.log` contained 2,460,699 bytes,
8,128 TCP receive entries and 11,123,189 accepted payload bytes. There were
173 full 8 KiB receive queues and repeated one-byte probes/small window updates.
Every port-443 packet also generated synchronous console/serial output; the
diagnostic strings incorrectly contained literal `\\n` rather than newlines.

Changes in `kernel/ntclks/net.c` and `drivers/e1000/e1000.c`:

- Packet-level TLS tracing defaults off (`NET_TCP_TRACE=1` is an explicit
  compile-time diagnostic switch); enabled traces have real line endings.
- Per-connection receive storage is a heap-allocated 65,535-byte ring. Reading
  a TLS header no longer shifts the whole queued response. Allocation failure
  leaves no published socket, and GC/reuse frees the buffer.
- Window offers extend in full MSS units and preserve the already-advertised
  right edge, including sequence wrap. Reads send a window update for a
  significant increase, including reopening zero windows, rather than offering
  a fresh five-byte window. Failed sends do not publish window state.
- The e1000 RX ring grows from 16 to 256 descriptors, matching Linux's default,
  so an entire receive window can arrive before the next task polls the NIC.
  Descriptor bounds and failed-allocation cleanup remain enforced.

Reference: Linux v6.12 `net/ipv4/tcp.c::__tcp_cleanup_rbuf`,
`net/ipv4/tcp_output.c::{tcp_select_window,__tcp_select_window}` and
`drivers/net/ethernet/intel/e1000/e1000.h::E1000_DEFAULT_RXD` in the pinned local
source tree. This is a bounded receive-path correction, not a complete Linux TCP
implementation; window scaling, congestion control and out-of-order buffering
are not certified by these tests.

Validation used actual upstream apk and cached, unmodified Alpine packages
served over HTTPS in an isolated host network namespace. The guest trusts a
fixture CA and verifies package signatures; downloaded files are independently
hashed by BusyBox in the guest. There is no Internet or proxy in the measured
path. The test autospawn process explicitly receives the normal executable PATH.

| Operation | Before | Final |
| --- | --- | --- |
| Fetch libcrypto3, 1,978,546 bytes | 5.42 s | 1.42 s |
| Fetch GCC, 60,496,469 bytes | not measured | 13.65 s (4.23 MiB/s) |
| Add gcc + leonos-musl-dev + make, 16 packages including dependencies | not measured | 29.20 s |

After installation, GCC, cc1, as, collect2 and ld produced a dynamically linked
stdio executable that printed `APK_GCC_NETWORK_OK` and exited zero. GCC's cached
APK SHA256 was `d1d482472ed91240731c60dbae8176bc1e32e1dfb694856da6759f6d32716325`.
The libcrypto3 serial log shrank from 260,195 to 10,309 bytes.

ASan/UBSan host regressions cover five-byte reads, zero-window reopening, ring
wrap, overlapping retransmissions, sequence wrap, TX failure, buffer allocation
failure/release and 64-packet NIC bursts across descriptor wrap. The new
small-read and burst tests both failed on the original implementation.

```sh
python3 tools/test_network.py
python3 tools/test_e1000.py
unshare --user --map-root-user --net python3 tools/test_tcp_download_qemu.py --label gcc-final
unshare --user --map-root-user --net python3 tools/test_tcp_download_qemu.py --label install --install
```

Results/serial logs are in `build/tcp-download/{before,final-small,gcc-final,install}`.
Results record the actual kernel hash; final results also record the driver hash. QEMU used KVM,
q35, e1000-82545em and one socket/two cores. This does not measure a public Alpine
mirror, VMware NAT, installed SATA throughput, or certify desktop frame rates.

## Earlier network bring-up

Scope: prepare the existing network path for future Alpine apk integration.
This work does not install apk or certify the full Linux networking ABI.

## Root causes and changes

- The e1000 module hid its actual initialization errors. It now reports missing
  PCI devices, invalid MMIO/MAC and reset failures. Initialization quiesces RX/TX,
  resets the controller and sets CTRL.SLU. A valid hardware MAC is required;
  allocation/registration failures release allocated DMA pages. Descriptor
  publication/consumption has memory ordering and RX length checks.
- Carrier is read from STATUS.LU. The driver manager reports hardware identity
  even when disconnected. PCnet detection produces an explicit diagnostic; a
  PCnet adapter is not treated as an e1000 adapter.
- `serviced/netmand.c` previously returned fabricated addresses and failure
  placeholders. Management now queries the actual kernel state. The service
  calls its internal management functions directly rather than issuing IPC to
  itself. Client IPC checks frame sizes and handles failed connections.
- `LEONOS_NET_CONTROL_IOCTL` is an explicitly LeonOS-specific, versioned UAPI
  extension on an IPv4 socket. DHCP and DNS-policy mutations require
  CAP_NET_ADMIN. It does not replace the Linux UDP/TCP data path and must not be
  described as a standard Linux interface-configuration ioctl.
- DHCP configures the supplied subnet, gateway and DNS. A failed acquisition
  leaves the interface unconfigured, rather than assigning the QEMU default
  address on unrelated networks. ACK identity and required lease options are
  checked. Lease time counts down, expiration clears the address, and serviced
  schedules reacquisition around half the remaining lease. This currently uses
  DORA reacquisition, not the complete unicast RENEWING/REBINDING state machine.
- `/etc/resolv.conf` is atomically published from the real DHCP or configured
  DNS server. The seed and installer no longer force `1.1.1.1`.
- AF_INET SOCK_DGRAM now supports datagram queues, bind/connect, source
  addresses, read/write/sendto/recvfrom/sendmsg/recvmsg, poll, nonblocking flags,
  truncation/peek, zero-length datagrams, receive timeouts and descriptor
  lifetime. This enables musl's ordinary DNS resolver without modifying musl.
- TCP address conversion no longer reverses IPv4 bytes. SOCK_CLOEXEC and
  SOCK_NONBLOCK are separated from the socket type. Nonblocking connect has
  EINPROGRESS/poll/SO_ERROR completion. FIONBIO and FIONREAD use actual state;
  unsupported socket options return an error instead of fabricated success.
- TCP objects remain alive while an open file description references them,
  including after fork and parent close. Idle GC cannot recycle a live fd.
  The receive window reflects available space. ACKs cover only retained bytes;
  overlapping retransmissions retain their new suffix. IPv4, UDP and TCP
  checksums are validated.
- The NTP service uses normal user-space UDP sockets. It validates the echoed
  origin, packet size, version/mode, leap indicator, stratum and server
  timestamps, unfolds the NTP era, and accounts for transit/processing delay.
  `/etc/ntp.conf` selects the server, defaulting to `pool.ntp.org`.
  CLOCK_REALTIME updates preserve fractional seconds and require CAP_SYS_TIME.

Reference material: the pinned Linux v6.12 e1000 and socket implementation in
`build/linux-6.12`, musl `src/network/res_msend.c`, and
[RFC 5905](https://www.rfc-editor.org/rfc/rfc5905). The RFC was retrieved through
the configured HTTP proxy at `127.0.0.1:12334`.

## Verification

Host tests directly compile the driver/transport implementations with
ASan/UBSan. They cover failed initialization cleanup, link state, UDP queue
semantics, TCP lifetime/window/retransmission, lease expiration and NTP packet
validation. Existing Unix message-batch tests and resource tests also pass.

```sh
python3 tools/test_e1000.py
python3 tools/test_network.py
python3 tools/test_linux_socket_batches.py
python3 tools/test_linux_resources.py
```

The guest uses an independent static musl probe and the production, unmodified
BusyBox and CPython executables. A private user/network namespace contains DNS,
HTTP, HTTPS and NTP fixtures; it does not change the host's DNS or clock.
The subnet is `10.37.0.0/24` to detect assumptions about QEMU's default subnet.

```sh
unshare --user --map-root-user --net python3 tools/test_network_qemu.py
unshare --user --map-root-user --net python3 tools/test_network_qemu.py --reuse --nic e1000 --smp 1
```

QEMU/KVM q35 results: e1000-82545em with 2 vCPUs and e1000 with 1 vCPU both
reported `[network] DONE failures=0`. Verified behavior:

- DHCP address `10.37.0.15`, gateway `.2`, DNS `.3`.
- UDP scatter/gather, source address and MSG_TRUNC; unmodified musl DNS.
- Nonblocking TCP handshake, actual SO_ERROR and inherited fd after parent close.
- An NTP fixture supplies 2040-01-01 to exercise era unfolding; CLOCK_REALTIME
  actually changes, including fractional seconds. The fixture changes only the
  disposable guest clock. Production images use the normal NTP configuration.
- Official BusyBox downloads a 1 MiB HTTP payload, checked byte for byte.
- Unmodified CPython downloads and checks the same payload over HTTPS with a
  trusted test CA, and rejects that server with the default untrusted CA set.
- Native libc HTTPS requests now use the same CA bundle and SNI/hostname
  verification: `leonos_http_request()` and `leonos_http_download()` both
  transferred an exact 1 MiB payload. Certificate hostname mismatch is
  rejected, and an HTTPS redirect to cleartext HTTP is rejected instead of
  silently downgrading the connection.
- TLS response reads are chunked into bounded buffers, while HTTP header bytes
  are accounted for separately from the caller's body capacity. This keeps
  the documented body buffer contract intact for large HTTPS responses.
- QMP disconnect/reconnect changes carrier without losing NIC identity;
  subsequent DHCP reacquisition succeeds.

Logs and machine-readable results are under `build/network-test/`, named
`e1000-82545em-smp2.{log,json}` and `e1000-smp1.{log,json}`. These prove the
controlled QEMU network path, not an actual public NTP server or VMware NAT run.

## Build artifacts

The hashes below identify the current network-enabled build 3718.

`python3 build.py run images-iso` completed with zero errors. Both packaged
root images contain the production runtime and certificate bundle; the live
image's `/usr/lib/leonos/libleonos.so.2` matches
`build/system/lib/libleonos.so.2`, and its CA bundle matches
`system/certs/cacert.pem` byte for byte.

- Live desktop: `build/images/leonos4.iso` (BIOS and UEFI GRUB).
  SHA256: `fc9cac98c51f23afec80217c8719c206a1a96d91338236bebb7c134f12392fa3`.
- Installer: `build/images/leonos4-installer.iso` (UEFI GRUB).
  SHA256: `704ef39d1967823d5e94e88754a91774c6bf1dcf9d9e860d046cb0eac194c628`.
- Both embed kernel SHA256
  `a456956f1f41e8851cbfe592513e2b9674c79aafdefa8fafe3a5cc4b7e550b9f`.
  This is build `4.6.2-3718`, including the verified HTTPS libc path.
  The final 2-vCPU QEMU network run uses this same kernel. Build output is in
  `build/taskbar-clock-images.log`, and final guest output in
  `build/network-test/e1000-82545em-smp2.log`.
  The live root and installer's `/install/root` payload were checked byte for
  byte against the built netctl, serviced, sudod and libleonos binaries.

## VMware configuration

The user's `LeonOS4.vmx` did not specify `ethernet0.virtualDev`; VMware logs
contained VLANCE/VMXNET messages. That is evidence to check the virtual NIC
model, not proof that the guest had an Intel adapter.

After `vmrun list` reported no running VMs, the configuration was backed up to
`LeonOS4.vmx.before-e1000-20260913`, and the following explicit selection was
added to `/home/xiaobai/vmware/LeonOS4/LeonOS4.vmx`:

```ini
ethernet0.virtualDev = "e1000"
```

The host's vmnet8 NAT and DHCP processes are running, with host address
`192.168.165.1/24`. This host-side check does not prove guest connectivity.

### VMware follow-up verification

On the next report, the VMX explicitly contained `virtualDev = "vlance"`.
The installed guest logged `VMware PCnet detected` and e1000 initialization
returned -19 (ENODEV). The failed boot was therefore using a PCnet adapter,
not an Intel e1000. The prior configuration change had not persisted; the
available evidence does not establish which UI action changed it.

The current configuration was backed up as
`LeonOS4.vmx.before-network-recheck-20260913`. Its network settings are now:

```ini
ethernet0.present = "TRUE"
ethernet0.virtualDev = "e1000"
ethernet0.connectionType = "nat"
ethernet0.startConnected = "TRUE"
```

The VMware main window held a live configuration lock even with zero running
VMs; `vmrun start` returned "The file is already in use". After the user exited
the main window, the lock disappeared and these settings were verified again.
No lock files were removed to bypass the live owner.

Actual VMware Workstation results, using the existing kernel/runtime:

- A disposable, diskless VM booted the production live ISO, loaded e1000, and
  acquired `192.168.165.128`, gateway/DNS `192.168.165.2`.
- The user's original VM then booted its installed ext2 system, loaded e1000
  and acquired `192.168.165.129` on DHCP attempt 1. VMware's own log confirms
  `virtualDev = "e1000"`, NAT and MAC `00:0c:29:06:29:97`.
- Both guests performed an actual CLOCK_REALTIME update through the production
  NTP service. The installed boot logged `wall clock set 2026-9-12 22:22:37`
  at uptime 3 seconds, matching the host's UTC time. This was not the controlled
  QEMU 2040 fixture. The original VM uses build `4.6.2-3701` already containing
  the network repair; no reinstall was needed for this configuration fix.

Evidence: `build/network-vmware-test/serial.log` (diskless live VM),
`build/network-vmware-test/installed-serial.log` (installed VM; latest boot
starts at line 21987), and the original `/home/xiaobai/installer-serial.log`.
VMware HTTP/HTTPS payload downloads have not been exercised in this follow-up;
the HTTPS and certificate checks above are controlled QEMU fixture tests.

## DHCP button and askpass follow-up

The installed guest still acquired a real VMware NAT lease at boot, but the
network controller's Renew DHCP button reported only `ret=-1`. Its ordinary
user request reached a root-only service operation; the service disconnected
the client without returning the permission error. This was independent of
e1000 carrier and boot-time DHCP success.

- Management IPC now returns typed errno responses. Permission denial is
  EACCES, and the connection remains usable for subsequent read-only queries.
  Kernel and publication errors propagate to the caller. Invalid frames or
  transport failures close the client connection and permit a later retry.
- The ordinary GUI runs `netctl.elf --renew-dhcp` through the existing upstream
  sudo/PAM integration and waits asynchronously. The short-lived privileged
  command performs the actual operation. Root may renew directly. The normal
  sudoers policy, peer-credential checks and kernel CAP_NET_ADMIN enforcement
  remain required; no password bypass is installed in production images.
- GUI font loading wrote its diagnostics to stdout. Upstream sudo's askpass
  protocol consumed that first line as the password and closed its pipe,
  causing the actual password write to fail with EPIPE. GUI font/window
  diagnostics now go to stderr. Askpass logs only dialog outcome and I/O errno,
  never password content.

The host regression compiles the real daemon/client/framing with ASan/UBSan
and separately exercises netctl's root, authorization, pending, cancellation
and failure handling. It passes:

```sh
python3 tools/test_netmand.py
```

The final 2-vCPU QEMU network probe reports 25 passing checks and
`[network] DONE failures=0`. Added checks use the actual service, reject ordinary
user mutations, permit subsequent reads, reject an unprivileged helper, and
execute a successful DHCP helper through official sudo. The noninteractive
probe adds a command-specific NOPASSWD rule only to its disposable test image.
The separate `--gui` mode uses the production password policy instead.

```sh
unshare --user --map-root-user --net python3 tools/test_network_qemu.py
unshare --user --map-root-user --net python3 tools/test_network_qemu.py --gui
```

The final GUI run also exits 0. It logs into the live desktop as the ordinary
test account, clicks Renew DHCP, cancels the first password dialog and checks
that no password reached PAM and no privileged helper ran. A second click and
the correct password produce successful askpass, PAM, DHCP helper and sudo
exits. The controller displays the renewed `10.37.0.15` lease, and its Resolve
button receives `10.37.0.2` from the isolated DNS fixture for `example.com`.
This address is deliberately fixture data, not a public DNS result.
Evidence: `build/network-gui-test/e1000-82545em-smp2.{log,json}`,
`authorization-cancelled.png`, `dhcp-renewed.png`, and `dns-after-renew.png`
in the same directory. The test image kernel matches the production ISO kernel
listed above, and no NOPASSWD test rule is present in the GUI image.

The DHCP button and askpass changes require updating the installed userland;
the earlier VMX-only repair did not. VMware GUI validation of these new changes
remains pending.

## Taskbar clock follow-up

The VMware log for installed build 3705 records a valid RTC and an NTP wall
clock update, but the taskbar still showed `00:00:00`. The SDK's
`leonos_time_info()` filled `unix_seconds` and set `valid` while leaving its
calendar fields zero. The desktop correctly refreshed each second but read
those zero hour/minute/second fields.

The SDK now uses musl `gmtime_r()` to fill every calendar field and sets `valid`
only after conversion succeeds. Null output, failed clock reads, negative
timestamps outside this unsigned SDK structure and calendar overflow return
errors. UTC semantics are preserved; this repair does not configure a local
timezone or change the kernel's Unix timestamp representation.

`python3 tools/test_time_info.py` failed on the original zero calendar and now
passes against the actual SDK implementation. It checks known dates, ticking
seconds, leap day, year 2040, the non-leap century 2100 and error paths, with a
non-UTC process TZ to check the UTC contract. Final `--gui` validation recognizes
the actual taskbar screenshots with Tesseract: `00:00:34` then `00:00:36` under
the isolated midnight NTP fixture. The standard 25 guest network checks also
pass with the final image's kernel/runtime.

Evidence: `build/taskbar-clock-gui.log`, `build/taskbar-clock-network.log`, and
`build/network-gui-test/clock-{first,second}.png`. Both production ISO builds
completed with zero errors. This display repair has QEMU validation; VMware
display validation remains pending.

## Remaining limits

- IPv4 client networking is the tested target. IPv6, PCnet/vmxnet3 drivers,
  IP fragment reassembly, complete ICMP error propagation and all Linux socket
  options are not implemented by this change.
- TCP still uses the existing bounded connection table and synchronous send
  machinery. Full nonblocking send behavior, Linux TCP congestion control,
  listen/accept, every shutdown state and all message flags are not certified.
  UDP currently limits payloads to 1472 bytes and has bounded receive queues.
  UDP SO_SNDTIMEO returns ENOPROTOOPT until the transmit path implements it;
  SO_RCVTIMEO is enforced and tested against a real expiration boundary.
- NTP is periodic clock correction, not a full chrony/ntpd clock discipline or
  a claim of sub-tick accuracy. The kernel tick is 10 ms.
- BusyBox `wget` provides HTTP. Its internal TLS implementation does not verify
  certificates and is disabled; CPython and the native libc HTTP helpers supply
  the tested verified HTTPS paths.
- The current TLS integration is mbedTLS TLS 1.2. TLS 1.3 negotiation and
  arbitrary public-site compatibility remain unverified.
- apk itself, repository signatures, package installation and rollback are
  outside this networking task and remain unverified.
