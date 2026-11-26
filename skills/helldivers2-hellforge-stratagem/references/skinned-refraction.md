# 角色折射、十字姿态与恢复

2026-11-26用户确认背包版与独立模块最后一轮修复效果正常。适用HellForge 0.4 / SDK16 / 构建25480438；没有单独验证联机、全部护甲组合或帧时间。

## 失败的两条路线

最初`opacity_stealth=0.94`的潜伏者材质未得到用户预期的模糊隐身。后来普通原生折射产生了认可的背景扰动，但角色体内出现固定十字姿态人形。

曾以为碰撞、阴影或内部占位人体被误绘制，排除了以下材质；用户确认仍未解决，不能把这轮过滤记成完整修复。

| 材质hash | 辅助用途 |
| --- | --- |
| `75F87AD2AE08E9C2` / `5990E5EFBCA8AE21` | 角色或通用阴影。 |
| `84B327056BAF1C6D` | 碰撞。 |
| `8CD6B9C938FEAF90` / `70CD11D0BB7DBD9E` | avatar内部g_body占位表面及其父材质。 |

显示代码没有生成新角色。关键差异在顶点程序：旧普通折射父材质`B45631ACD16D5183`没有`BLENDINDICES`、`BLENDWEIGHTS`和`c_skin_matrices`，替换带骨骼角色表面后按初始姿势绘制。固定十字人形不应继续只按“多生成一个实体”查找。

## 实测通过的替代材质

原生`content/art_shared/base_shaders/base_glass_container`，hash `AC39020959C7E54B`，位于共享包`18235E0C9EC0E636`。该GPU资源包含刚性与蒙皮变体，其中8个顶点程序有骨骼输入，也有读取`__tex_hdr0`背景画面的像素程序。

从本机GameDataReader读取原材质，复制成私有实例；将Main的`+0x10`八字节清零、`+0x18`写父材质hash，避免沿用基底的档案局部shader位置。不替换原版共享材质。构建代码见[BuildCloak.cs](../assets/BuildCloak.cs)，其中只含原创构建逻辑，不含提取资源。
这是接收GameDataReader的C#构建类，需要在引用本机兼容HellForge.Core的工具中调用`BuildCloak.Run(game, modsRoot)`，不是可单独运行的脚本。它写入两个既定MOD目录的`data/9ba626afa44a3aa3.patch_1`及GPU部分；移植时先改目标目录和私有前缀，别覆盖同名其他资源补丁。

| 参数 | 通过版本的值 |
| --- | --- |
| `ior_intensity` / `noise_distortion_amount` | `0.018` / `0.012`。 |
| `ior_noise_tiling` / `flow_speed_max` | `1.75` / `0.12`。 |
| `glass_fog` / `liquid_fog` / `water_disruption_fog_alpha` | 全部为0。 |
| `glass_thickness` / `roughness_intensity` | `0.03` / `0.08`。 |
| `specular` / `emissive_intensity` / `bounds_mask` / `ior_mask_alpha_threshold` | 全部为0。 |
| `glass_color` / `liquid_color` | 均为白色`[1,1,1]`，不加有色玻璃染色。 |
| `water_normal_map` / `water_noise` | 使用潜伏者normal与cloak纹理。 |

继续预加载潜伏者包`46E418DD4A6542C4`和共享包；确认资源实际驻留，不用已知名字的`can_get`当作离线资源枚举器。主材质未就绪时，先用私有潜伏者材质，再用可恢复网格隐藏兜底。

私有主材质为背包`51DC9E0984F7F853`、模块`77DD8C78A2880FBF`；备用分别为`EDF55883971B9297`、`D88561D05964E494`。它们只用于隔离命名，不要求未来MOD沿用这些ID。

## Lua材质与恢复的坑

真实材质槽是IdString userdata，旧版只接受number/string，报`unsupported material slot ID`后根本没有替换外观。允许userdata，用`type..tostring(slot)`作共享槽的持久身份，每次调用前重新取原生slot；不要跨帧缓存slot、mesh或material userdata。

一组LOD可共享材质槽。原材质hash应按unit内共享槽保存一次，否则后处理的LOD可能把迷彩材质当作原材质，解除后无法恢复。设置前先保存，部分失败也能撤销；恢复先还原材质，再还原原网格可见布尔值。

原生父单位隐藏与Lua网格隐藏叠加，曾让后来挂接的背包保存到false，最终永久不显示。现在原生只跟踪挂接与转发可见性调用，Lua独立管理可恢复显示；开关、换包、死亡清理必须使用同一所有权。

每100ms维护缓存网格；只有装备、生命身份、迷彩根或挂接版本改变才扫描世界，不每帧遍历全图。HUD保持独立UI世界、常驻GUI和10Hz变化更新。没有帧时间测量，不能宣称零开销。

## 样例与验证边界

最新脚本保存在独立模块Skill的`assets/module-example/optical_view.lua`，桥接前缀为`__liuli_module_`，`poll`新增第11项资源就绪布尔值。原`assets/optical_view.lua`保留0.3.3历史HUD样例，不能当成最新材质实现。

离线检查能确认私有材质父引用、GPU骨骼输入、Lua槽处理和恢复逻辑，不能渲染出实际角色。最终通过依据是用户对最后一次直接安装修复反馈“完美”；不把它扩大成所有分辨率、装备与联网情形通过。
