## Solanaceae extention and plugin to serve StableDiffusion

!! currently only works with [stduhpf's stablediffusion.cpp http server api](https://github.com/leejet/stable-diffusion.cpp/pull/367)  !!

### example config for `totato`
```json
{
	"PluginManager": {
		"autoload": {
			"entries": {
				"./build/bin/libplugin_sdbot-webui.so": true
			}
		}
	},
	"SDBot": {
		"server_host": "127.0.0.1",
		"server_port": 8080,
		"endpoint_type": "sdcpp_stduhpf_wip2",
		"url_txt2img": "/txt2img",

		"width": 512,
		"height": 512,

		"prompt_prefix": "<lora:lcm-lora-sdv1-5:1>",
		"steps": 8,
		"cfg_scale": 1.0
	}
}
```
