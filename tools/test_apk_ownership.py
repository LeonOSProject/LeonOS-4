#!/usr/bin/env python3
"""Check policy coverage and staging inventory, not APK installation support."""
import hashlib
import json
from pathlib import Path
import tempfile
import unittest

from apk_ownership import ROOT, inventory, load_policy


class OwnershipTests(unittest.TestCase):
    def setUp(self):
        self.policy, self.rules = load_policy(ROOT / "configs/apk-ownership.json",
                                             ROOT / "configs/components.toml")

    def test_inventory_content_and_guest_links(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory) / "root"
            (root / "bin").mkdir(parents=True)
            (root / "usr/bin").mkdir(parents=True)
            (root / "bin/busybox").write_bytes(b"upstream fixture")
            (root / "bin/sh").symlink_to("busybox")
            (root / "usr/bin/sh").symlink_to("/bin/sh")
            (root / "usr/bin/fastfetch").write_bytes(b"custom logo fixture")
            (root / "usr/bin/fastfetch").chmod(0o755)
            (root / "usr/bin/cycle").symlink_to("cycle")
            (root / "unknown").write_bytes(b"unclassified")
            outside = Path(directory) / "outside"
            outside.mkdir()
            (outside / "secret").write_text("must never be inventoried")
            (root / "host-link").symlink_to(outside, target_is_directory=True)
            before = {item["path"]: item for item in inventory(root, self.rules)}
            self.assertEqual(before["/usr/bin/sh"]["group"], "busybox")
            self.assertEqual(before["/usr/bin/fastfetch"]["group"], "leonos-fastfetch")
            self.assertEqual(before["/usr/bin/fastfetch"]["mode"], "0755")
            self.assertEqual(before["/bin/busybox"]["sha256"],
                             hashlib.sha256(b"upstream fixture").hexdigest())
            self.assertEqual(before["/unknown"]["classification"], "fallback-needs-review")
            self.assertFalse(any("secret" in name for name in before))
            self.assertNotIn("/bin", before)
            (root / "bin/busybox").write_bytes(b"updated")
            after = {item["path"]: item for item in inventory(root, self.rules)}
            self.assertNotEqual(before["/bin/busybox"]["sha256"], after["/bin/busybox"]["sha256"])

    def test_invalid_and_ambiguous_policy_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "policy.json"
            for mutation in ("missing", "duplicate", "path", "escape"):
                policy = json.loads(json.dumps(self.policy))
                if mutation == "missing":
                    policy["groups"]["sl"]["components"] = []
                elif mutation == "duplicate":
                    policy["groups"]["sl"]["components"].append("vim")
                elif mutation == "path":
                    policy["groups"]["sl"]["paths"] = ["bin/busybox"]
                else:
                    policy["groups"]["sl"]["paths"] = ["../host"]
                path.write_text(json.dumps(policy))
                with self.subTest(mutation=mutation), self.assertRaises(ValueError):
                    load_policy(path, ROOT / "configs/components.toml")

    def test_policy_is_not_a_fake_database(self):
        self.assertEqual(self.policy["apk_registration"], "not-installed")
        self.assertEqual(self.policy["groups"]["leonos-fastfetch"]["future_apk_conflicts"], ["fastfetch"])
        self.assertFalse((ROOT / "system/rootfs/lib/apk/db/installed").exists())


if __name__ == "__main__":
    unittest.main()
