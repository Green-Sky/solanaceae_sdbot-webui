#pragma once

#include <solanaceae/util/span.hpp>
#include <solanaceae/message3/registry_message_model.hpp>
#include <solanaceae/contact/fwd.hpp>

#include <httplib.h>

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

	//TransferManager& _tm;

	//std::map<uint64_t, std::variant<ContactFriend, ContactConference, ContactGroupPeer>> _task_map;
	std::map<uint64_t, Contact4> _task_map;
	std::queue<std::pair<uint64_t, std::string>> _prompt_queue;
	uint64_t _last_task_counter = 0;

	std::optional<uint64_t> _current_task;
	std::shared_ptr<httplib::Client> _cli;
	std::optional<std::future<std::vector<uint8_t>>> _curr_future;

	std::default_random_engine _rng;

	public:
		struct EndpointI {
			RegistryMessageModelI& _rmm;
			std::default_random_engine& _rng;
			EndpointI(RegistryMessageModelI& rmm, std::default_random_engine& rng) : _rmm(rmm), _rng(rng) {}
			virtual ~EndpointI(void) {}

			virtual bool handleResponse(Contact4 contact, ByteSpan data) = 0;
		};

	private:
		std::unique_ptr<EndpointI> _endpoint;

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

	//protected: // tox events
		//bool onToxEvent(const Tox_Event_Friend_Message* e) override;
		//bool onToxEvent(const Tox_Event_Group_Message* e) override;
	protected: // mm
		bool onEvent(const Message::Events::MessageConstruct& e) override;
};

