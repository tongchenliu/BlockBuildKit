# Shared builds and the dataset / 作品分享与数据集

**English** (中文在下面)

Block Build Kit never uploads anything by itself. Builds reach the dataset only when someone exports a file and posts it through the **Share your build** form on GitHub, ticking the consent boxes (CC0 1.0, may be used for AI training, 13 or older).

The goal is an open dataset of how real people build places step by step, for research on AI that helps build worlds (autocomplete for building, "build me a corner café", and so on).

## Turning recording on (optional)

**Project Settings → Plugins → Block Build Kit → Sharing → Record build sessions (local only)**. While it is on, the build status line shows **REC**, and each Play writes `Saved/BlockBuild/Sessions/<time>.json` on your computer. Turn it off any time; delete that folder to remove old recordings.

## The share file: `blockbuildkit.share/1`

Made by **Tools → Block Build Kit → Export build for sharing** in `Saved/BlockBuild/Share/build_<time>.json`.

| Field | Meaning |
| --- | --- |
| `schema`, `plugin_version`, `engine_version`, `exported_utc` | Format and version info |
| `catalog[]` | Each different mesh once: `mesh` (asset path), `name`, `category` (inventory tab), `type` (rough kind: wall, floor, door, window, roof, stairs, railing, column, road, light, seating, table, furniture, plant, vehicle, sign, block, primitive, prop), `size_cm` and `min_cm` (local bounds) |
| `pieces[]` | The finished build: `m` (index into `catalog`), `loc` (cm), `rot` (pitch, yaw, roll in degrees), `scale` (only if not 1) |
| `type_counts` | How many pieces of each type |
| `sessions[]` | Recorded sessions, see below (empty if recording was off) |

`type` is guessed from the mesh and folder names, so it is only a hint. `size_cm` lets builds made with different packs be compared ("a 400 cm wall next to a 100 cm door").

## A recorded session: `blockbuildkit.session/1`

| Field | Meaning |
| --- | --- |
| `started_utc`, `duration_s`, `level` | When, how long, which level |
| `start_pieces[]` | What was already built when the session started (`mesh`, `loc`, `yaw`) |
| `events[]` | Every step: `t` (seconds since start) and `op` |
| `end_piece_count` | Pieces at the end |

`op` values: `mode` (`on`), `pick` (`slot`, `mesh` put into a hotbar slot), `slot` (`slot`, `mesh`), `yaw`, `grid` (`cm`), `height` (`z`), `snap` (`on`), `place` / `remove` / `undo` (`mesh`, `loc`, `yaw`, `piece` index).

## Collecting the dataset

`Tools/collect_builds.py` (Python 3, no extra packages) downloads every shared build from the repository's issues, keeps only submissions with all consent boxes ticked, and writes:

```
dataset/
  index.jsonl          one line per build: issue, date, description, kind, pieces used, file
  builds/<issue>.json  the shared file
```

```
python Tools/collect_builds.py --repo tongchenliu/BlockBuildKit --out dataset
```

Optional: set `GITHUB_TOKEN` to a personal access token for a higher API rate limit.

## License

The plugin code is MIT. Build files shared through the form are released by their authors under **CC0 1.0**. Mesh names in a file are only names; no model files are shared, so bought assets stay with their owners.

---

## 中文

Block Build Kit 自己不会上传任何东西。只有当别人自己导出文件，通过 GitHub 上的 **Share your build** 表单提交，并勾选同意（CC0 1.0、可用于 AI 训练、已满 13 岁）之后，作品才会进入数据集。

目标是建立一个开放的数据集，记录真实的人是怎么一步步搭出场景的，用来研究帮人搭建世界的 AI，比如搭建自动补全，或者"给我搭一个街角咖啡馆"。

**开启录制（可选）：** 项目设置 → 插件 → Block Build Kit → Sharing → Record build sessions (local only)。开启后建造状态栏会显示「录制中」，每次 Play 会在你自己电脑的 `Saved/BlockBuild/Sessions/` 里写一个文件。随时可以关掉，删掉这个文件夹就能清除旧录制。

**导出分享文件：** Tools → Block Build Kit → Export build for sharing，文件格式见上面的英文表格。`type` 是根据模型名和文件夹名猜的类型，只作参考；`size_cm` 让用不同资源包搭的作品也能互相比较。

**收集数据：** 运行 `python Tools/collect_builds.py --repo tongchenliu/BlockBuildKit --out dataset`，会下载所有勾选了全部同意项的提交。

**许可：** 插件代码是 MIT。通过表单分享的作品文件由作者以 CC0 1.0 发布。文件里只有模型名称，不含模型文件，购买的资源仍归原作者。
