# Use code-video in DIY Electronics with AI

This project vendors `@global/code-video` version `0.1.0` from the
[PatSnap SkillHub registry](https://skillhub.patsnap.info). The package contains
instructions only: it does not include scene modules or rendering scripts.
Build those files in the target project when a media task needs them.

Invoke `$code-video` for an animation explaining a project, feature or usage
flow. The agent draws frames with Canvas2D, synthesizes audio, and renders MP4
with Playwright and ffmpeg. A still-frame export can produce a PNG cover,
diagram or storyboard image (代码绘制静帧). This skill does not provide AI image
generation; use the available image-generation tool for that capability.

For example: “Use $code-video to make a 20-second animation explaining this
demo's verified data flow, and export a cover PNG.”

## Choose the destination before writing

For media belonging to this lab, execute this skill's `scripts/repo-root.sh`.
It resolves the checkout through the skill's real location, follows the local
`publication.developmentRoot` setting, and rejects publication staging as a
generation destination. A missing private-development setting blocks media
creation in staging; it does not prevent installing or reading this skill.

If the user explicitly names another target project, use that project's
authorized destination. Read its `AGENTS.md`, README, manifest and validation
evidence before writing. Do not redirect an explicit target to the lab.

Keep scene sources, rendering tools and reviewed small media with the target
project, for example under `docs/media/`. Keep dependencies, frame caches and
large raw outputs outside tracked content or in that project's ignored build
directory. Use relative paths and configurable executable/font paths.

Generated illustrations are explanations, not hardware-validation evidence.
Preserve the upstream fact checks and label planned or illustrative behavior.
Follow the target repository's publication policy before public delivery;
installation and rendering do not grant publication approval.

## Runtime prerequisites

- Node.js and the `playwright-core` package in the media working directory.
- An installed system Chrome executable passed via `executablePath`; the
  upstream workflow does not download a browser.
- `ffmpeg` and `ffprobe` on PATH for rendering, muxing and stream checks.
- Fonts available on the target machine. The upstream Chinese layout uses
  macOS PingFang SC; configure an appropriate font on other systems.

Check these prerequisites at the start of an actual generation task. Then
perform the scene, frame, duration and audio-level checks in `SKILL.md`.
Skill installation and discovery checks do not verify media output or sound
quality. The upstream rendering-time estimate is not a benchmark of this setup.

## Installation and provenance

The canonical source is `.agents/skills/code-video/`. The existing `.claude`
compatibility link exposes the same source. The repository installer
automatically includes this skill in its user-level targets:

```sh
./scripts/install-agent-skill.sh --target codex
```

Project-local discovery needs no global copy. Reload the agent if its skill
list was captured before installation. See [source.json](source.json) for the
registry version and original package digests. Local adaptations move the
upstream version into supported frontmatter metadata, add the integration
link, and supply picker metadata and destination routing. The original
workflow remains intact.

The upstream package contains no license file or redistribution grant.
Its presence in this local checkout does not establish redistribution rights
under the repository's MIT license. Keep the new skill outside public exports
until its rights review and exact publication approval are complete. The
existing six-skill release record and approved file scope are unchanged.

SkillHub's `.skillhub/metadata.json` remains ignored local installation state.
Reinstalling with `--force` can replace local adaptations; review an upstream
update separately instead of overwriting the vendored skill in place.
