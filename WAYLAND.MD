* sudo apt install qtbase5-dev qtchooser qttools5-dev-tools libqt5serialport5-dev build-essential pkg-config libusb-1.0-0-dev libudev-dev gnome-shell-extension-appindicator libpulse-dev libfftw3-dev libpipewire-0.3-dev
* qmake && make
* QT_QPA_PLATFORM=xcb LD_LIBRARY_PATH=./bin ./bin/Prismatik

Ensure that you switch to the Wayland capture device
