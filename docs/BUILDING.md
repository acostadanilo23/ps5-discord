# Building ps5-discord

## 1. Host build

The host build exists to validate parsing, REST behavior and navigation data before copying anything to the console.

Ubuntu/Debian/WSL:

```bash
sudo apt update
sudo apt install -y build-essential cmake pkg-config libcurl4-openssl-dev
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure
```

For a one-off host run, provide the credential through the environment instead of putting it in the repository:

```bash
export PS5_DISCORD_TOKEN='...'
./build/ps5-discord-host
```

The host console supports:

- `g <index>`: open a server
- `c <index>`: open a channel
- `s <text>`: send a message
- `r`: refresh the server list
- `p`: print current state
- `q`: quit

The network service refreshes an open chat automatically using short polling. This is intentionally the beta transport; the application code is separated from the transport so a supported realtime transport can replace it later.

## 2. PS5 toolchain prerequisites

Install the prerequisites used by `ps5-homebrew-ui` / `ps5-native-app-boilerplate`:

```bash
sudo apt install -y curl git make ninja-build ccache pkg-config python3 python3-pip python3-venv \
  tar wget unzip clang-18 clang-format-18 clang-tidy-18 lld-18
```

The bootstrap pins `ps5-homebrew-ui` to commit:

```text
bbe19054ee2e7ec069254775273aeb8846c4bd3e
```

That upstream project supplies the native PS5 renderer, DualSense input, fonts, packaging and public homebrew toolchain integration. This repository overlays only the Discord-specific application code.

## 3. Build the PS5 application

```bash
chmod +x scripts/bootstrap-ps5.sh
./scripts/bootstrap-ps5.sh
```

Default development identity:

```text
Title ID: PPSA99251
Name:     PS5 Discord
Category: Media
```

Use a different title ID if that ID is already present on your console:

```bash
TITLE_ID=PPSA12345 ./scripts/bootstrap-ps5.sh
```

The completed package is copied to:

```text
dist/<TITLE_ID>/
dist/<TITLE_ID>.zip
```

Keep the title ID stable after first installation so the app keeps the same writable `/download0` area.

## 4. Session file on the PS5

The beta intentionally does not compile a credential into `eboot.bin`.

Create this file in the application's writable data area:

```text
/download0/ps5-discord/token.txt
```

The file contains only the credential followed optionally by a newline. Never commit it to Git. The client reads it at startup and keeps the value only in process memory.

If the file is missing, the PS5 UI shows the expected path. After creating it, press **Triangle** to retry.

## 5. Controller mapping

Normal view:

- D-pad Up/Down: move selection
- D-pad Left/Right: switch Servers / Channels / Chat panes
- Cross: open selected server or channel
- Square: compose a message while the Chat pane is active
- Triangle: refresh

Compose view:

- D-pad: move around the keyboard
- Cross: type selected character
- Square: backspace
- Triangle: insert a space
- Options: send
- Circle: cancel

## 6. Beta limitations

Current beta target intentionally excludes:

- voice/video
- screen sharing
- calls
- rich media rendering
- GIF search
- server administration
- automatic actions
- client fingerprint spoofing or enforcement-evasion logic

Text chat, attachments metadata, servers, channels, sending, controller navigation and automatic chat refresh are in scope.

## 7. Discord compatibility

This is an experimental personal client. Discord can change undocumented user-client behavior without notice, and using a normal user credential from an alternative client can violate Discord's terms. The project does not attempt to disguise itself as an official client.
