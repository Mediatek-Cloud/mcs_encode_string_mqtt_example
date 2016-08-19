# mcs_encode_string_mqtt_example

## Usage

* copy `mcs_encode_string_mqtt` to your `{SDK_Root}/project/mt7687_hdk/apps/mcs_encode_string_mqtt`

* Edit the `{SDK_Root}/project/mt7687_hdk/apps/mcs_encode_string_mqtt/main.c`:

```
#define deviceId "Input your deviceId"
#define deviceKey "Input your deviceKey"
#define Ssid "Input your wifi Ssid"
#define Password "Input your wifi password"
#define topic "mcs/{Input your deviceId}/{Input your deviceKey}/+"
#define host "com"
#define server "mqtt.mcs.mediatek.com"
#define port "1883"
#define clientId "mt7687"
```

* build code, on your SDK_Root : `./build.sh mt7687_hdk mcs_encode_string_mqtt`

* Burn .bin to your 7687 device.

## SDK version

* [3.3.1](https://cdn.mediatek.com/download_page/index.html?platform=RTOS&version=v3.3.1&filename=LinkIt_SDK_V3.3.1_public.tar.gz)
