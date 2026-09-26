---
name: helldivers2-lua-mod
description: 研究与制作 Helldivers 2 Lua MOD；使用 SageLibrary 离线查找当前或历史字段，核对版本、组件关联及 BingusSharedLoader 加载边界时使用。
---

# Helldivers 2 Lua MOD 与字段搜索

## 起点

先确认用户要的是**游戏中的哪一个量**（如 C4 背包备弹 6，不是手持 1），以及目标版本/装备资源；不要按屏幕上的一个数字全内存搜索后直接改写。先运行仓库根目录 `python sagelib.py search <资源路径片段或hash>`；伤害和弹丸用 `damage:<id>`、`projectile:<id>`。SQL 索引来自用户自行提供的解码文件；若无索引，先按仓库 README 的 `build` 流程获得用户提供的数据，不向用户声称该工具自动解开游戏封装。

SageLibrary 的 `evidence` 有明确边界：`historical_not_current` 仅供发现，`matching_build_export_not_live` 表示用户声明运行时导出且游戏可执行文件和 DLL 的 SHA 与索引记录一致，不是实时读数；`stale_game_build` / `unverified_current_install` 不能用于当前版偏移或改值。即使版本一致，也要核对目标 hash、自 ID、表头/整表哈希、原值与同表正对照。当前版未解码时做一次定向**只读**导出；不要假定旧版 RVA 不变。字段位置不能证明玩法含义，unknown schema 不猜测。

## Bingus 前置与模块

以 [BingusSharedLoader](https://github.com/CowboyBingus/BingusSharedLoader) 为 Lua MOD 前置，先读其当前版本的 `docs/AUTHORING.md`、`src/discover.lua`、`src/shared_loader.lua`。入口首行 `-- HD2-Addon: mods/<author>/<name>` 为明文 UTF-8，资源名哈希需匹配实际档案资源；以 Loader 当前版本文档为准，不编造 `create_api` 或游戏字段扫描接口。工具本身只读、可不安装 Bingus。

修改仅针对通过确认的记录；开始前验证 EXE/game.dll SHA、表头、完整表/行与字段原值，写后回读，支持重复调用幂等及失败回滚。用户需区分：数据候选、离线构建、运行时成功写入、实际游戏效果四个等级。只做所需的定向检查，尽早交付可用样例并取得用户实测；不把成功推断到未测试的游戏版本或其他武器。

## 可复用案例

DBS-2 案例（2026-09-26 离线定位、游戏效果待验证）：`WeaponRoundsComponentData` 映射 50×16，记录 26×136；资源 `0x72170A55A1F37FF1` 映射偏移 624 的值 8 是记录索引。目标记录起始 `800+8×136=1888`；+72 的 float32 容量 2.0，+76 第二容量 0.0，+80/+84 备弹/补给 40/40。旧失败包错误地把映射 8 当容量，还直接拼接档案头并调用不存在的 `loader.register`，不得复用。应用前锁 EXE/DLL 哈希、整表 SHA、资源映射和原始字段；只改 float32 `2.0→4.0`，写后回读；档案需通过 `make_archive` 构建并放入 ZIP 的 `data/`。离线通过不等于游戏里 HUD 变化。

2026-09-25 MOD 共存案例：K9 Faster 2.13 只声明修改弧光组件；同次运行中，另一个次抛 MOD 的 `WeaponDataComponent` 整表 SHA 校验失败，但不能归咎于 K9。若确认目标整行未变且仅无关行有差异，可锁 EXE/DLL 哈希、验证完整映射区 SHA、目标资源映射和目标整行 SHA/字段原值，然后从现场表复制、只改目标字段。若目标行/映射不符必须拒写；恢复时仅操作自己持有的指针。须以注入无关行变更、目标行冲突、映射冲突和恢复测试证明共存行为，**离线通过不等于实机效果验证**。

详见 [C4 背包备弹](references/C4_CASE.md)：从“手持 1／背包 6”的语义，沿背包资源 ID 找到 `DepositComponentData`；用同类背包三字段正对照，改 C4 起始与容量 `6/6→8/8` 或 `10/10`，补给量 3 保留。用户在 2026-09-25 反馈组合 MOD 效果有效，但并未逐项报告所有可选档位和附带 EAT 功能。

伤害、耐久伤害、穿甲、拆毁、弹丸和换弹的已知字段与限制见 [字段说明](references/WEAPON_FIELDS.md)。不同层级 ID 不可混同：资源路径 hash ≠弹丸自 ID ≠伤害 ID；复用弹丸或伤害 ID 时不能仅根据中文武器名筛选。换弹 Duration 为 0 表示使用默认能力时长，`0/1.3` 不会提速；“速度提升 30%”若已确认时长参数应除以 1.3，而不是机械地减去 30%。
