# libkamoflage
libkamoflage is a lightweight GUI abstraction layer for C++. Currently supporting Android.

# license
libkamoflage is released under the GNU Lesser General Public License, version 2.

Please note that libkamoflage depends on additional third party libraries that has
their own licenses. If you distribute software including or using libkamoflage make
sure you follow gpl v2, and also the additional licensing requirements specified in
the third party libraries.

# requirements

 * A development host running some form of GNU/Linux (tested on Ubuntu 15.04)
 * the Android SDK
 * the Android NDK
 * a local libsvgandroid repository ( git clone https://github.com/pltxtra/libsvgandroid.git )
   - make sure you configure it and build it properly

# configure

Quick:

```
./configure --ndk-path ~/Source/Android/android-ndk --sdk-path ~/Source/Android/android-sdk-linux --libsvgandroid-path ~/Source/Android/libsvgandroid --target-platform android-10
```

# compiling

either:

```
make debug
```

or:

```
make release
```

# usage

Include libkamofage.so as a prebuilt shared library in your Android.mk file. Please refer to the
Android NDK documentation for information about specifics.

Include libkamoflage.jar in your libs directory. Please refer to the Android SDK documentation for
information about specifics.
