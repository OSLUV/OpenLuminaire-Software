cd build
make -j4 || exit
cd ..
sudo openocd -f interface/cmsis-dap.cfg -f target/rp2040.cfg -c "adapter speed 5000" -c "program build/app.elf verify reset"
