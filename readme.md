# lgspkctl

Rozhuk Ivan <rozhuk.im@gmail.com> 2019-2024

Tool to control and configure LG sounbar.\
THIS IS UNDONE PROJECT!!!\
Based on: https://github.com/google/python-temescal/


## Licence
BSD licence.


## Donate
Support the author
* **GitHub Sponsors:** [!["GitHub Sponsors"](https://camo.githubusercontent.com/220b7d46014daa72a2ab6b0fcf4b8bf5c4be7289ad4b02f355d5aa8407eb952c/68747470733a2f2f696d672e736869656c64732e696f2f62616467652f2d53706f6e736f722d6661666266633f6c6f676f3d47697448756225323053706f6e736f7273)](https://github.com/sponsors/rozhuk-im) <br/>
* **Buy Me A Coffee:** [!["Buy Me A Coffee"](https://www.buymeacoffee.com/assets/img/custom_images/orange_img.png)](https://www.buymeacoffee.com/rojuc) <br/>
* **PayPal:** [![PayPal](https://srv-cdn.himpfen.io/badges/paypal/paypal-flat.svg)](https://paypal.me/rojuc) <br/>
* **Bitcoin (BTC):** `1AxYyMWek5vhoWWRTWKQpWUqKxyfLarCuz` <br/>


## Compilation

### Linux
``` shell
sudo apt-get install build-essential git cmake fakeroot
git clone --recursive https://github.com/rozhuk-im/lgspkctl.git
cd lgspkctl
mkdir build
cd build
cmake ..
make -j 4
```

### FreeBSD/DragonFlyBSD
``` shell
sudo pkg install git cmake
git clone --recursive https://github.com/rozhuk-im/lgspkctl.git
cd lgspkctl
mkdir build
cd build
cmake ..
make -j 4
```

