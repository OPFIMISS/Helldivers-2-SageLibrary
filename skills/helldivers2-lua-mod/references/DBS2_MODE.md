# DBS-2 独立自动模式：研究证据与边界（2026-09-26）

本次只读研究对照了当前版本 EXE `F5FEE03DCFDB2E553A4752C283590950AC13316B376D8196AA556FF0400D5F06`、game.dll `2E2C3B7C2500646DADD5F2B4C6E0504DBB7E7896139F64CDDC0D1813C718F51E` 的实体表导出；字段名称部分来自历史 `1.006.301` 实例，不能把旧版枚举当作本版常数。新模式**没有实现或实机验证**。

| 模型资源 hash | 武器 | 当前 WeaponData index | 当前记录 +144/+148 | 当前 +184/+188 | 独立弹药 |
| --- | --- | ---: | --- | --- | --- |
| `0x72170A55A1F37FF1` | DBS-2 | 177 | 2/4 | 3/0 | WeaponRounds index8，float32 容量 2.0 |
| `0x8A307BD1811A5FE9` | 司炉者 | 180 | 1/0 | 11/3 | WeaponMagazine index136，30 发 |
| `0xA8A91EB54892B6B2` | 仲裁者 | 280 | 1/3 | 11/3 | WeaponMagazine index211，45 发 |
| `0x992B6F65A5BAB53D` | 司炉者下挂火焰 | 249 | 待确认 | 待确认 | WeaponMagazine index192，50 发；历史 SprayWeapon index13 |

当前 WeaponData 映射区 11680 字节、记录步长 1232；当前司炉者的功能字 +184 是 11，旧版解码名称对应的值是 12（`UnderBarrelWeapon`），证明不能复制旧枚举。DBS-2 的单发/双管已占用原武器功能。当前司炉者主枪弹丸 ID 38（速度 285、伤害 ID 97、碰撞爆炸 0）；旧版伤害 97 的 AP 为 `[2,2,2,0]`，但当前版 AP 尚未核实。

Filediver 对当前档案定向提取 `.unit` GLB 与 `.state_machine` JSON：DBS-2 和司炉者均有 `attach_underbarrel` 骨骼，前者在 `barrel_pivot` 下、后者在 `boss` 下；DBS-2 状态机有 `fire_mode`，司炉者状态机未见同名变量。骨骼存在不表示装备组件已挂载。历史生成实体中司炉者下挂资源独自占用弹匣、喷射和 loadout 记录；扫描各已知组件表未找到父→子**完整 64 位 hash** 直接引用，说明关联可能在其他资源或采用其他编码，不能以零命中证明无关联。

**可实施路径**：保留 DBS-2 原来的霰弹两种射击方式；先证明宿主如何引用/实例化独立下挂实体，再给子实体配置 15 发 WeaponMagazine、经当前 DamageSettings 证明的 AP2 子弹、ProjectileWeapon、换弹/射击动画和联网同步，最后在宿主中增加已验证的切换功能。只改射速、功能值或弹丸会共享霰弹池或损坏现有模式，不满足独立 15 发要求。未证明的实体创建/挂载 API 不写入 MOD。
