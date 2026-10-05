// UPSTREAM: OpenRA.Mods.Common/AudioLoaders/OggLoader.cs @b6fc03f L19-150(形态逐语义;
//          解码后端替换)
// Ogg/Vorbis 音频:NVorbis 的容器头解析与 PCM 流面。OggStream.Read 的换算
// 逐句:count = bytes/2(16 位样点),count -= count % channels(奇数消费
// 截齐),float 样点 × 32767 后 (short) 截断 —— NVorbis 的 ClipSamples =
// 夹到 [-1,1] 再截断。
// 偏离(D94,COVERAGE 登记):解码后端 NVorbis(C#)→ stb_vorbis(third_party/
// StbVorbis,公有领域/MIT)。Ogg/Vorbis 样点值非位精确规范,PCM 字节不保证
// 与上游一致;元数据面(声道/位深/采样率/时长)等价。上游 mods 无 ogg 资产,
// 黄金不含 ogg 段。PCM 与 D74 同族物化为整段 vector。
// [UPSTREAM continued] OggLoader.cs L19-150 with verbatim shape but a
// replaced decode backend. The conversion of OggStream.Read, statement by
// statement: count = bytes/2 (16-bit samples), count -= count % channels
// (the odd-consumer truncation), and the float samples × 32767 truncated by
// the (short) cast — NVorbis's ClipSamples = clamping to [-1,1] before the
// truncation. Deviation (D94, registered in COVERAGE): the decode backend
// NVorbis (C#) → stb_vorbis (third_party/StbVorbis, public domain/MIT).
// Ogg/Vorbis sample values are not a bit-exact specification, so the PCM
// bytes are not guaranteed to match upstream; the metadata surface
// (channels/bits/rate/duration) is equivalent. The upstream mods ship no ogg
// assets and the golden carries no ogg section. The PCM materializes into a
// whole vector like the D74 family.
#pragma once
import std;

namespace ora::fmt {

/// OggFormat 的元数据面。
/// The metadata surface of OggFormat.
struct OggInfo {
  std::int32_t int4_channels = 0;
  std::int32_t int4_sample_bits = 16;
  std::int32_t int4_sample_rate = 0;
  float fp4_length_in_seconds = 0.0f;
  std::int64_t int8_total_samples = 0;  // 每声道样点数 | per-channel samples
};

/// OggLoader.TryParseSound(L21-35)+ GetPCMInputStream 整读:stb_vorbis 打开
/// 失败/解码异常归 false(catch 面同上游)。
/// OggLoader.TryParseSound (L21-35) + a full GetPCMInputStream read: an
/// stb_vorbis open failure / decode exception maps to false (the catch
/// surface as upstream).
bool TryParseOgg(std::span<const std::byte> vec_file, OggInfo& info, std::vector<std::byte>& vec_pcm);

}  // namespace ora::fmt
