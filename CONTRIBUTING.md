# Contributing to ExigeOS

Thank you for your interest in contributing!  ExigeOS is primarily an educational project — clarity and didactic value are as important as correctness.

## Philosophy

- **Explain everything.**  Every hardware register, every magic constant, every architectural decision should have a comment explaining *why*, not just *what*.
- **Keep it simple.**  Avoid abstractions that aren't justified by the current feature set.  Three clear lines of code beat a premature helper function.
- **No external dependencies.**  The kernel must remain freestanding (no libc, no POSIX).

## Getting started

1. Fork the repository and clone your fork.
2. Install the build dependencies listed in [README.md](README.md).
3. Build and run to confirm a working baseline: `make && make run`

## How to contribute

### Bug reports

Open an issue and include:
- Your OS and host toolchain versions (`gcc --version`, `nasm --version`, `qemu-system-i386 --version`).
- The exact command you ran and the full output / error message.
- For QEMU issues: the QEMU version and audio backend in use.

### Feature proposals

Open an issue to discuss the feature before writing code.  Good candidates include:
- New shell commands
- Additional hardware drivers (serial port, RTC on RPi3 via SPI)
- A second solfège octave or additional note names

### Pull requests

1. Branch from `main`: `git checkout -b feature/my-feature`
2. Keep changes focused — one pull request per feature or fix.
3. Update the relevant source comments if hardware behaviour is affected.
4. Update `README.md` if commands or build instructions change.
5. Ensure the project still builds cleanly on both platforms:
   ```bash
   make clean && make PLATFORM=x86
   make clean && make PLATFORM=rpi3
   ```
6. Open the pull request against `main`.

## Code style

- C99 (`-std=gnu99`), 4-space indentation, no trailing whitespace.
- Comments in English only.
- Keep source lines under 100 characters where practical.
- Static functions for all helpers not exposed through a header.

## Questions?

Open a [discussion](../../discussions) or an issue labelled `question`.
