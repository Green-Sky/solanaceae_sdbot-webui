cmake_minimum_required(VERSION 3.9 FATAL_ERROR)

add_library(happyhttp STATIC
	./happyhttp/happyhttp.h
	./happyhttp/happyhttp.cpp
)

target_include_directories(happyhttp PUBLIC .)
target_compile_features(happyhttp PUBLIC cxx_std_11)

