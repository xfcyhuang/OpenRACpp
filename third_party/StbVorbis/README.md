# StbVorbis(stb_vorbis.c 单文件 Ogg/Vorbis 解码器)

- `stb_vorbis.c`:上游 https://github.com/nothings/stb 的 `stb_vorbis.c`,
  逐字节未改(单文件、仅解码)。
- `LICENSE`:stb 双许可证(MIT 或公有领域,任选;见其文)。

C++ 侧经 `src/formats/ogg_loader` 以 `#define STB_VORBIS_HEADER_ONLY` +
同文件第二翻译单元实现编译接入。**偏离登记**:上游以 NVorbis(C#)解码,
Ogg/Vorbis 样点值非位精确规范,两解码器的 PCM 字节不保证一致(头信息/声道/
采样率等元数据一致);黄金不含 ogg 段(上游 mods 亦无 ogg 资产)。

# StbVorbis (the single-file stb_vorbis.c Ogg/Vorbis decoder)

- `stb_vorbis.c`: verbatim from https://github.com/nothings/stb
  (`stb_vorbis.c`; single file, decode only).
- `LICENSE`: the stb dual license (MIT or public domain, either; see it).

The C++ side compiles it in via `#define STB_VORBIS_HEADER_ONLY` plus a
second translation unit of the same file, consumed by
`src/formats/ogg_loader`. **Registered deviation**: upstream decodes with
NVorbis (C#); Ogg/Vorbis sample values are not a bit-exact specification, so
the two decoders' PCM bytes are not guaranteed equal (the metadata —
channels/sample rate/etc. — agrees); the golden carries no ogg section (the
upstream mods ship no ogg assets either).
