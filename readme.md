# lgspkctl

Rozhuk Ivan <rozhuk.im@gmail.com> 2019-2024

Tool to control and configure LG sounbar.\
THIS IS UNDONE PROJECT!!!\
Based on: https://github.com/google/python-temescal/


## Licence
BSD licence.


## Donate
Support the author
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

