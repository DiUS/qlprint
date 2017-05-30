/*
 * Copyright 2017 DiUS Computing Pty Ltd. All rights reserved.
 *
 * Released under GPLv3, see LICENSE for details.
 *
 * @author Johny Mattsson <jmattsson@dius.com.au>
 */
#include "ql.h"
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

struct ql_ctx
{
  char *printer;
  int fd;
};

#define ESC 0x1b

#define NUM_STATUS_READ_RETRIES 100

#define full_write(fd, buf) (retry_write(fd, buf, sizeof(buf)) == (ssize_t)sizeof(buf))

ssize_t retry_write(int fd, const char *buf, size_t len)
{
  size_t written = 0;
  while (written != len)
  {
    ssize_t n = write(fd, buf + written, len - written);
    if (n == -1)
    {
      if (errno == EAGAIN || errno == EINTR)
        continue;
      else
        break;
    }
    else
      written += n;
  }
  return written;
}


ql_ctx_t ql_open(const char *printer)
{
  int fd = open(printer, O_RDWR);
  if (fd < 0)
    return NULL;

  ql_ctx_t ctx = malloc(sizeof(struct ql_ctx));
  if (!ctx)
    return NULL;
  ctx->printer = strdup(printer);
  ctx->fd = fd;

  const char clear[200] = { 0, };
  (void)full_write(fd, clear); // recommended to clear old/errored jobs

  return ctx;
}

void ql_close(ql_ctx_t ctx)
{
  free(ctx->printer);
  close(ctx->fd);
  free(ctx);
}

bool ql_init(ql_ctx_t ctx)
{
  const char init[] = { ESC, '@' };
  return full_write(ctx->fd, init);
}


bool ql_request_status(ql_ctx_t ctx)
{
  const char status_req[] = { ESC, 'i', 'S' };
  return full_write(ctx->fd, status_req);
}


bool ql_read_status(ql_ctx_t ctx, ql_status_t *status)
{
  for (int i = 0; i < NUM_STATUS_READ_RETRIES; ++i)
  {
    int ret = read(ctx->fd, status, sizeof(*status));
    if (ret == 32)
      return true;
    else if ( (ret == 0) // "no data yet, too bad we just eof'd your fd, sucker"
           || (ret == -1 && errno == EBADF)) // in case we messed up, somehow
    {
      close(ctx->fd);
      ctx->fd = open(ctx->printer, O_RDWR);
      if (ctx->fd < 0)
        return false;
    }
    else if (ret == -1 && (errno != EAGAIN || errno != EINTR))
      return false; // non-recoverable
  }
  errno = ETIME;
  return false;
}


bool ql_set_mode(ql_ctx_t ctx, unsigned mode)
{
  char cmd[] = { ESC, 'i', 'M', mode };
  return full_write(ctx->fd, cmd);
}


bool ql_set_expanded_mode(ql_ctx_t ctx, unsigned mode)
{
  char cmd[] = { ESC, 'i', 'K', mode };
  return full_write(ctx->fd, cmd);
}


bool ql_set_autocut_every_n(ql_ctx_t ctx, uint8_t n)
{
  char cmd[] = { ESC, 'i', 'A', n };
  return full_write(ctx->fd, cmd);
}


bool ql_set_margin(ql_ctx_t ctx, uint16_t dots)
{
  char cmd[] = { ESC, 'i', 'd', dots & 0xff, dots >> 8};
  return full_write(ctx->fd, cmd);
}


bool ql_needs_mode_switch(const ql_status_t *status)
{
  switch(status->model_code)
  {
    case '3': case '4':
    case 'P': case 'Q':
      return true;
    default: break;
  }
  return false;
}


bool ql_switch_to_raster_mode(ql_ctx_t ctx)
{
  #define MODE_ESC_P  0
  #define MODE_RASTER 1
  #define MODE_P_TOUCH_TEMPLATE 3
  char cmd[] = { ESC, 'i', 'a', MODE_RASTER };
  return full_write(ctx->fd, cmd);
}


static void pack_column(uint8_t *out, uint16_t bytes, uint16_t colno, const ql_raster_image_t *img, uint8_t black_below_v)
{
  for (unsigned n = 0; n < bytes; ++n, ++out)
  {
    *out = 0;
    for (unsigned i = 0; i < 8; ++i)
    {
      unsigned img_row = n * 8 + i;
      if (img_row < img->height)
        if (img->data[img_row * img->width +  colno] < black_below_v)
          *out |= 1 << (7 - i);
    }
//for(int i = 7; i >= 0; --i) fprintf(stderr, "%c", (*out & (1<<i)) ? '#' : '.');
  }
//fprintf(stderr,"\n");
}


bool ql_print_raster_image(ql_ctx_t ctx, const ql_status_t *status, const ql_raster_image_t *img, const ql_print_cfg_t *cfg)
{
  unsigned dn = 90; // default raster transmission block size (720 pixels)
  if (status->model_code == 'P' || status->model_code == '4')
    dn = 162; // 1296 pixels

  if (img->width > dn * 8)
    return false; // image too wide for printer

  char print_info[] = { ESC, 'i', 'z',
    cfg->flags | 0x80,
    (cfg->flags & QL_PRINT_CFG_MEDIA_TYPE) ? cfg->media_type : 0,
    (cfg->flags & QL_PRINT_CFG_MEDIA_WIDTH) ? cfg->media_width : 0,
    (cfg->flags & QL_PRINT_CFG_MEDIA_LENGTH) ? cfg->media_length : 0,
    img->width & 0xff, img->width >> 8, 0, 0,
    cfg->first_page ? 0 : 1, 0 };
  if (!full_write(ctx->fd, print_info))
    return false;

  for (unsigned w = 0; w < img->width; ++w)
  {
    char block[dn + 3];
    block[0] = 'g'; block[1] = 0; block[2] = dn;
    pack_column((uint8_t *)block+3, dn, w, img, cfg->threshold);
    if (!full_write(ctx->fd, block))
      return false;
  }

  char done[] = { 0x1a }; // print with feeding
  return full_write(ctx->fd, done);
}


