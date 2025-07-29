#include "./webapi_sdcpp_stduhpf_wip2.hpp"

#include <httplib.h>
#include <nlohmann/json.hpp>
#include <sodium.h>

#include <cstdint>
#include <memory>
#include <chrono>
#include <string>

std::shared_ptr<httplib::Client> WebAPI_sdcpp_stduhpf_wip2::getCl(void) {
	if (_cl == nullptr) {
		const std::string server_host {_conf.get_string("SDBot", "server_host").value()};
		_cl = std::make_shared<httplib::Client>(server_host, _conf.get_int("SDBot", "server_port").value());
		_cl->set_read_timeout(std::chrono::minutes(2)); // because of discarding futures, it can block main for a while
	}

	return _cl;
}

WebAPI_sdcpp_stduhpf_wip2::WebAPI_sdcpp_stduhpf_wip2(ConfigModelI& conf) :
	_conf(conf)
{
}

WebAPI_sdcpp_stduhpf_wip2::~WebAPI_sdcpp_stduhpf_wip2(void) {
}


std::shared_ptr<WebAPITaskI> WebAPI_sdcpp_stduhpf_wip2::txt2img(
	std::string_view prompt,
	int16_t width,
	int16_t height
	// more
) {
	auto cl = getCl();
	if (!cl) {
		return nullptr;
	}

	nlohmann::json j_body;
	// eg
	//{
	//  "prompt": "a lovely schnitzel",
	//  "negative_prompt": "",
	//  "width": 512,
	//  "height": 512,
	//  "guidance_params": {
	//    "cfg_scale": 1,
	//    "guidance": 3.5
	//  },
	//  "sample_steps": 8,
	//  "sample_method": "euler_a",
	//  "seed": -1,
	//  "batch_count": 1,
	//  "schedule": "default",
	//  "model": -1,
	//  "diffusion_model": -1,
	//  "clip_l": -1,
	//  "clip_g": -1,
	//  "t5xxl": -1,
	//  "vae": -1,
	//  "tae": -1,
	//  "keep_vae_on_cpu": false,
	//  "kep_clip_on_cpu": false,
	//  "vae_tiling": true,
	//  "tae_decode": true
	//}
	//j_body["width"] = _conf.get_int("SDBot", "width").value_or(512);
	j_body["width"] = width;
	//j_body["height"] = _conf.get_int("SDBot", "height").value_or(512);
	j_body["height"] = height;

	j_body["prompt"] = std::string{_conf.get_string("SDBot", "prompt_prefix").value_or("")} + std::string{prompt};
	// TODO: negative prompt

	j_body["seed"] = -1;
	j_body["sample_steps"] = _conf.get_int("SDBot", "steps").value_or(20);
	j_body["guidance_params"]["cfg_scale"] = _conf.get_double("SDBot", "cfg_scale").value_or(6.5);
	//j_body["sampler_index"] = std::string{_conf.get_string("SDBot", "sampler_index").value_or("Euler a")};

	std::string body = j_body.dump();

	const std::string url {_conf.get_string("SDBot", "url_txt2img").value_or("/txt2img")};

	try {
		// not restful -> returns imediatly with task id
		auto res = cl->Post(url, body, "application/json");
		if (!static_cast<bool>(res)) {
			std::cerr << "SDB error: post to sd server failed!\n";
			return nullptr;
		}

		std::cerr << "SDB http complete " << res->status << " " << res->reason << "\n";

		if (
			res.error() != httplib::Error::Success ||
			res->status != 200
		) {
			return nullptr;
		}


		try {
			// {"task_id":"1753652554405483588"}
			auto j_res = nlohmann::json::parse(res->body);

			if (!j_res.contains("task_id")) {
				std::cerr << "SDB error: response not a task_id\n";
				return nullptr;
			}

			uint64_t task_id{};
			const auto& j_task_id = j_res.at("task_id");
			if (!j_task_id.is_number_unsigned()) {
				// meh, conversion time
				task_id = std::stoull(j_task_id.get<std::string>().c_str());
				//std::cout << "converted " << j_task_id << " to " << task_id << "\n";
			} else {
				task_id = j_task_id.get<uint64_t>();
			}

			std::cout << "SDB: sdcpp task id: " << task_id << "\n";

			return std::make_shared<WebAPITask_sdcpp_stduhpf_wip2>(
				cl,
				"/result", // TODO: from conf?
				task_id
			);
		} catch (...) {
			std::cerr << "SDB error: failed parsing response as json\n";
			return nullptr;
		}
	} catch (...) {
		std::cerr << "SDB http request error\n";
		return nullptr;
	}

	// ???
	return nullptr;
}

WebAPITask_sdcpp_stduhpf_wip2::WebAPITask_sdcpp_stduhpf_wip2(std::shared_ptr<httplib::Client> cl, const std::string& url, uint64_t task_id)
	: _cl(cl), _url(url), _task_id(task_id)
{
}

//WebAPITask_sdcpp_stduhpf_wip2::~WebAPITask_sdcpp_stduhpf_wip2(void) {
//}

bool WebAPITask_sdcpp_stduhpf_wip2::ready(void) {
	// polling api

	try {
		auto res = _cl->Get(_url + "?task_id=" + std::to_string(_task_id));
		if (!static_cast<bool>(res)) {
			std::cerr << "SDB error: post to sd server failed! (in task)\n";
			_done = true;
			return true;
		}

		if (
			res.error() != httplib::Error::Success ||
			res->status != 200
		) {
			std::cerr << "SDB error: post to sd server failed! (in task)\n";
			_done = true;
			return true;
		}

		auto j_res = nlohmann::json::parse(res->body);
		if (j_res.at("status") != "Done") {
			// not ready (likely path)
			return false;
		}

		// TODO: add support for multiple images
		auto& j_image = j_res.at("data").at(0); // ??

		_result.width = j_image.at("width");
		_result.height = j_image.at("height");

		{ // data
			auto& j_data = j_image.at("data");

			_result.data.resize(j_data.get<std::string>().size()); // HACK: do a better estimate
			size_t decoded_size {0};
			sodium_base642bin(
				_result.data.data(), _result.data.size(),
				j_data.get<std::string>().data(), j_data.get<std::string>().size(),
				" \n\t",
				&decoded_size,
				nullptr,
				sodium_base64_VARIANT_ORIGINAL
			);
			_result.data.resize(decoded_size);
		}

		{ // file name?
			// == "png" or jpeg or somehting
			_result.file_name = std::string{"output_0."} + j_image.value("encoding", "png");
		}

		_done = true;
		return true;
	} catch (...) {
		// rip
		_done = true;
		return true;
	}

	return false;
}

std::optional<WebAPITaskI::Result> WebAPITask_sdcpp_stduhpf_wip2::get(void) {
	if (!_done) {
		assert(false && "what ya doin?");
		return std::nullopt;
	}

	if (_result.data.empty()) {
		return std::nullopt;
	}

	return std::move(_result);
}

