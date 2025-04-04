# v4l2-sample-capture

## Install dependencies
```
sudo apt install git build-essential cmake
```

## Clone repo:
```
git clone https://github.com/molysgaard/v4l2-sample-capture.git
cd v4l2-sample-capture
```

## Setup build system and compile:
```
mkdir build
cd build
cmake ..
make
```

## Run program:

Test V4L2 `mmap` IO-mode
```
v4l2-sample-capture mmap
```

Test V4L2 `userptr` IO-mode
```
v4l2-sample-capture userptr
```
