# Third-party notices and scope of licensing

The GNU General Public License version 3 only (`GPL-3.0-only`) in `LICENSE`
applies **only to original ViPER4Linux-RE application and integration code
for which its copyright holder can grant that license**.
Copyright (C) 2026 Gerald Frank (`username227`); see `COPYRIGHT`.
It does not relicense third-party source code or assets. Earlier versions
released under MIT remain subject to their original license grants.

## ViPERDSP

- Source: https://github.com/likelikeslike/ViPERDSP
- Pinned commit: `6fc1f1b8b2123c8cc49f3b3c7c7d5fbbf8a60933`
- Located in `third_party/ViPERDSP` as a Git submodule in development checkouts.
- Upstream README identifies the original authors and reverse-engineering
  contributors; its legal notice restricts the project to personal,
  noncommercial use and states that the referenced ViPERFX_RE project is
  unlicensed/all-rights-reserved.

No license or redistribution permission for ViPERDSP is granted by this
repository's GPL-3.0-only license. Distribution of the combined binary or a unified
archive containing ViPERDSP must not be assumed permitted. Clarify all
applicable permissions before publicly releasing those artifacts.

The local three downstream build corrections are applied to a temporary copy
of the upstream sources; the Git submodule remains unchanged.
