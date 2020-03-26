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
The dispmanx layer is removed when the program exits.\n\
\n\
Options are:\n\
    -d display      - dispmanx display number, default is 0 (try 'tvservice -l' for list).\n\
    -r              - just report display size in format 'WIDTHxHEIGHT' and exit.\n\
    -t seconds      - enable timeout, default is 0 (never timeout).\n\
")

int main(int argc, char *argv[])
{
    int number = 0;
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

    int size = height * width * 3; // 3 bytes per pixel

    void *image = malloc(size);
    if (!image) die("Out of memory\n");

    // read stdin directly to the image
    int r = 0;
    while (r < size)
    {
        int n = read(0, image + r, size);
        if (n <= 0) die("Expected %d bytes of RGB data on stdin, but only got %d bytes\n", size, r);
        r += n;
    }

    DISPMANX_UPDATE_HANDLE_T update = vc_dispmanx_update_start(0);
    if (!update) die("vc_dispmanx_update_start failed\n");

    unsigned int junk; // no idea what this is for
    DISPMANX_RESOURCE_HANDLE_T resource = vc_dispmanx_resource_create(VC_IMAGE_RGB888, width, height, &junk);
    if (!resource) die("vc_dispmanx_resource_create failed\n");

    VC_RECT_T image_rect;
    vc_dispmanx_rect_set( &image_rect, 0, 0, width, height);
    if (vc_dispmanx_resource_write_data(resource, VC_IMAGE_RGB888, width*3, image, &image_rect )) die("vc_dispmanx_resource_write_data failed\n");

    // no idea what this is either
    VC_RECT_T junk_rect;
    vc_dispmanx_rect_set(&junk_rect, 0, 0, width<<16, height<<16);

    VC_DISPMANX_ALPHA_T alpha = {DISPMANX_FLAGS_ALPHA_FROM_SOURCE | DISPMANX_FLAGS_ALPHA_FIXED_ALL_PIXELS, 255, 0}; // 255 -> opaque
    if (!vc_dispmanx_element_add(update, display, 0, &image_rect, resource, &junk_rect, DISPMANX_PROTECTION_NONE, &alpha, NULL, VC_IMAGE_ROT0))
        die("vc_dispmanx_element_add failed\n");

    if (vc_dispmanx_update_submit_sync(update)) die("vc_dispmanx_update_submit_sync failed\n");

    if (timeout) sleep(timeout); else select(0, NULL, NULL, NULL, NULL);

    return 0;
}
