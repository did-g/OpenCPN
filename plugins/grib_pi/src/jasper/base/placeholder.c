#include <assert.h>
#include "jasper/jas_malloc.h"
#include "jasper/jas_image.h"
#include "jasper/jas_stream.h"
#include "jasper/jas_cm.h"
#include "jasper/jas_icc.h"
#include "jasper/jas_debug.h"

static int no_ops(jas_image_t *image, jas_stream_t *out, const char *optstr)
{
    return -1;
}

int jp2_encode(jas_image_t *image, jas_stream_t *out, const char *optstr)
{
    return no_ops(image, out, optstr);
}

int jpc_encode(jas_image_t *image, jas_stream_t *out, const char *optstr)
{
    return no_ops(image, out, optstr);
}
