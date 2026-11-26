---
name: helldivers2-hellforge04-assets
description: 为 HellForge 0.4 / SDK 16 制作原创武器和背包的资源补丁，保留原版挂点、骨架及动画；用于旧版OBJ编码器拒绝权重格式或碰撞网格时的离线建模流程。
---

# HellForge 0.4 原创模型

本流程的离线输出已在游戏构建25480438上成功编码。2026-11-25用户确认
打击者0.3.0正常；2026-11-26随后确认迷彩模块可与普通背包共用，最终骨骼折射修正正常，
HUD也已通过用户实测。独立安装路径见相邻的helldivers2-hellforge-modules Skill，
透明十字姿态根因与修正见helldivers2-hellforge-stratagem/references/skinned-refraction.md。
模型动画未逐项验收，联机尚未单独验证。
不要把资源构建成功写成实战通过。游戏启动和安装位置遵从用户当前授权。

## 先保留哪些内容

从实际安装的 `GameDataReader` 读取基底unit；不能把新增一个body节点视作武器模型已完成。
手部IK、枪口、瞄具、弹药及背包挂点都依赖原版节点名称。

unit头 `+0x34` 指向joint表。设节点数为N，表的组成是16字节头、
N个64字节局部变换、N个64字节世界矩阵、N个4字节u16对、N个4字节薄哈希。
u16对的后两个字节是父节点索引。头 `+0x08` 为bones，`+0x20` 为state_machine。

`AnimationSet.UnitJoints` 读局部姿态，`AnimationSet.Named` 从bones补已知节点名。
未知节点也应保留，不要删掉；可暂用唯一假名传给UnitWriter，再把生成joint表恢复成
原始完整表，保持所有节点的索引、父子关系、真实薄哈希和rest pose。
UnitWriter只会hash字符串，直接传 `"0x12345678"` 不会得到该哈希。

## 绑定原创几何

用 `Gltf.Read` 和 `ModelUnit.Build` 取得原创三角面，再将每个顶点的Joint映射到原骨架。
静态枪体绑原版root；转轮、击锤等分别绑对应运动节点。背包可绑boss_jiggle。
`UnitWriter.Write` 会按实际骨架生成inverse-bind和GPU布局，随后恢复原joint表。
引用基底bones/state_machine前要保证其原配装资源包也会加载。

游戏单位为米，+Y枪口向前，+Z朝上。glTF转换为 `(x,-z,y)`。
根据原版挂点校准枪口和握把，避免原创模型看起来正确而子弹从错误位置发出。
凹多边形需正确三角化，不能用简单扇形填充扳机护圈；交叠的外表面要避免共面闪烁。

模型仍需同名physics。复制基底physics时检查类型与原名，把其中 `+0x08`、`+0x38`、
`+0x40` 中等于原unit名的引用改成新unit名。别全局替换原版unit。
材质独立复制，颜色LUT也独立复制后改引用，不修改原版共享材质。

## 编码与交付

0.4的OBJ编码器可能拒绝武器c_culling或背包权重格式。可以用离线资源编码器的
UnitWriter与ResourceArchive生成旧版原生档案补丁，而运行DLL仍用安装的SDK16编译。
这不等于把MOD升级成0.5，也不需要替换用户框架。

`ResourceArchive.Write` 的GPU与stream部分按输出分别保存。补丁目标应是原配装会加载
的全部相关档案，不根据新资源名称猜档案。基底P-4在本构建涉及三个档案，补给背包一个。
图标遵循原版红/绿遮罩含义并使用raw编码，不能把红绿原图误当成最终游戏颜色。

本机示例在 `D:/HellForge/HellForge/development/Field-Support-0.4/research/model-builder`，
复用前先读Program.cs；当前只实现打击者和迷彩背包两个入口，不是通用任意装备导入器。
它的构建参数为游戏data目录、MOD目录、striker或optical。成品另存交付目录，
连同源码、原创几何、使用说明和实测范围一起提供。无需重复全盘哈希或反复回归。
