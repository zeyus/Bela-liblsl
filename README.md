# Bela-liblsl

An ARMv7 Bela / Beaglebone black Lab Streaming Layer implementation

The [Bela](https://bela.io/) platform is a low-latency audio and sensor processing platform based on the BeagleBone Black. It is designed for real-time audio and other low-latency applications. [Lab Streaming Layer (LSL)](https://labstreaminglayer.org/#/) is a protocol for streaming data between applications, and is commonly used in neuroscience and other fields.

## Overview

This repo contains the code and library needed to demonstrate real-time stream consumption from liblsl within the Bela context. It will discover available streams and will consume and print them to the console. Of course, you could create outlets, send data from the Bela, or process the data that's recieved. This just demonstrates the basic functionality of the library. It's also to help you get started with LSL on the Bela platform, including how to build it yourself if you want (or if the library is not compatible with some combination of OS and LSL version).

You can see it in action here:

https://github.com/user-attachments/assets/2a0322d6-ab97-474f-a939-cfdea2470679


## Running the example

1. Clone the repo
2. Upload the contents of the [`src`](./src) folder to your Bela project
3. Set the following in the "Make Parameters" section of the Bela IDE Settings:
   `CPPFLAGS=-std=c++14 -I/root/Bela/projects/<your_project_name>/include;LDLIBS=/root/Bela/projects/<your_project_name>/lib/liblsl.so`
   Don't forget to replace `<your_project_name>` with the name of your project folder.
4. Click "Build" and then "Run"
5. You should see the output of the LSL stream discovery and consumption in the console. If you have a stream available, it will print the data to the console.

## Building the library

The [`liblsl.so`](./lib/liblsl.so) library is included in the [`lib`](./lib) folder, you can reuse this in other projects if you have the same Bela version. If not, or if you just want to build it yourself, you can do so by following these steps.

1. Ensure your [Bela has an internet connection](https://learn.bela.io/using-bela/bela-techniques/connecting-to-wifi/)
2. SSH into your Bela board `ssh root@bela.local` or `ssh root@<your_bela_ip_address>`
3. Run the following commands to install the dependencies (for Debian 9 / Stretch - Bela v0.3.8h):
   ```bash
   sudo apt-get update
   sudo apt-get install -y build-essential cmake llvm-7 clang-7 distcc
   update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-7 10
   update-alternatives --install /usr/bin/clang clang /usr/bin/clang-7 10
   update-alternatives --set clang++ /usr/bin/clang++-7
   update-alternatives --set clang /usr/bin/clang-7
   ```
   This will install and set up llvm and clang version 7 which is the latest version supported on that version of Debian.
4. Download and extract the [dart-sdk](https://dart.dev/get-dart/archive), probably the latest dev version, until the stable version has native-assets support. The following demonstrates this for dart-sdk version 3.8.0-246.0.dev:
    ```bash
    # download the archive
    wget https://storage.googleapis.com/dart-archive/channels/dev/release/3.8.0-246.0.dev/sdk/dartsdk-linux-x64-release.zip
    # extract the archive
    unzip dart-sdk-linux-x64-release.zip
    # delete the zip file
    rm -f dart-sdk-linux-x64-release.zip
    # add the dart-sdk to your PATH
    $PATH=$(pwd)/dart-sdk/bin:$PATH
    # make it available for the current session
    export PATH
    ```
3. Download a [liblsl.dart](https://github.com/zeyus/liblsl.dart) release, e.g. `v0.5.0`, you can also use the main branch, but just remember clone with `git clone --recurse-submodules https://github.com/zeyus/liblsl.dart.git` to do a `git submodule update --init --recursive` after:
   ```bash
   # downlaod the release
   wget https://github.com/zeyus/liblsl.dart/archive/refs/tags/v0.5.0.zip
   # extract the archive
   unzip liblsl.dart-v0.5.0.zip
   # delete the zip file
   rm -f liblsl.dart-v0.5.0.zip
   ```
4. Build the library, and test that it works:
   ```bash
    cd liblsl.dart-0.5.0/packages/liblsl
    # build the library (this takes a while)
    dart --enable-excperiment=native-assets test
    # if all the tests pass, then you have a working library
    # you can see the shared library dependencies with the following command
    ldd .dart_tool/native_assets/lib/liblsl.so
    ```
5. Copy the library to the `lib` folder of your project:
   ```bash
   cp .dart_tool/native_assets/lib/liblsl.so /root/Bela/projects/<your_project_name>/lib/
   ```
6. Clean up:
    ```bash
    cd
    # delete the liblsl.dart folder
    rm -rf liblsl.dart-0.5.0
    # delete the dart-sdk folder
    rm -rf dart-sdk
    ```
7. Congrats! You've now build the `liblsl.so` library and can use it in your Bela project. Take a look at an example of how to use it in this project's [`render.cpp`](./src/render.cpp) code.

## License

This project is licensed under the MIT License. See the [LICENSE](./LICENSE) file for details.

liblsl is licensed under the MIT License. See the [liblsl/LICENSE](./src/include/LICENSE) file for details.
