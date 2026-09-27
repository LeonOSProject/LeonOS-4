#!/bin/sh
# Release guard for the ntclks kernel submodule (design "Git 与发布流程"):
# release artifacts must come from a clean kernel checkout whose HEAD is
# exactly the gitlink recorded in the parent's HEAD commit. Development
# builds (`make kernel`, `make all`) are deliberately not gated: a dirty
# submodule is allowed there.
#
# usage: ntclks-release-guard.sh <repo-root> <ntclks-checkout> <gitlink-path>
#   repo-root      parent repository root (where `git ls-tree HEAD <path>` runs)
#   ntclks-checkout the kernel checkout the build uses (NTCLKS_DIR)
#   gitlink-path   submodule path recorded in the parent index/HEAD
set -u
LC_ALL=C
export LC_ALL

repo_root=${1:?usage: ntclks-release-guard.sh <repo-root> <ntclks-checkout> <gitlink-path>}
checkout=${2:?usage: ntclks-release-guard.sh <repo-root> <ntclks-checkout> <gitlink-path>}
gitlink_path=${3:?usage: ntclks-release-guard.sh <repo-root> <ntclks-checkout> <gitlink-path>}

note() {
    printf 'ntclks release guard: %s\n' "$1" >&2
    shift
    for line in "$@"; do
        printf '       %s\n' "$line" >&2
    done
}

# 1. Uninitialized submodule: an uninitialized kernel/ntclks is an empty
# directory without its own .git (git -C would silently walk up into the
# parent repository, so the .git marker is what we test).
if [ ! -e "$repo_root/$gitlink_path/.git" ]; then
    note "$gitlink_path is not initialized (no .git in $repo_root/$gitlink_path)" \
        'release builds must come from the committed submodule:' \
        'run: git submodule update --init --recursive && make -C kernel/ntclks fetch'
    exit 1
fi

# 2. Dirty checkout: any modification or untracked file refuses a release.
dirty=$(git -C "$repo_root/$gitlink_path" status --porcelain)
if [ -n "$dirty" ]; then
    count=$(printf '%s\n' "$dirty" | wc -l)
    note "$gitlink_path working tree is dirty ($count entries); release builds require a clean submodule" \
        "HEAD=$(git -C "$repo_root/$gitlink_path" rev-parse HEAD)" \
        "gitlink=$(git -C "$repo_root" ls-tree HEAD "$gitlink_path" | awk 'NR == 1 {print $3}')" \
        "see: git -C $gitlink_path status --porcelain"
    exit 1
fi

# 3. The build's checkout must also be clean when it is not the submodule
#    itself (NTCLKS_DIR pointed at an external kernel checkout).
if [ "$checkout" != "$repo_root/$gitlink_path" ]; then
    ext_dirty=$(git -C "$checkout" status --porcelain 2>/dev/null)
    if [ -n "$ext_dirty" ]; then
        count=$(printf '%s\n' "$ext_dirty" | wc -l)
        note "NTCLKS_DIR checkout $checkout is dirty ($count entries); release builds require a clean kernel checkout" \
            "HEAD=$(git -C "$checkout" rev-parse HEAD 2>/dev/null || echo unknown)"
        exit 1
    fi
fi

# 4. Gitlink match: the submodule must sit exactly on the commit the parent
#    recorded. Both SHAs are named so a mismatch is diagnosable at a glance.
gitlink=$(git -C "$repo_root" ls-tree HEAD "$gitlink_path" | awk 'NR == 1 {print $3}')
head=$(git -C "$repo_root/$gitlink_path" rev-parse HEAD)
if [ -z "$gitlink" ]; then
    note "no gitlink recorded at $gitlink_path in HEAD" \
        "HEAD=$head" \
        'commit the submodule pin before releasing'
    exit 1
fi
if [ "$head" != "$gitlink" ]; then
    note "gitlink mismatch at $gitlink_path: gitlink=$gitlink HEAD=$head" \
        "the submodule is at $head but the parent records $gitlink" \
        "run: git submodule update --init --recursive   # to sync to the gitlink" \
        "or commit the new pin: git add $gitlink_path"
    exit 1
fi

exit 0
