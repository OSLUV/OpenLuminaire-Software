set -exu

# . /home/louis/Documents/ee/esp/esp-idf/export.sh

cd build
ninja -j8
esptool.py --chip esp32c6 -p /dev/ttyACM1 -b 460800 --before=no_reset --after=no_reset write_flash --flash_mode dio --flash_freq 80m --flash_size 2MB 0x0 bootloader/bootloader.bin 0x10000 hello_world.bin 0x8000 partition_table/partition-table.bin