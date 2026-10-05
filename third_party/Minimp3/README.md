# Minimp3(minimp3.h 单文件 MP3 解码器)

- `minimp3.h`:上游 https://github.com/lieff/minimp3 的 `minimp3.h`,逐字节
  未改(单文件、仅解码)。
- `LICENSE`:CC0 1.0(公有领域)。

C++ 侧经 `src/formats/mp3_loader` 以 `MINIMP3_IMPLEMENTATION` 编译接入。
**偏离登记**:上游以 MP3Sharp(JavaLayer 系)解码,MP3 样点值非位精确规范,
两解码器的 PCM 字节不保证一致(头信息/声道/采样率等元数据一致);黄金不含
mp3 段(上游 mods 亦无 mp3 资产)。

# Minimp3 (the single-file minimp3.h MP3 decoder)

- `minimp3.h`: verbatim from https://github.com/lieff/minimp3
  (`minimp3.h`; single file, decode only).
- `LICENSE`: CC0 1.0 (public domain).

The C++ side compiles it in with `MINIMP3_IMPLEMENTATION`, consumed by
`src/formats/mp3_loader`. **Registered deviation**: upstream decodes with
MP3Sharp (the JavaLine family); MP3 sample values are not a bit-exact
specification, so the two decoders' PCM bytes are not guaranteed equal (the
metadata — channels/sample rate/etc. — agrees); the golden carries no mp3
section (the upstream mods ship no mp3 assets either).
