# Third-party notices and scope of licensing

The MIT license in `LICENSE` applies **only to the original ViPER for Linux
application and integration code**. It does not relicense third-party source
code or assets.

## ViPERDSP

- Source: https://github.com/likelikeslike/ViPERDSP
- Pinned commit: `6fc1f1b8b2123c8cc49f3b3c7c7d5fbbf8a60933`
- Located in `third_party/ViPERDSP` as a Git submodule in development checkouts.
- Upstream README identifies the original authors and reverse-engineering
  contributors; its legal notice restricts the project to personal,
  noncommercial use and states that the referenced ViPERFX_RE project is
  unlicensed/all-rights-reserved.

No license or redistribution permission for ViPERDSP is granted by this
repository's MIT license. Distribution of the combined binary or a unified
archive containing ViPERDSP must not be assumed permitted. Clarify all
applicable permissions before publicly releasing those artifacts.

The local three downstream build corrections are applied to a temporary copy
of the upstream sources; the Git submodule remains unchanged.
