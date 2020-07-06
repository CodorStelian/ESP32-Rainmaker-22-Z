# Multi Device Example

## Build and Flash firmware

Follow the ESP RainMaker Documentation [Get Started](https://rainmaker.espressif.com/docs/get-started.html) section to build and flash this firmware. Just note the path of this example.

## What to expect in this example?

- This example just demonstrates how you can have multiple devices on the same board for practical use.
- It has 6 devices
    - 2 Lightbulbs(solid state relay module)
	- RGB led strip with 24 pixels
	- Temperature Sensor(SHT31)
    - Humidity Sensor(SHT31)
	- Luminosity Sensor(BH1750)
- It uses the esp timer to demonstrate the temperature, humidity and luminosity sensors.
- Pressing the BOOT button will toggle the state of the primary lightbulb. This will also reflect on the phone app.
- Toggling the buttons on the phone app should toggle the lightbulbs, and also print messages like these on the ESP32 monitor:

```
I (16073) app_main: Received value = true for Bedroom Light - power
```

- You may also try changing the hue, saturation and brightness for RGB led strip from the phone app.
- The temperature and humidity value are refreshed every 5 minutes and luminosity value is refreshed every minute.
- You can check the temperature, humidity and luminosity changes in the phone app.

### RGB strip led or sensors not working?

The RGB led strip is connected to GPIO 5.
The solid state relay module(4 channels) is connected to GPIO 19, 18, 17 and 16.
The temperature and humidity sensor(SHT31) and luminosity sensor are connected to GPIO 22(SCL) and GPIO 21(SDA).

### Reset to Factory

Press and hold the BOOT button for more than 3 seconds to reset the board to factory defaults. You will have to provision the board again by Bluetooth to use it.

