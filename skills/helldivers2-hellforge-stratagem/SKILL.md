---
name: helldivers2-hellforge-stratagem
description: 为《绝地潜兵 2》HellForge 私服制作、研究或排查像 Beelzebub、Franxs、Vix 那样新增独立战略配备的原生 C/C++ 模组。适用于战备注册、投送载荷、原版风格图标、简中名称与说明、战备包构建与验证；不适用于纯 Bingus Lua 模组或只替换原版装备。
---

# HellForge 战略配备

新增战备通常有两层：原生 DLL 用 HellForge SDK 注册独立战备与玩法；`hellforge-data.json` 为图标、模型或效果制作资源补丁。原版投送、现有载荷和菜单资源可独立复用。独立战备也不等于必须有自定义图标、模型或伤害。

## 开工顺序

1. 先识别原包：`hellforge-mod.json` 的 `code.dll`、`code.sdk`，以及 `src/` 和 `hellforge-data.json`。别把 HellForge 原生注册当成 Bingus 的 Lua 发现；已有 Lua 辅助逻辑不改变这一点。以本机 `app/sdk/hellforge_mod.h`、`docs/modding/` 和 `HellForge.Cli --help` 为准，核对 SDK 与支持的游戏构建，不根据压缩包标题猜版本。
2. 写出一张战备规格：稳定资源名、借用的投送类型、载荷、行为、分组、指令、冷却与次数、资源依赖、UI 名称/说明、联机验证目标。决定走纯注册、独立原版式载荷或事件驱动，见 [实现与注册](references/implementation.md)。默认从最小可运行方案开始，先用基底图标确认链路。
3. 自定义图标或写简中时读 [图标与文案](references/presentation.md)。图标包与注册名要逐字一致；未构建或未部署时保留基底图标。汉化只改显示文本，不改匹配用原名、稳定标识、配置键或联机消息。
4. 源码通过本机 CLI 构建；有资源则另做数据构建并检查部署计划。先单独启用新包验证“出击前可选 → 能输入 → 正确投送 → 行为正常且原版不受影响”，再按需验证组合包和联机。日志注册成功只证明注册，不能写成实战通过。

## 判断故障位置

0.4/SDK16制作即时SEAF支援、复合弹道武器或飞矛距离修改时，先读
[实测与原生路径](references/hellforge04-verified.md)。其中区分用户已通过的
版本和历史失败，包含运输机乘员崩溃、直射火箭和Lua HUD线程的已知坑。
原生模组需要常驻能量条、像素数字或与NarrowHUD共存的界面时，读
[原生Lua HUD](references/native-lua-hud.md)，其中附0.3.3用户实测通过的绘制脚本与桥接约定。
制作飞矛式原生锁定与持续追踪、双轴弹头模式、准心部位框及中文点阵面板时，读
[飞矛锁定与火箭HUD](references/native-spear-rockets.md)。2026-11-28用户确认主要锁定与HUD表现；
空爆发射、最新装填音和速度加成仍待实战确认，不能一并记成通过。
Vix炮艇出现5秒电磁打击、不能接管或双重倒计时时，读[AC130生命周期](references/ac130-lifecycle.md)。
制作电视制导导弹、接管弹体视角、排查接管后未响应或镜头被覆盖时，读
[电视弹摄像头接管](references/tv-missile-camera.md)。2026-11-28用户确认镜头正常；
新增Shift高低速切换仅通过编译与离线检查，联机仍待验证。
角色出现透明十字人形、材质槽userdata错误或解除后背包永久隐藏时，读
[骨骼折射与恢复](references/skinned-refraction.md)。2026-11-26用户确认最后一轮修正正常。
制作空投拾取后不占背包槽的能力模块、身体附件或外骨骼原型时，使用相邻的
[独立模块Skill](../helldivers2-hellforge-modules/SKILL.md)，其中保留已通过的原生安装事务与源码。

- 模组未列出：查根目录清单、目录层级、重复 ID、SDK 门槛及构建错误。
- 模组加载但找不到战备：查新增条目返回值与分类回退，去**出击前配装**找；不默认出现在舰船升级或购买页面。多个包同时缺项、只剩先注册的战备，或日志报告 `48 stratagems`、`too many definitions`、`64 new projectile types` 时，读 [0.4 注册容量扩展](references/hellforge04-capacity.md)。附精确版本源码补丁；目前仅编译与离线检查通过，未获实战确认。
- 预览/打开菜单崩溃：优先排查 `.icon` 对应的纹理和 GUI 材质是否真正构建并部署；用基底图标隔离。
- 看得到但投送不对：查 `base` 的投送语义、`payload` 顺序、资产包及注册阶段。
- 能投送但玩法不对：查绑定的实际实体、私有数据、事件触发和主机权威。没触发事件时不猜内存偏移。
