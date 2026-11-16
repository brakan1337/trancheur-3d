cmake_minimum_required(VERSION 3.25)
project(trancheur VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(TRANCHEUR_TESTS "Compiler les tests" ON)
option(TRANCHEUR_SANITIZERS "Compiler avec AddressSanitizer et UndefinedBehaviorSanitizer" OFF)

include(FetchContent)

# Clipper2 : la référence pour décaler et découper des polygones, utilisée par plusieurs trancheurs.
# SYSTEM : ses en-têtes ne déclenchent pas nos avertissements bloquants.
set(CLIPPER2_UTILS OFF CACHE BOOL "" FORCE)
set(CLIPPER2_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CLIPPER2_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(clipper2
  GIT_REPOSITORY https://github.com/AngusJohnson/Clipper2.git
  GIT_TAG Clipper2_1.5.4
  GIT_SHALLOW TRUE
  SOURCE_SUBDIR CPP
  SYSTEM)
FetchContent_MakeAvailable(clipper2)

function(avertissements_bloquants cible)
  if(MSVC)
    target_compile_options(${cible} PRIVATE /W4 /WX /permissive- /utf-8)
  else()
    target_compile_options(${cible} PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Werror)
  endif()
  if(TRANCHEUR_SANITIZERS AND NOT MSVC)
    target_compile_options(${cible} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(${cible} PRIVATE -fsanitize=address,undefined)
  endif()
endfunction()

add_library(trancheur
  src/maillage.cpp
  src/formes.cpp
  src/tranchage.cpp
  src/trajectoires.cpp
  src/gcode.cpp
  src/apercu.cpp)
target_include_directories(trancheur PUBLIC include)
target_link_libraries(trancheur PUBLIC Clipper2)
avertissements_bloquants(trancheur)

add_executable(trancheur_cli src/main.cpp)
set_target_properties(trancheur_cli PROPERTIES OUTPUT_NAME trancheur)
target_link_libraries(trancheur_cli PRIVATE trancheur)
avertissements_bloquants(trancheur_cli)

add_executable(banc bench/banc.cpp)
target_link_libraries(banc PRIVATE trancheur)
avertissements_bloquants(banc)

if(TRANCHEUR_TESTS)
  enable_testing()
  FetchContent_Declare(doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG v2.5.3
    GIT_SHALLOW TRUE
    SYSTEM)
  FetchContent_MakeAvailable(doctest)

  add_executable(tests
    tests/principal.cpp
    tests/test_maillage.cpp
    tests/test_tranchage.cpp
    tests/test_trajectoires.cpp
    tests/test_gcode.cpp)
  target_link_libraries(tests PRIVATE trancheur doctest::doctest)
  avertissements_bloquants(tests)
  add_test(NAME tests COMMAND tests)
endif()
