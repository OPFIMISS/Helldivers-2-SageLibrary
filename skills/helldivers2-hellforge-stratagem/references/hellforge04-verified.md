# HellForge 0.4 实测与原生路径

适用0.4.0-beta、SDK16、游戏构建25480438。2026-11-25用户确认
SEAF 0.3.1、打击者0.3.0、飞矛索敌0.1.0正常。未单独验证主客联机，
未测飞矛的精确极限距离。用户随后确认光学迷彩0.3.3的HUD正常显示，
认可像素小字效果；本次反馈未确认身体隐身或帧率开销。

## 即时SEAF运输机支援

用独立可选战备行，call_in_type=3、delivery=6、call_in_time=0，
payload_count=0、loads_item=0，清除行+88的mission event。保留donor的
可选目录身份。输入完成即执行，原生B9AAB0负责使用与冷却。

B9AAB0的position可能是z约800米的天空目标点，不能作降落中心。
必须按发起peer找到活着的avatar，从scene根节点读取实际世界坐标。
主机排队六轮，每5秒一架；所有peer预加载资源，只有主机生成。

复用原生落点A147C0、上下文A13C20、进场A12530。运输机实体资源
3EC790806D0B0540，类型B04D927AFB137B9B；飞行目的地位于
[game+33264D8]+48，记录步长1C。

**context+648必须置0。** 默认1会走重兵驻守的随机composition；
普通地图955E10可能返回NULL，导致953017读取NULL+A8崩溃。
context+630传68字节u32名单：首项6，其后六项1，剩余清零。
context+628保留1，这是原生roster枚举，对应game+21CB890；SDK中
同一roster叫4/alternate_670，两者不是同一枚举空间。
row1为Battlefront SEAF，士兵resource/unit为75F7E96AF7DCD303。

士兵资源包792B2A2BA11AE85D必须加载。seaf_squad包6EB59430A00A525C
内的7A3637CD33A54E4C只是helper，不能证明真实士兵模型已驻留。
无需开启整个星球词条、改共享roster或替换全局SpawnSquad。

完整实现：D:/HellForge/HellForge/development/Field-Support-0.4/src/seaf_support.h
和seaf_assets.h；研究在同目录上级research/seaf-native.md。

## 打击者复合弹道

在原生615940的pellet生成路径识别私有projectile，一次展开8颗霰弹
和1枚火箭，用确定性seed并保留原生权限标记；不要另发重复RPC。
直射火箭用EAT17/type132基底，180m/s、gravity=0、drag=0、lifetime=5，
expiry_explosion=0；枪口沿forward前移0.65m。P33榴弹基底会抛射、
乱弹或枪口自爆，不能仅换显示名称就拿来做直射导弹。

换弹需改私有WeaponReload.duration=0.3。八颗弹丸各58普通/耐久伤害、
AP4；独立爆炸400、AP4、半径1m、demolition30。SDK16无新版新增
爆炸API，现成兼容层在development/Striker-work/src/compat/sdk16_explosion.h。
不要把SDK17接口写入0.4成品。武器源码见同目录上级striker.c。

## 飞矛发射前距离

武器25AA2FD4643CF4EE的SensorEye记录303、偏移0，从300改600。
导弹1A4323835B84CEA7的seeker是另一条记录，不要改错。
503BC0取得设置，64C3C0刷新运行实例，64C468把记录+0写入实例+0C。
64C6F0做距离判定，64C9C7读取范围，64CA49用comiss比较实际距离。
SDK字段修改已进入原生限制路径，无需再猜跳转补丁。保留雾、视线和
目标类型限制，因此600米设置不等于任意环境都能600米锁定。
详见development/Spear-Range-0.4/RESEARCH.md。

## 迷彩排错经验与已通过的HUD

背包需用独立HellpodRack载荷生成并升起；直接把装备实体当投送架会
卡在地里。迷彩克隆补给包且own_all=1后仍继承补给库存和按5动作，
应清私有Deposit容量/库存/补充量，并验证私有152字节记录+24原值2629
后换成game+3483C5C的no-action ID；A4385A的8B4824为读取证据。
SDK16未命名此字段，不能伪造HfField绕过SDK布局检查。

on_game_thread的线程不等于引擎Lua线程。hook lua_pcall时先找到包含
stingray表和update函数的状态，再固定绘制线程；保存/恢复Lua栈，
防重入，记录pcall错误。跨线程通过原子快照传能量，不直接读可变状态。
0.3.2用户确认H开关、背包隐藏与敌人识别屏蔽生效，但身体仍可见、UI仍缺失。
Lua绘制不报错不等于屏幕可见。参照本地NarrowHUD/Chef的top_world，常驻HUD
应选Application.worlds中第一个不同于main_world的UI世界，用窗口尺寸布局；
主场景GUI受渲染缩放和后处理影响。不能无证据归因于与NarrowHUD冲突。

0.3.3新增独立UI世界的常驻GUI，最多10Hz、仅更新变化图形；身体按网格隐藏，
只在相关身份/挂接变化时扫描并缓存，不要每帧遍历世界。mesh对象不要跨帧保留，
仅保存原可见布尔值。维护Lua全局回调时，其他MOD在外层包装自己的函数不应
导致重复争抢最外层、不断增长调用链。

2026-11-25用户实测确认0.3.3的HUD正常且像素小字效果良好，可复用独立UI世界、
常驻GUI、10Hz变化更新和3x5点阵百分比的组合。本次反馈未再次确认身体隐身、
联机或帧率，不扩大通过范围。桥接约定、绘制细节与可复用脚本见
[原生Lua HUD](native-lua-hud.md)。本地研究记录为
Field-Support-0.4/research/optical-0.3.3.md。
