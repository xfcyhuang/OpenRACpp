// UPSTREAM: OpenRA.Game/Sound/Sound.cs @b6fc03f L22-34(ISoundLoader/
//          ISoundFormat)+
//           OpenRA.Game/ObjectCreator.cs @b6fc03f L149-156(GetLoader 名分派)+
//           OpenRA.Mods.{Cnc,Common}/AudioLoaders/*.cs(五格式名)
// 声音格式链:名字分派表(Manifest.SoundFormats,如 "Aud, Wav")+
// TryParseSound 的统一签名(嗅探 + 元数据 + 整段 PCM 物化 = D74 形态)。
// The sound-format chain: the name-dispatch table (Manifest.SoundFormats,
// e.g. "Aud, Wav") plus the unified TryParseSound signature (sniff +
// metadata + the whole-PCM materialization = the D74 shape).
#pragma once
import std;

namespace ora::sound {

/// ISoundFormat 的元数据面(Channels/SampleBits/SampleRate/
/// LengthInSeconds)。
/// The metadata face of ISoundFormat (Channels/SampleBits/SampleRate/
/// LengthInSeconds).
struct SoundFormatInfo {
  std::int32_t int4_channels = 0;
  std::int32_t int4_sample_bits = 0;
  std::int32_t int4_sample_rate = 0;
  float fp4_length_in_seconds = 0.0f;
};

/// 单个声音格式加载器(各 formats 的 TryParse* 适配后同签名)。
/// One sound-format loader (the formats' TryParse* adapted to one
/// signature).
using SoundLoaderFn = bool (*)(std::span<const std::byte> vec_file, SoundFormatInfo& info_out,
                               std::vector<std::byte>& vec_pcm);

/// ObjectCreator.GetLoaders&lt;ISoundLoader&gt;(Manifest.SoundFormats,
/// "sound")(ModData.cs L108):名字 → 加载器链;未知名抛
/// InvalidOperationException 文本逐字。
/// ObjectCreator.GetLoaders<ISoundLoader>(Manifest.SoundFormats, "sound")
/// (ModData.cs L108): names → the loader chain; an unknown name throws the
/// InvalidOperationException text verbatim.
std::vector<SoundLoaderFn> MakeSoundLoaders(const std::vector<std::string>& vec_format_names);

}  // namespace ora::sound
