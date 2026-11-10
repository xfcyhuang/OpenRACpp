# tests/golden/maps —— .oramap 地图读取黄金语料

来源与用途见工单 #7（T06）。本目录受根 `.gitattributes` 的
`tests/golden/** -text` 保护（禁止任何换行转换，字节保真）。

## 内容

- `ore-lord.oramap` / `doubles.oramap` / `a-path-beyond.oramap`：
  上游 `mods/ra/maps/` 的官方地图包原样复制（zip 容器）。选型覆盖：
  ore-lord = 最小 64×64 TEMPERAT 主选；doubles = SNOW tileset + 112×54；
  a-path-beyond = 128×128 大图、合法 PickAny 模板 65535 使用。
- `*.mapinfo.txt`：oracle（tools/golden_gen map-info 模式）无头
  `new Map(modData, package)` 全量加载后的关键字段 + 图块/资源直方图转储，
  格式见下；C++ 侧 `tests/map_read.cpp` 逐字节对拍。
- `tilesets/temperat.yaml`、`tilesets/snow.yaml`：上游
  `mods/ra/tilesets/` 对应 tileset 原样复制（.mapinfo.txt 的地形参数来源）。
- `.oramap`、`tilesets/*.yaml` 与 `*.mapinfo.txt` 均带 `.sha256` 旁车，
  且全部登记在 golden_gen 产物清单（gen 复制 / verify 比对）——上游基线
  漂移时 `run.cmd check` 自动报警。

## mapinfo 转储格式（一经入库不得改动，改格式 = 全量重产）

逐行 `key value`，UTF-8 无 BOM、LF 行尾：

- 标量字段：`uid`（SHA1 十六进制 40 字符）、`mapformat`、`requiresmod`、
  `title`、`author`、`tileset`、`mapsize W,H`、`bounds L,T,W,H`（**宽高形**，
  与 map.yaml 原文同形，非右下角坐标）、`visibility`（C# [Flags] ToString，
  单值无分隔、组合值 `, ` 分隔）、`categories`（逗号连接）、`players`、
  `actors`、`invalidcustomrules`、`replacedinvalidtiles`（C# bool：`True`/`False`）。
- `tilerecords N` + N 行 `tile <type>,<index> <count>`：Map 加载（含
  PostInit 无效图块替换）后 Tiles 网格的 (type,index) 直方图，按 type、
  index 升序；迭代序 i=宽外层、j=高内层（与 map.bin 读取序一致）。
- `resourcerecords N` + N 行 `resource <type>,<density> <count>`：同上。
- `terraininfo id=<Id> name=<Name> tilesize=<W>,<H> terraintypes=<N>`、
  `defaulttile <type>,<index>`、`terrainidx`（直方图升序前 5 条的
  `type,index->GetTerrainIndex`，空格分隔）。

## 许可

地图包与 tileset 均为上游 OpenRA 项目内容，GPL-3.0（见 COPYING）。
