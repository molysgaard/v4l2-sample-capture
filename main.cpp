#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>
#include <string.h>
#include <errno.h>
#include <map>
#include <stdexcept>

#define DEVICE "/dev/video1"
#define OUTPUT_FILE "/tmp/frame.raw"

// Macro for perror with line number
#define PERROR(msg) do { \
    char err_msg[256]; \
    snprintf(err_msg, sizeof(err_msg), "%s (line %d)", msg, __LINE__); \
    perror(err_msg); \
} while(0)

// Enum for camera controls
enum CameraControl {
    GROUP_HOLD = 0x009a2003,
    SENSOR_MODE = 0x009a2008,
    GAIN = 0x009a2009,
    EXPOSURE = 0x009a200a,
    FRAME_RATE = 0x009a200b,
    TRIGGER_MODE = 0x009a200f,
    IO_MODE = 0x009a2010,
    SINGLE_TRIGGER = 0x009a2012,
    BINNING_MODE = 0x009a2013,
    SENSOR_CONFIGURATION = 0x009a2032,
    SENSOR_MODE_I2C_PACKET = 0x009a2033,
    SENSOR_CONTROL_I2C_PACKET = 0x009a2034,
    BYPASS_MODE = 0x009a2064,
    OVERRIDE_ENABLE = 0x009a2065,
    HEIGHT_ALIGN = 0x009a2066,
    SIZE_ALIGN = 0x009a2067,
    WRITE_ISP_FORMAT = 0x009a2068,
    SENSOR_SIGNAL_PROPERTIES = 0x009a2069,
    SENSOR_IMAGE_PROPERTIES = 0x009a206a,
    SENSOR_CONTROL_PROPERTIES = 0x009a206b,
    SENSOR_DV_TIMINGS = 0x009a206c,
    LOW_LATENCY_MODE = 0x009a206d,
    PREFERRED_STRIDE = 0x009a206e,
    OVERRIDE_CAPTURE_TIMEOUT_MS = 0x009a206f,
    BLACK_LEVEL = 0x009a2076,
    SENSOR_MODES = 0x009a2082
};

// Enum for IO modes
enum IOMode {
    IO_MODE_MMAP,
    IO_MODE_USERPTR
};

// Structure to hold control information
struct ControlInfo {
    const char* name;
    __u32 type;
    __s64 min;
    __s64 max;
    __s64 step;
    __s64 default_value;
    __u32 flags;
};

