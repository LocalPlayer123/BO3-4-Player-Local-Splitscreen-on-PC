# Per-view performance mode: investigation, not an implemented feature

## Requested behavior

Lower internal scene resolution and graphics quality for local clients 2/3
(players 3/4), while preserving players 1/2, full-size pane destinations, HUD,
gameplay entities, and correct depth/temporal rendering. Default off.

The uncommitted lens-flare-only prototype does not meet this requirement. It has
not been deployed or pushed to the draft PR. No meaningful performance gain has
been measured.

## Verified stock option scope

Stock Lua at KingslayerKyle/T7LuaRepo revision
5f45af23887a1d850fba748c45a16ab746da3600, PC_Ship_2025-05-31:

- ui/T7/Utility/PCUtility.lua routes user options to
  Engine.SetProfileVar(controller, key, value), but hardware options to
  Engine.SetHardwareProfileValue(key, value), without a controller argument.
- StartMenu_Options_Graphics_PC uses hardware options for r_picmip,
  r_modelLodLimit, r_aaTechnique, r_ssaoTechnique, and r_motionBlurMode.
- r_sceneBrightness is a user option, but is not a performance control.

Thus applying the ordinary Low options through the menu API cannot isolate
players 3/4. This does not prove that custom per-view engine hooks are impossible.
Do not temporarily change global dvars around a draw: resource allocation and
asynchronous rendering can outlive that scope.

## Offline renderer findings

Addresses are RVAs for checksum 0x06531394, image base 0x140000000. Code and
exception data were available in a cached decrypted memory-layout image; relevant
initialized data and render-target behavior tables were not available.

- Existing pane layout setup (0x010F4E20, 0x010F6795) feeds the same rectangle
  into screen placement (0x013E5600 / 0x013E5B20). Shrinking pane geometry would
  also shrink/reposition the HUD, not upscale a smaller scene into its old pane.
- Renderer setup at 0x01CE3060 maintains separate scene and display rectangles.
  The dimension-ratio-converted rectangle is copied into view +0x290..+0x29C;
  the original rectangle into +0x2A0..+0x2AC, with another converted copy at
  +0x2B0..+0x2BC. Local-client identity is copied to +0x398.
- Conversion uses frame-wide dimension pairs at 0x0F464CB0/+4 and
  0x0F464CBC/+4. These are not verified per-client render-scale settings.
- The converted rectangle reaches command-state viewport fields through
  0x01CF5F00 and D3D11 RSSetViewports through 0x01CF4384.
- Postprocessing at 0x01C64700 also uses the converted rectangle. Target-binding
  behavior depends on an unavailable table at 0x02F80100. A final reduced-scene
  source-to-unchanged-pane upscale, including UVs/filtering, was not established.

## Implementation gate

Do not expose a working-mode toggle until these are verified:

1. Scene color/depth/screen-space/history resource ownership for each view.
2. Correct reduced source rectangle and unchanged destination composite,
   including UVs, filtering, pane boundaries, and HUD ordering.
3. Per-view equivalents of selected quality options, without changing shared
   resources used by players 1/2.
4. Byte-guarded hook sites, failure containment, and state reset across maps.
5. Live image correctness and measured GPU/frame-time changes with mode off/on.

The current offline evidence is insufficient to implement that contract safely.
The next investigation needs initialized render-target metadata and a traced
composite path. A Steam launch with RenderDoc was verified in the actual game
process, but a four-player capture is blocked by unavailable controllers. Normal
Steam launch options have been restored; no render-scaling change was deployed.