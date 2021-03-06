/*
 * Most of this code is derived from gstreamer-rockchip's
 * ximagesink.c, which does not have an explicit license statement.
 *
 */

#include "drm-utils.h"
#include <drm.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm_fourcc.h>

#include <errno.h>
#include <string.h>

#include "mpp_log.h"

typedef enum { FALSE, TRUE } BOOL;

static drmModePlane *
drm_find_plane_for_crtc (int fd, drmModeRes * res, drmModePlaneRes * pres,
    int crtc_id)
{
  drmModePlane *plane;
  int i, pipe;

  plane = NULL;
  pipe = -1;
  for (i = 0; i < res->count_crtcs; i++) {
    if (crtc_id == res->crtcs[i]) {
      pipe = i;
      break;
    }
  }

  if (pipe == -1)
    return NULL;

  for (i = 0; i < pres->count_planes; i++) {
    plane = drmModeGetPlane (fd, pres->planes[i]);
    if (plane->possible_crtcs & (1 << pipe))
      return plane;
    drmModeFreePlane (plane);
  }

  return NULL;
}

static drmModeCrtc *
drm_find_crtc_for_connector (int fd, drmModeRes * res, drmModeConnector * conn,
    unsigned * pipe)
{
  int i;
  int crtc_id;
  drmModeEncoder *enc;
  drmModeCrtc *crtc;

  crtc_id = -1;
  for (i = 0; i < res->count_encoders; i++) {
    enc = drmModeGetEncoder (fd, res->encoders[i]);
    if (enc) {
      if (enc->encoder_id == conn->encoder_id) {
        crtc_id = enc->crtc_id;
        drmModeFreeEncoder (enc);
        break;
      }
      drmModeFreeEncoder (enc);
    }
  }

  if (crtc_id == -1)
    return NULL;

  for (i = 0; i < res->count_crtcs; i++) {
    crtc = drmModeGetCrtc (fd, res->crtcs[i]);
    if (crtc) {
      if (crtc_id == crtc->crtc_id) {
        if (pipe)
          *pipe = i;
        return crtc;
      }
      drmModeFreeCrtc (crtc);
    }
  }

  return NULL;
}

static BOOL
drm_connector_is_used (int fd, drmModeRes * res, drmModeConnector * conn)
{
  BOOL result;
  drmModeCrtc *crtc;

  result = FALSE;
  crtc = drm_find_crtc_for_connector (fd, res, conn, NULL);
  if (crtc) {
    result = crtc->buffer_id != 0;
    drmModeFreeCrtc (crtc);
  }

  return result;
}

static drmModeConnector *
drm_find_used_connector_by_type (int fd, drmModeRes * res, int type)
{
  int i;
  drmModeConnector *conn;

  conn = NULL;
  for (i = 0; i < res->count_connectors; i++) {
    conn = drmModeGetConnector (fd, res->connectors[i]);
    if (conn) {
      if ((conn->connector_type == type)
          && drm_connector_is_used (fd, res, conn))
        return conn;
      drmModeFreeConnector (conn);
    }
  }

  return NULL;
}

static drmModeConnector *
drm_find_first_used_connector (int fd, drmModeRes * res)
{
  int i;
  drmModeConnector *conn;

  conn = NULL;
  for (i = 0; i < res->count_connectors; i++) {
    conn = drmModeGetConnector (fd, res->connectors[i]);
    if (conn) {
      if (drm_connector_is_used (fd, res, conn))
        return conn;
      drmModeFreeConnector (conn);
    }
  }

  return NULL;
}

static drmModeConnector *
drm_find_main_monitor (int fd, drmModeRes * res)
{
  /* Find the LVDS and eDP connectors: those are the main screens. */
  static const int priority[] = { DRM_MODE_CONNECTOR_LVDS,
    DRM_MODE_CONNECTOR_eDP
  };
  int i;
  drmModeConnector *conn;

  conn = NULL;
  for (i = 0; !conn && i < sizeof(priority)/sizeof(priority[0]); i++)
    conn = drm_find_used_connector_by_type (fd, res, priority[i]);

  /* if we didn't find a connector, grab the first one in use */
  if (!conn)
    conn = drm_find_first_used_connector (fd, res);

  return conn;
}