// Map to store control information
const std::map<CameraControl, ControlInfo> controlMap = {
    {GROUP_HOLD, {"group_hold", V4L2_CTRL_TYPE_BOOLEAN, 0, 0, 0, 0, V4L2_CTRL_FLAG_EXECUTE_ON_WRITE}},
    {SENSOR_MODE, {"sensor_mode", V4L2_CTRL_TYPE_INTEGER64, 0, 1, 1, 0, V4L2_CTRL_FLAG_SLIDER}},
    {GAIN, {"gain", V4L2_CTRL_TYPE_INTEGER64, 0, 48001, 100, 0, V4L2_CTRL_FLAG_SLIDER}},
    {EXPOSURE, {"exposure", V4L2_CTRL_TYPE_INTEGER64, 1, 78617814, 1, 10000, V4L2_CTRL_FLAG_UPDATE | V4L2_CTRL_FLAG_SLIDER}},
    {FRAME_RATE, {"frame_rate", V4L2_CTRL_TYPE_INTEGER64, 1000, 96302, 100, 78800, V4L2_CTRL_FLAG_UPDATE | V4L2_CTRL_FLAG_SLIDER}},
    {TRIGGER_MODE, {"trigger_mode", V4L2_CTRL_TYPE_INTEGER, 0, 7, 1, 0, 0}},
    {IO_MODE, {"io_mode", V4L2_CTRL_TYPE_INTEGER, 0, 5, 1, 0, 0}},
    {SINGLE_TRIGGER, {"single_trigger", V4L2_CTRL_TYPE_BUTTON, 0, 0, 0, 0, V4L2_CTRL_FLAG_WRITE_ONLY | V4L2_CTRL_FLAG_EXECUTE_ON_WRITE}},
    {BINNING_MODE, {"binning_mode", V4L2_CTRL_TYPE_INTEGER, 0, 7, 1, 0, 0}},
    {SENSOR_CONFIGURATION, {"sensor_configuration", V4L2_CTRL_TYPE_U32, 0, 4294967295, 1, 0, V4L2_CTRL_FLAG_READ_ONLY | V4L2_CTRL_FLAG_VOLATILE | V4L2_CTRL_FLAG_HAS_PAYLOAD}},
    {SENSOR_MODE_I2C_PACKET, {"sensor_mode_i2c_packet", V4L2_CTRL_TYPE_U32, 0, 4294967295, 1, 0, V4L2_CTRL_FLAG_READ_ONLY | V4L2_CTRL_FLAG_VOLATILE | V4L2_CTRL_FLAG_HAS_PAYLOAD}},
    {SENSOR_CONTROL_I2C_PACKET, {"sensor_control_i2c_packet", V4L2_CTRL_TYPE_U32, 0, 4294967295, 1, 0, V4L2_CTRL_FLAG_READ_ONLY | V4L2_CTRL_FLAG_VOLATILE | V4L2_CTRL_FLAG_HAS_PAYLOAD}},
    {BYPASS_MODE, {"bypass_mode", V4L2_CTRL_TYPE_INTEGER_MENU, 0, 1, 0, 0, 0}},
    {OVERRIDE_ENABLE, {"override_enable", V4L2_CTRL_TYPE_INTEGER_MENU, 0, 1, 0, 0, 0}},
    {HEIGHT_ALIGN, {"height_align", V4L2_CTRL_TYPE_INTEGER, 1, 16, 1, 1, 0}},
    {SIZE_ALIGN, {"size_align", V4L2_CTRL_TYPE_INTEGER_MENU, 0, 2, 0, 0, 0}},
    {WRITE_ISP_FORMAT, {"write_isp_format", V4L2_CTRL_TYPE_INTEGER, 1, 1, 1, 1, 0}},
    {SENSOR_SIGNAL_PROPERTIES, {"sensor_signal_properties", V4L2_CTRL_TYPE_U32, 0, 4294967295, 1, 0, V4L2_CTRL_FLAG_READ_ONLY | V4L2_CTRL_FLAG_HAS_PAYLOAD}},
    {SENSOR_IMAGE_PROPERTIES, {"sensor_image_properties", V4L2_CTRL_TYPE_U32, 0, 4294967295, 1, 0, V4L2_CTRL_FLAG_READ_ONLY | V4L2_CTRL_FLAG_HAS_PAYLOAD}},
    {SENSOR_CONTROL_PROPERTIES, {"sensor_control_properties", V4L2_CTRL_TYPE_U32, 0, 4294967295, 1, 0, V4L2_CTRL_FLAG_READ_ONLY | V4L2_CTRL_FLAG_HAS_PAYLOAD}},
    {SENSOR_DV_TIMINGS, {"sensor_dv_timings", V4L2_CTRL_TYPE_U32, 0, 4294967295, 1, 0, V4L2_CTRL_FLAG_READ_ONLY | V4L2_CTRL_FLAG_HAS_PAYLOAD}},
    {LOW_LATENCY_MODE, {"low_latency_mode", V4L2_CTRL_TYPE_BOOLEAN, 0, 0, 0, 0, 0}},
    {PREFERRED_STRIDE, {"preferred_stride", V4L2_CTRL_TYPE_INTEGER, 0, 65535, 1, 0, 0}},
    {OVERRIDE_CAPTURE_TIMEOUT_MS, {"override_capture_timeout_ms", V4L2_CTRL_TYPE_INTEGER, -1, 2147483647, 1, 5000, 0}},
    {BLACK_LEVEL, {"black_level", V4L2_CTRL_TYPE_INTEGER64, 0, 0, 1, 0, V4L2_CTRL_FLAG_SLIDER | V4L2_CTRL_FLAG_EXECUTE_ON_WRITE}},
    {SENSOR_MODES, {"sensor_modes", V4L2_CTRL_TYPE_INTEGER, 0, 30, 1, 30, V4L2_CTRL_FLAG_READ_ONLY}}
};

