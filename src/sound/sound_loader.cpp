// UPSTREAM: OpenRA.Game/Sound/Sound.cs @b6fc03f L22-34 + OpenRA.Game/
//          ObjectCreator.cs @b6fc03f L149-156 + 五个 AudioLoaders
//          (sound_loader.hpp 的实现)
//          Implementation of sound_loader.hpp.
import std;
#include "sound/sound_loader.hpp"

#include "formats/aud_reader.hpp"
#include "formats/mp3_loader.hpp"
#include "formats/ogg_loader.hpp"
#include "formats/voc_loader.hpp"
#include "formats/wav_reader.hpp"

namespace ora::sound {

namespace {

/// 各格式 Info → SoundFormatInfo 的同构转发(字段名一致)。
/// The isomorphic forwarding of each format's Info → SoundFormatInfo (the
/// field names match).
template <class Info>
bool ForwardParse(bool (*fn_try)(std::span<const std::byte>, Info&, std::vector<std::byte>&),
                  std::span<const std::byte> vec_file, SoundFormatInfo& info_out,
                  std::vector<std::byte>& vec_pcm) {
  Info info_concrete{};
  if (!fn_try(vec_file, info_concrete, vec_pcm))
    return false;
  info_out.int4_channels = info_concrete.int4_channels;
  info_out.int4_sample_bits = info_concrete.int4_sample_bits;
  info_out.int4_sample_rate = info_concrete.int4_sample_rate;
  info_out.fp4_length_in_seconds = info_concrete.fp4_length_in_seconds;
  return true;
}

bool TryParseAudForward(std::span<const std::byte> vec_file, SoundFormatInfo& info_out,
                        std::vector<std::byte>& vec_pcm) {
  return ForwardParse(&fmt::TryParseAud, vec_file, info_out, vec_pcm);
}

bool TryParseWavForward(std::span<const std::byte> vec_file, SoundFormatInfo& info_out,
                        std::vector<std::byte>& vec_pcm) {
  // WavInfo.Channels 为 int16(上游 short),单列直转
  // WavInfo.Channels is an int16 (upstream's short); forwarded directly.
  fmt::WavInfo info_concrete{};
  if (!fmt::TryParseWav(vec_file, info_concrete, vec_pcm))
    return false;
  info_out.int4_channels = info_concrete.int2_channels;
  info_out.int4_sample_bits = info_concrete.int4_sample_bits;
  info_out.int4_sample_rate = info_concrete.int4_sample_rate;
  info_out.fp4_length_in_seconds = info_concrete.fp4_length_in_seconds;
  return true;
}

bool TryParseVocForward(std::span<const std::byte> vec_file, SoundFormatInfo& info_out,
                        std::vector<std::byte>& vec_pcm) {
  return ForwardParse(&fmt::TryParseVoc, vec_file, info_out, vec_pcm);
}

bool TryParseOggForward(std::span<const std::byte> vec_file, SoundFormatInfo& info_out,
                        std::vector<std::byte>& vec_pcm) {
  return ForwardParse(&fmt::TryParseOgg, vec_file, info_out, vec_pcm);
}

bool TryParseMp3Forward(std::span<const std::byte> vec_file, SoundFormatInfo& info_out,
                        std::vector<std::byte>& vec_pcm) {
  return ForwardParse(&fmt::TryParseMp3, vec_file, info_out, vec_pcm);
}

}  // namespace

std::vector<SoundLoaderFn> MakeSoundLoaders(const std::vector<std::string>& vec_format_names) {
  std::vector<SoundLoaderFn> vec_loaders;
  for (const std::string& str_format : vec_format_names) {
    SoundLoaderFn fn_loader = nullptr;
    if (str_format == "Aud")
      fn_loader = &TryParseAudForward;
    else if (str_format == "Wav")
      fn_loader = &TryParseWavForward;
    else if (str_format == "Voc")
      fn_loader = &TryParseVocForward;
    else if (str_format == "Ogg")
      fn_loader = &TryParseOggForward;
    else if (str_format == "Mp3")
      fn_loader = &TryParseMp3Forward;

    if (fn_loader == nullptr)
      throw std::runtime_error(
          std::format("Unable to find a sound loader for type '{}'.", str_format));
    vec_loaders.push_back(fn_loader);
  }
  return vec_loaders;
}

}  // namespace ora::sound
