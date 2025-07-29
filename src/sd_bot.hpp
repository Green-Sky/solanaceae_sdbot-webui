#pragma once

#include <solanaceae/util/span.hpp>
#include <solanaceae/message3/registry_message_model.hpp>
#include <solanaceae/contact/fwd.hpp>

#include "./webapi_interface.hpp"

#include <map>
#include <vector>
#include <queue>
#include <string>
#include <memory>
#include <optional>
#include <random>
#include <future>

// fwd
struct ConfigModelI;

class SDBot : public RegistryMessageModelEventI {
	ContactStore4I& _cs;
	RegistryMessageModelI& _rmm;
	RegistryMessageModelI::SubscriptionReference _rmm_sr;
	ConfigModelI& _conf;

	std::map<uint64_t, Contact4> _task_map;
	std::queue<std::pair<uint64_t, std::string>> _prompt_queue;
	uint64_t _last_task_counter = 0;

	std::optional<uint64_t> _current_task_id;
	std::shared_ptr<WebAPITaskI> _current_task;

	std::default_random_engine _rng;

	private:
		std::unique_ptr<WebAPII> _endpoint;

	public:
		SDBot(
			ContactStore4I& cs,
			RegistryMessageModelI& rmm,
			ConfigModelI& conf
		);
		~SDBot(void);

		float iterate(void);

	public: // conf
		bool use_webp_for_friends = true;
		bool use_webp_for_groups = true;

	protected: // mm
		bool onEvent(const Message::Events::MessageConstruct& e) override;
};

