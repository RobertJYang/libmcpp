在docker下使用bingo build构建libmcpp时 会依赖boost的program_options库，
将三个boost库文件，放到~/.conan/data/目录下的boost版本中，目录如下：
  1.82.0.B001/openUBMC.release/rc/package/295f5ceaff90a1afe2a22ca78ccdeb749ab95b30/lib

需要手动创建lib目录。

docker中安装meson
sudo apt install meson