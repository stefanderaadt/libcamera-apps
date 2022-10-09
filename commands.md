libcamera-motion --lores-width 128 --lores-height 96 --post-process-file ../assets/motion_detect.json --inline --circular -o test.h264

cmake .. -DENABLE_DRM=1 -DENABLE_X11=0 -DENABLE_QT=0 -DENABLE_OPENCV=0 -DENABLE_TFLITE=0 && make -j1 && sudo make install
