#!/usr/bin/env bash
# Opt-in renderer investigation. Use from Steam launch options, not as a Proton
# replacement. Preserves Steam's complete runtime command and executable rewrite.
set -euo pipefail

renderdoc="${BO3_RENDERDOC_DIR:-$HOME/.cache/bo3-splitscreen-build/renderdoc/renderdoc_1.47}"
captures="${BO3_CAPTURE_DIR:-$HOME/.local/share/bo3-splitscreen-captures}"
if [[ ! -x "$renderdoc/bin/renderdoccmd" ]]; then
    printf 'RenderDoc missing: %s\n' "$renderdoc/bin/renderdoccmd" >&2
    exit 1
fi
if [[ "${1:-}" == "--check" ]]; then
    "$renderdoc/bin/renderdoccmd" version
    "$renderdoc/bin/renderdoccmd" vulkanlayer --explain
    printf 'Captures: %s\nNo game launched.\n' "$captures"
    exit 0
fi
if [[ "${1:-}" == "--" ]]; then shift; fi
if [[ $# == 0 ]]; then
    printf 'Use this script from Steam launch options with %%command%%, or --check.\n' >&2
    exit 2
fi

args=("$@")
game=""
for i in "${!args[@]}"; do
    arg="${args[$i]}"
    if [[ "${arg##*/}" == "BlackOps3.exe" ]]; then
        game="${arg%/*}"
        args[$i]="$game/boiii.exe"
    elif [[ "${arg##*/}" == "boiii.exe" ]]; then
        game="${arg%/*}"
    fi
done
if [[ -z "$game" || ! -f "$game/boiii.exe" || ! -f "$game/BlackOps3.exe" ]]; then
    printf 'No valid BO3 executable in Steam command; refusing direct/guessed launch.\n' >&2
    exit 2
fi
mkdir -p "$captures"
printf 'RenderDoc capture session: OFFLINE ONLY. F12/Print Screen captures a frame.\n' >&2
printf 'Captures stay local in %s. Remove this launch option after testing.\n' "$captures" >&2
cd "$game"
# Let the registered Vulkan layer load inside DXVK. Injecting into Steam's
# supervisor with renderdoccmd instead can make Steam lose child ownership.
export ENABLE_VULKAN_RENDERDOC_CAPTURE=1
export RENDERDOC_CAPFILE="$captures/bo3"
exec "${args[@]}" -nointro