#include "./sd_bot.hpp"

#include <solanaceae/util/config_model.hpp>

#include <solanaceae/contact/components.hpp>
#include <solanaceae/message3/components.hpp>

#include <nlohmann/json.hpp>
#include <sodium.h>

#include <fstream>
#include <filesystem>
#include <chrono>

#include <iostream>

struct Automatic1111_v1_Endpoint : public SDBot::EndpointI {
	Automatic1111_v1_Endpoint(RegistryMessageModelI& rmm, std::default_random_engine& rng) : SDBot::EndpointI(rmm, rng) {}

	bool handleResponse(Contact3 contact, ByteSpan data) override {
		//std::cout << std::string_view{reinterpret_cast<const char*>(data.ptr), data.size} << "\n";

		// extract json result
		const auto j = nlohmann::json::parse(
			std::string_view{reinterpret_cast<const char*>(data.ptr), data.size},
			nullptr,
			false
		);
		//std::cout << "json dump: " << j.dump() << "\n";

		if (j.count("images") && !j.at("images").empty() && j.at("images").is_array()) {
			for (const auto& i_j : j.at("images").items()) {
				// decode data (base64)
				std::vector<uint8_t> png_data(data.size); // just init to upper bound
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

				std::filesystem::create_directories("sdbot_img_send");
				//const std::string tmp_img_file_name = "sdbot_img_" + std::to_string(_current_task.value()) + ".png";
				const std::string tmp_img_file_name = "sdbot_img_" + std::to_string(_rng()) + ".png";
				const std::string tmp_img_file_path = "sdbot_img_send/" + tmp_img_file_name;

				std::ofstream(tmp_img_file_path).write(reinterpret_cast<const char*>(png_data.data()), png_data.size());
				_rmm.sendFilePath(contact, tmp_img_file_name, tmp_img_file_path);
			}
		} else {
			std::cerr << "SDB json response did not contain images?\n";
			return false;
		}

		return true;
	}
};

struct SDcpp_wip1_Endpoint : public SDBot::EndpointI {
	SDcpp_wip1_Endpoint(RegistryMessageModelI& rmm, std::default_random_engine& rng) : SDBot::EndpointI(rmm, rng) {}

	bool handleResponse(Contact3 contact, ByteSpan data) override {
		//std::cout << std::string_view{reinterpret_cast<const char*>(data.ptr), data.size} << "\n";

		std::string_view data_str {reinterpret_cast<const char*>(data.ptr), data.size};
		auto nl_pos {std::string_view::npos};
		bool succ {false};
		do {
			// for each line, should be "data: <json>" or empty
			nl_pos = data_str.find_first_of('\n');

			// npos is also valid
			auto line = data_str.substr(0, nl_pos);

			// at least minimum viable
			if (line.size() >= std::string_view{"data: {}"}.size()) {
				//std::cout << "got data line!!!!!!!!!!!:\n";
				//std::cout << line << "\n";
				line.remove_prefix(6);

				const auto j = nlohmann::json::parse(
					line,
					nullptr,
					false
				);

				if (
					!j.empty() &&
					j.value("type", "notimag") == "image" &&
					j.contains("data") &&
					j.at("data").is_string()
				) {
					const auto& img_data_str = j.at("data").get<std::string>();
					// decode data (base64)
					std::vector<uint8_t> png_data(img_data_str.size()); // just init to upper bound
					size_t decoded_size {0};
					sodium_base642bin(
						png_data.data(), png_data.size(),
						img_data_str.data(), img_data_str.size(),
						" \n\t",
						&decoded_size,
						nullptr,
						sodium_base64_VARIANT_ORIGINAL
					);
					png_data.resize(decoded_size);

					std::filesystem::create_directories("sdbot_img_send");
					//const std::string tmp_img_file_name = "sdbot_img_" + std::to_string(_current_task.value()) + ".png";
					const std::string tmp_img_file_name = "sdbot_img_" + std::to_string(_rng()) + ".png";
					const std::string tmp_img_file_path = "sdbot_img_send/" + tmp_img_file_name;

					std::ofstream(tmp_img_file_path).write(reinterpret_cast<const char*>(png_data.data()), png_data.size());
					succ = _rmm.sendFilePath(contact, tmp_img_file_name, tmp_img_file_path);
				}
			}

			if (nl_pos == std::string_view::npos || nl_pos+1 >= data_str.size()) {
				break;
			}

			data_str = data_str.substr(nl_pos+1);
		} while (nl_pos != std::string_view::npos);

		return succ;
	}
};

struct SDcpp_stduhpf_wip1_Endpoint : public SDBot::EndpointI {
	SDcpp_stduhpf_wip1_Endpoint(RegistryMessageModelI& rmm, std::default_random_engine& rng) : SDBot::EndpointI(rmm, rng) {}

