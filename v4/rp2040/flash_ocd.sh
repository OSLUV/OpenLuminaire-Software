###################################################
## Copyright (c) 2024 OpenAeros                  ##
## SPDX-License-Identifier: $LICENSE             ##
## Author: Louis Goessling <louis@goessling.com> ##
###################################################

cd build
make -j4 || exit
sudo openocd -f interface/cmsis-dap.cfg -f target/rp2350.cfg -c "adapter speed 2000" -c "program app.elf verify reset"
