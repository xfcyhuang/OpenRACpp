// UPSTREAM: OpenRA.Mods.Common/Traits/World/MusicPlaylist.cs @e180e3225
//          (注册装配)。
#include "meta/generic_record.hpp"
#include "mods/music_playlist.hpp"
#include "sim/actor.hpp"
#include "sim/actor_init.hpp"
#include "sim/trait_registry.hpp"
#include "sound/sound.hpp"
#include "ui/ui.hpp"

namespace ora::mods {

void RegisterCommonTraitsBatch18() {
  auto& registry = sim::TraitRegistry::Instance();

  registry.Register(
      "MusicPlaylistInfo",
      [](const meta::RecordObject& rec_info, sim::ActorInitializer& init,
         ora::WorldArena& arena) -> sim::TraitBase* {
        return arena.Create<MusicPlaylist>(
            init.Self().world(), MusicPlaylistInfoData::Parse(rec_info),
            ora::ui::Ui::Instance().SoundDeps());
      });
}

}  // namespace ora::mods
