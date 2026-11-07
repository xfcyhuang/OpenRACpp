add_library(ora_game STATIC game.cpp sound_info.cpp music_info.cpp sound/sound.cpp miniyaml.cpp miniyaml_merge.cpp ruleset.cpp
  map_bin.cpp map_read.cpp tileset.cpp
  render/palette.cpp render/lcw.cpp render/xor_delta.cpp render/tmp_ra.cpp render/shp_td.cpp
  pathfinding/priority_queue.cpp pathfinding/path_graph.cpp pathfinding/astar.cpp pathfinding/wangle.cpp pathfinding/move_sim.cpp
  combat/weapon.cpp combat/warhead_damage.cpp combat/health.cpp combat/armament.cpp combat/projectile.cpp combat/combat_sim.cpp
  world/actor.cpp world/trait.cpp world/trait_container.cpp world/world.cpp world/sync_hash.cpp
  network/order.cpp network/order_packet.cpp network/order_manager.cpp
  replay/replay_format.cpp replay/replay_record.cpp replay/replay_player.cpp replay/replay_world.cpp
  production/production.cpp
  economy/economy.cpp
  input/input_events.hpp input/screen_map.cpp input/selection.cpp input/selection_logic.cpp input/order_generator.cpp input/production_panel.cpp
  support/mersenne_twister.cpp support/sha1.cpp filesystem/inflate.cpp filesystem/zip_read.cpp)
target_sources(ora_game PRIVATE game.hpp miniyaml.hpp miniyaml_merge.hpp ruleset.hpp
  map_bin.hpp map_read.hpp tileset.hpp
  render/palette.hpp render/lcw.hpp render/xor_delta.hpp render/tmp_ra.hpp render/shp_td.hpp
  pathfinding/priority_queue.hpp pathfinding/path_graph.hpp pathfinding/astar.hpp pathfinding/wangle.hpp pathfinding/move_sim.hpp
  combat/weapon.hpp combat/warhead_damage.hpp combat/health.hpp combat/armament.hpp combat/projectile.hpp combat/combat_sim.hpp
  world/actor.hpp world/trait.hpp world/trait_container.hpp world/world.hpp world/sync_hash.hpp
  network/order.hpp network/order_packet.hpp network/order_manager.hpp
  replay/replay_format.hpp replay/replay_record.hpp replay/replay_player.hpp replay/replay_world.hpp
  production/production.hpp
  economy/economy.hpp
  input/screen_map.hpp input/selection.hpp input/selection_logic.hpp input/order_generator.hpp input/production_panel.hpp
  support/mersenne_twister.hpp support/sha1.hpp filesystem/inflate.hpp filesystem/zip_read.hpp)
target_include_directories(ora_game PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(ora_game PUBLIC ora_std)