	bool handleResponse(Contact3 contact, ByteSpan data) override {
		bool succ = true;

		try {
			// extract json result
			const auto j = nlohmann::json::parse(
				std::string_view{reinterpret_cast<const char*>(data.ptr), data.size}
			);

			if (!j.is_array()) {
				std::cerr << "SDB: json response was not an array\n";
				return false;
			}

			for (const auto& j_entry : j) {
				if (!j_entry.is_object()) {
					std::cerr << "SDB warning: non object entry, skipping\n";
					continue;
				}

				// for each returned image
				// "channel": 3, // rgb?
				// "data": base64 encoded image file
				// "encoding": "png",
				// "height": 512,
				// "width": 512

				if (j_entry.contains("encoding")) {
					if (!j_entry["encoding"].is_string() || j_entry["encoding"] != "png") {
						std::cerr << "SDB warning: unknown encoding '" << j_entry["encoding"] << "'\n";
					}
				}

				if (!j_entry.contains("data") || !j_entry.at("data").is_string()) {
					std::cerr << "SDB warning: non data entry, skipping\n";
					continue;
				}

				const auto& img_data_str = j_entry.at("data").get<std::string>();
				// decode data (base64)
				std::vector<uint8_t> png_data(img_data_str.size()); // just init to upper bound
				size_t decoded_size {0};
				sodium_base642bin(
					png_data.data(), png_data.size(),
					img_data_str.data(), img_data_str.size(),
					" \n\t",
					&decoded_size,
					nullptr,
					sodium_base64_VARIANT_ORIGINAL
				);
				png_data.resize(decoded_size);

				std::filesystem::create_directories("sdbot_img_send");
				//const std::string tmp_img_file_name = "sdbot_img_" + std::to_string(_current_task.value()) + ".png";
				const std::string tmp_img_file_name = "sdbot_img_" + std::to_string(_rng()) + ".png";
				const std::string tmp_img_file_path = "sdbot_img_send/" + tmp_img_file_name;

				std::ofstream(tmp_img_file_path).write(reinterpret_cast<const char*>(png_data.data()), png_data.size());
				succ = succ && _rmm.sendFilePath(contact, tmp_img_file_name, tmp_img_file_path);
			}
		} catch (...) {
			return false;
		}

		return succ;
	}
};

SDBot::SDBot(
	Contact3Registry& cr,
	RegistryMessageModelI& rmm,
	ConfigModelI& conf
) : _cr(cr), _rmm(rmm), _rmm_sr(_rmm.newSubRef(this)), _conf(conf) {
	_rng.seed(std::random_device{}());
	_rng.discard(3137);

	if (!_conf.has_string("SDBot", "endpoint_type")) {
		_conf.set("SDBot", "endpoint_type", std::string_view{"automatic1111_v1"}); // automatic11 default
	}

	//HACKy
	{ // construct endpoint
		const std::string_view endpoint_type = _conf.get_string("SDBot", "endpoint_type").value();
		if (endpoint_type == "automatic1111_v1") {
			_endpoint = std::make_unique<Automatic1111_v1_Endpoint>(_rmm, _rng);
		} else if (endpoint_type == "sdcpp_wip1") {
			_endpoint = std::make_unique<SDcpp_wip1_Endpoint>(_rmm, _rng);
		} else if (endpoint_type == "sdcpp_stduhpf_wip1") {
			_endpoint = std::make_unique<SDcpp_stduhpf_wip1_Endpoint>(_rmm, _rng);
		} else {
			std::cerr << "SDB error: unknown endpoint type '" << endpoint_type << "'\n";
			// TODO: throw?
			_endpoint = std::make_unique<Automatic1111_v1_Endpoint>(_rmm, _rng);
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
	if (_current_task.has_value() != _curr_future.has_value()) {
		std::cerr << "SDB inconsistent state\n";

		if (_current_task.has_value()) {
			_task_map.erase(_current_task.value());
			_current_task = std::nullopt;
		}

		if (_curr_future.has_value()) {
			_curr_future.reset(); // might block and wait
		}
	}

	if (!_prompt_queue.empty() && !_current_task.has_value()) { // dequeue new task
		const auto& [task_id, prompt] = _prompt_queue.front();

		_current_task = task_id;

		if (_cli == nullptr) {
			const std::string server_host {_conf.get_string("SDBot", "server_host").value()};
			_cli = std::make_shared<httplib::Client>(server_host, _conf.get_int("SDBot", "server_port").value());
			_cli->set_read_timeout(std::chrono::minutes(2)); // because of discarding futures, it can block main for a while
		}

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
			const std::string url {_conf.get_string("SDBot", "url_txt2img").value()};
			_curr_future = std::async(std::launch::async, [url, body, cli = _cli]() -> std::vector<uint8_t> {
				if (!static_cast<bool>(cli)) {
					return {};
				}
				// TODO: move to endpoint
				auto res = cli->Post(url, body, "application/json");
				if (!static_cast<bool>(res)) {
					std::cerr << "SDB error: post to sd server failed!\n";
					return {};
				}
				std::cerr << "SDB http complete " << res->status << " " << res->reason << "\n";
				if (
					res.error() != httplib::Error::Success ||
					res->status != 200
				) {
					return {};
				}

				return std::vector<uint8_t>(res->body.cbegin(), res->body.cend());
			});
		} catch (...) {
			std::cerr << "SDB http request error\n";
			// cleanup
			_task_map.erase(_current_task.value());
			_current_task = std::nullopt;
			_curr_future.reset(); // might block and wait
		}

		_prompt_queue.pop();
	}


	if (_curr_future.has_value() && _curr_future.value().wait_for(std::chrono::milliseconds(1)) == std::future_status::ready) {
		const auto& contact = _task_map.at(_current_task.value());

		const auto data = _curr_future.value().get();

		if (_endpoint->handleResponse(contact, ByteSpan{data})) {
			// TODO: error handling
		}

		_task_map.erase(_current_task.value());
		_current_task = std::nullopt;
		_curr_future.reset();
	}


	// if active web connection, 5ms
	//return static_cast<bool>(_con) ? 0.005f : 1.f;

	// if active web connection, 50ms
	if (_curr_future.has_value() && _curr_future.value().valid()) {
		return 0.05f;
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

