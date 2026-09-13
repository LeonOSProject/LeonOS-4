/* SPDX-License-Identifier: MIT
 * Alpine GCC expects libssp_nonshared.a, as provided by aports/main/musl.
 * Forward to musl's real stack-protector failure handler.
 */
extern _Noreturn void __stack_chk_fail(void);

__attribute__((visibility("hidden")))
_Noreturn void __stack_chk_fail_local(void)
{
    __stack_chk_fail();
}
