#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
UI_REPO="https://github.com/blackbearreloaded/ps5-homebrew-ui.git"
UI_COMMIT="bbe19054ee2e7ec069254775273aeb8846c4bd3e"
WORK="$ROOT/.build/ps5-homebrew-ui-$UI_COMMIT"
TITLE_ID="${TITLE_ID:-PPSA99251}"
APP_NAME="${APP_NAME:-PS5 Discord}"

case "$TITLE_ID" in
  PPSA[0-9][0-9][0-9][0-9][0-9]) ;;
  *) echo "TITLE_ID must look like PPSA12345" >&2; exit 2 ;;
esac

mkdir -p "$ROOT/.build"
if [[ ! -d "$WORK/.git" ]]; then
  git clone "$UI_REPO" "$WORK"
  git -C "$WORK" checkout --detach "$UI_COMMIT"
fi

mkdir -p "$WORK/src/ps5discord/discord" "$WORK/src/ps5discord/voice" "$WORK/src/ps5discord/audio"
cp "$ROOT/include/ps5discord/json.hpp" "$WORK/src/ps5discord/json.hpp"
cp "$ROOT/include/ps5discord/config.hpp" "$WORK/src/ps5discord/config.hpp"
cp "$ROOT/include/ps5discord/discord/models.hpp" "$WORK/src/ps5discord/discord/models.hpp"
cp "$ROOT/include/ps5discord/discord/rest_client.hpp" "$WORK/src/ps5discord/discord/rest_client.hpp"
cp "$ROOT/include/ps5discord/discord/service.hpp" "$WORK/src/ps5discord/discord/service.hpp"
cp "$ROOT/include/ps5discord/voice/protocol.hpp" "$WORK/src/ps5discord/voice/protocol.hpp"
cp "$ROOT/include/ps5discord/audio/opus.hpp" "$WORK/src/ps5discord/audio/opus.hpp"
cp "$ROOT/include/ps5discord/audio/device.hpp" "$WORK/src/ps5discord/audio/device.hpp"
cp "$ROOT/src/config.cpp" "$WORK/src/ps5discord/config.cpp"
cp "$ROOT/src/discord/rest_client.cpp" "$WORK/src/ps5discord/discord/rest_client.cpp"
cp "$ROOT/src/discord/service.cpp" "$WORK/src/ps5discord/discord/service.cpp"
cp "$ROOT/src/voice/protocol.cpp" "$WORK/src/ps5discord/voice/protocol.cpp"
cp "$ROOT/src/audio/opus.cpp" "$WORK/src/ps5discord/audio/opus.cpp"
cp "$ROOT/src/audio/device.cpp" "$WORK/src/ps5discord/audio/device.cpp"
cp "$ROOT/overlay/src/concepts/ps5discord.cpp" "$WORK/src/concepts/ps5discord.cpp"
cp "$ROOT/overlay/src/concepts/concepts.hpp" "$WORK/src/concepts/concepts.hpp"
cp "$ROOT/overlay/src/concepts/registry.cpp" "$WORK/src/concepts/registry.cpp"

cat > "$WORK/.env" <<'ENV'
PACBREW_PACKAGES=libcurl openssl opus
APP_WRAP_SYMBOLS=fcntl
APP_DEFINITIONS=PS5_DISCORD_PS5=1
APP_IMPORT_STUBS=.deps/ps5-discord/libSceAudioIn.so
ENV

make -C "$WORK" init TITLE_ID="$TITLE_ID" APP_NAME="$APP_NAME" APP_CATEGORY=media CONTENT_SUFFIX=PS5DISCORD000001
bash "$WORK/tools/setup-native-dependencies.sh" >/dev/null
mkdir -p "$WORK/.deps/ps5-discord"
cc -O2 -fPIC -c "$ROOT/overlay/tooling/audio_in_stub.c" -o "$WORK/.deps/ps5-discord/audio_in_stub.o"
"$WORK/.deps/native/ps5-payload-sdk/bin/prospero-lld" --shared -soname libSceAudioIn.prx \
  -o "$WORK/.deps/ps5-discord/libSceAudioIn.so" "$WORK/.deps/ps5-discord/audio_in_stub.o"
make -C "$WORK" "$@"

mkdir -p "$ROOT/dist"
cp -a "$WORK/dist/." "$ROOT/dist/"

echo "Build complete: $ROOT/dist/$TITLE_ID/"