// Set a V4L2 control dynamically
int set_control(int fd, CameraControl control_id, __s32 value) {
    struct v4l2_control ctrl;
    ctrl.id = control_id;
    ctrl.value = value;
    
    if (ioctl(fd, VIDIOC_S_CTRL, &ctrl) < 0) {
        PERROR("VIDIOC_S_CTRL");
        return -1;
    }
    return 0;
}

// Add this function to capture n frames and return the nth one
int capture_nth_frame(int fd, void* buffer, struct v4l2_buffer& buf, int n) {
    if (n < 1) {
        fprintf(stderr, "Frame number must be >= 1\n");
        return -1;
    }

    // Start streaming if not already started
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd, VIDIOC_STREAMON, &type) < 0) {
        PERROR("VIDIOC_STREAMON");
        return -1;
    }

    // Skip n-1 frames
    for (int i = 1; i < n; i++) {
        if (ioctl(fd, VIDIOC_DQBUF, &buf) < 0) {
            PERROR("VIDIOC_DQBUF");
            return -1;
        }
        
        // Re-queue the buffer for the next frame
        if (ioctl(fd, VIDIOC_QBUF, &buf) < 0) {
            PERROR("VIDIOC_QBUF");
            return -1;
        }
    }

    // Capture the nth frame
    if (ioctl(fd, VIDIOC_DQBUF, &buf) < 0) {
        PERROR("VIDIOC_DQBUF");
        return -1;
    }

    return 0;
}

// Function to parse IO mode from command line
IOMode parse_io_mode(const char* mode) {
    if (strcmp(mode, "mmap") == 0) {
        return IO_MODE_MMAP;
    } else if (strcmp(mode, "userptr") == 0) {
        return IO_MODE_USERPTR;
    }
    throw std::runtime_error("IO mode must be either 'mmap' or 'userptr'");
}

