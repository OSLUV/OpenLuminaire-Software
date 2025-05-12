###################################################
## Copyright (c) 2024 OpenAeros                  ##
## SPDX-License-Identifier: $LICENSE             ##
## Author: Louis Goessling <louis@goessling.com> ##
###################################################

cd build
make -j4 || exit
sudo picotool load app.uf2 -xf