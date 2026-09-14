# Wired dual-HX711 tray scales

Two HX711 ADCs can read two load cells supporting one tray. The display calibrates
each cell and sums the readings. This is not two independently selectable scales.
Support is disabled by default; existing Bluetooth scale use is unchanged.

## Wiring

The plugin defaults match the reserved scale pins on the built-in controller
configurations, including Lego and Pro 1.1:

| Controller GPIO | Dual-HX711 connection |
| --- | --- |
| 17 (default) | Shared PD_SCK / clock for both ADCs |
| 18 (default) | First/left ADC DOUT |
| 39 (default) | Second/right ADC DOUT |
| GND | Board ground |

These are GPIO numbers, not ESP32 module pad numbers or connector positions.
Check the connector pinout on your particular dual board before plugging it in.
GPIOs are configurable in **Settings → Plugins → Hardware Scales Plugin**, stored
in display Preferences (`hws_a`, `hws_clk`, `hws_l`, `hws_r`), and sent to the
controller when it connects. No pin macros or custom firmware build are needed.
Use **Save and Restart** to apply the enable switch or pin changes, following the
same startup registration pattern as Boiler Refill. The controller rejects
duplicate pins and pins reserved for machine outputs, sensors, the accessory bus,
USB, flash/PSRAM or boot strapping. Invalid pin assignments appear in Calibration.

Each load cell connects to its own ADC's channel A: excitation E+/E- and signal
A+/A-. Wire colours are not universal.

Use **3.3 V digital logic** at the ESP32. An HX711 supports a separate digital
supply from its analogue/excitation supply, but module wiring varies. A board
whose digital supply is tied to 5 V must not drive the ESP32 DOUT pins directly.
Confirm the module schematic before choosing its power connection.

The interface is HX711-specific serial, not I2C or standard SPI. Both chips must
share PD_SCK for this driver. Channel A gain 128 is selected after each read.
Both 10 SPS and 80 SPS are supported; the RATE connection on the board selects
the conversion rate. Readings sent to the display are limited to 20 Hz.

Reference: [AVIA HX711 datasheet](https://datasheet.lcsc.com/lcsc/Avia-Semicon-Xiamen-HX711_C43656.pdf).

## Firmware and calibration

Build/upload both the normal `controller` firmware and `display` (or
`display-headless`) firmware from this revision. An old controller does not
provide the new raw-scale messages. No special PlatformIO environment is needed.

Enable **Settings → Plugins → Hardware Scales Plugin**, set the controller GPIOs,
and save/restart. Then open **Settings → Calibration → Wired tray scales (dual HX711)**:

1. Wait for both ADCs to connect.
2. Leave the empty tray installed and allow at least two seconds to settle.
   Choose **Capture empty tray**.
3. Enter a known mass between 10 and 500 g. Place it near the left support,
   let it settle, and capture the left position.
4. Move the same mass near the right support, let it settle, and capture/save.
5. Check the reading with the mass at the left, centre and right. Remove the mass
   and confirm the empty tray returns close to zero.

Both positions may load both cells: calibration solves for each cell's separate
sensitivity. The positions must distribute the load differently; two captures at
the same position are rejected. Moving/noisy loads can also be rejected.
Calibration saves immediately as one NVS record on the display. It is associated
with the selected GPIO mapping; changing pins invalidates the old calibration. Recalibrate
after replacing a cell, ADC, or changing its mounting. Cancelling preserves the
previous saved calibration. Tare changes only the current zero, not calibration.

The web panel has a Tare button, and the display's existing tare gesture also
tares wired scales. Brew and grind pre-start events tare automatically. Weight
continues updating after stopping so the existing final-weight logic can settle.

## Source selection and faults

Enabling wired scales selects them for brew/grind weight, the display, live web
weight and shot history. Bluetooth scale measurements and tare are ignored while
wired mode is selected. Disable the plugin and save/restart to use Bluetooth again. The selected
source is reported as `scaleSource` in WebSocket status. `sr` reports
whether that source is usable, and `cw` reports its current weight. The old
`bc` field and duplicate `bw` weight field have been removed; clients must use
`sr` and `cw`. The calibration API uses `samplesReady` to indicate that
enough fresh samples have been collected for a calibration capture.

Volumetric availability follows the existing firmware policy for either scale
source: regular builds require a healthy scale; nightly builds also allow
estimated flow when the controller supports dimming. There is no wired-only
restriction on that fallback when starting a process.

Missing either ADC, ADC saturation, a DOUT stuck low, or approximately one second
without a valid update makes the wired scale unavailable. An active weight-target
brew/grind already using scale measurements is stopped rather than switching
its measurement source mid-process.
Time-only processes remain unaffected. No calibration is performed during an
active process or firmware update. An unavailable scale uses the existing scale
warning; configure its severity in the normal warning settings.

Scale selection, readiness, tare routing and loss handling live in the display
controller. Both sources require a fresh valid weight before they are ready;
a BLE connection alone does not qualify. BLE readings expire after 1.5 seconds,
and a lost/stale BLE scale stops weight-target processes through the same path.
The display, warnings and web status all use this shared readiness state. Selecting
wired scales prevents BLE measurements from updating the active weight or freshness.

The controller only owns the scale pins once enabled. Sampling runs in a separate
task; conversion readiness never blocks the heater/pump loop. Calibration/tare
are performed on the display; the existing controller `Tare` command still resets
flow estimation and is intentionally not used to re-zero the tray at shot end.

## Mechanical limits

Two 750 g cells do not guarantee 1.5 kg usable capacity. Each cell must stay below
750 g under the worst load distribution, including the tray and cup. Taring does
not remove that physical load. Use suitable mounts and mechanical overload stops.
ADC saturation detection is not a calibrated per-cell overload detector.

## Validation

`test/hardware_scale/test_hardware_scale.cpp` exercises shared-clock readiness,
25-pulse reads, signed ADC values, stuck-low detection, and two-position
calibration (including opposite cell polarity and degenerate captures).
`test/hardware_scale/test_plugin.cpp` covers calibration persistence, tare,
stale/disconnected readings, pin changes and the inactive plugin state.
`test/scale_selection/test_scale_selection.cpp` covers both sources: selection,
freshness, battery availability, manual/automatic tare routing and stopping only
weight-target processes when their scale becomes unavailable.
The implementation still requires a physical check of calibration, repeatability,
cup placement, tare and cable-disconnect behaviour before relying on it for shots.
