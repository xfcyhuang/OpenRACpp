// UPSTREAM: OpenRA.Mods.Common/Traits/Render/RenderSprites.cs @b6fc03f
//          L24-302 全文(RenderSprites 的仿真/装配语义面;RenderPreview 的
//          IActorPreview 面随 Phase 6 预览装配 —— IRenderActorPreviewSpritesInfo
//          的接口位保留空实现)
//          The whole of RenderSprites.cs L24-302 (the sim/assembly semantic
//          faces; RenderPreview's IActorPreview faces ride Phase 6 — the
//          IRenderActorPreviewSpritesInfo slot keeps an empty
//          implementation).
//
// 机制对照 / Mechanism mapping:
//  - ctor 的 world.Map.Sequences → Animation::Deps 注入(SequenceSet 取自
//    Map::Sequences 装配面;高度查询 = Map::DistanceAboveTerrain)
//    The ctor's world.Map.Sequences → the Animation::Deps injection (the
//    SequenceSet from Map::Sequences' assembly face; the height query =
//    Map::DistanceAboveTerrain).
//  - RenderAnimations 的 yield 生成器闭包(ctror 即挂、每次 Render 重放)
//    → Render() 内的即时遍历(逐枚举求值等价 —— 生成器逐项读当前 anims)
//    RenderAnimations's yield-generator closure (hung at the ctor, replayed
//    per Render) → Render()'s on-the-spot iteration (equivalent
//    per-enumeration evaluation — the generator reads the live anims).
//  - Game.CosmeticRandom 域不涉及;StandSequences 随机在 WithInfantryBody
//    (LocalRandom 域,见其头部)
//    No Game.CosmeticRandom domain here; the StandSequences pick lives in
//    WithInfantryBody (the LocalRandom domain, see its header).
#pragma once
import std;

#include "core/int2.hpp"
#include "core/wangle.hpp"
#include "core/wvec.hpp"
#include "gfx/animation.hpp"
#include "gfx/gfx_util.hpp"
#include "gfx/renderable.hpp"
#include "meta/generic_record.hpp"
#include "sim/actor.hpp"
#include "sim/actor_init.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::gfx {
class WorldRenderer;
}

namespace ora::mods {

using sim::Actor;
using sim::ActorInitializer;
using sim::TraitBase;

/// RenderSpritesInfo 的解析面(L30-45)
/// The parsed face of RenderSpritesInfo (L30-45).
struct RenderSpritesInfoData {
  std::string str_image;                              // L33 Image
  std::vector<std::pair<std::string, std::string>> vec_faction_images;  // L36
  std::string str_palette;                            // L40 Palette
  std::string str_player_palette = "player";          // L44 PlayerPalette

  static RenderSpritesInfoData Parse(const meta::RecordObject& rec_info);

