# Desktop refresh performance

The compositor previously wrote a small framebuffer region and then issued
`FBIOPAN_DISPLAY`. The kernel interpreted that request as a complete VMware
SVGA update, so cursor movement and application frames repeatedly submitted and
waited for the whole display. The desktop now uses the LeonOS fbdev extension
`LEONOS_FBIOBLIT` (`0x46f2`) for each blit, validating active VT ownership
and copying the pixels before presenting them. The kernel clamps the
rectangle to the framebuffer and submits only that region; `FBIOPAN_DISPLAY`
remains the explicit full-refresh operation.

This removes the unnecessary full-screen transfer and synchronous wait from
normal dirty-region repaint. It does not claim a hardware refresh rate: VMware
host scheduling, the selected virtual GPU, and application rendering still
determine the observed rate. `glxgears`' counter measures submitted frames and
must not be used alone as scanout evidence.

## 2026-09-24: asynchronous present

The legacy `framebuffer_present_region` fallback path always finished by
calling `framebuffer_vmware_sync`, which rings the `VMWARE_SVGA_REG_SYNC`
doorbell **and** spins reading `SVGA_REG_BUSY` until the host has drained the
FIFO. Because `LEONOS_FBIOBLIT` executes inside the kernel's global execution
transaction with local interrupts masked, that busy-wait pinned one core per
frame and serialised every other core's syscall behind the same ticket lock.
Under multi-core desktop load the loop was the dominant cost of a compositor
present and the visible cause of CPU-0 saturation when running Doom or a
Terminal repaint burst.

`framebuffer_present_region` now publishes the update and only rings the
doorbell (a new `framebuffer_vmware_kick` helper, non-blocking). The full
synchronous drain runs exclusively when `framebuffer_vmware_fifo_update`
reports FIFO backpressure, at which point waiting is required before retrying
the exact damage region. The newer SVGA backend already rings its own doorbell
inside `svga_fifo_packet_locked`, so the change strictly reduces the work done
under the execution lock on that path too. The global execution lock itself is
unchanged; reducing its scope across subsystems remains separate work.

For a compositor sample, create `/etc/leonos/desktop-profile` in the guest and
restart the desktop. It logs `[desktop-perf] frames=... elapsed_ms=...
paint_ms=... inputm_ms=...` every five seconds. The profile is disabled by
default and has no effect on normal images.

Verification on 2026-09-12:

- `python3 -m unittest tools.test_runtime_responsiveness.RuntimeResponsivenessTests.test_framebuffer_reports_hardware_limits_and_remaps_after_mode_change tools.test_runtime_responsiveness.RuntimeResponsivenessTests.test_window_repaints_reuse_live_shared_memory`: pass.
- `python3 build.py run app:desktop`: 0 errors.
- Existing `tools/test_svga.py` remains the driver-level FIFO/update regression;
  VMware-specific 60 FPS measurement remains pending.
