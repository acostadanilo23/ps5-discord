# ps5-discord

Experimental Discord client for a jailbroken PlayStation 5.

## Status

Target: **beta**, not a proof of concept. The project is being structured around a usable text-chat client with persistent session state, guild/channel navigation, message history, sending, realtime Gateway events, reconnect/backoff, rate-limit handling, and a controller-first UI.

The PS5 build is based on the public `ps5-native-app-boilerplate` / `ps5-homebrew-ui` ecosystem and does not require proprietary Sony SDK files.

## Important Discord note

This repository is for personal experimental use. It does **not** include techniques for impersonating/fingerprinting the official Discord client or bypassing Discord enforcement. A user-token backend may violate Discord's terms even when operated manually. Use at your own risk.

## Planned beta feature set

- Persistent local auth/session configuration
- Current-user profile
- Guild list
- Text-channel list
- Message history with pagination
- Send text messages manually
- Gateway WebSocket event stream
- Heartbeat, reconnect, exponential backoff and resume state
- Basic replies/reactions display
- Image/attachment metadata and URLs
- Controller-first three-column Discord-style UI
- On-screen text entry
- Host build/test path before PS5 deployment
- PS5 package output suitable for ShadowMountPlus-style deployment

Voice is intentionally out of the first beta.

## Build architecture

The repository keeps application/network code independent from the PS5 toolchain. `scripts/bootstrap-ps5.sh` creates a pinned PS5 build workspace from the public homebrew toolchain and overlays this project's sources.

More detailed build and deployment instructions will live in `docs/BUILDING.md` as the beta is assembled.
