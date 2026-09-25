# Helldivers 2 SageLibrary

[English](README_EN.md) · [Bingus Shared Loader](https://github.com/CowboyBingus/BingusSharedLoader)

面向 AI/模组作者的**轻量、只读**绝地潜兵 2 字段查询工具：一次建立 SQLite 本地索引，之后通过命令行 JSON 或 MCP 工具搜索资源名、64 位资源哈希、伤害 ID、弹丸 ID。**无需运行游戏即可查询已有的解码文件；不附带游戏数据，不会解密/解封装新游戏文件，不会写游戏进程，也不会凭字段名猜测功能。** Python 3.10+；CLI 仅依赖标准库。

> **MOD 前置：**使用本工具查询数据**不需要**安装 Bingus；若根据结果编写/运行本项目所研究的 Lua MOD，则需要 [BingusSharedLoader](https://github.com/CowboyBingus/BingusSharedLoader) 作为前置框架。该框架负责加载 Lua，**不提供万能字段搜索 API**。务必遵守所用框架的对应版本和游戏版本要求。

## 立即使用

准备**自行合法获取的、已解码**游戏数据：实体文件（例如 Filediver 导出的明文 LDLD 数据，原始字节或 gzip）、可选伤害文件、弹丸表、资源名表和类型名表。[Filediver](https://github.com/xypwn/filediver) 是可研究的资源提取工具；提取成功并不意味着某文件已解码，也不意味着字段语义已证明。输入均为用户本机文件，仓库不包含游戏文件。

```powershell
python sagelib.py build --entities "C:\data\generated_entities.decoded.gz" --damage "C:\data\generated_damage_settings.decoded.dl_bin" --projectiles "C:\data\ProjectileSettings.bin" --names "C:\data\cracked.txt" --types "C:\data\dl_type_names.txt" --source historical
python sagelib.py search c4_charge_backpack
python sagelib.py search 0x2A18F81C44A26771
python sagelib.py search damage:206
python sagelib.py search projectile:96
```

只提供手头真实存在的输入即可，不要求五份文件齐全。可用 `--db C:\path\index.sqlite3` 自定义索引路径。名称表每行一个**原始资源路径**；没有名称表仍可按资源哈希搜索实体。支持的具体字段：`DepositComponentData` 的起始/上限/补给候选值、`WeaponReloadComponentData` 的 Ability ID/Duration、经自 ID 核对的伤害和弹丸记录。提供类型名表后，其余组件最多只返回 `candidate_mapping_only_unknown_layout` 资源关联候选，绝不把未知字节猜成数值字段。输出 JSON 包含证据等级、原始文件 SHA-256、行偏移和结果数量；JSON stdout 不混调试文字。

## 游戏更新与证据等级

| `evidence` | 含义 |
| --- | --- |
| `historical_not_current` | 输入标记为历史资料，即使游戏哈希相同也不能当作当前版实测。 |
| `matching_build_export_not_live` | 用户声明输入为同版运行时导出，且查询时本机 EXE/game.dll 哈希与建库时一致；**不是运行中实时读数**。 |
| `stale_game_build` | 本机游戏文件哈希与导出时不同；不得用旧偏移写游戏。 |
| `unverified_current_install` | 未提供/无法访问游戏安装目录，不能核对当前版本。 |

使用**一次性只读导出的、全部来自同一版游戏**的数据建库时，执行：

```powershell
python sagelib.py build --entities "C:\data\generated_entities.runtime.bin" --projectiles "C:\data\ProjectileSettings.bin" --names "C:\data\cracked.txt" --types "C:\data\dl_type_names.txt" --source runtime-export --game-root "E:\SteamLibrary\steamapps\common\Helldivers 2"
python sagelib.py search c4_charge_backpack --game-root "E:\SteamLibrary\steamapps\common\Helldivers 2"
```

`runtime-export` 是**建库者对输入来源的声明**，程序只核对文件指纹和游戏版本，无法独立证明该文件真来自进程；**不要混用历史资料与本版导出然后标成 runtime-export**。游戏更新后需要获取、核对新版解码数据或一次只读运行时导出，重建索引。本工具目前不自动解开 `data/game/generated_*.dl_bin` 的新封装，也不自动连接游戏；不声称能无条件获取所有最新版字段。

## MCP（可选）

先 `python -m pip install "mcp>=1.30,<2"`，再将运行命令 `python /path/to/Helldivers-2-SageLibrary/mcp_server.py` 配置到支持 stdio 的 MCP 客户端；服务器提供只读 `search_fields(query_text, limit=20)`。请设置环境变量 `SAGELIB_DB` 为已生成的 SQLite 索引绝对路径，`SAGELIB_GAME_ROOT` 为当前游戏安装根目录（如果有）；**MCP 查询不会读取任意客户端传来的路径，也不会改动游戏或重建索引**。不同客户端的配置文件格式可能不同。 当前 MCP 入口固定官方 SDK 1.x；SDK 2.x API 不兼容。

例如通用的 JSON 形态（按客户端格式调整）：

```json
{"mcpServers":{"helldivers2-sagelibrary":{"command":"python","args":["/absolute/path/Helldivers-2-SageLibrary/mcp_server.py"],"env":{"SAGELIB_DB":"/absolute/path/index.sqlite3","SAGELIB_GAME_ROOT":"/absolute/path/to/Helldivers 2"}}}}
```

## 开发与验证

`python -m unittest discover -s tests -v`。测试只生成合成结构，不上传或依赖游戏资源。C4 从错误组件到正确背包储量表的复盘见 [C4 案例](skills/helldivers2-lua-mod/references/C4_CASE.md)，可复用的 MOD 开发规则见 [Skill](skills/helldivers2-lua-mod/SKILL.md)。查到某个值不等于证实其作用：仍需同类记录对照、版本锁定和针对性游戏内验证。
