#pragma once

#include "./webapi_interface.hpp"

#include <cstdint>
#include <solanaceae/util/config_model.hpp>

#include <memory>

// fwd
namespace httplib {
	class Client;
}

// in its second api iteration, stduhpf switched away from a rest api
struct WebAPI_sdcpp_stduhpf_wip2 : public WebAPII {
	// TODO: const config
	ConfigModelI& _conf;

	std::shared_ptr<httplib::Client> _cl;
	std::shared_ptr<httplib::Client> getCl(void);

	WebAPI_sdcpp_stduhpf_wip2(ConfigModelI& conf);

	~WebAPI_sdcpp_stduhpf_wip2(void) override;

	std::shared_ptr<WebAPITaskI> txt2img(
		std::string_view prompt,
		int16_t width,
		int16_t height
		// more
	) override;
};

// discard after get() !!
struct WebAPITask_sdcpp_stduhpf_wip2 : public WebAPITaskI {
	std::shared_ptr<httplib::Client> _cl;
	const std::string _url;

	uint64_t _task_id {};
	bool _done {false};
	Result _result;

	WebAPITask_sdcpp_stduhpf_wip2(std::shared_ptr<httplib::Client> cl, const std::string& url, uint64_t task_id);

	~WebAPITask_sdcpp_stduhpf_wip2(void) override {}

	bool ready(void) override;
	std::optional<Result> get(void) override;
};


