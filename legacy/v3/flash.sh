
cd build
make -j4 || exit
mkdir mnt
stat /dev/$1 || exit
sudo umount mnt
echo Mount...
sudo mount /dev/$1 mnt || exit
echo Copy...
sudo cp app.uf2 mnt
echo Sync...
sync
sudo umount mnt