// Modify main function signature and add IO mode handling
int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <io_mode> [frame_number]\n", argv[0]);
        fprintf(stderr, "  io_mode: mmap or userptr\n");
        fprintf(stderr, "  frame_number: optional, defaults to 10\n");
        return EXIT_FAILURE;
    }

    IOMode io_mode;
    try {
        io_mode = parse_io_mode(argv[1]);
    } catch (const std::runtime_error& e) {
        fprintf(stderr, "Error: %s\n", e.what());
        return EXIT_FAILURE;
    }

    int frame_number = 10;  // default to frame 10
    if (argc > 2) {
        frame_number = atoi(argv[2]);
        if (frame_number < 1) {
            fprintf(stderr, "Frame number must be >= 1\n");
            return EXIT_FAILURE;
        }
    }

    int fd = open(DEVICE, O_RDWR);
    if (fd < 0) {
        PERROR("Failed to open video device");
        return EXIT_FAILURE;
    }

    // Query supported controls
    struct v4l2_queryctrl queryctrl;
    memset(&queryctrl, 0, sizeof(queryctrl));

    printf("Available controls:\n");
    for (queryctrl.id = V4L2_CID_BASE; queryctrl.id < V4L2_CID_LASTP1; queryctrl.id++) {
        if (ioctl(fd, VIDIOC_QUERYCTRL, &queryctrl) == 0) {
            printf("  %-20s (ID: 0x%08X) Type: %d\n", queryctrl.name, queryctrl.id, queryctrl.type);
        }
    }

    // Set some example controls dynamically
    set_control(fd, SENSOR_MODE, 0);  // sensor_mode
    set_control(fd, GAIN, 5000); // gain
    set_control(fd, EXPOSURE, 10000); // exposure

    // Set video format
    struct v4l2_format fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = 2464;
    fmt.fmt.pix.height = 2064;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_SRGGB10;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;

    if (ioctl(fd, VIDIOC_S_FMT, &fmt) < 0) {
        PERROR("VIDIOC_S_FMT");
        close(fd);
        return EXIT_FAILURE;
    }

    // Modify buffer setup based on IO mode
    struct v4l2_requestbuffers req;
    memset(&req, 0, sizeof(req));
    req.count = 1;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = (io_mode == IO_MODE_MMAP) ? V4L2_MEMORY_MMAP : V4L2_MEMORY_USERPTR;

    if (ioctl(fd, VIDIOC_REQBUFS, &req) < 0) {
        PERROR("VIDIOC_REQBUFS");
        close(fd);
        return EXIT_FAILURE;
    }

    // Buffer setup
    struct v4l2_buffer buf;
    memset(&buf, 0, sizeof(buf));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = (io_mode == IO_MODE_MMAP) ? V4L2_MEMORY_MMAP : V4L2_MEMORY_USERPTR;
    buf.index = 0;

    void* buffer = nullptr;
    size_t buffer_size;

    if (io_mode == IO_MODE_MMAP) {
        // Existing MMAP code
        if (ioctl(fd, VIDIOC_QUERYBUF, &buf) < 0) {
            PERROR("VIDIOC_QUERYBUF");
            close(fd);
            return EXIT_FAILURE;
        }
        buffer_size = buf.length;
        buffer = mmap(NULL, buffer_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);
        if (buffer == MAP_FAILED) {
            PERROR("mmap");
            close(fd);
            return EXIT_FAILURE;
        }
    } else {
        // USERPTR mode
        buffer_size = fmt.fmt.pix.sizeimage;
        // Align buffer to page size
        buffer_size = (buffer_size + getpagesize() - 1) & ~(getpagesize() - 1);
        
        // Allocate user buffer
        buffer = aligned_alloc(getpagesize(), buffer_size);
        if (!buffer) {
            PERROR("aligned_alloc");
            close(fd);
            return EXIT_FAILURE;
        }

        buf.length = buffer_size;
        buf.m.userptr = (unsigned long)buffer;
    }

    // Queue buffer
    if (ioctl(fd, VIDIOC_QBUF, &buf) < 0) {
        PERROR("VIDIOC_QBUF");
        if (io_mode == IO_MODE_MMAP) {
            munmap(buffer, buffer_size);
        } else {
            free(buffer);
        }
        close(fd);
        return EXIT_FAILURE;
    }

    // Use existing capture_nth_frame function
    if (capture_nth_frame(fd, buffer, buf, frame_number) < 0) {
        if (io_mode == IO_MODE_MMAP) {
            munmap(buffer, buffer_size);
        } else {
            free(buffer);
        }
        close(fd);
        return EXIT_FAILURE;
    }

    // Save frame to file
    FILE *fp = fopen(OUTPUT_FILE, "wb");
    if (!fp) {
        PERROR("Failed to open output file");
    } else {
        fwrite(buffer, buf.bytesused, 1, fp);
        fclose(fp);
        printf("Frame %d saved to %s (using %s mode)\n", 
               frame_number, OUTPUT_FILE, 
               io_mode == IO_MODE_MMAP ? "mmap" : "userptr");
    }

    // Stop streaming
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd, VIDIOC_STREAMOFF, &type) < 0) {
        PERROR("VIDIOC_STREAMOFF");
    }

    // Cleanup
    if (io_mode == IO_MODE_MMAP) {
        munmap(buffer, buffer_size);
    } else {
        free(buffer);
    }
    close(fd);

    return EXIT_SUCCESS;
}
