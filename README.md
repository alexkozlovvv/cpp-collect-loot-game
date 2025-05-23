# Collect-loot-game
Проект многопользовательской игры, в которой персонаж, перемещаясь по карте, должен собирать предметы и относить их в место сбора. 

## Сборка

Инструкция тестировалась на Conan, установленном через `pip install conan==1.52.0`.

## Сборка под Linux

При сборке под Linux обязательно указываются флаги:
* `-s compiler.libcxx=libstdc++11`
* `-s build_type=???`

Вот пример конфигурирования для Release и Debug:
```
# mkdir -p build-release 
# cd build-release
# conan install .. --build=missing -s build_type=Release -s compiler.libcxx=libstdc++11
# cmake .. -DCMAKE_BUILD_TYPE=Release
# cd ..

# mkdir -p build-debug
# cd build-debug
# conan install .. --build=missing -s build_type=Debug -s compiler.libcxx=libstdc++11
# cmake .. -DCMAKE_BUILD_TYPE=Debug
# cd ..

```

## Сборка под Windows

Нужно выполнить два шага:
1. В conanfile.txt нужно изменить `cmake` на `cmake_multi`. После этого можно конфигурировать таким способом:
2. В CMakeLists.txt заменить `include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)` на `include(${CMAKE_BINARY_DIR}/conanbuildinfo_multi.cmake)`.

После этого можно запустить подобный снипет:

```
# mkdir build 
# cd build
# conan install .. --build=missing -s build_type=Debug
# conan install .. --build=missing -s build_type=Release
# conan install .. --build=missing -s build_type=RelWithDebInfo
# conan install .. --build=missing -s build_type=MinSizeRel
# cmake ..
```

В таком случае будут собираться все конфигурации (что не быстро). Можно сэкономить время, оставив только нужные.

Запускать сборку нужно только через родной cmd. В других консолях иногда возникают проблемы.

## Запуск докера

Можно собирать и запускать сервер одной командой (вернее, двумя) в докере. Делается это так:

```
docker build -t my_http_server .
docker run --rm -p 80:8080 my_http_server
```

## Запуск

В папке `build` выполнить команду
```sh
bin/game_server ../data/config.json ../static/
```
После этого можно открыть в браузере:
* http://127.0.0.1:8080/api/v1/maps для получения списка карт и
* http://127.0.0.1:8080/api/v1/map/map1 для получения подробной информации о карте `map1`
* http://127.0.0.1:8080/ для чтения статического контента и запуска игры (в каталоге static)