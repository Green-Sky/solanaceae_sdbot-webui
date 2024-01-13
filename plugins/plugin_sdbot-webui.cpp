#include <solanaceae/plugin/solana_plugin_v1.h>

#include "../src/sd_bot.hpp"

#include <memory>
#include <iostream>

#define RESOLVE_INSTANCE(x) static_cast<x*>(solana_api->resolveInstance(#x))
#define PROVIDE_INSTANCE(x, p, v) solana_api->provideInstance(#x, p, static_cast<x*>(v))

static std::unique_ptr<SDBot> g_sdbot = nullptr;

extern "C" {

SOLANA_PLUGIN_EXPORT const char* solana_plugin_get_name(void) {
	return "SDBot-webui";
}

SOLANA_PLUGIN_EXPORT uint32_t solana_plugin_get_version(void) {
	return SOLANA_PLUGIN_VERSION;
}

SOLANA_PLUGIN_EXPORT uint32_t solana_plugin_start(struct SolanaAPI* solana_api) {
	std::cout << "PLUGIN SDB START()\n";

	if (solana_api == nullptr) {
		return 1;
	}

	Contact3Registry* cr;
	RegistryMessageModel* rmm = nullptr;
	ConfigModelI* conf = nullptr;

	{ // make sure required types are loaded
		cr = RESOLVE_INSTANCE(Contact3Registry);
		rmm = RESOLVE_INSTANCE(RegistryMessageModel);
		conf = RESOLVE_INSTANCE(ConfigModelI);

		if (cr == nullptr) {
			std::cerr << "PLUGIN SDB missing Contact3Registry\n";
			return 2;
		}

		if (rmm == nullptr) {
			std::cerr << "PLUGIN SDB missing RegistryMessageModel\n";
			return 2;
		}

		if (conf == nullptr) {
			std::cerr << "PLUGIN SDB missing ConfigModelI\n";
			return 2;
		}
	}

	// static store, could be anywhere tho
	// construct with fetched dependencies
	g_sdbot = std::make_unique<SDBot>(*cr, *rmm, *conf);

	// register types
	PROVIDE_INSTANCE(SDBot, "SDBot", g_sdbot.get());

	return 0;
}

SOLANA_PLUGIN_EXPORT void solana_plugin_stop(void) {
	std::cout << "PLUGIN SDB STOP()\n";

	g_sdbot.reset();
}

SOLANA_PLUGIN_EXPORT float solana_plugin_tick(float delta) {
	(void)delta;
	//std::cout << "PLUGIN SDB TICK()\n";
	return g_sdbot->iterate();
}

} // extern C

