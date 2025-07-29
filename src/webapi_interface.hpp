#pragma once

#include <string_view>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>
#include <string>

struct WebAPITaskI;

struct WebAPII {
	virtual ~WebAPII(void) {}

	virtual std::shared_ptr<WebAPITaskI> txt2img(
		std::string_view prompt,
		int16_t width,
		int16_t height
		// more
	) = 0;
};

// only knows of a single prompt
struct WebAPITaskI {
	virtual ~WebAPITaskI(void) {}

	// true if done or failed
	virtual bool ready(void) = 0;

	struct Result {
		std::vector<uint8_t> data;
		int16_t width {};
		int16_t height {};

		std::string file_name;
	};

	virtual std::optional<Result> get(void) = 0;
};

