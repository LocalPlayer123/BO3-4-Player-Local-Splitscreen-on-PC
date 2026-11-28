# Temporary offline rendering capture

This is investigation tooling, **not** a low-fidelity implementation. It does not
install or enable the lens-flare prototype. RenderDoc captures may contain game
resources and visible names; keep them local and out of Git/public PRs.

## Prepared tooling

Official portable Linux RenderDoc 1.47 is installed in the user's build cache.
The detached archive signature was checked against the publisher fingerprint:

1B03 9DB9 A471 8A2D 699D E031 AC61 2C31 20C3 4695

The Vulkan layer is registered at user scope. Registration alone does not enable
capture; the wrapper opts in using ENABLE_VULKAN_RENDERDOC_CAPTURE and sets
RENDERDOC_CAPFILE. It directly executes Steam's original supervisor command:
wrapping the supervisor in renderdoccmd caused Steam to lose game ownership.
No graphics settings, game DLLs, or system packages were changed by setup.
Steam launch options were temporarily changed with user authorization and backed
up; restore the original option after the session.

## Capture procedure

1. Keep a copy of the current BO3 Steam launch options before changing them.
2. Temporarily replace the launch options with the absolute path to
   tools/capture_steam.sh in double quotes, followed by a space and %command%.
   This script replaces the existing bash executable-rewrite launch option;
   do not combine both wrappers. It preserves Steam's original runtime command,
   rewrites BlackOps3.exe to boiii.exe, and sets the actual game working directory.
3. Launch through Steam as usual. **Do not launch boiii.exe directly.**
4. Stay offline. Verify a RenderDoc overlay is present before proceeding. If it
   is absent, stop: successful tooling setup does not prove Proton injection.
5. Start a stock map with four local players. Once all panes are visible, press
   F12 or Print Screen once. Steam may also take a screenshot on F12. Expect a
   substantial pause, memory use, and potentially a large capture.
6. Exit normally and restore the original Steam launch options. Never use capture
   injection in public/online play. Do not compare normal FPS to captured FPS.

Captures are written under the user's local-share bo3-splitscreen-captures
directory. Open the capture in the same RenderDoc build for Vulkan event,
viewport, color/depth target, and final-composite inspection. Capturing through
Proton observes DXVK/Vulkan, not the original D3D11 calls directly.

## Required analysis

- Confirm capture includes the actual game rather than just a launcher.
- Identify each pane's geometry, depth and screen-space effect passes.
- Trace scene color through final upscale/composition into the full-size pane.
- Identify source UVs, per-pane boundaries, temporal resources, and HUD ordering.
- Map any reduced-quality choices to per-view state, not shared hardware options.

The overlay and loaded capture library were confirmed in the actual game process.
A four-player capture is still blocked by unavailable controllers. No selective
render-scaling change has been validated yet.