# Game Debug Trace Logger

A DLL hijacking tool that captures debug traces and messages left by game developers. This tool uses XInput DLL replacement to inject into games and capture debug output.

## How it works

This project creates a replacement `xinput1_3.dll` that forwards calls to the original system DLL while intercepting Windows debug APIs to capture debug messages output by the game.

## Features

- Captures OutputDebugString calls (both ASCII and Unicode)
- Shows debug messages in a console window and/or logs to file
- Minimal performance impact on the game
- Configurable filtering to focus on specific debug messages
- Hotkeys to control logging at runtime
- Support for both 32-bit and 64-bit games

## Usage

1. Build the DLL using the provided Makefile (requires MinGW/MSYS2)
2. Copy the resulting `xinput1_3.dll` to the same directory as the game executable
   - Use the x86 version for 32-bit games
   - Use the x64 version for 64-bit games
3. Run the game - a console window will appear showing captured debug messages

## Hotkeys

- Alt+L: Toggle logging on/off
- Alt+I: Show process information

## Configuration

Edit `xinput_logger.cfg` to customize behavior:

## Building

### Prerequisites

- MSYS2 with MinGW-64 toolchain
- XInput development libraries

### Build Instructions

```bash
make