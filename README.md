# Block Build Kit

**English** · [中文](README.zh-CN.md)

A Minecraft-style build mode for Unreal Engine, made for people who are new to Unreal.
Walk around your level in **Play**, snap meshes together on a grid, and when you are happy, **bake** the result into the level as normal actors. No need to learn the level editor's gizmos first.

> Status: beta (0.2.0). Developed with Unreal Engine 5.8. It is a C++ plugin, so the first time you open a project with it, Unreal compiles it (you need Visual Studio with the "Game development with C++" workload).

## Quick start

1. Close the Unreal Editor.
2. In your project folder (where the `.uproject` file is), create a folder called `Plugins` if there isn't one, and put the `BlockBuildKit` folder inside it:
   `MyProject/Plugins/BlockBuildKit/BlockBuildKit.uplugin`
3. Open the project. When Unreal asks to rebuild missing modules, click **Yes**.
4. Press **Play**, then press **F**. Build mode is on. Press **I** to open the inventory; the engine's basic shapes are already there.
5. When you are done, stop Play and choose **Tools → Block Build Kit → Bake build into level**. Your build becomes normal Static Mesh Actors in the outliner folder `BlockBuild` (Ctrl+Z undoes it). Save the level.

## Use your own pieces

Put Static Meshes under `Content/BuildPieces`. Every sub-folder becomes a tab in the inventory:

```
Content/BuildPieces/
  Walls/   SM_Wall_A, SM_Wall_Window, ...
  Floors/  SM_Floor_2x2, ...
  Props/   SM_Bench, SM_Lamp, ...
```

You can point the kit at any other folders (for example a pack you bought) in **Project Settings → Plugins → Block Build Kit → Piece Folders**.
Pieces stand on whatever you aim at (their lowest point touches the surface), so meshes with the pivot at the bottom or the centre both work.

## Controls (in build mode)

| Key | Action |
| --- | --- |
| F | Build mode on / off (changeable in settings) |
| I | Inventory: click a piece to put it in the current slot |
| 1–9 / mouse wheel | Pick a hotbar slot |
| Right mouse | Place |
| Left mouse | Remove the piece you aim at (only pieces you built) |
| Q / E | Rotate 45° |
| C | Grid size 25 / 50 / 100 / 150 cm |
| N | Edge snapping to neighbouring pieces on / off |
| PageUp / PageDown | Raise / lower 25 cm, Home resets |
| Z | Undo the last placement |

Your build is saved automatically (SaveGame slot `BlockBuild`) and comes back every time you press Play.

## Settings

**Project Settings → Plugins → Block Build Kit**

- **Enabled**: switch the build mode off without removing the plugin
- **Chinese UI (中文界面)**: in-game text in Chinese
- **Toggle Key**: default F
- **Save Slot Name**: default `BlockBuild`
- **Piece Folders**: default `/Game/BuildPieces`
- **Include Basic Shapes**: Cube, Plane, Cylinder, Cone, Sphere from the engine
- **Record build sessions (local only)**: off by default, see "Share your build"

## Notes

- **Template guns:** in the First Person template, left mouse also fires the gun. In your character Blueprint, check the node **Is Build Mode Active** before shooting. There is also **Set Build Mode Active** to turn the mode on or off from Blueprint.
- **Packaged games:** add your piece folders to **Project Settings → Packaging → Additional Asset Directories to Cook**, otherwise they are not in the build.
- **Console:** `blockbuild export <name>` writes the build to `Saved/BlockBuild/<name>.json`; `blockbuild clear` empties it; `blockbuild.bake` (editor) is the same as the Tools menu entry. **Tools → Block Build Kit → Clear saved build** starts over.
- **Bought assets (Fab etc.):** you can use them with the kit in your own project, but don't upload them to a public repository. Most store licenses don't allow redistributing the asset files.

## Share your build (optional)

Want to help build an open dataset of how people build places? Nothing is ever uploaded automatically.

1. Optional: turn on **Project Settings → Plugins → Block Build Kit → Sharing → Record build sessions (local only)**. The status line shows **REC** while it records your steps (on your computer only).
2. In the editor, choose **Tools → Block Build Kit → Export build for sharing**. You get one `.json` file with piece names, sizes, positions and your recorded steps. No user name, computer name or model files.
3. Post it with the **[Share your build](https://github.com/tongchenliu/BlockBuildKit/issues/new?template=share-build.yml)** form (the export notification has a link). Shared files are public and released under CC0 1.0, and may be used to train AI that helps build worlds.

Details, file format and the download script: [DATA.md](DATA.md).

## License

[MIT](LICENSE): free to use, change and share, including in commercial projects. Keep the copyright notice.

Unreal® is a trademark or registered trademark of Epic Games, Inc. in the United States of America and elsewhere. This project is not affiliated with Epic Games.
