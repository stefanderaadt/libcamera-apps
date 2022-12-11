libcamera-motion --lores-width 128 --lores-height 96 --post-process-file ../assets/motion_detect.json --inline --circular --motion-output motion -o -

cmake .. -DENABLE_DRM=1 -DENABLE_X11=0 -DENABLE_QT=0 -DENABLE_OPENCV=0 -DENABLE_TFLITE=0 && make -j1 && sudo make install

libcamera-motion --nopreview --width 1280 --height 720 --framerate 25 --bitrate 10000000 --codec H264 --profile baseline --inline --circular --output - --motion-output motion --lores-width 128 --lores-height 96 --post-process-file ../assets/motion_detect.json
