# tests/golden/render —— 渲染黄金语料

T09（issue #10）渲染解码器的确定性锚点语料。

## 内容

- `temperat.pal`：GPL 干净调色板（上游 `mods/ra/maps/chernobyl/temperat.pal` 原样副本，
  随 OpenRA 项目 GPL 分发，见 COPYING；与 EA 内容包 local.mix 内的 temperat.pal
  逐字节相同——调研实测）。`render_decode` 的 palette 锚点断言基于此文件
  （6-bit→8-bit 展开 `(b<<2)|((b<<2)>>6)`，索引 0/1/3/4/16/80/95/96/127/255 十点锚定）。
- `COPYING`：GPLv3 全文（上游同源授权凭证）。

EA 素材（.tem/.shp）不可入库；真实素材锚点（clear1.tem 帧数据、1tnk.shp
64 帧直方图与 TrimmedFrame 尺寸、58 文件批量解码）在 `tests/render_assets.cpp`，
素材缺失时以 ctest SKIP（77）跳过，获取方式见 `docs/render-assets.md`。
