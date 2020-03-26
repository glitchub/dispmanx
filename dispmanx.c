#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/select.h>

#include "bcm_host.h"

#define die(...) fprintf(stderr, __VA_ARGS__), exit(1)

#define usage() die("\
Usage:\n\
\n\
    dispmanx [options] < RGB.data \n\
\n\
Write raw RGB data from stdin to Raspberry Pi display via the dispmanx stack.\n\
This is necessary to put graphics on HDMI display when the DSI touchscreen is\n\
in use.\n\
\n\
Note the original screen will be restored when the program exits.\n\
\n\
Options are:\n\
    -d display      - dispmanx display number, default is 2 (try 'tvservice -l' for list)\n\
    -r              - just report display size in format 'HxW' anbd exit\n\
    -t seconds      - enable timeout, dfault is 0 (never timeout)\n\
")

int main(int argc, char *argv[])
{
    int number = 2;
    int timeout = 0;
    int report = 0;

    while (1) switch (getopt(argc, argv, ":d:rt:"))
    {
        case 'd': number = atoi(optarg); break;
        case 'r': report = 1; break;
        case 't': timeout = atoi(optarg); break;

        case ':':            // missing
        case '?': usage();   // or invalid options
        case -1: goto optx;  // no more options
    } optx:

    bcm_host_init();

    DISPMANX_DISPLAY_HANDLE_T display = vc_dispmanx_display_open(number);

    DISPMANX_MODEINFO_T info;
    if (vc_dispmanx_display_get_info(display, &info)) die("vc_dispmanx_display_get_info failed\n");
    int height = info.height, width = info.width;
    if (report)
    {
        printf("%dx%d\n", width, height);
        return 0;
    }

    if (isatty(0)) die("Requires RGB data on stdin\n");

    void *image = malloc(height * width * 3);
    if (!image) die("Out of memory\n");

    int r = 0;
    while (r < height*width*3)
    {
        int n = read(0, image + r, height*width*3);
        if (n <= 0) die("Expected %d bytes of RGB data on stdin, but only got %d bytes\n", height*width*3, r);
        r += n;
    }

    DISPMANX_UPDATE_HANDLE_T update = vc_dispmanx_update_start(0);
    if (!update) die("vc_dispmanx_update_start failed\n");

    uint32_t vc_image_ptr; // no idea what this is!
    DISPMANX_RESOURCE_HANDLE_T resource = vc_dispmanx_resource_create(VC_IMAGE_RGB888, width, height, &vc_image_ptr);
    if (!resource) die("vc_dispmanx_resource_create failed\n");

    // no idea what this is either
    VC_RECT_T src_rect;
    vc_dispmanx_rect_set(&src_rect, 0, 0, width<<16, height<<16);

    VC_RECT_T dst_rect;
    vc_dispmanx_rect_set( &dst_rect, 0, 0, width, height);
    if (vc_dispmanx_resource_write_data(resource, VC_IMAGE_RGB888, width*3, image, &dst_rect )) die("vc_dispmanx_resource_write_data failed\n");

    VC_DISPMANX_ALPHA_T alpha = { DISPMANX_FLAGS_ALPHA_FROM_SOURCE | DISPMANX_FLAGS_ALPHA_FIXED_ALL_PIXELS, 255, 0 }; // 255 -> opaque
    if (!vc_dispmanx_element_add(update, display, 2000, &dst_rect, resource, &src_rect, DISPMANX_PROTECTION_NONE, &alpha, NULL, VC_IMAGE_ROT0)) die("vc_dispmanx_element_add failed\n");

    if (vc_dispmanx_update_submit_sync( update )) die("vc_dispmanx_update_submit_sync failed\n");

    if (timeout) sleep(timeout); else select(0, NULL, NULL, NULL, NULL);

    return 0;
}
