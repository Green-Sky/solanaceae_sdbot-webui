cmake_minimum_required(VERSION 3.9 FATAL_ERROR)

add_library(solanaceae_sdbot-webui STATIC
	./sd_bot.hpp
	./sd_bot.cpp
)

target_compile_features(solanaceae_sdbot-webui PUBLIC cxx_std_17)
target_link_libraries(solanaceae_sdbot-webui PUBLIC
	httplib::httplib
	solanaceae_contact
	solanaceae_message3
	nlohmann_json::nlohmann_json
	libsodium
)

