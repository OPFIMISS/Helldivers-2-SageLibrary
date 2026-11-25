# 实现与注册

## 入口与版本

优先读本机 `app/sdk/docs/modding/content.md`（New entities、Stratagems、Selection headings）、`concepts.md`、`runtime.md`、`data.md` 对应小节；具体成员以同版 `app/sdk/hellforge_mod.h` 为准。优先参考 SDK 自带 `examples/mods/blue-sentry` 和 `stratagem-categories`。文中数值限制来自本机 SDK 16 文档快照，不跨版本硬套。

清单的 `id`、`gameplay: true`、`code.dll`、`code.sdk` 是加载信息。C/C++ 入口是导出的 `hellforge_mod_init(const HfApi*, HfMod*)`；C++ 保持 C 链接。预检所需 API 版本、`HfApi.size` 和函数成员后再注册，不能只把清单的 SDK 号调低。定义结构体清零并填写 `size`。初始化中途返回错误不会撤销此前已排队的登记，故按战备逐项隔离失败，记录返回值及 `last_log`。

## 三条实现路线

| 路线 | 怎么选 | 参考 |
|---|---|---|
| 纯注册 | 原版投送及效果符合需求，只需新增可选条目。`add_stratagem` 借用可选战备的 `base`，省略 `payload` 即继承。先用它验证清单、菜单、指令和图标。 | SDK `stratagem-categories` |
| 独立载荷 | 要不同的武器、哨戒、载具或轨道火力。按依赖顺序登记实际需要的伤害、弹丸、实体，最后把新实体填进战备的 `payload`。 | 别西卜 `flak_sentry.c`、`incendiary_gatling_barrage.c` |
| 事件驱动 | 原版数据不能表达延时、持续区域、分段效果或远程操控。让实际投送有适当载荷/标记，使用 `HF_ON_STRATAGEM`、任务事件和游戏线程回调，把逻辑与菜单注册分开。 | 弗兰克斯 `AddScriptedUses` / `Called`；维克斯 `on_stratagem` |

- 独立载荷的常见依赖方向：私有伤害 → 私有弹丸 → `HfEntityDef` / `HfSet` → `register_entity` → `HfStratagem`。只复制必要对象；`own_all` 私有化实体组件记录，不会自动复制记录引用的弹丸、伤害、爆炸、模型或声音。核对实际发射记录，涉及弹匣首发、弹序和挂载件时尤其如此。要暴露 F8 编辑器字段，检查 `editor_like` / `editor_entities` 以及实际数据归属。不要把样本的偏移、资源 ID 或非 SDK 接口当成通用做法。
- `payload` 是资源引用，不是瞬时实体 ID。按照**当前基底的原版投送规则**验证位置和数量：SDK 示例的哨戒是 `{sentry, hellpod}`，别西卜轨道扫射用 `{barrage}`，鹈鹕投送型以 `{cargo, pelican}` 为例。检查 `package`、`loads_item`、额外音效/模型包和加载时机；图标归档存在不意味着载荷资产已经驻留。
- `HF_ON_STRATAGEM` 表示信标激活，用 `HfStratagemEvent.type` 与本次注册返回的类型匹配，复制必要位置/归属数据；需要等实体落地时再监听 `HF_ON_SPAWN`，用资源和本次任务的上下文确认对象。内容登记在初始化，逻辑等事件；`ENTITIES_READY` 可能因数据重载重复触发。任务开始/结束清理任务状态与队列。只有主机可执行的操作由主机执行，联机传递请求时核对任务、来源和去重；加入方可单独做本地呈现。不要把客户端也能看到信标当作客户端能生成效果。

## 注册字段

- `HfStratagem.name`：命名空间内唯一且稳定；存档识别依赖该名称派生的 ID。`.title` / `.description` 仅显示，返回的数字类型和分类 handle 只用于当次运行，不应硬编码或写入存档。
- `code`：1–9 个 `U/D/L/R`。与当前装载的原版、其他模组和本包指令检查相同与双向前缀冲突，SDK 不代查。`cooldown = 0` 与 `uses = 0` 都表示继承基底；`uses = -1` 表示无限。
- `category`、`beam`、`flags` 控制 HUD 配色、信标和属性；`selection_category`、`selection_order` 控制选择菜单分组和顺序，两组字段不要混用。先 `add_stratagem_category`，失败可回退原版分组。跨模组共用分类需要相同全限定名、标题和顺序。SDK 16 文档给出的总量上限为新增战备 48、分组 28，核对当前版本及已启用包。
- 默认由本地后端在这次会话中提供，不是永久解锁，不自动新增舰船购买/升级条目，也不自动继承基底的舰船升级包。联机要用匹配的模组内容、配置及注册条件；不同本地化构建即使 ID 一样，也不要承诺能混用。

## 构建与验收

用实际安装的 CLI；`--out` 指向独立工作目录，不动现有游戏 `data/`。仅指定 `build-data --mod` 是规格检查，不代表生成了运行时补丁。

```powershell
.\app\cli\HellForge.Cli.exe build-mod --mod <mod-folder>
.\app\cli\HellForge.Cli.exe build-data --mod <mod-folder> --game-data <game-data> --out <output-folder>
.\app\cli\HellForge.Cli.exe package-mod --mod <mod-folder> --out <output-folder>
```

构建完成后分三层验收：清单/SDK/数据补丁、当次 `modloader.log` 和 `mod-<id>.log` 的实际注册、一次实战的输入/投送/行为。复杂包再检查重复召唤、任务退出清理、主机与加入方行为、原版装备并存。把失败定位到具体条目，不因包能启动就推导所有战备有效。

## 本地取证索引

以用户 HellForge 根目录为起点。只查本次要用的部分，不复制样本源文件、资产或品牌到新包，除非获得相应许可。

- `mods/Beelzebubs Strat Pack*/src/beelzebub.c`：1.10.16 入口、共用分组、逐项注册、资源包预载。`flak_sentry.c` 看私有弹丸/伤害与哨戒投送；`incendiary_gatling_barrage.c` 看一项轨道载荷；`belt_fed_flak_cannon.c` 看武器、背包与挂架；`rail_tank.c` 看鹈鹕投送和额外运行时逻辑。清单最低 SDK 为 8，不表示每项可选扩展都只需 SDK 8。
- `beelzebub-zh/Franxs_Stratagem_Pack_*.zip` 的 `src/franxs_stratagems.c`：原版式火力网和事件驱动战备同包存在。对照 `AddBarrage` 与 `AddScriptedUses`、`Called`、消息处理及任务收尾。
- `beelzebub-zh/Vix_Advanced_Wartime_Stratagems_*.zip` 的 `src/vix_awst.cpp`、`vix_railstorm.h`、`vix_gunship_lua.h`：按配置条件注册、独立支援武器/背包、原生模组管理的脚本镜头与 HUD；不要把该 HUD 必须依赖的字体和控制逻辑跟菜单标题混为一谈。
- 本机历史 `logs/sessions/d277abb1ffd342fb98094cc38c228ecf/game/mod-giblett.beelzebub-pack-zh.log` 记录过 `0 of 14 stratagems refused`，说明该次会话注册通过，并非 14 项实战报告。
