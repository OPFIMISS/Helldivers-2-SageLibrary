# HP HUD+ 攻击自动显示失效

2026-11-26，HellForge 0.4 / SDK16 / 游戏构建25480438。修复版本`ehp-1.6.1-bsl18-hf3`，用户确认游戏内恢复正常。

## 为什么Q标记能显示，攻击却不能

自动显示以8Hz观察HP变化，先要求伤害归属于本地玩家，再用Spottable识别敌人。Q标记走独立路径，绕过此次敌人分类，因此Q能用不代表自动分类成功。

旧代码取生命值组件表指针，通过VirtualQuery找到内存分配起点，假设起点是原始整块LDLD实体数据，再扫描Spottable。HellForge可以单独分配或复制组件表，这个假设失效。`spot=false`和按类型缓存的false又把启动时失败永久保留，日志出现`local hit on unclassified entity`，无法创建自动血条。

问题在NarrowHUD组合中被发现，但没有证据表明NarrowHUD抢走伤害事件。修复没有修改NarrowHUD，也没有放宽伤害归属判断。

## 当前版本的可靠读取

以原生getter`game.dll+0x4FE440`为依据，而非猜原始实体blob地址。

| 布局 | 当前构建值 |
| --- | --- |
| 状态指针 | `[game.dll+0x346BF98]`。 |
| Spottable组件编号 | 125。 |
| 实时表指针 | `[state+0xF12860]`，属于`state+0xF12478+8*component`表数组。 |
| 索引区 | 1386个16字节槽，完整64位hash为key，从`hash%1386`线性探测，索引在槽`+8`。 |
| 记录区 | 索引区后`0x56A0`，每条72字节，类别在记录`+0x14`。 |

这些地址只适用于本构建。新版本先研究getter，不能将该表布局套到所有组件。

只缓存成功分类；表尚未就绪、临时读取失败或无目标记录时不永久缓存false，表指针变化则清缓存。保留本地归属、Q优先级、部位HP和现有设置。`HP changed without local credit`可由队友或SEAF造成，不为让血条出现而删除这层过滤。

## 还要更新真正执行的档案

本例松散源码为`Addon/0cdb2ce39c96e9a4.lua`，实际Lua资源位于`Addon/9ba626afa44a3aa3.patch_0`。只改松散Lua不能保证加载器执行新代码。

资源type为`a14e8dfa2cd117e2`，name为`0cdb2ce39c96e9a4`，路径`mods/combat/enemy_hp`。payload为uint32文本长度、uint32版本2、UTF-8源码。用ResourceArchive结构化读写，仅替换此资源，再确认档案中的payload与源码一致，不手拼档案头。

## 通过范围

定向离线检查覆盖无表后恢复、临时失败重试、换表清缓存、本地命中创建血条和排除队友伤害。用户随后确认攻击自动显示正常；未泛化为所有HUD之间无冲突。
