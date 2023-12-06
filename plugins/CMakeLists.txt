cmake_minimum_required(VERSION 3.14...3.24 FATAL_ERROR)

add_library(plugin_sdbot-webui SHARED
	./plugin_sdbot-webui.cpp
)

target_link_libraries(plugin_sdbot-webui PUBLIC
	solanaceae_plugin
	solanaceae_sdbot-webui
)

