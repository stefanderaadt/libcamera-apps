#!/bin/sh

cmake .. -DENABLE_DRM=1 -DENABLE_X11=0 -DENABLE_QT=0 -DENABLE_OPENCV=0 -DENABLE_TFLITE=0

make -j1  # use -j1 on Raspberry Pi 3 or earlier devices

sudo make install