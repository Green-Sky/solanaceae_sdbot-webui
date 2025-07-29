#include "./sd_bot.hpp"

#include <solanaceae/util/config_model.hpp>
#include <solanaceae/contact/contact_store_i.hpp>

#include <solanaceae/contact/components.hpp>
#include <solanaceae/message3/components.hpp>

#include "./webapi_sdcpp_stduhpf_wip2.hpp"

#include <fstream>
#include <filesystem>
#include <chrono>

#include <iostream>
#include <stdexcept>

SDBot::SDBot(
	ContactStore4I& cs,
	RegistryMessageModelI& rmm,
	ConfigModelI& conf
) : _cs(cs), _rmm(rmm), _rmm_sr(_rmm.newSubRef(this)), _conf(conf) {
	_rng.seed(std::random_device{}());
	_rng.discard(3137);

	if (!_conf.has_string("SDBot", "endpoint_type")) {
		_conf.set("SDBot", "endpoint_type", std::string_view{"sdcpp_stduhpf_wip2"}); // default
	}

	//HACKy
	{ // construct endpoint
		const std::string_view endpoint_type = _conf.get_string("SDBot", "endpoint_type").value();
		if (endpoint_type == "sdcpp_stduhpf_wip2") {
			_endpoint = std::make_unique<WebAPI_sdcpp_stduhpf_wip2>(_conf);
		} else {
			throw std::runtime_error("missing endpoint type in config, cant continue!");
		}
	}

	// TODO: use defaults based on endpoint_type

	if (!_conf.has_string("SDBot", "server_host")) {
		_conf.set("SDBot", "server_host", std::string_view{"127.0.0.1"});
	}
	if (!_conf.has_int("SDBot", "server_port")) {
		_conf.set("SDBot", "server_port", int64_t(7860)); // automatic11 default
	}
	if (!_conf.has_string("SDBot", "url_txt2img")) {
		_conf.set("SDBot", "url_txt2img", std::string_view{"/sdapi/v1/txt2img"}); // automatic11 default
	}
	if (!_conf.has_int("SDBot", "width")) {
		_conf.set("SDBot", "width", int64_t(512));
	}
	if (!_conf.has_int("SDBot", "height")) {
		_conf.set("SDBot", "height", int64_t(512));
	}
	if (!_conf.has_int("SDBot", "steps")) {
		_conf.set("SDBot", "steps", int64_t(20));
	}
	if (!_conf.has_double("SDBot", "cfg_scale")) {
		_conf.set("SDBot", "cfg_scale", 6.5);
	}

	_rmm_sr.subscribe(RegistryMessageModel_Event::message_construct);
}

SDBot::~SDBot(void) {
}

float SDBot::iterate(void) {
	if (_current_task_id.has_value() != (_current_task != nullptr)) {
		std::cerr << "SDB inconsistent state\n";

		if (_current_task_id.has_value()) {
			_task_map.erase(_current_task_id.value());
			_current_task_id = std::nullopt;
		}

		_current_task.reset();
	}

	if (!_prompt_queue.empty() && !_current_task_id.has_value()) { // dequeue new task
		const auto& [task_id, prompt] = _prompt_queue.front();

		_current_task = _endpoint->txt2img(
			prompt,
			_conf.get_int("SDBot", "width").value_or(512),
			_conf.get_int("SDBot", "height").value_or(512)
		);
		if (_current_task == nullptr) {
			// ERROR
			std::cerr << "SDB error: call to txt2img failed!\n";
			_task_map.erase(task_id);
		} else {
			_current_task_id = task_id;
		}

		_prompt_queue.pop();
	}


	if (_current_task_id && _current_task && _current_task->ready()) {

		auto res_opt = _current_task->get();
		if (res_opt) {
			const auto& contact = _task_map.at(_current_task_id.value());

			std::filesystem::create_directories("sdbot_img_send");
			const std::string tmp_img_file_name = "sdbot_img_" + std::to_string(_rng()) + "." + res_opt.value().file_name;
			const std::string tmp_img_file_path = "sdbot_img_send/" + tmp_img_file_name;

			std::ofstream(tmp_img_file_path).write(reinterpret_cast<const char*>(res_opt.value().data.data()), res_opt.value().data.size());
			_rmm.sendFilePath(contact, tmp_img_file_name, tmp_img_file_path);
		} else {
			std::cerr << "SDB error: call to txt2img failed (task returned nothing)!\n";
		}


		_current_task_id.reset();
		_current_task.reset();
	}

	// if active web connection, 100ms
	if (_current_task_id.has_value()) {
		return 0.1f;
	} else {
		return 1.f;
	}
}

bool SDBot::onEvent(const Message::Events::MessageConstruct& e) {
	if (!e.e.all_of<Message::Components::ContactTo, Message::Components::ContactFrom, Message::Components::MessageText, Message::Components::TagUnread>()) {
		std::cout << "SDB: got message that is not";

		if (!e.e.all_of<Message::Components::ContactTo>()) {
			std::cout << " contact_to";
		}
		if (!e.e.all_of<Message::Components::ContactFrom>()) {
			std::cout << " contact_from";
		}
		if (!e.e.all_of<Message::Components::MessageText>()) {
			std::cout << " text";
		}
		if (!e.e.all_of<Message::Components::TagUnread>()) {
			std::cout << " unread";
		}

		std::cout << "\n";
		return false;
	}

	if (e.e.any_of<Message::Components::TagMessageIsAction>()) {
		std::cout << "SDB: got message that is";
		if (e.e.all_of<Message::Components::TagMessageIsAction>()) {
			std::cout << " action";
		}
		std::cout << "\n";
		return false;
	}

	std::string_view message_text = e.e.get<Message::Components::MessageText>().text;

	if (message_text.empty()) {
		// empty message?
		return false;
	}

	const auto contact_to = e.e.get<Message::Components::ContactTo>().c;
	const auto contact_from = e.e.get<Message::Components::ContactFrom>().c;

	const auto& cr = _cs.registry();

	const bool is_private = cr.any_of<Contact::Components::TagSelfWeak, Contact::Components::TagSelfStrong>(contact_to);

	if (is_private) {
		std::cout << "SDB private message " << message_text << " (l:" << message_text.size() << ")\n";
		{ // queue task
			const auto id = ++_last_task_counter;
			_task_map[id] = contact_from; // reply privately
			_prompt_queue.push(std::make_pair(uint64_t{id}, std::string{message_text}));
		}
	} else {
		assert(cr.all_of<Contact::Components::Self>(contact_to));
		const auto contact_self = cr.get<Contact::Components::Self>(contact_to).self;
		if (!cr.all_of<Contact::Components::Name>(contact_self)) {
			std::cerr << "SDB error: dont have self name\n";
			return false;
		}
		const auto& self_name = cr.get<Contact::Components::Name>(contact_self).name;

		const auto self_prefix = self_name + ": ";

		// check if for us. (starts with <botname>: )
		if (message_text.substr(0, self_prefix.size()) == self_prefix) {
			std::cout << "SDB public message " << message_text << " (l:" << message_text.size() << ")\n";
			const auto id = ++_last_task_counter;
			_task_map[id] = contact_to; // reply publicly
			_prompt_queue.push(std::make_pair(uint64_t{id}, std::string{message_text.substr(self_prefix.size())}));
		}
	}

	// TODO: mark message read?

	return true;
}

