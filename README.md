# Hubble xG24 Dual-Mode Demo

A demo for the SiLabs xG24 +20 dBm Pro Kit. It reports its temperature on both
Terrestrial and Satellite networks.

The firmware is built on the
[Hubble Device SDK](https://github.com/HubbleNetwork/hubble-device-sdk) and
[Zephyr](https://zephyrproject.org/). It starts from the SDK's
[`sat-dual-stack`](https://github.com/HubbleNetwork/hubble-device-sdk/tree/v3.1.0/samples/zephyr/sat-dual-stack)
sample.

## Requirements

- A cryptographic key provided by Hubble Network.
- The hardware listed below:

## Hardware

| Item | Value |
| --- | --- |
| Kit | xG24 +20 dBm Pro Kit (xG24-PK6010A) |
| Radio board | BRD4187C (EFR32MG24B220F1536IM48) |
| Mainboard | BRD4002A |
| Zephyr board | `xg24_rb4187c` |
| Power | USB, or 2x AA batteries |

> [!NOTE]
> The board has a CR2032 coin cell slot. A coin cell is unable to supply enough current to
> drive a satellite transmission.

## What the firmware does

1. Advertises as **Hubble-xG24** and waits for provisioning over BLE: the time,
   the device location and the satellite orbital parameters.
2. Sends Hubble BLE beacons that carry the temperature. It takes a new reading
   every 5 minutes.
3. At the next satellite pass, BLE stops and the device advertises to the satellite
   while it is overhead. Then, it returns to BLE.

### Payload

The BLE beacon and the satellite packet carry the same payload. The layout
follows the
[Dash sensor widget](https://hubble.com/docs/guides/dashboard/sensor-widget),
so Dash decodes the value without extra configuration.

| Offset | Size | Field |
| --- | --- | --- |
| 0 | 1 | Type `0x01`, Temperature High Res |
| 1 | 2 | Temperature, `int16`, little-endian, in 0.01 °C |
| 3 | 1 | Padding `0x00`, satellite packet only |

When sent over satellite, an extra 4th byte (0x00) padding as added per a requirement of using the satellite network.

## Set up the workspace

You need a Zephyr development environment. Follow the
[Zephyr Getting Started Guide](https://docs.zephyrproject.org/latest/develop/getting_started/index.html)
to install the dependencies and `west`, then:

```sh
mkdir hubble-workspace && cd hubble-workspace
git clone --recurse-submodules https://github.com/HubbleNetwork/hubble-demo-xg24-dual-mode.git
west init -l hubble-demo-xg24-dual-mode
west update
west zephyr-export
west packages pip --install
west sdk install --toolchains arm-zephyr-eabi

# RAIL and the Bluetooth controller library for EFR32
west blobs fetch hal_silabs
```

## Hubble Key
Outlined below are the steps taken to create a Hubble key:
1. Log in or sign up at [hubble.com](https://hubble.com/).
2. Go to [Hubble Dashboard](https://dash.hubble.com/devices).
3. Click "Add a Device".
4. Click "I just need a Device Key".
5. Set a recognizable device name.
6. Set "Encryption" to "AES-256-CTR".
7. Set "Encryption Mode" to "Unix Time".
8. Copy your key somewhere locally; it won't be visible on [hubble.com](https://hubble.com/) again.

## Build and flash

Each device needs its own Hubble device key. Get the key from Dash and give it
to the build as base64:

```sh
cd hubble-demo-xg24-dual-mode
west build -b xg24_rb4187c app -- -DCONFIG_HUBBLE_DEVICE_KEY=\"<your-base64-key>\"
west flash
```

Build options, in `menuconfig` under *Hubble xG24 dual-mode demo options*:

| Option | Default | Description |
| --- | --- | --- |
| `CONFIG_HUBBLE_DEVICE_KEY` | `""` | Device key, base64. |
| `CONFIG_APP_SAT_RETRY_SEC` | `3600` | Seconds to wait after a pass prediction failure. |
| `CONFIG_APP_SAT_DEBUG` | `n` | Transmit to the satellite 120 s after each beacon period starts, not at the next pass. For bench tests only. |

Advertising parameters:

| Parameter | Value | Set by |
| --- | --- | --- |
| Beacon interval | 1000–1200 ms | `ADV_INTERVAL_MIN_MS` / `ADV_INTERVAL_MAX_MS` in [app/src/app_ble.c](app/src/app_ble.c) |
| Provisioning interval (connectable) | 100–150 ms | GAP fast interval `BT_GAP_ADV_FAST_INT_MIN_2` / `BT_GAP_ADV_FAST_INT_MAX_2` in [app/src/app_ble.c](app/src/app_ble.c) |
| Tx power | 0 dBm | `CONFIG_BT_CTLR_TX_PWR_0` in [app/prj.conf](app/prj.conf) |

## Provision and resync ephemeris

Provisioning gives the device the time, its location and the satellite orbital
parameters (ephemeris). Do it after each flash or reset, when the device
moves, or when the ephemeris is old. The device keeps nothing across a reset.

You need a laptop with Bluetooth, Python3 and a Hubble API token. The
companion script is in the SDK submodule:

```sh
cd hubble-workspace/hubble-demo-xg24-dual-mode/hubble-device-sdk
pip install -r tools/requirements-companion.txt

export HUBBLE_API_TOKEN=<your-hubble-api-token>
python tools/dual-stack-companion.py
```


## License

[Apache-2.0](LICENSE)
