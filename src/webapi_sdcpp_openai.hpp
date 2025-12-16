#pragma once

#include "./webapi_interface.hpp"

#include <cstdint>
#include <solanaceae/util/config_model.hpp>

#include <memory>
#include <future>

// fwd
namespace httplib {
	class Client;
}

// this is supposedly an openai spec compatible one, in master
struct WebAPI_sdcpp_openai : public WebAPII {
	// TODO: const config
	ConfigModelI& _conf;

	std::shared_ptr<httplib::Client> _cl;
	std::shared_ptr<httplib::Client> getCl(void);

	WebAPI_sdcpp_openai(ConfigModelI& conf);

	~WebAPI_sdcpp_openai(void) override;

	std::shared_ptr<WebAPITaskI> txt2img(
		std::string_view prompt,
		int16_t width,
		int16_t height
		// more
	) override;
};

// discard after get() !!
struct WebAPITask_sdcpp_openai : public WebAPITaskI {
	std::future<Result> _future;

	WebAPITask_sdcpp_openai(const std::string& url, const std::string& body, std::shared_ptr<httplib::Client> cl);
	~WebAPITask_sdcpp_openai(void) override {}

	bool ready(void) override;
	std::optional<Result> get(void) override;
};