  /// GetImage(L74-80)
  std::string GetImage(const game::ActorInfo& actor,
                       std::string_view str_faction) const;
};

/// RenderSprites(L83-301):渲染族的承载底座(动画登记/伤害前缀/调色板刷新)
/// RenderSprites (L83-301): the rendering family's carrying base (the
/// animation registry/the damage prefixes/the palette refresh).
class RenderSprites : public TraitBase,
                            public sim::ITick,
                            public sim::INotifyOwnerChanged,
                            public sim::INotifyEffectiveOwnerChanged,
                            public sim::IActorPreviewInitModifier,
                            public sim::IRender {
 public:
  /// AnimationWrapper(L93-142):登记动画 + 调色板 + 可见性/位置变化检测
  /// AnimationWrapper (L93-142): a registered animation + palette + the
  /// visibility/position change detection.
  struct AnimationWrapper {
    gfx::AnimationWithOffset* ptr_animation = nullptr;  // 上游 readonly 字段
    std::string str_palette;
    bool b_is_player_palette = false;
    gfx::PaletteReference* ptr_palette_reference = nullptr;

    bool b_cached_visible_ = false;               // L100-102 缓存三件
    WVec wvec_cached_offset_{};                   // the three caches.
    const gfx::ISpriteSequence* b_cached_sequence_ = nullptr;

    /// CachePalette(L111-114)
    void CachePalette(gfx::WorldRenderer& wr, const sim::Player& owner);

    /// OwnerChanged(L116-121):玩家调色板下次绘制重取
    /// OwnerChanged (L116-121): the player palette re-fetches on the next
    /// draw.
    void OwnerChanged();

    /// IsVisible(L123)
    bool IsVisible() const;

    /// Tick(L125-141):tick 动画并回报"渲染位/尺寸是否变化"
    /// Tick (L125-141): ticks the animation and reports whether the
    /// renderable position or size changed.
    bool Tick();
  };

  RenderSprites(const ActorInitializer& init,
                const RenderSpritesInfoData& info_data);

  ORA_TRAIT_INTERFACES(
      RenderSprites, OpenRA_Mods_Common_Traits_Render_RenderSprites,
      sim::ITick, sim::INotifyOwnerChanged,
      sim::INotifyEffectiveOwnerChanged, sim::IActorPreviewInitModifier,
      sim::IRender)

  const RenderSpritesInfoData& Info() const { return info_data_; }

  /// 测试面:wrapper 表视图 | the test face: the wrapper-table view.
  const std::vector<AnimationWrapper>& WrappersForTest() const {
    return vec_anims_;
  }

  /// MakeFacingFunc(L151-158):IFacing trait 的 facing 闭包(无 trait =
  /// WAngle.Zero;WithSpriteBody/WithInfantryBody 所需)
  /// MakeFacingFunc (L151-158): the IFacing trait's facing closure (no
  /// trait = WAngle.Zero; required by WithSpriteBody/WithInfantryBody).
  static std::function<WAngle()> MakeFacingFunc(Actor& self);

  /// GetImage(L167-173):按阵营解析图像名(缓存)
  /// GetImage (L167-173): the faction-resolved image name (cached).
  const std::string& GetImage(Actor& self);

  /// UpdatePalette(L175-180)
  void UpdatePalette();

  void OnOwnerChanged(Actor& self, sim::Player& old_owner,
                      sim::Player& new_owner) override;
  void OnEffectiveOwnerChanged(Actor& self, sim::Player& old_effective_owner,
                               sim::Player& new_effective_owner) override;

  void Tick(Actor& self) override;

  /// Add(L237-248):登记伴随动画(上游缺省 palette = Info 的二取一)
  /// Add (L237-248): registers a companion animation (upstream's default
  /// palette = Info's either-or).
  void Add(gfx::AnimationWithOffset& anim, std::string_view str_palette = {},
           bool b_is_player_palette = false);

  /// Remove(L250-253)
  void Remove(gfx::AnimationWithOffset& anim);

  /// UnnormalizeSequence(L255-268):剥既有伤害前缀
  /// UnnormalizeSequence (L255-268): strips any existing damage prefix.
  static std::string UnnormalizeSequence(std::string_view str_sequence);

  /// NormalizeSequence(L270-280):伤害状态前缀(有序四档,序列存在者胜)
  /// NormalizeSequence (L270-280): the damage-state prefix (the ordered
  /// four tiers; the first existing sequence wins).
  static std::string NormalizeSequence(const gfx::Animation& anim,
                                       sim::DamageState state,
                                       std::string_view str_sequence);

  /// AutoSelectionSize(L283-287)= AutoRenderSize(L289-295):首可见动画的
  /// 缩放图像尺寸(上游 FirstOrDefault = default(int2) 即 00)
  /// AutoSelectionSize (L283-287) = AutoRenderSize (L289-295): the first
  /// visible animation's scaled image size (upstream FirstOrDefault's
  /// default(int2), i.e. 00).
  int2 AutoSelectionSize() const { return AutoRenderSize(); }
  int2 AutoRenderSize() const;

  void ModifyActorPreviewInit(Actor& self,
                              sim::TypeDictionary& inits) override;

  // ———— IRender(L185-220;虚 —— RenderSpritesEditorOnly 覆空)————
  // ———— IRender (L185-220; virtual — RenderSpritesEditorOnly empties
  //      it) ————
  virtual void Render(Actor& self, gfx::WorldRenderer& wr,
                      std::vector<gfx::RenderItem>& vec_out);
  std::vector<Rectangle> ScreenBounds(Actor& self,
                                      gfx::WorldRenderer& wr) override;

  /// Animation::Deps 装配面(上游 ctor 的 world.Map 两依赖;测试/装配处
  /// 注入 —— Map::Sequences 未装配时上游本身即抛)
  /// The Animation::Deps assembly face (the upstream ctor's two world.Map
  /// dependencies; injected by the test/assembly site — upstream itself
  /// throws when Map::Sequences is unassembled).
  static gfx::Animation::Deps MakeAnimationDeps(Actor& self);

 private:
  /// RenderAnimations(L203-213):可见动画的逐项展开(上游 yield 序)
  /// RenderAnimations (L203-213): the per-animation expansion of the
  /// visible ones (upstream's yield order).
  void RenderAnimations(Actor& self, std::vector<gfx::RenderItem>& vec_out);

  RenderSpritesInfoData info_data_;
  std::string str_faction_;
  std::vector<AnimationWrapper> vec_anims_;  // 值存;AnimationWithOffset*
                                            // 由登记者拥有,指针稳定
  bool b_should_refresh_palettes_ = false;
  std::string str_cached_image_;
  std::unique_ptr<sim::FactionInit>
      up_owned_faction_init_;  // ModifyActorPreviewInit 的登记物(所有权面)
                               // ModifyActorPreviewInit's registration (the
                               // ownership face).
};


/// RenderSpritesEditorOnly(RenderSpritesEditorOnly.cs L27-33):编辑器
/// only 的隐形承载(Render 覆空;mpspawn 等编辑器摆位 actor 所需)
/// RenderSpritesEditorOnly (RenderSpritesEditorOnly.cs L27-33): the
/// invisible editor-only carrier (Render emptied out; needed by the
/// editor-placement actors like mpspawn).
class RenderSpritesEditorOnly final : public RenderSprites {
 public:
  RenderSpritesEditorOnly(const ActorInitializer& init,
                          const RenderSpritesInfoData& info)
      : RenderSprites(init, info) {}

  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_Render_RenderSpritesEditorOnly;
  gen::TypeId GetTraitTypeId() const override { return kTypeId; }
  static constexpr auto kTraitUpcasts =
      ora::sim::MakeTraitUpcasts<RenderSpritesEditorOnly, RenderSprites,
                                 sim::ITick, sim::INotifyOwnerChanged,
                                 sim::INotifyEffectiveOwnerChanged,
                                 sim::IActorPreviewInitModifier, sim::IRender>();
  std::span<const ora::sim::TraitUpcastEntry> TraitUpcasts() const override {
    return kTraitUpcasts;
  }

  void Render(Actor& self, gfx::WorldRenderer& wr,
              std::vector<gfx::RenderItem>& vec_out) override {
    // SpriteRenderable.None(空枚举 = 追加零项;共享出参不可清空 —— 第九批
    // 修正:与其它 IRender trait 同 actor 共存时 clear 会抹掉前者)
    // SpriteRenderable.None (an empty enumeration = appends zero items; the
    // shared out vector must not be cleared — the batch-9 fix: a clear
    // wipes the earlier traits when coexisting on one actor).
    (void)self;
    (void)wr;
    (void)vec_out;
  }
};

}  // namespace ora::mods
