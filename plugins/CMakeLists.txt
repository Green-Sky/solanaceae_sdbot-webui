cmake_minimum_required(VERSION 3.14...3.24 FATAL_ERROR)

add_library(plugin_sdbot-webui SHARED
	./plugin_sdbot-webui.cpp
)

set_target_properties(plugin_sdbot-webui PROPERTIES
	C_VISIBILITY_PRESET hidden
	POSITION_INDEPENDENT_CODE ON
)
target_compile_definitions(plugin_sdbot-webui PUBLIC ENTT_API_IMPORT)

target_link_libraries(plugin_sdbot-webui PUBLIC
	solanaceae_plugin
	solanaceae_sdbot-webui
)