const char *ql_decode_model(const ql_status_t *status)
{
  switch(status->model_code)
  {
    case '1': return "QL-560";
    case '2': return "QL-570";
    case '3': return "QL-580N";
    case '4': return "QL-1060N";
    case '5': return "QL-700";
    case '6': return "QL-710W";
    case '7': return "QL-720NW";
    case 'O': return "QL-500/550";
    case 'P': return "QL-1050";
    case 'Q': return "QL-650TD";
    default:
    {
      static char buf[] = "unrecognised (type code 0x$$)";
      char *q = strchr(buf, '$');
      sprintf(q, "%02hhx)", status->model_code);
      return buf;
    }
  }
}

const char *ql_decode_mode(const ql_status_t *status)
{
  if (status->mode & QL_MODE_AUTOCUT)
    return "auto-cut";
  else
    return "no-auto-cut";
}

const char *ql_decode_errors(const ql_status_t *status)
{
  typedef struct {
    uint16_t bit;
    const char *str;
  } strmap_t;

  #define ERR1(x) (x << 0)
  #define ERR2(x) (x << 8)
  const strmap_t strmap[] = {
    { ERR1(QL_ERR_1_NO_MEDIA),                  "no-media " },
    { ERR1(QL_ERR_1_END_OF_MEDIA),              "end-of-media " },
    { ERR1(QL_ERR_1_CUTTER_JAM),                "cutter-jam " },
    { ERR1(QL_ERR_1_PRINTER_IN_USE),            "printer-in-use " },
    { ERR1(QL_ERR_1_PRINTER_TURNED_OFF),        "printer-turned-off " },
    { ERR1(QL_ERR_1_HIGH_VOLTAGE_ADAPTER),      "high-voltage-adapter " },
    { ERR1(QL_ERR_1_FAN_MOTOR_ERROR),           "fan-motor-error " },

    { ERR2(QL_ERR_2_REPLACE_MEDIA),             "replace-media " },
    { ERR2(QL_ERR_2_EXPANSION_BUFFER_FULL),     "expansion-buffer-full " },
    { ERR2(QL_ERR_2_COMMUNICATION_ERROR),       "communication-error " },
    { ERR2(QL_ERR_2_COMMUNICATION_BUFFER_FULL), "communication-buffer-full " },
    { ERR2(QL_ERR_2_COVER_OPEN),                "cover-open " },
    { ERR2(QL_ERR_2_CANCEL_KEY),                "cancel-key-pressed " },
    { ERR2(QL_ERR_2_MEDIA_CANNOT_BE_FED),       "media-cannot-be-fed " },
    { ERR2(QL_ERR_2_SYSTEM_ERROR),              "system-error " }
  };

  static char *buf = 0;
  if (!buf)
  {
    int len = 0;
    for (unsigned i = 0; i < (sizeof(strmap)/sizeof(strmap[0])); ++i)
      len += strlen(strmap[i].str);
    buf = malloc(len + 1);
    if (!buf)
      return "<host-out-of-memory>";
  }

  uint16_t errs = ERR1(status->err_info_1) | ERR2(status->err_info_2);
  buf[0] = 0;
  for (unsigned i = 0; i < (sizeof(strmap)/sizeof(strmap[0])); ++i)
  {
    if (errs & strmap[i].bit)
      strcat(buf, strmap[i].str);
  }
  return (buf[0] == 0) ? "none" : buf;
}

const char *ql_decode_media_type(const ql_status_t *status)
{
  switch(status->media_type)
  {
    case QL_MEDIA_TYPE_NO_MEDIA: return "no-media";
    case QL_MEDIA_TYPE_CONTINUOUS:
    case QL_MEDIA_TYPE_CONTINUOUS_ALT:
      return "continuous-length-tape";
    case QL_MEDIA_TYPE_DIECUT_LABELS:
    case QL_MEDIA_TYPE_DIECUT_LABELS_ALT:
      return "die-cut-labels";
    default: {
      static char buf[] = "unknown (code 0x$$)";
      char *q = strchr(buf, '$');
      sprintf(q, "%02hhx)", status->media_type);
      return buf;
    }
  }
}

void ql_decode_print_status(FILE *f, const ql_status_t *status, unsigned flags)
{
  if (!status)
    return;

  const char *fmt_s = "%17s: %s\n";
  const char *fmt_u = "%17s: %u\n";
  if (flags & QL_DECODE_MODEL)
    fprintf(f, fmt_s, "Printer", ql_decode_model(status));
  if (flags & QL_DECODE_MODE)
    fprintf(f, fmt_s, "Mode", ql_decode_mode(status));
  if (flags & QL_DECODE_ERROR)
    fprintf(f, fmt_s, "Errors", ql_decode_errors(status));
  if (flags & QL_DECODE_MEDIA)
  {
    fprintf(f, fmt_s, "Media type", ql_decode_media_type(status));
    fprintf(f, fmt_u, "Media width (mm)", status->media_width_mm);
    if (status->media_type != QL_MEDIA_TYPE_CONTINUOUS)
      fprintf(f, fmt_u, "Media length (mm)", status->media_length_mm);
  }
}

