# Analog Clock (C++ / OpenGL / GLFW)

## Open and run

1. Install Visual Studio 2026 with **Desktop development with C++** and a Windows SDK (toolset v145).
2. Open `analogClockProject.slnx`.
3. Select **Debug / x64** and press F5, or **Release / x64** and Ctrl+F5.

GLFW 3.4 headers and Windows libraries are included under `third_party/glfw`.
No user-specific library paths or separate dependency downloads are needed.
Both x64 and x86 (Win32) configurations are supported.

## Build and distribute

From PowerShell in this folder:

```powershell
.\build.ps1 -Package
.\build.ps1 -Platform x86 -Package
```

The packages are written to `dist/AnalogClock-x64.zip` and `dist/AnalogClock-x86.zip`.
Extract the whole ZIP on the target Windows machine and open `analogClockProject.exe`.
Keep `glfw3.dll`, `logo.jpeg`, and `GLFW-LICENSE.md` with the executable.
Release builds use a static C++ runtime; no separate Visual C++ runtime installation is required.
The target computer needs an OpenGL-capable graphics driver.

This is a Windows desktop app. Deploy by distributing its ZIP; ordinary web hosting cannot run this executable.
Press **Escape** or **Q** to exit. The clock displays the computer's local date and time.
Drag with the left mouse button to rotate the clock all the way around. Press **B** to flip between the front and rear, or **R** to reset the view.
Use the buttons on the left to add or subtract hours, minutes, and seconds.
The clock continues ticking at the adjusted time for the current session.

## Repairs

- Replaced the original author's absolute GLFW include/library paths with repository-relative paths in every configuration.
- Included architecture-matched GLFW libraries and automatic copying of the runtime DLL and logo.
- Fixed the stale logo reference and resolve the image beside the executable, including Unicode folder paths.
- Kept output and intermediate files separate for each configuration and platform.
- Included a repeatable build/package command and the GLFW license.

GLFW download source: https://www.glfw.org/download and https://github.com/glfw/glfw/releases/tag/3.4.

## Local Visual Studio debug-symbol workaround

This machine's linker reports LNK1101 (incorrect MSPDB140.DLL version) when
generating PDB debug symbols, although the installed tool file versions match.
The project therefore defaults `EnableNativeDebugSymbols` to `false` and embeds
compiler symbols in object files. Debug builds retain unoptimized code but do
not support normal source-level stepping without a linked PDB.
Repair/update the Visual Studio C++ tools, then enable linked debug symbols with
`/p:EnableNativeDebugSymbols=true` when invoking MSBuild. Release packaging does
not require PDB files. Whole-program optimization is disabled to avoid its PDB dependency.

For a noninteractive rendering/asset check, run the built executable with
`--smoke-test` from any working directory. It creates a hidden OpenGL window,
renders three frames, checks the logo and OpenGL errors, and exits with code 0 on success.

## Browser version

Open docs/index.html for the browser clock, or publish the docs folder with GitHub Pages.
See PUBLISH-ON-GITHUB.md for step-by-step publishing instructions.
Run node test-clock.cjs to check clock behavior. No packages are required.
