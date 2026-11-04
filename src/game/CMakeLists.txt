add_library(ora_game STATIC game.cpp miniyaml.cpp miniyaml_merge.cpp ruleset.cpp
  map_bin.cpp map_read.cpp tileset.cpp
  pathfinding/priority_queue.cpp pathfinding/path_graph.cpp pathfinding/astar.cpp pathfinding/wangle.cpp pathfinding/move_sim.cpp
  world/actor.cpp world/trait.cpp world/trait_container.cpp world/world.cpp world/sync_hash.cpp
  network/order.cpp network/order_packet.cpp network/order_manager.cpp
  support/mersenne_twister.cpp support/sha1.cpp filesystem/inflate.cpp filesystem/zip_read.cpp)
target_sources(ora_game PRIVATE game.hpp miniyaml.hpp miniyaml_merge.hpp ruleset.hpp
  map_bin.hpp map_read.hpp tileset.hpp
  pathfinding/priority_queue.hpp pathfinding/path_graph.hpp pathfinding/astar.hpp pathfinding/wangle.hpp pathfinding/move_sim.hpp
  world/actor.hpp world/trait.hpp world/trait_container.hpp world/world.hpp world/sync_hash.hpp
  network/order.hpp network/order_packet.hpp network/order_manager.hpp
  support/mersenne_twister.hpp support/sha1.hpp filesystem/inflate.hpp filesystem/zip_read.hpp)
target_include_directories(ora_game PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(ora_game PUBLIC ora_std)
