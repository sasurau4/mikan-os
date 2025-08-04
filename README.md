# mikan-os

If you want to create mikan-os, go to https://github.com/uchan-nos/mikanos and buy the book!

## Pre-requisite

- LLVM 18

### Install tools

```
sudo apt install okteta
apt-get install qemu-system
bash -c "$(wget -O - https://apt.llvm.org/llvm.sh)"
```

### Setup update-alternatives

```
sudo update-alternatives --install /usr/bin/clang clang /usr/bin/clang++-18 1
sudo update-alternatives --install /usr/bin/lld-link lld-link /usr/bin/lld-link-18 1
sudo update-alternatives --install /usr/bin/llvm-lib llvm-lib /usr/bin/llvm-lib-18 1
```

### How to debug with gdb

https://qiita.com/ktamido/items/2e25d505d475933dcd91
