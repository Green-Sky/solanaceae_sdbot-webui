#include "./sd_bot.hpp"

#include <solanaceae/util/config_model.hpp>

#include <solanaceae/contact/components.hpp>
#include <solanaceae/message3/components.hpp>

#include <nlohmann/json.hpp>
#include <sodium.h>

#include <vector>
#include <fstream>
#include <filesystem>
#include <cstdint>
#include <iostream>

SDBot::SDBot(
	Contact3Registry& cr,
	RegistryMessageModel& rmm,
	ConfigModelI& conf
) : _cr(cr), _rmm(rmm), _conf(conf) {
	_rng.seed(std::random_device{}());
	_rng.discard(3137);

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

	_rmm.subscribe(this, RegistryMessageModel_Event::message_construct);
}

SDBot::~SDBot(void) {
}

float SDBot::iterate(void) {
	if (static_cast<bool>(_con) && _con->outstanding()) {
		_con->pump();
	} else if (!_prompt_queue.empty()) { // dequeue new task
		const auto& [task_id, prompt] = _prompt_queue.front();

		_current_task = task_id;

		// TODO: reuse connection?
		// TODO: read from config
		_con = std::make_unique<happyhttp::Connection>("127.0.0.1", 7860);
		_con->setcallbacks(
			+[](const happyhttp::Response* r, void* ud) { static_cast<SDBot*>(ud)->onHttpBegin(r); },
			+[](const happyhttp::Response* r, void* ud, const uint8_t* data, int n) { static_cast<SDBot*>(ud)->onHttpData(r, data, n); },
			+[](const happyhttp::Response* r, void* ud) { static_cast<SDBot*>(ud)->onHttpComplete(r); },
			this
		);

		static const char* headers []  {
			"accept: application/json",
			"Content-Type: application/json",
			nullptr,
		};

		nlohmann::json j_body;
		j_body["width"] = _conf.get_int("SDBot", "width").value_or(512);
		j_body["height"] = _conf.get_int("SDBot", "height").value_or(512);

		//j_body["prompt"] = prompt;
		//"<lora:lcm-lora-sdv1-5:1>"
		j_body["prompt"] = std::string{_conf.get_string("SDBot", "prompt_prefix").value_or("")} + prompt;
		// TODO: negative prompt

		j_body["seed"] = -1;
#if 0
		j_body["steps"] = 20;
		//j_body["steps"] = 5;
		j_body["cfg_scale"] = 6.5;
		j_body["sampler_index"] = "Euler a";
#else
		//j_body["steps"] = 4;
		j_body["steps"] = _conf.get_int("SDBot", "steps").value_or(20);
		//j_body["cfg_scale"] = 1;
		j_body["cfg_scale"] = _conf.get_double("SDBot", "cfg_scale").value_or(6.5);
		//j_body["sampler_index"] = "LCM Test";
		j_body["sampler_index"] = std::string{_conf.get_string("SDBot", "sampler_index").value_or("Euler a")};
#endif

		j_body["batch_size"] = 1;
		j_body["n_iter"] = 1;
		j_body["restore_faces"] = false;
		j_body["tiling"] = false;
		j_body["enable_hr"] = false;

		std::string body = j_body.dump();

		try {
			_con->request("POST", "/sdapi/v1/txt2img", headers, reinterpret_cast<const uint8_t*>(body.data()), body.size());
		} catch (const happyhttp::Wobbly& e) {
			std::cerr << "SDB http request error: " << e.what() << "\n";
			// cleanup
			_task_map.erase(_current_task.value());
			_current_task = std::nullopt;
			_con.reset();
		}

		_prompt_queue.pop();
	}

	// if active web connection, 5ms
	return static_cast<bool>(_con) ? 0.005f : 1.f;
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

	const bool is_private = _cr.any_of<Contact::Components::TagSelfWeak, Contact::Components::TagSelfStrong>(contact_to);

	if (is_private) {
		std::cout << "SDB private message " << message_text << " (l:" << message_text.size() << ")\n";
		{ // queue task
			const auto id = ++_last_task_counter;
			_task_map[id] = contact_from; // reply privately
			_prompt_queue.push(std::make_pair(uint64_t{id}, std::string{message_text}));
		}
	} else {
		assert(_cr.all_of<Contact::Components::Self>(contact_to));
		const auto contact_self = _cr.get<Contact::Components::Self>(contact_to).self;
		if (!_cr.all_of<Contact::Components::Name>(contact_self)) {
			std::cerr << "SDB error: dont have self name\n";
			return false;
		}
		const auto& self_name = _cr.get<Contact::Components::Name>(contact_self).name;

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

void SDBot::onHttpBegin(const happyhttp::Response* r) {
	std::cout << "SDB http begin " << r->getstatus() << " " << r->getreason() << "\n";
	// TODO: handle errors
	_con_data.clear();
}

void SDBot::onHttpData(const happyhttp::Response* /*r*/, const unsigned char* data, int n) {
	//std::cout << "SDB http data\n";
	// TODO: handle errors
	for (int i = 0; i < n; i++) {
		_con_data.push_back(data[i]);
	}
}

void SDBot::onHttpComplete(const happyhttp::Response* r) {
	std::cout << "SDB http complete " << r->getstatus() << " " << r->getreason() << "\n";
	if (r->getstatus() == happyhttp::OK) {
		std::cout << "SDB data\n";
		//std::cout << std::string_view{reinterpret_cast<const char*>(_con_data.data()), _con_data.size()} << "\n";

		// extract json result
		const auto j = nlohmann::json::parse(std::string_view{reinterpret_cast<const char*>(_con_data.data()), _con_data.size()});

		if (j.count("images") && !j.at("images").empty() && j.at("images").is_array()) {
			for (const auto& i_j : j.at("images").items()) {
				// decode data (base64)
				std::vector<uint8_t> png_data(_con_data.size()); // just init to upper bound
				size_t decoded_size {0};
				sodium_base642bin(
					png_data.data(), png_data.size(),
					i_j.value().get<std::string>().data(), i_j.value().get<std::string>().size(),
					" \n\t",
					&decoded_size,
					nullptr,
					sodium_base64_VARIANT_ORIGINAL
				);
				png_data.resize(decoded_size);

				// hand png to download manager
				const auto& contact = _task_map.at(_current_task.value());

				std::filesystem::create_directories("sdbot_img_send");
				//const std::string tmp_img_file_name = "sdbot_img_" + std::to_string(_current_task.value()) + ".png";
				const std::string tmp_img_file_name = "sdbot_img_" + std::to_string(_rng()) + ".png";
				const std::string tmp_img_file_path = "sdbot_img_send/" + tmp_img_file_name;

				std::ofstream(tmp_img_file_path).write(reinterpret_cast<const char*>(png_data.data()), png_data.size());
				_rmm.sendFilePath(contact, tmp_img_file_name, tmp_img_file_path);
			}
		} else {
			std::cerr << "SDB json response did not contain images?\n";
		}

		_task_map.erase(_current_task.value());
		_current_task = std::nullopt;
		_con.reset();
	}
}

