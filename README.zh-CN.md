# Block Build Kit 方块建造工具包

[English](README.md) · **中文**

一个给 Unreal Engine 新手用的「我的世界」式建造模式。
在 **Play（运行）** 状态下直接在关卡里走动，把模型按网格一块块拼起来；满意之后一键**烘焙**进关卡，变成普通的 Actor。不用先学会编辑器里的移动/旋转手柄。

> 状态：测试版（0.2.0），基于 Unreal Engine 5.8 开发。这是 C++ 插件，第一次打开项目时 Unreal 会自动编译它，需要安装 Visual Studio 并勾选「使用 C++ 的游戏开发」。

## 快速开始

1. 关掉 Unreal 编辑器。
2. 在你的项目文件夹（有 `.uproject` 文件的那一层）里新建 `Plugins` 文件夹（已有就不用），把 `BlockBuildKit` 整个文件夹放进去：
   `MyProject/Plugins/BlockBuildKit/BlockBuildKit.uplugin`
3. 打开项目，弹出「是否重新编译缺失的模块」时点 **是（Yes）**。
4. 点 **Play**，按 **F** 进入建造模式。按 **I** 打开背包，里面已经有引擎自带的基础形状可以直接用。
5. 搭完后停止 Play，点菜单 **Tools（工具）→ Block Build Kit → Bake build into level**。搭的东西会变成大纲里 `BlockBuild` 文件夹下的普通静态网格体 Actor（Ctrl+Z 可撤销），记得保存关卡。

## 用自己的模型

把静态网格体（Static Mesh）放到 `Content/BuildPieces` 下面，每个子文件夹就是背包里的一个分类：

```
Content/BuildPieces/
  Walls/   SM_Wall_A, SM_Wall_Window, ...
  Floors/  SM_Floor_2x2, ...
  Props/   SM_Bench, SM_Lamp, ...
```

也可以在 **项目设置 → 插件 → Block Build Kit → Piece Folders** 里换成任何文件夹（比如你买的资源包）。
物件会立在你瞄准的表面上（最低点贴地），所以轴心在底部或中心的模型都能用。

## 操作（建造模式中）

| 按键 | 作用 |
| --- | --- |
| F | 开 / 关建造模式（可在设置里改） |
| I | 背包：点物件放进当前物品格 |
| 1–9 / 滚轮 | 选物品格 |
| 右键 | 放置 |
| 左键 | 拆除瞄准的物件（只能拆自己搭的） |
| Q / E | 旋转 45° |
| C | 网格 25 / 50 / 100 / 150 cm |
| N | 贴边吸附开 / 关 |
| PageUp / PageDown | 升高 / 降低 25 cm，Home 归零 |
| Z | 撤销上一次放置 |

搭的东西会自动存档（SaveGame 槽位 `BlockBuild`），每次 Play 都会恢复。

## 设置

**项目设置 → 插件 → Block Build Kit**

- **Enabled**：不删插件也能关掉建造模式
- **Chinese UI (中文界面)**：游戏内文字显示中文
- **Toggle Key**：开关键，默认 F
- **Save Slot Name**：存档槽位名，默认 `BlockBuild`
- **Piece Folders**：物件文件夹，默认 `/Game/BuildPieces`
- **Include Basic Shapes**：包含引擎的立方体、平面、圆柱、圆锥、球
- **Record build sessions (local only)**：默认关闭，见「分享你的作品」

## 注意

- **模板里的枪**：第一人称模板里左键还会开枪。在角色蓝图里开枪之前先判断 **Is Build Mode Active** 节点；也可以用 **Set Build Mode Active** 从蓝图开关建造模式。
- **打包游戏**：要把物件文件夹加到 **项目设置 → 打包 → Additional Asset Directories to Cook**，否则打包后没有这些模型。
- **控制台命令**：`blockbuild export <名字>` 把搭建内容导出到 `Saved/BlockBuild/<名字>.json`；`blockbuild clear` 清空；`blockbuild.bake`（编辑器里）等于菜单里的烘焙。**Tools → Block Build Kit → Clear saved build** 可以从头开始。
- **买来的资源（Fab 等）**：可以在你自己的项目里配合这个插件用，但不要上传到公开仓库，大多数商店许可证不允许再分发资源文件。

## 分享你的作品（可选）

想帮忙建一个"真人如何搭建场景"的开放数据集吗？插件永远不会自动上传任何东西。

1. 可选：打开 **项目设置 → 插件 → Block Build Kit → Sharing → Record build sessions (local only)**。录制时状态栏会显示「录制中」，搭建步骤只存在你自己的电脑上。
2. 在编辑器里点 **Tools → Block Build Kit → Export build for sharing**，会得到一个 `.json` 文件，里面有物件名称、尺寸、位置和录下的步骤，没有用户名、电脑名或模型文件。
3. 用 **[Share your build](https://github.com/tongchenliu/BlockBuildKit/issues/new?template=share-build.yml)** 表单提交（导出完成的提示里有链接）。分享的文件会公开，以 CC0 1.0 发布，并可能用于训练帮人搭建世界的 AI。

详细说明、文件格式和下载脚本见 [DATA.md](DATA.md)。

## 许可证

[MIT](LICENSE)：可以免费使用、修改、分享，商业项目也可以，保留版权声明即可。

Unreal® 是 Epic Games, Inc. 在美国及其他地区的商标或注册商标。本项目与 Epic Games 无关。
