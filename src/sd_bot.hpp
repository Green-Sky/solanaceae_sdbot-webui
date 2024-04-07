#pragma once

#include <solanaceae/util/span.hpp>
#include <solanaceae/message3/registry_message_model.hpp>
#include <solanaceae/contact/contact_model3.hpp>

#include <happyhttp/happyhttp.h>

#include <map>
#include <queue>
#include <string>
#include <memory>
#include <optional>
#include <random>

// fwd
struct ConfigModelI;

class SDBot : public RegistryMessageModelEventI {
	Contact3Registry& _cr;
	RegistryMessageModel& _rmm;
	ConfigModelI& _conf;

	//TransferManager& _tm;

	//std::map<uint64_t, std::variant<ContactFriend, ContactConference, ContactGroupPeer>> _task_map;
	std::map<uint64_t, Contact3> _task_map;
	std::queue<std::pair<uint64_t, std::string>> _prompt_queue;
	uint64_t _last_task_counter = 0;

	std::optional<uint64_t> _current_task;
	std::unique_ptr<happyhttp::Connection> _con;
	std::vector<uint8_t> _con_data;

	std::default_random_engine _rng;

	public:
		struct EndpointI {
			RegistryMessageModel& _rmm;
			std::default_random_engine& _rng;
			EndpointI(RegistryMessageModel& rmm, std::default_random_engine& rng) : _rmm(rmm), _rng(rng) {}
			virtual ~EndpointI(void) {}

			virtual bool handleResponse(Contact3 contact, ByteSpan data) = 0;
		};

	private:
		std::unique_ptr<EndpointI> _endpoint;

	public:
		SDBot(
			Contact3Registry& cr,
			RegistryMessageModel& rmm,
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

	public: // http cb
		void onHttpBegin(const happyhttp::Response* r);
		void onHttpData(const happyhttp::Response* r, const unsigned char* data, int n);
		void onHttpComplete(const happyhttp::Response* r);
};

