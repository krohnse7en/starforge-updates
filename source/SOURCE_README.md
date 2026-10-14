# StarForge source · v0.20.0

StarForge is a Windows level editor for Starship. Original editor code is MIT licensed. Public tester downloads and the signed update feed remain in this repository's Releases and channels directories.

## Run a development checkout

1. Use Windows 10/11 with Microsoft Edge, .NET Framework 4.8 and Python 3. Prepare a working tester installation with your own supported game copy.
2. Run `python tools/prepare_local_assets.py "C:/path/to/your/StarForge"`. This imports generated data and runtime files into ignored local folders. It does not copy your levels, saves, controller settings, sessions or private key.
3. Run `powershell -ExecutionPolicy Bypass -File tools/build-launcher.ps1`.
4. Open `editor/StarForge.exe`. Development updates are disabled so official updates do not overwrite your local edits.

The editor can show fallback shapes without model data; full libraries, voice preview and original stages require local generated assets. Do not serve this folder directly with an ordinary web server: the launcher supplies workspace/save/update endpoints and config.js.

## Layout

- editor: browser UI, shared tools, local Windows launcher and updater.
- engine: modified Starship runtime/renderer source snapshots; apply them and native-actors.patch to the matching upstream Starship checkout before rebuilding the game. The patch contains the cross-floor Titania compatibility and Rasco cleanup fixes, plus passive solid warp pillars, gates and meteors.
- tools: development launcher build, local asset importer and tester setup source.

The runtime snapshots are based on HarbourMasters/Starship commit 3322643f0bbf80d2f8f7e1c0d2c4c578515f6c90. Other Starship source and dependencies come from upstream; their own build instructions and licenses still apply. The tester download provides a matching compiled runtime for editor development.

## Contributing

Open an issue with reproduction steps and version information, or submit a pull request. Keep both authored and original-stage interfaces consistent. Update the version, beginner Help manual and changelog for delivered changes. Test both startup paths and the actual behavior in isolated copies. Never commit personal workspace files, generated game assets or private signing material.

This source export separates generated audio, dialogue, original movement scripts and group layouts from the editor's JavaScript. The ignored generated/game-data.js restores these datasets locally. Compatibility names/IDs and simple empty examples remain in the source.
