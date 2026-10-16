# Security

These tools load code into the game (`version.dll`, `.asi` mods) and edit save files, so security reports
are taken seriously.

## Reporting a problem
Please report security issues privately, not in a public issue: use GitHub's
**Security > Report a vulnerability** on this repository. Describe the problem, the affected file and
version, and how to reproduce it.

## Verifying downloads
- Official downloads are only on this repository's Releases page and the author's Nexus Mods pages.
- The released files are built from this source with `scripts\build.bat` and packed with
  `scripts\package.ps1`. You can build them yourself and compare.
- Antivirus heuristics sometimes flag DLL loaders and unsigned tools. If in doubt, build from source.
