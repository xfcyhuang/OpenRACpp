add_library(ora_game STATIC game.cpp sound_info.cpp music_info.cpp sound/sound.cpp sound/music_player.cpp sound/sfx.cpp miniyaml.cpp miniyaml_merge.cpp settings.cpp ruleset.cpp
  map_bin.cpp map_read.cpp tileset.cpp
  ui/expression.cpp ui/widget.cpp ui/widget_loader.cpp ui/chrome_provider.cpp ui/widget_render.cpp
  ui/mainmenu_logic.cpp ui/settings_panel.cpp
  render/palette.cpp render/lcw.cpp render/xor_delta.cpp render/tmp_ra.cpp render/shp_td.cpp render/font.cpp render/png_decode.cpp
  pathfinding/priority_queue.cpp pathfinding/path_graph.cpp pathfinding/astar.cpp pathfinding/wangle.cpp pathfinding/move_sim.cpp
  combat/weapon.cpp combat/warhead_damage.cpp combat/health.cpp combat/armament.cpp combat/projectile.cpp combat/combat_sim.cpp
  world/actor.cpp world/trait.cpp world/trait_container.cpp world/world.cpp world/sync_hash.cpp world/bot.cpp world/movement.cpp
  network/order.cpp network/order_packet.cpp network/order_manager.cpp network/lobby_session.cpp
  replay/replay_format.cpp replay/replay_record.cpp replay/replay_player.cpp replay/replay_world.cpp
  production/production.cpp
  economy/economy.cpp
  input/screen_map.cpp input/selection.cpp input/selection_logic.cpp input/order_generator.cpp input/production_panel.cpp
  input/hotkey.cpp input/hotkey_registry.cpp input/tap_history.cpp input/viewport_controller.cpp input/hotkey_actions.cpp
  support/mersenne_twister.cpp support/sha1.cpp filesystem/inflate.cpp filesystem/zip_read.cpp)
target_sources(ora_game PUBLIC FILE_SET cxx_modules TYPE CXX_MODULES FILES
  game.cppm miniyaml.cppm filesystem/filesystem.cppm support/support.cppm
  pathfinding/pathfinding.cppm combat/combat.cppm render/render.cppm
  world/world.cppm sound/sound.cppm input/input.cppm ui/ui.cppm)
target_include_directories(ora_game PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(ora_game PUBLIC ora_std ora_platforms)
