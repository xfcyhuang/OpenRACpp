// UPSTREAM: OpenRA.Mods.Common/SpriteLoaders/PngSheetLoader.cs @b6fc03f L36-164(全文逐语义)
// PNG 精灵表:整图或切片。帧区来源两路 —— 手工 Frame[i] 嵌入元数据
// ("x,y,w,h;offsetX,offsetY",tEXt 键)优先;否则按 FrameSize/FrameAmount/
// Offset 切片(默认整图单帧,FrameAmount 单独出现时水平等分)。逐帧自源
// 数据按行距拷贝。越界帧区/非法尺寸抛 InvalidDataException(消息逐字,
// Size.ToString = "W,H")。
// 形态适配:Stream → span(Png 解码复用 src/formats/png.cpp);上游
// TypeDictionary metadata(PngSheetMetadata + EmbeddedSpritePalette)暂不
// 出参 —— 引擎消费面(SpriteCache/FrameLoader)对 metadata 仅透传不读,
// mods 序列批再接(COVERAGE 登记);Stream 重读(加载器约定 TryParse 后
// 流位置复位)由 span 天然满足。
// [UPSTREAM continued] the whole of PngSheetLoader.cs L36-164, verbatim
// semantics. A PNG sprite sheet: the whole image or slices. Frame regions
// come two ways — manual Frame[i] embedded metadata ("x,y,w,h;offsetX,
// offsetY" over tEXt keys) first; otherwise the FrameSize/FrameAmount/Offset
// slicing (the whole image as one frame by default; a lone FrameAmount splits
// horizontally). Each frame copies row-by-row from the source data.
// Out-of-bounds regions / illegal sizes raise InvalidDataException (messages
// verbatim; Size.ToString = "W,H"). Shape adaptations: Stream → span (the
// Png decode reuses src/formats/png.cpp); upstream's TypeDictionary metadata
// (PngSheetMetadata + EmbeddedSpritePalette) leaves as no output for now —
// the engine consumers (SpriteCache/FrameLoader) only pass metadata through
// without reading it, and the mods-sequences batch wires it (registered in
// COVERAGE); the Stream re-read convention (loaders leave the stream
// position reset after TryParse) holds naturally over a span.
#pragma once
import std;

#include "gfx/sprite_frame.hpp"

namespace ora::fmt {

/// PngSheetLoader.TryParseSprite(L46-92):8 字节签名探测 + 解析 + 切帧。
/// PngSheetLoader.TryParseSprite (L46-92): the 8-byte signature probe +
/// parse + slicing.
bool TryParsePngSheet(std::span<const std::byte> vec_file,
                      std::vector<std::unique_ptr<gfx::ISpriteFrame>>& vec_frames);

}  // namespace ora::fmt
