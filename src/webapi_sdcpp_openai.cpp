#include "./webapi_sdcpp_openai.hpp"

#include <httplib.h>
#include <nlohmann/json.hpp>
#include <sodium.h>

#include <optional>
#include <stdexcept>
#include <string>
#include <chrono>
#include <future>

std::shared_ptr<httplib::Client> WebAPI_sdcpp_openai::getCl(void) {
	if (_cl == nullptr) {
		const std::string server_host {_conf.get_string("SDBot", "server_host").value_or("127.0.0.1")};
		_cl = std::make_shared<httplib::Client>(server_host, _conf.get_int("SDBot", "server_port").value_or(1234));
		_cl->set_read_timeout(std::chrono::minutes(10));
	}

	return _cl;
}


WebAPI_sdcpp_openai::WebAPI_sdcpp_openai(ConfigModelI& conf) :
	_conf(conf)
{
	if (!_conf.has_int("SDBot", "server_port")) {
		_conf.set("SDBot", "server_port", int64_t(1234));
	}
	if (!_conf.has_string("SDBot", "url_txt2img")) {
		_conf.set("SDBot", "url_txt2img", std::string_view{"/v1/images/generations"});
	}
}

WebAPI_sdcpp_openai::~WebAPI_sdcpp_openai(void) {
}

std::shared_ptr<WebAPITaskI> WebAPI_sdcpp_openai::txt2img(
	std::string_view prompt,
	int16_t width,
	int16_t height
	// more
) {
	auto cl = getCl();
	if (!cl) {
		return nullptr;
	}


	std::string body;
	nlohmann::json j_body;
	// eg
	//{
	//  "model": "unused",
	//  "prompt": "A lovely cat<sd_cpp_extra_args>{\"seed\": 357925}</sd_cpp_extra_args>",
	//  "n": 1,
	//  "size": "128x128",
	//  "response_format": "b64_json"
	//}
	try {
		j_body["model"] = "unused";
		j_body["size"] = std::to_string(width) + "x" + std::to_string(height);
		j_body["n"] = 1;
		j_body["response_format"] = "b64_json";

		nlohmann::json j_sd_extra_args;
		j_sd_extra_args["seed"] = -1;
		if (_conf.has_int("SDBot", "steps")) {
			j_sd_extra_args["steps"] = _conf.get_int("SDBot", "steps").value();
		}
		if (_conf.has_double("SDBot", "cfg_scale")) {
			j_sd_extra_args["cfg_scale"] = _conf.get_double("SDBot", "steps").value();
		} else if (_conf.has_int("SDBot", "cfg_scale")) {
			j_sd_extra_args["cfg_scale"] = _conf.get_int("SDBot", "steps").value();
		}

		j_body["prompt"] =
			std::string{_conf.get_string("SDBot", "prompt_prefix").value_or("")}
			+ std::string{prompt}
			+ "<sd_cpp_extra_args>"
			+ j_sd_extra_args.dump(-1, ' ', true)
			+ "</sd_cpp_extra_args>"
		;

		body = j_body.dump();
	} catch (...) {
		std::cerr << "SDB error: failed creating body json\n";
		return nullptr;
	}

	//std::cout << "body: " << body << "\n";

	try {
		const std::string url {_conf.get_string("SDBot", "url_txt2img").value_or("/v1/images/generations")};
		return std::make_shared<WebAPITask_sdcpp_openai>(url, body, getCl());
	} catch (...) {
		std::cerr << "SDB error: scheduling post to sd server failed!\n";
		return nullptr;
	}

	return nullptr;
}

WebAPITask_sdcpp_openai::WebAPITask_sdcpp_openai(const std::string& url, const std::string& body, std::shared_ptr<httplib::Client> cl) {
	_future = std::async(std::launch::async, [url, body, cl]() -> WebAPITaskI::Result {
		if (!static_cast<bool>(cl)) {
			return {};
		}

		try {
			auto res = cl->Post(url, body, "application/json");
			if (!static_cast<bool>(res)) {
				std::cerr << "SDB error: post to sd server failed!\n";
				return {};
			}
			std::cerr << "SDB: http complete " << res->status << " " << res->reason << "\n";

			if (
				res.error() != httplib::Error::Success ||
				res->status != 200
			) {
				return {};
			}

			//std::cerr << "------ res body: " << res->body << "\n";

			const auto j_res = nlohmann::json::parse(res->body);

			// TODO: add support for multiple images
			auto& j_image = j_res.at("data").at(0);

			Result result;

			auto& j_data = j_image.at("b64_json");
			result.data.resize(j_data.get<std::string>().size()); // HACK: do a better estimate
			size_t decoded_size {0};
			sodium_base642bin(
				result.data.data(), result.data.size(),
				j_data.get<std::string>().data(), j_data.get<std::string>().size(),
				" \n\t",
				&decoded_size,
				nullptr,
				sodium_base64_VARIANT_ORIGINAL
			);
			result.data.resize(decoded_size);

			result.file_name = std::string{"output_0."} + j_image.value("output_format", "png");

			return result;
		} catch (...) {
			std::cerr << "SDB error: post to sd server failed!\n";
		}

		return {};
	});

	if (!_future.valid()) {
		throw std::runtime_error("failed to create future");
		std::cerr << "SDB error: scheduling post to sd server failed, invalid future returned.\n";
	}
}

bool WebAPITask_sdcpp_openai::ready(void) {
	return _future.wait_for(std::chrono::microseconds(10)) == std::future_status::ready;
}

std::optional<WebAPITaskI::Result> WebAPITask_sdcpp_openai::get(void) {
	return _future.get();
}

