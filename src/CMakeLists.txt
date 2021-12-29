idf_component_register(SRCS ghostfat.c images.c main.c msc.c screen.c usb_descriptors.c
                    INCLUDE_DIRS "."
                    REQUIRES boards)

target_sources(${COMPONENT_TARGET} PUBLIC
  "$ENV{IDF_PATH}/components/tinyusb/tinyusb/src/tusb.c"
  "$ENV{IDF_PATH}/components/tinyusb/tinyusb/src/common/tusb_fifo.c"
  "$ENV{IDF_PATH}/components/tinyusb/tinyusb/src/device/usbd.c"
  "$ENV{IDF_PATH}/components/tinyusb/tinyusb/src/device/usbd_control.c"
  "$ENV{IDF_PATH}/components/tinyusb/tinyusb/src/class/cdc/cdc_device.c"
  "$ENV{IDF_PATH}/components/tinyusb/tinyusb/src/class/dfu/dfu_rt_device.c"
  "$ENV{IDF_PATH}/components/tinyusb/tinyusb/src/class/hid/hid_device.c"
  "$ENV{IDF_PATH}/components/tinyusb/tinyusb/src/class/midi/midi_device.c"
  "$ENV{IDF_PATH}/components/tinyusb/tinyusb/src/class/msc/msc_device.c"
  "$ENV{IDF_PATH}/components/tinyusb/tinyusb/src/class/usbtmc/usbtmc_device.c"
  "$ENV{IDF_PATH}/components/tinyusb/tinyusb/src/class/vendor/vendor_device.c"
  "$ENV{IDF_PATH}/components/tinyusb/tinyusb/src/portable/espressif/esp32sx/dcd_esp32sx.c"
)