# Contributing

Thanks for your interest!

- **Bug reports and suggestions** are welcome as GitHub issues. Please use the issue templates and
  attach the relevant log (`mods\ModLoader.log`, `mods\SkillUnlock.log`) and your
  game version.
- **Code changes:** this project is source-available under a custom licence (see LICENSE), not an
  open-source licence. Please open an issue before writing code. Pull requests can only be accepted if you
  agree that your contribution may be distributed under the project's licence.
- **Saves:** never attach your save files to a public issue. They are tied to your Steam account.

## Development
- Build: `scripts\build.bat` (Visual Studio 2019/2022 C++ x64 tools; the save tool uses Windows' built-in
  C# compiler). Warnings are treated as errors.
- Package: `powershell -ExecutionPolicy Bypass -File scripts\package.ps1` writes the release archives
  to `dist\`.
- Every game address used by the mods is documented in comments at the top of each source file.
