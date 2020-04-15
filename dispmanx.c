#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/select.h>

#include "bcm_host.h"

#define die(...) fprintf(stderr, __VA_ARGS__), exit(1)
#define assert(q) if (!(q)) die("Failed assert on line %d: %s", __LINE__, #q)

#define usage() die("\
Usage:\n\
\n\
    dispmanx [options] < RGB.data \n\
\n\
Write raw RGB data from stdin to Raspberry Pi display via the dispmanx stack.\n\
The dispmanx layer is removed when the program exits.\n\
\n\
Options are:\n\
    -d display      - dispmanx display number, default is 0\n\
    -l layer        - dispmanx layer number, default is 0\n\
    -r              - just report display size in format 'WIDTHxHEIGHT' and exit\n\
    -t seconds      - enable timeout, default is 0 (never timeout)\n\
\n\
Use 'tvservice -l' to see list of dispmanx displays.\n\
Use 'vcgencmd dispmanx_list' to see list dispmanx layers.\n\
")

DISPMANX_DISPLAY_HANDLE_T display = 0;
DISPMANX_ELEMENT_HANDLE_T element = 0;

// Invoked by atexit to shut dispmanx down.
// Because broadcom doesn't know about exit handlers or file handles?
void cleanup(void)
{
    if (element)
    {
        DISPMANX_UPDATE_HANDLE_T update;
        assert(update = vc_dispmanx_update_start(0));
        assert(!vc_dispmanx_element_remove(update, element));
        assert(!vc_dispmanx_update_submit_sync(update));
    }
    if (display) assert(!vc_dispmanx_display_close(display));
}

int main(int argc, char *argv[])
{
    int number = 0;
    int timeout = 0;
    int report = 0;
    int layer = 0;

    while (1) switch (getopt(argc, argv, ":d:l:rt:"))
    {
        case 'd': number = atoi(optarg); break;
        case 'l': layer = atoi(optarg); break;
        case 'r': report = 1; break;
        case 't': timeout = atoi(optarg); break;

        case ':':            // missing
        case '?': usage();   // or invalid options
        case -1: goto optx;  // no more options
    } optx:

    bcm_host_init();

    atexit(cleanup); // remove element and close display on exit

    display = vc_dispmanx_display_open(number);

    DISPMANX_MODEINFO_T info;
    assert(!vc_dispmanx_display_get_info(display, &info));
    int height = info.height, width = info.width;
    if (report)
    {
        printf("%dx%d\n", width, height);
        return 0;
    }

    if (isatty(0)) die("Requires RGB data on stdin\n");

    int size = height * width * 3; // 3 bytes per pixel

    void *image;
    assert(image = malloc(size));

    // read stdin directly to the image
    int r = 0;
    while (r < size)
    {
        int n = read(0, image + r, size);
        if (n <= 0) die("Expected %d bytes of RGB data on stdin, but only got %d bytes\n", size, r);
        r += n;
    }

    // create the target resource aka display surface
    unsigned int wtf_int; // don't know what this is for
    DISPMANX_RESOURCE_HANDLE_T resource;
    assert(resource = vc_dispmanx_resource_create(VC_IMAGE_RGB888, width, height, &wtf_int));

    // copy the image to a rectangle
    VC_RECT_T image_rect;
    vc_dispmanx_rect_set( &image_rect, 0, 0, width, height);
    assert(!vc_dispmanx_resource_write_data(resource, VC_IMAGE_RGB888, width*3, image, &image_rect));

    // don't know what this is for
    VC_RECT_T wtf_rect;
    vc_dispmanx_rect_set(&wtf_rect, 0, 0, width<<16, height<<16);

    // don't know why this is needed since RGB888 doesn't have alpha
    VC_DISPMANX_ALPHA_T wtf_alpha = {DISPMANX_FLAGS_ALPHA_FROM_SOURCE | DISPMANX_FLAGS_ALPHA_FIXED_ALL_PIXELS, 255, 0}; // 255 -> opaque

    // update the resource with image_rect and sync it to the display
    DISPMANX_UPDATE_HANDLE_T update;
    assert(update = vc_dispmanx_update_start(0));
    assert(element = vc_dispmanx_element_add(update, display, layer, &image_rect, resource, &wtf_rect, DISPMANX_PROTECTION_NONE, &wtf_alpha, NULL, DISPMANX_NO_ROTATE));
    assert(!vc_dispmanx_update_submit_sync(update));

    // now wait
    if (timeout) sleep(timeout); else select(0, NULL, NULL, NULL, NULL);

    return 0;
}
