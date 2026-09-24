# EV Engine Sound publication boundary

Follow the repository rules and `docs/open-source-boundary.md`.

- Keep the sound core, generic control interface, public board support, and standalone demo
  reproducible without private dependencies. Vehicle targets are not validated support.
- New proprietary vehicle decoders, PID/CAN mappings, model-specific calibration, compatibility
  workarounds, vehicle gateway designs, commercial configuration, and derived firmware belong
  under this project's ignored `private/` directory or a separate private repository outside
  this checkout. Do not put these implementations in the public `core/` or `firmware/` tree.
- Raw vehicle captures, identifiers, credentials, and private test evidence remain private.
  Public validation notes may contain only sanitized conclusions within the intended public scope.
- Do not force-add ignored private files, add them as public submodules, or include them in EDA
  exports, release archives, screenshots, videos, or build artifacts intended for publication.
  Git ignore rules do not protect non-Git uploads or code already tracked by Git.
- Keep existing public-source research separate from proprietary implementations. Do not delete
  research or rewrite Git history merely to establish this boundary.
- Default public builds must work with `private/` absent. Adding a private integration requires
  an explicit opt-in build path and a separate output location; no such integration exists yet.
- Preserve existing and third-party licenses. A private directory is not a license exception.
