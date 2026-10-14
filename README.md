# ps5-discord

Experimental Discord text client for a jailbroken PlayStation 5.

## Current target: beta

This repository is intentionally beyond a Hello World / MVP. The current beta architecture contains:

- current-user profile loading
- server (guild) list
- text-channel list
- message history
- manual text-message sending
- attachment metadata
- automatic short-poll chat refresh
- basic Discord rate-limit handling
- asynchronous network worker so rendering does not block
- controller-first three-column PS5 UI
- built-in on-screen keyboard
- persistent token file outside `eboot.bin`
- host console for validating the network layer before console deployment
- unit tests for JSON/model parsing
- GitHub Actions host CI
- reproducible PS5 overlay build pinned to `ps5-homebrew-ui`

Voice/video are not part of the first beta.

## Architecture

```text
Discord HTTPS API
       |
       v
RestClient ---- rate-limit handling
       |
       v
Service worker ---- 1.5 s chat polling
       |
       +------------------+
       |                  |
       v                  v
Host console        PS5 UI concept
                      |
               ps5-homebrew-ui
                      |
                ps5-opengl / native app
```

The network/service layer is independent from the PS5 UI so the transport can be replaced later without rewriting the application.

## Host quick start

```bash
sudo apt install -y build-essential cmake pkg-config libcurl4-openssl-dev libopus-dev
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure

export PS5_DISCORD_TOKEN='...'
./build/ps5-discord-host
```

## PS5 build

Install the public homebrew toolchain prerequisites, then:

```bash
chmod +x scripts/bootstrap-ps5.sh
./scripts/bootstrap-ps5.sh
```

The default development title ID is `PPSA99251`. Override it when needed:

```bash
TITLE_ID=PPSA12345 ./scripts/bootstrap-ps5.sh
```

Output is copied to `dist/<TITLE_ID>/` and `dist/<TITLE_ID>.zip`.

Detailed instructions: [`docs/BUILDING.md`](docs/BUILDING.md).

## Authentication on PS5

The credential is deliberately not compiled into the application. The PS5 build reads:

```text
/download0/ps5-discord/token.txt
```

The UI explains this path when no token exists and lets you retry with Triangle.

## Controller mapping

- D-pad: navigate rows / switch columns
- Cross: open server or channel
- Square: compose in an open chat
- Triangle: refresh
- Compose: Cross types, Square deletes, Triangle inserts a space, Options sends, Circle cancels

## Discord / account warning

This is a personal experimental client. It does not include client-fingerprint spoofing, automated actions, spam tooling, or enforcement-evasion logic. It identifies its HTTP traffic as `ps5-discord`.

Using a normal user credential from an alternative client may violate Discord's terms and Discord may change undocumented behavior at any time. Use at your own risk and never commit credentials to this repository.
