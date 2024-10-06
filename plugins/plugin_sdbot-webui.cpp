#include <solanaceae/plugin/solana_plugin_v1.h>

#include "../src/sd_bot.hpp"

#include <solanaceae/util/config_model.hpp>

#include <entt/entt.hpp>
#include <entt/fwd.hpp>

#include <memory>
#include <iostream>

static std::unique_ptr<SDBot> g_sdbot = nullptr;

constexpr const char* plugin_name = "SDBot-webui";

extern "C" {

SOLANA_PLUGIN_EXPORT const char* solana_plugin_get_name(void) {
	return plugin_name;
}

SOLANA_PLUGIN_EXPORT uint32_t solana_plugin_get_version(void) {
	return SOLANA_PLUGIN_VERSION;
}

SOLANA_PLUGIN_EXPORT uint32_t solana_plugin_start(struct SolanaAPI* solana_api) {
	std::cout << "PLUGIN " << plugin_name << " START()\n";

	if (solana_api == nullptr) {
		return 1;
	}

	try {
		auto* cr = PLUG_RESOLVE_INSTANCE_VERSIONED(Contact3Registry, "1");
		auto* rmm = PLUG_RESOLVE_INSTANCE(RegistryMessageModelI);
		auto* conf = PLUG_RESOLVE_INSTANCE(ConfigModelI);

		// static store, could be anywhere tho
		// construct with fetched dependencies
		g_sdbot = std::make_unique<SDBot>(*cr, *rmm, *conf);

		// register types
		PLUG_PROVIDE_INSTANCE(SDBot, plugin_name, g_sdbot.get());
	} catch (const ResolveException& e) {
		std::cerr << "PLUGIN " << plugin_name << " " << e.what << "\n";
		return 2;
	}

	return 0;
}

SOLANA_PLUGIN_EXPORT void solana_plugin_stop(void) {
	std::cout << "PLUGIN " << plugin_name << " STOP()\n";

	g_sdbot.reset();
}

SOLANA_PLUGIN_EXPORT float solana_plugin_tick(float delta) {
	(void)delta;
	return g_sdbot->iterate();
}

} // extern C

