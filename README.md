# Description
An osu! Bongo Cat overlay with smooth paw movement and simple skinning ability, written in C++. Originally created by [HamishDuncanson](https://github.com/HamishDuncanson).

You can find how to configure the application in our [wiki](https://github.com/kuroni/bongocat-osu/wiki/Settings).

Download the program [here](https://github.com/kuroni/bongocat-osu/releases).

Hugs and kisses to [CSaratakij](https://github.com/CSaratakij) for creating the Linux port for this project!

Any suggestion and/or collaboration, especially that relating to sprites, is welcomed! Thank you!

[Original post](https://www.reddit.com/r/osugame/comments/9hrkte/i_know_bongo_cat_is_getting_old_but_heres_a_nicer/) by [Kuvster](https://github.com/Kuvster).

## Two-PC streaming setup

If your game and streaming machines are separate PCs, `bongo.exe` alone can't
help: it captures input on whichever PC it runs on, but you need the animation
on the *other* PC for OBS to capture it. Use the **`bongo-server` +
`bongo-client`** pair instead.

- **Game PC** runs `bongo-server.exe` — captures keyboard + mouse + the
  foreground osu! window rect, just a small console window.
- **Streaming PC** runs `bongo-client.exe` — draws the bongo cat. OBS captures
  this window.

Each PC needs the relevant `.exe` plus its `settings.txt`. The streaming PC
also needs the `config.json`, `font/`, and `img/` runtime assets (i.e. the
same files `bongo.exe` reads — drop the `.exe` next to them).

### How it works

72-byte UDP state packets at 120 Hz (~70 kbit/s). Sub-millisecond LAN latency.
Each packet is a full state snapshot (256 VK keys + cursor + screen size +
osu! window rect), so dropped packets are harmless — the next one (8 ms
later) overwrites it.

### 1. Find your streaming PC's LAN IP

On the **streaming PC**, press `Win + R`, type `cmd`, run `ipconfig`. Under
your active adapter, note the **IPv4 Address** (e.g. `192.168.0.1`)

### 2. Configure the server

On the **game PC**, edit `server settings.txt` and set `client ip:` to the IP
from step 1. The other defaults (`client port: 47812`, `send hz: 120`) work
as-is.

### 3. Configure the client

The defaults in `client settings.txt` work as-is. Optional: set
`allowed server ip:` to your game PC's LAN IP to reject packets from any
other source (default `any` accepts from anywhere on the LAN — fine for a
trusted home network).

If you change `listen port` on the client, make `client port` on the server
match.

### 4. Run both

1. Streaming PC: launch `bongo-client.exe`. Idle bongo cat appears.
2. Game PC: launch `bongo-server.exe`. Console shows `sending to <ip>:<port>`.
3. Type or move on the game PC — bongo cat animates on the streaming PC.
4. In OBS, add a **Window Capture** source for the `BONGO CLIENT` window.

### 5. Firewall

First launch of `bongo-client.exe`: Windows Defender prompts. Click **Allow
access** on **Private networks only** (don't tick Public).

Missed the prompt? Control Panel → Windows Defender Firewall → Allow an app →
add `bongo-client.exe`, tick **Private** only.

The game PC sends outbound only and needs no firewall rule.

## Changing the skin

The overlay loads its artwork from the `img/osu` folder. To use a custom skin,
replace the PNG files in that folder with your own images while keeping the same
filenames.

## Keybindings and config

The included config.json sets 2560x1440 resolution and binds all keys. The F1 key
adds smoke.

### Mouse paw color (osu! mode)

The mouse arm is drawn procedurally, so its colors come from the `osu` block in
`config.json` rather than from a PNG. Defaults give a yellow arm with a green
tip, mimicking the keyboard hand sprites.

| Key | Type | Meaning |
| --- | --- | --- |
| `paw` | `[r, g, b]` or `[r, g, b, a]` | Fill color of the arm (the shoulder-side section). |
| `pawEdge` | `[r, g, b]` or `[r, g, b, a]` | Outline color of the arm. |
| `pawTip` | `[r, g, b]` or `[r, g, b, a]` | Fill color of the tip (the cursor-side section). Omit to reuse `paw`. |
| `pawEdgeTip` | `[r, g, b]` or `[r, g, b, a]` | Outline color of the tip. Omit to reuse `pawEdge`. |
| `pawTipLength` | number `0.0`–`1.0` | Fraction of the arm covered by the tip color, measured from the cursor end. `0` disables the tip; `1` makes the whole arm the tip color. |

The boundary between `paw` and `pawTip` is a hard line, not a gradient — the
strip is subdivided at the threshold so each segment renders as a solid color.
Note the casing exactly: `pawTip`, `pawEdgeTip`, `pawTipLength` (capital T).

Example block:

```json
"osu": {
    "mouse": true,
    "toggleSmoke": false,
    "paw":          [255, 179, 35],
    "pawEdge":      [0, 0, 0],
    "pawTip":       [154, 152, 32],
    "pawEdgeTip":   [0, 0, 0],
    "pawTipLength": 0.60,
    "key1": [90],
    "key2": [88],
    "smoke": [67],
    "wave": []
}
```

After editing `config.json`, press **Ctrl + R** with the bongo window focused to
reload without restarting.

## Security

The setup opens **one UDP port (default 47812) on the streaming PC**.

- Server only sends; game PC has zero new attack surface.
- Client validates magic/version/size and silently drops anything else. Valid
  packets can only nudge the on-screen bongo cat — no code path to disk,
  network, or execution.
- Drain-loop cap: at most 256 packets per frame, then rendering continues.
- Set `allowed server ip:` for a source-IP allowlist.
- Allow on **Private** firewall profile only. **Never port-forward 47812.**

| Concern | Risk | Mitigation |
| --- | --- | --- |
| Program on the streaming PC crashing bongo-client | very low | fixed-size packets, strict validation, drain cap |
| LAN peer spoofing packets to make bongo cat animate weirdly | low (cosmetic) | source-IP allowlist |
| Public-internet attacker | low | no port forwarding, Private profile |
| Passive LAN observer reading keystrokes | low-moderate on **untrusted Wi-Fi** | packets are unencrypted "keys currently held" bitmaps — a coarse side channel, not text capture. Don't use a 2-PC setup on hostile networks (cafe Wi-Fi, dorm LAN). |

For typical home use the residual risk is essentially zero.

## Troubleshooting

- **Bongo cat doesn't move.**
  - Confirm both PCs share a subnet (first three numbers of the IPv4 match).
  - Confirm the server console shows the startup "sending to ..." line.
  - Re-check the firewall step.
  - Set `client ip: 127.0.0.1` and run both on one PC. If that works, the
    problem is networking (firewall / wrong IP / different subnet).
- **Paw sits slightly off in osu! mode.** Expected if the two PCs have
  different resolutions — the client tracks *relative* position, not absolute
  pixels. The server forwards the foreground osu! window rect so letterboxing
  stays accurate.
- **`bind() to port 47812 failed`.** Another program is using the port.
  Change `listen port` (client) and `client port` (server) to a matching free
  port between 1024 and 65535.

## Further information
In order to play with fullscreen on Windows 10, run both osu! and this application in Windows 7 compability mode.

Press Ctrl + R to reload configuration and images (will only reload configurations when the window is focused).

Supported operating system:
* Windows
* Linux (tested with Arch Linux with WINE Staging 5). Note: You **must** use WINE Staging, because for whatever reason on stable WINE bongocat-osu doesn't register keyboard input from other windows.

_Notice_: If you're using WINE on Linux, make sure that osu! and this application run in the same `WINEPREFIX`.

## For developers
This project uses [SFML](https://www.sfml-dev.org/index.php) and [JsonCpp](https://github.com/open-source-parsers/jsoncpp). JsonCpp libraries are directly included in the source using the provided `amalgamation.py` from the developers.

### Libraries and dependency

#### Windows and MinGW
To build the source, download the SFML libraries [here](https://www.sfml-dev.org/index.php), copy `Makefile.windows` to `Makefile`, then replace *`<SFML-folder>`* in `Makefile` with the desired folder.

#### Linux
You need to have these dependencies installed. Check with your package manager for the exact name of these dependencies on your distro:
- g++
- libxdo
- sdl2
- sfml
- x11
- xrandr

Then, copy `Makefile.linux` to `Makefile`.

### Building and testing
To build, run this command from the base directory:

```sh
make
```

To test the program, run this from the base directory:

```sh
make test
```

Alternatively, you can copy the newly-compiled `bin/bongo.exe` or `bin/bongo` into the base directory and execute it.

If you have troubles compiling, it can be due to version mismatch between your compiler and SFML. See [#43](https://github.com/kuroni/bongocat-osu/issues/43) for more information.

