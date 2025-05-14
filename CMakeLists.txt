cmake_minimum_required(VERSION 3.11)

project(game_server CXX)
set(CMAKE_CXX_STANDARD 20)

# Включение CTest
# include(CTest)
# include(${CONAN_BUILD_DIRS_CATCH2}/Catch.cmake)

include(${CMAKE_BINARY_DIR}/conanbuildinfo_multi.cmake)
conan_basic_setup(TARGETS)

# find_package(Boost 1.78.0 REQUIRED)
# if(Boost_FOUND)
#   include_directories(${Boost_INCLUDE_DIRS})
# endif()

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# Добавляем библиотеку, указывая, что она статическая.
add_library(MyLib STATIC 
	src/extra_data.h
	src/loot_generator.cpp
	src/loot_generator.h
	src/collision_detector.cpp
	src/collision_detector.h
	src/geom.h
	src/app.h
	src/app.cpp
	src/app_serialization.h
	src/logging.cpp
	src/logging.h
	src/http_server.cpp
	src/http_server.h
	src/sdk.h
	src/model.h
	src/model_serialization.h
	src/model.cpp
	src/tagged.h
	src/boost_json.cpp
	src/json_loader.h
	src/json_loader.cpp
	src/request_handler.cpp
	src/request_handler.h
	src/infrastructure.cpp
	src/infrastructure.h
	src/connection_pool.h
	src/postgres.h
)
# Подключаем библиотеку к game_server
target_link_libraries(MyLib PUBLIC CONAN_PKG::boost Threads::Threads CONAN_PKG::libpq CONAN_PKG::libpqxx)

add_executable(game_server src/main.cpp)
add_executable(game_server_tests 
	tests/loot_generator_tests.cpp 
	tests/model-tests.cpp
	tests/collision-detector-tests.cpp
	tests/state-serialization-tests.cpp
)

target_link_libraries(game_server MyLib)
target_link_libraries(game_server_tests CONAN_PKG::catch2 MyLib)



# add_executable(game_server
# 	src/main.cpp
# 	src/extra_data.h
# 	src/loot_generator.cpp
# 	src/loot_generator.h
# 	src/app.h
# 	src/app.cpp
# 	src/logging.cpp
# 	src/logging.h
# 	src/http_server.cpp
# 	src/http_server.h
# 	src/sdk.h
# 	src/model.h
# 	src/model.cpp
# 	src/tagged.h
# 	src/boost_json.cpp
# 	src/json_loader.h
# 	src/json_loader.cpp
# 	src/request_handler.cpp
# 	src/request_handler.h
# )
# target_link_libraries(game_server PRIVATE Threads::Threads)

#target_include_directories(game_server PRIVATE CONAN_PKG::boost)
#target_link_libraries(game_server PRIVATE CONAN_PKG::boost Threads::Threads)

# add_executable(game_server_tests
# 	tests/loot_generator_tests.cpp
# 	tests/model-tests.cpp
# 	src/loot_generator.h
# 	src/loot_generator.cpp
# 	src/model.h
#     src/model.cpp
#     src/tagged.h
# )
#target_link_libraries(game_server_tests PRIVATE CONAN_PKG::catch2)

# используем "импортированную" цель CONAN_PKG::boost
# target_include_directories(game_server PRIVATE CONAN_PKG::boost Threads::Threads)
# target_link_libraries(game_server CONAN_PKG::boost)
# target_link_libraries(game_server PRIVATE Threads::Threads)
