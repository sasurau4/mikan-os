# mikan-os

If you want to create mikan-os, go to https://github.com/uchan-nos/mikanos and buy the book!

## Pre-requisite

- LLVM 18

### Setup update-alternatives

```
sudo update-alternatives --install /usr/bin/clang clang /usr/bin/clang++-18 1
sudo update-alternatives --install /usr/bin/lld-link lld-link /usr/bin/lld-link-18 1
sudo update-alternatives --install /usr/bin/llvm-lib llvm-lib /usr/bin/llvm-lib-18 1
```
