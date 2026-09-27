#include <assert.h>
#include <stdio.h>
#include <reliefnt/usercopy.h>
#include <reliefnt/paging.h>
#include <reliefnt/sched.h>

static struct task current;
static uint64_t flags[3];
static unsigned cow_copies;
struct task *sched_current_task(void) { return &current; }
static unsigned index_for(uint64_t address) { return (address - RELIEFNT_USER_BASE) / 4096; }
bool address_space_user_page_readable(const struct address_space *as, uint64_t address)
{
    (void)as;
    unsigned index = index_for(address);
    uint64_t required = RELIEFNT_PAGE_PRESENT | RELIEFNT_PAGE_USER;
    return index < 3 && (flags[index] & required) == required;
}
bool address_space_user_page_writable(const struct address_space *as, uint64_t address)
{
    (void)as;
    unsigned index = index_for(address);
    return index < 3 && (flags[index] & RELIEFNT_PAGE_WRITABLE);
}
bool address_space_handle_cow_fault(struct address_space *as, uint64_t address)
{
    (void)as;
    unsigned index = index_for(address);
    if (index >= 3 || !(flags[index] & RELIEFNT_PAGE_COW)) return false;
    flags[index] = RELIEFNT_PAGE_PRESENT | RELIEFNT_PAGE_USER | RELIEFNT_PAGE_WRITABLE;
    ++cow_copies;
    return true;
}
int syscall_handle_user_page_fault(uint64_t address, uint64_t error)
{
    (void)address; (void)error;
    return 0;
}
int main(void)
{
    current.kind = TASK_KIND_USER;
    flags[0] = RELIEFNT_PAGE_PRESENT | RELIEFNT_PAGE_USER | RELIEFNT_PAGE_WRITABLE;
    flags[1] = RELIEFNT_PAGE_PRESENT | RELIEFNT_PAGE_USER | RELIEFNT_PAGE_COW;
    flags[2] = RELIEFNT_PAGE_PRESENT | RELIEFNT_PAGE_USER;
    assert(user_range_writable(RELIEFNT_USER_BASE + 1, 0));
    assert(!user_range_writable(0, 4));
    assert(!user_range_writable(UINT64_MAX - 2, 16));
    assert(user_range_writable(RELIEFNT_USER_BASE, 4096));
    assert(cow_copies == 0);
    assert(user_range_writable(RELIEFNT_USER_BASE + 4090, 12));
    assert(cow_copies == 1);
    assert(!user_range_writable(RELIEFNT_USER_BASE + 8190, 12));
    assert(user_range_ok(RELIEFNT_USER_BASE + 8192, 4));
    assert(!user_range_writable(RELIEFNT_USER_BASE + 8192, 4));
    assert(!user_range_writable(RELIEFNT_USER_BASE + 12288, 4));
    flags[2] = RELIEFNT_PAGE_PROTNONE | RELIEFNT_PAGE_USER;
    assert(!user_range_ok(RELIEFNT_USER_BASE + 8192, 4));
    assert(!user_range_writable(RELIEFNT_USER_BASE + 8192, 4));
    puts("GPU copy-out tests passed: readonly pages, COW, cross-page and overflow ranges");
    return 0;
}