int drm_init(int drm, int *crtc_id, int *plane_id)
{
    int ret = -1;
    drmModeRes *res;
    drmModeConnector *conn;
    drmModeCrtc *crtc;
    drmModePlaneRes *pres;
    drmModePlane *plane;
    unsigned pipe;

    res = drmModeGetResources (drm);
    if (!res)
        goto resources_failed;

    conn = drm_find_main_monitor (drm, res);
    if (!conn)
        goto connector_failed;

    crtc = drm_find_crtc_for_connector (drm, res, conn, &pipe);
    if (!crtc)
        goto crtc_failed;

    pres = drmModeGetPlaneResources (drm);
    if (!pres)
        goto plane_resources_failed;

    plane = drm_find_plane_for_crtc (drm, res, pres, crtc->crtc_id);
    if (!plane)
        goto plane_failed;

    *crtc_id = crtc->crtc_id;
    *plane_id = plane->plane_id;

    ret = 0;

  bail:
    if (plane)
        drmModeFreePlane (plane);
    if (pres)
        drmModeFreePlaneResources (pres);
    if (crtc)
        drmModeFreeCrtc (crtc);
    if (conn)
        drmModeFreeConnector (conn);
    if (res)
        drmModeFreeResources (res);

    drmClose (drm);
    return ret;

    /* ERRORS */
  resources_failed:
    {
        mpp_err("drmModeGetResources failed: %s (%d)\n",
                strerror (errno), errno);
        goto bail;
    }

  connector_failed:
    {
        mpp_err("Could not find a valid monitor connector\n");
        goto bail;
    }

  crtc_failed:
    {
        mpp_err("Could not find a crtc for connector\n");
        goto bail;
    }

  plane_resources_failed:
    {
        mpp_err("drmModeGetPlaneResources failed: %s (%d)\n",
                strerror (errno), errno);
        goto bail;
    }

  plane_failed:
    {
        mpp_err("Could not find a plane for crtc\n");
        goto bail;
    }
}

// FIXME does fullscreen only, ignores window for now
void drm_display_frame(MppFrame frame, int drm, int crtc_id, int plane_id)
{
    uint32_t gem_handle;
    MppBuffer frame_buf = mpp_frame_get_buffer(frame);
    int ret = drmPrimeFDToHandle(drm, mpp_buffer_get_fd(frame_buf), &gem_handle);
    if (ret < 0) {
        fprintf(stderr, "drmPrimeFDToHandle failed: %s\n", strerror(-ret));
        return;
    }

    // FIXME hardcodes NV12
    uint32_t bo_handles[4] = { gem_handle, gem_handle, 0, 0 };
    uint32_t pitches[4] = { (uint32_t)mpp_frame_get_hor_stride(frame),
                            (uint32_t)mpp_frame_get_hor_stride(frame), 0, 0 };
    uint32_t offsets[4] = {
        0, (uint32_t)(mpp_frame_get_hor_stride(frame) * mpp_frame_get_hor_stride(frame)),
        0, 0 };
    uint32_t fb_id;
    static uint32_t last_fb_id;
    unsigned width = mpp_frame_get_width(frame);
    unsigned height = mpp_frame_get_height(frame);
    ret = drmModeAddFB2 (drm, width, height, DRM_FORMAT_NV12,
                         bo_handles, pitches, offsets, &fb_id, 0);
    if (ret < 0) {
        fprintf(stderr, "drmModeAddFB2 failed: %s\n", strerror(-ret));
        return -1;
    }

    // FIXME assumes screen has same size as stream
    ret = drmModeSetPlane (drm, plane_id,
                           crtc_id, fb_id, 0, 0, 0,
                           mpp_frame_get_width(frame), mpp_frame_get_height(frame),
                           /* source/cropping coordinates are given in Q16 */
                           0, 0, width << 16, height << 16);

    if (last_fb_id)
        drmModeRmFB (drm, last_fb_id);

    last_fb_id = fb_id;
}
