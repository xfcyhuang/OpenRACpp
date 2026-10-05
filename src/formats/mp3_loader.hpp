// UPSTREAM: OpenRA.Mods.Common/AudioLoaders/Mp3Loader.cs @b6fc03f L19-120(形态逐语义;
//          解码后端替换)
// MP3 音频:IsMp3 嗅探逐句 —— 首 3 字节 ASCII "ID3"(ID3v2 元数据头)或首
// u16(小端)== 0xfbff(MPEG 帧同步字节 FF FB)。解码输出 16 位 PCM。
// 偏离(D95,COVERAGE 登记):解码后端 MP3Sharp(JavaLayer 系)→ minimp3
// (third_party/Minimp3,CC0)。MP3 样点值非位精确规范,PCM 字节不保证与上游
// 一致;元数据面(声道/位深/采样率)等价;LengthInSeconds 上游经 TagLib 精确
// 解析(首猜 = 文件大小 × 8 / (2·ch·rate)),C++ 以解码样点数/rate 计(更接近
// TagLib 精确值;仅 UI 时长展示可观察)。上游 mods 无 mp3 资产,黄金不含 mp3
// 段。PCM 与 D74 同族物化。
// [UPSTREAM continued] Mp3Loader.cs L19-120 with verbatim shape but a
// replaced decode backend. The IsMp3 sniff, statement by statement: the
// leading three ASCII bytes "ID3" (an ID3v2 metadata header) or the leading
// u16 (little-endian) == 0xfbff (the MPEG frame-sync bytes FF FB). The
// decode output is 16-bit PCM. Deviation (D95, registered in COVERAGE): the
// decode backend MP3Sharp (the JavaLayer family) → minimp3
// (third_party/Minimp3, CC0). MP3 sample values are not a bit-exact
// specification, so the PCM bytes are not guaranteed to match upstream; the
// metadata surface (channels/bits/rate) is equivalent; upstream's
// LengthInSeconds comes from TagLib's exact parse (with the first guess =
// filesize × 8 / (2·ch·rate)), while C++ computes decoded-samples/rate
// (closer to TagLib's exact value; observable only in the UI duration
// display). The upstream mods ship no mp3 assets and the golden carries no
// mp3 section. The PCM materializes like the D74 family.
#pragma once
import std;

namespace ora::fmt {

/// Mp3Format 的元数据面。
/// The metadata surface of Mp3Format.
struct Mp3Info {
  std::int32_t int4_channels = 0;
  std::int32_t int4_sample_bits = 16;
  std::int32_t int4_sample_rate = 0;
  float fp4_length_in_seconds = 0.0f;
};

/// Mp3Loader.IsMp3(L21-41):ID3 前缀或 0xfbff 帧同步(短文件 = 上游越界读
/// 异常逃出 catch 面,归 false)。
/// Mp3Loader.IsMp3 (L21-41): the ID3 prefix or the 0xfbff frame sync (a
/// short file maps upstream's out-of-bounds read exception escaping the
/// catch surface to false).
bool IsMp3(std::span<const std::byte> vec_file);

/// Mp3Loader.TryParseSound(L43-60)+ GetPCMInputStream 整读:嗅探 + minimp3
/// 解码一体;上游 catch 一切异常归 false,此处同面。
/// Mp3Loader.TryParseSound (L43-60) + a full GetPCMInputStream read: sniff +
/// minimp3 decode in one; upstream catches every exception into false, and
/// so does this.
bool TryParseMp3(std::span<const std::byte> vec_file, Mp3Info& info, std::vector<std::byte>& vec_pcm);

}  // namespace ora::fmt
