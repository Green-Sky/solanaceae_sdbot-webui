## Solanaceae extention and plugin to serve StableDiffusion

!! currently only works with automatic1111's api !!

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
		"url_txt2img": "/sdapi/v1/txt2img",

		"width": 512,
		"height": 512,

		"prompt_prefix": "<lora:lcm-lora-sdv1-5:1>",
		"steps": 8,
		"cfg_scale": 1.0,
		"sampler_index": "LCM Test"
	}
}
```
