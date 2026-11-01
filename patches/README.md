# Patches for the shared toolchain

`tools/core` is the upstream [arcade-decomp](https://github.com/cheviman/arcade-decomp)
submodule pinned at commit `ed4e157b4bb3d34b6555766262c70d6f010ba619` (branch `tmnt`).

`0001-m68000-translate-pea-return.patch` fixes one translation gap: functions that push a
return address with `PEA` and tail-jump to a helper, then continue at the pushed address when
the helper returns. Without it the translated C unwinds the caller instead of resuming at the
pushed label (DoDonPachi's entity code does this at `003478`/`00347e`).

Apply it to a fresh core checkout:

```bash
git -C tools/core apply ../../patches/0001-m68000-translate-pea-return.patch
```

`scripts/build-local.sh` and `scripts/build-local.ps1` apply it automatically when needed.
The patch is intended to be sent upstream. Until it is, the build modifies the submodule's
working tree, not its recorded commit.
