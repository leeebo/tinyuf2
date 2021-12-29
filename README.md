# ESP32S2/S3 TinyUF2 Bootloader

Source project: https://github.com/adafruit/tinyuf2

## Build for ESP Board

1. export your ESP-IDF path (`release/v4.4`)
2. cd to project
   ```
   cd ports/espressif
   ```

3. build and flash with 

   ```
   idf.py -DBOARD=espressif_saola_1_wrover build flash
   ```

>Please change `espressif_saola_1_wrover` to your board

>You can also merge your bin using:

> esptool.py --chip esp32s2 merge_bin --output uf2_bootloder.bin 0x1000 build/bootloader/bootloader.bin 0x8000 build/partition_table/partition-table.bin 0xe000 build/ota_data_initial.bin 0x2d0000 build/tinyuf2.bin

>Then write from `0x0`:

> esptool.py --chip esp32s2 write_flash --flash_mode dio 0x0 uf2_bootloder.bin

> ESP32S2 have different address with ESP32S3, please check the address infomation after build


## Convert Binary to UF2

To create your own UF2 file, simply use the [Python conversion script](https://github.com/Microsoft/uf2/blob/master/utils/uf2conv.py) on a .bin file, specifying the family id as `ESP32S2`, ``ESP32S3` or their magic number as follows. Note you must specify application address of 0x00 with the -b switch, the bootloader will use it as offset to write to ota partition.

1. add convert script to path
   ```
   export PATH=$PATH:./utils
   ```
2. convert as follow

    ```
    uf2conv.py firmware.bin -c -b 0x00 -f ESP32S2
    uf2conv.py firmware.bin -c -b 0x00 -f 0xbfdd4eee

    uf2conv.py firmware.bin -c -b 0x00 -f ESP32S3
    uf2conv.py firmware.bin -c -b 0x00 -f 0xc47e5767
    ```

## Usage

There are a few ways to enter UF2 mode:

- There is no `ota application` and/or `ota_data` partition is corrupted
- `PIN_BUTTON_UF2` is gnd when 2nd stage bootloader indicator is on e.g **RGB led = Purple**. Note: since most ESP32S2 board implement `GPIO0` as button for 1st stage ROM bootloader, it can be used for dual-purpose button here as well. The difference is the pressing order:
  - Holding `GPIO0` then reset -> ROM bootloader
  - Press reset, see indicator on (purple RGB) then press `GPIO0` -> UF2 bootloader
- `PIN_DOUBLE_RESET_RC` GPIO is attached to an 100K resistor and 1uF Capacitor to serve as 1-bit memory, which hold the pin value long enough for double reset detection. Simply press double reset to enter UF2
- Request by application using [system reset reason](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/system.html?highlight=esp_reset_reason#reset-reason) with hint of `0x11F2`. Reset reason hint is different than hardware reset source, it is written to RTC's store6 register and hold value through a software reset. Since Espressif only uses an dozen of value in `esp_reset_reason_t`, it is safe to hijack and use *0x11F2* as reset reason to enter UF2 using following snippet.
  ```
  #include "esp_private/system_internal.h"
  void reboot_to_uf2(void)
  {
    // Check out esp_reset_reason_t for other Espressif pre-defined values
    enum { APP_REQUEST_UF2_RESET_HINT = 0x11F2 };

    // call esp_reset_reason() is required for idf.py to properly links esp_reset_reason_set_hint()
    (void) esp_reset_reason();
    esp_reset_reason_set_hint(APP_REQUEST_UF2_RESET_HINT);
    esp_restart();
  }
  ```

## 2nd Stage Bootloader

After 1st stage ROM bootloader runs, which mostly checks GPIO0 to determine whether it should go into ROM DFU, 2nd stage bootloader is loaded. It is responsible for determining and loading either UF2 or user application (OTA0, OTA1). This is the place where we added detection code for entering UF2 mode mentioned by above methods.

Unfortunately ESP32S2 doesn't have a dedicated reset pin, but rather using [power pin (CHIP_PU) as way to reset](https://github.com/espressif/esp-idf/issues/494#issuecomment-291921540). This makes it impossible to use any RAM (internal and PSRAM) to store the temporary double reset magic. However, using an resistor and capacitor attached to a GPIO, we can implement a 1-bit memory to hold pin value long enough for double reset detection.

**TODO** guide and schematic as well as note for resistor + capacitor.

## UF2 Application as 3rd stage Bootloader

UF2 is actually a **factory application** which is pre-flashed on the board along with 2nd bootloader and partition table. When there is no user application or 2nd bootloader "double reset alternative" decide to load uf2. Therefore it is technically 3rd stage bootloader.

It will show up as mass storage device and accept uf2 file to write to user application partition. UF2 bootloader will always write/update firmware to **ota_0** partition, since the actual address is dictated by **partitions.csv**, uf2 file base address **MUST** be 0x00, the uf2 will parse the partition table and start writing from address of ota_0. It also makes sure there is no out of partition writing.

After complete writing, uf2 will set the ota0 as bootable and reset, and the application should be running in the next boot.

NOTE: uf2 bootloader, customized 2nd bootloader and partition table can be overwritten by ROM DFU and/or UART uploading. Especially the `idf.py flash` which will upload everything from the user application project. It is advisable to upload only user application only with `idf.py app-flash` and leave other intact provided the user partition table matched this uf2 partition.

## Partition

Following is typical partition for 4MB flash, check out the `partition-xMB.csv` for details.

```
# Name,   Type, SubType, Offset,  Size, Flags
# bootloader.bin,,          0x1000, 32K
# partition table,          0x8000, 4K

nvs,      data, nvs,      0x9000,  20K,
otadata,  data, ota,      0xe000,  8K,
ota_0,    0,    ota_0,   0x10000,  1408K,
ota_1,    0,    ota_1,  0x170000,  1408K,
uf2,      app,  factory,0x2d0000,  256K,
ffat,     data, fat,    0x310000,  960K,
```

