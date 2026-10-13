# StarForge updates

Public Windows tester builds and signed update feeds for StarForge, the Starship level editor.

## Download

Open [Releases](https://github.com/krohnse7en/starforge-updates/releases) and download the latest Windows tester ZIP. Extract the whole folder, then open Setup.exe.

You supply your own supported Star Fox 64 US 1.0 or US 1.1 Rev A ROM, or your own sf64.o2r. Releases do not contain a ROM, sf64.o2r, personal projects, saves, settings or custom music.

## Updates

StarForge checks the tester feed on startup and every hour while its launcher runs. Help → Check for updates provides a manual check. When an update appears, click its button once: the app verifies the download, saves/backups the project and workspace, closes, replaces program files, and reopens.

Projects, game archives, saves, controller settings, custom music and workspace files are preserved. Close a Starship playtest first. The first updater-enabled build needs a manual download for users of older versions.

The `channels/testers.json` feed is signed. It contains public release metadata only. The development signing key and personal level data are not stored in this repository.

## Testing

Include the version, stage, object name, preview time and reproduction steps in bug reports. Review debug reports before sharing them; they can contain local file paths and project contents. Use Starship to confirm actual collision, terrain, damage and stage-specific behavior.

This repository distributes builds and release metadata. Starship and bundled dependency credits/licenses are inside each download.
