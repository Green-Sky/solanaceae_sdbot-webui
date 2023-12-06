cmake_minimum_required(VERSION 3.14...3.24 FATAL_ERROR)

include(FetchContent)

add_subdirectory(./happyhttp)

if (
	NOT TARGET libsodium AND
	NOT TARGET unofficial-sodium::sodium AND
	NOT TARGET unofficial-sodium::sodium_config_public AND
	NOT TARGET sodium
)
	# for find sodium
	list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

	find_package(unofficial-sodium CONFIG QUIET)
	find_package(sodium QUIET)
	if(unofficial-sodium_FOUND) # vcpkg
		if(TARGET unofficial-sodium::sodium)
			#TODO: alias can not target another alias
			#target_link_libraries(toxcore unofficial-sodium::sodium)
			#add_library(libsodium ALIAS unofficial-sodium::sodium)

			add_library(libsodium INTERFACE)
			target_link_libraries(libsodium INTERFACE unofficial-sodium::sodium)
		endif()
		if(TARGET unofficial-sodium::sodium_config_public)
			#TODO: alias can not target another alias
			#target_link_libraries(toxcore unofficial-sodium::sodium_config_public)
			#add_library(libsodium ALIAS unofficial-sodium::sodium_config_public)

			add_library(libsodium INTERFACE)
			target_link_libraries(libsodium INTERFACE unofficial-sodium::sodium_config_public)
		endif()
	elseif(sodium_FOUND)
		#add_library(libsodium ALIAS sodium)
		add_library(libsodium INTERFACE)
		target_link_libraries(libsodium INTERFACE sodium)
	else()
		message(SEND_ERROR "missing libsodium")
	endif()
endif()

# TODO: move entt dep into solanaceae_contact
if (NOT TARGET EnTT::EnTT)
	FetchContent_Declare(EnTT
		GIT_REPOSITORY https://github.com/skypjack/entt.git
		GIT_TAG v3.12.2
		EXCLUDE_FROM_ALL
	)
	FetchContent_MakeAvailable(EnTT)
endif()

if (NOT TARGET solanaceae_util)
	FetchContent_Declare(solanaceae_util
		GIT_REPOSITORY https://github.com/Green-Sky/solanaceae_util.git
		GIT_TAG master
		EXCLUDE_FROM_ALL
	)
	FetchContent_MakeAvailable(solanaceae_util)
endif()

if (NOT TARGET solanaceae_contact)
	FetchContent_Declare(solanaceae_contact
		GIT_REPOSITORY https://github.com/Green-Sky/solanaceae_contact.git
		GIT_TAG master
		EXCLUDE_FROM_ALL
	)
	FetchContent_MakeAvailable(solanaceae_contact)
endif()

if (NOT TARGET solanaceae_message3)
	FetchContent_Declare(solanaceae_message3
		GIT_REPOSITORY https://github.com/Green-Sky/solanaceae_message3.git
		GIT_TAG master
		EXCLUDE_FROM_ALL
	)
	FetchContent_MakeAvailable(solanaceae_message3)
endif()

if (NOT TARGET solanaceae_plugin)
	FetchContent_Declare(solanaceae_plugin
		GIT_REPOSITORY https://github.com/Green-Sky/solanaceae_plugin.git
		GIT_TAG master
		EXCLUDE_FROM_ALL
	)
	FetchContent_MakeAvailable(solanaceae_plugin)
endif()

if (NOT TARGET nlohmann_json::nlohmann_json)
	FetchContent_Declare(json
		URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
		URL_HASH SHA256=d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d
		EXCLUDE_FROM_ALL
	)
	FetchContent_MakeAvailable(json)
endif()

