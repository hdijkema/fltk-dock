cmake_minimum_required(VERSION 3.16)

project(fltk-dock LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(fltk-dock SHARED
  fltk-dock_global.h
  fltk_dock.cpp
  fltk_dock.h
)

target_compile_definitions(fltk-dock PRIVATE FLTK_DOCK_LIBRARY)
