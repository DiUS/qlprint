/*
 * Copyright 2017 DiUS Computing Pty Ltd. All rights reserved.
 *
 * Released under GPLv3, see LICENSE for details.
 *
 * @author Johny Mattsson <jmattsson@dius.com.au>
 */
#include "ql.h"
#include "loadpng.h"
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

static bool timed_out = false;
void on_alarm(int ignored)
{
  (void)ignored;
  timed_out = true;
}


void syntax(void)
{
  fprintf(stderr,
"Syntax:\n"
"  qlprint [-p lp] -i\n"
"          [-p lp] [-m margin] [-a] [-C|-D] [-W width] [-L length] [-Q] [-n num] [-t threshold] [-x timeout] png...\n"
"Where:\n"
"  -p lp         Printer port (default /dev/usb/lp0)\n"
"  -i            Print status information only, then exit\n"
"  -m margin     Margin (dots)\n"
"  -a            Enable auto-cut\n"
"  -C            Request continuous-length-tape when printing (error if not)\n"
"  -D            Request die-cut-labels when printing (error if not)\n"
"  -W width      Request particular width media when printing (error if not)\n"
"  -L length     Request particular length media when printing (error if not)\n"
"  -Q            Prioritise quality of speed\n"
"  -n num        Print num copies\n"
"  -t threshold  Threshold for black-vs-white (default 128, i.e. 0-127=black)\n"
"  -x timeout    Time to wait for successful print, in seconds (default 5)\n"
"  png...        One or more png files to print\n"
"\n");

  exit(EXIT_FAILURE);
}

int main (int argc, char *argv[])
{
  bool info_only = false;
  int32_t margin = -1;
  bool autocut = false;
  int num = 1;
  ql_print_cfg_t cfg = {
    .threshold = 0x80,
    .flags = 0
  };
  const char *printer = "/dev/usb/lp0";
  unsigned timeout = 5;
  int opt;
  while ((opt = getopt(argc, argv, "ip:m:an:CDW:L:Qx:")) != -1)
  {
    switch(opt)
    {
      case 'i': info_only = true; break;
      case 'p': printer = optarg; break;
      case 'm': margin = atoi(optarg); break;
      case 'a': autocut = true; break;
      case 'n': num = atoi(optarg); break;
      case 'C': cfg.media_type = QL_MEDIA_TYPE_CONTINUOUS;
                cfg.flags |= QL_PRINT_CFG_MEDIA_TYPE; break;
      case 'D': cfg.media_type = QL_MEDIA_TYPE_DIECUT_LABELS;
                cfg.flags |= QL_PRINT_CFG_MEDIA_TYPE; break;
      case 'W': cfg.media_width = atoi(optarg);
                cfg.flags |= QL_PRINT_CFG_MEDIA_WIDTH; break;
      case 'L': cfg.media_length = atoi(optarg);
                cfg.flags |= QL_PRINT_CFG_MEDIA_LENGTH; break;
      case 'Q': cfg.flags |= QL_PRINT_CFG_QUALITY_PRIO; break;
      case 'x': timeout = atoi(optarg); break;
      default: syntax();
    }
  }

  if (optind >= argc && !info_only)
    syntax();

  ql_ctx_t ctx = ql_open(printer);
  if (!ctx)
  {
    fprintf(stderr, "Unable to open '%s': %s\n", printer, strerror(errno));
    return EXIT_FAILURE;
  }

  if (!ql_init(ctx))
  {
    fprintf(stderr, "Failed to send initialisation sequence to printer: %s\n",
      strerror(errno));
    return EXIT_FAILURE;
  }

  if (!ql_request_status(ctx))
  {
    fprintf(stderr, "Failed to request status from printer: %s\n",
      strerror(errno));
    return EXIT_FAILURE;
  }

  ql_status_t status = { 0, };
  if (!ql_read_status(ctx, &status))
  {
    fprintf(stderr, "Failed to read status from printer: %s\n",
      strerror(errno));
    return EXIT_FAILURE;
  }

/*
for (int i = 0; i < 32; ++i)
  printf("%02hhx ", ((char *)&status)[i]);
printf("\n");
*/

  if (info_only)
  {
    ql_decode_print_status(stdout, &status,
      QL_DECODE_MODEL | QL_DECODE_MEDIA | QL_DECODE_ERROR | QL_DECODE_MODE);
    return EXIT_SUCCESS;
  }

  if (margin >= 0 && !ql_set_margin(ctx, (uint16_t)margin))
  {
    fprintf(stderr, "Failed to set margin: %s\n", strerror(errno));
    return EXIT_FAILURE;
  }
  if (autocut &&
      (!ql_set_mode(ctx, QL_MODE_AUTOCUT) ||
       !ql_set_autocut_every_n(ctx, argc - optind)))
  {
    fprintf(stderr, "Failed to set autocut: %s\n", strerror(errno));
    return EXIT_FAILURE;
  }

  if (ql_needs_mode_switch(&status) && !ql_switch_to_raster_mode(ctx))
  {
    fprintf(stderr, "Failed to set raster mode: %s\n", strerror(errno));
    return EXIT_FAILURE;
  }

  signal(SIGALRM, on_alarm);
  while (num--)
  {
    cfg.first_page = true;
    for (int i = optind; i < argc; ++i)
    {
      ql_raster_image_t *img = loadpng(argv[i]);
      if (!img)
      {
        fprintf(stderr, "Failed to load image '%s'\n", argv[i]);
        return EXIT_FAILURE;
      }
/*
for(int i = 0; i < img->height; ++i)
{
  for(int j = 0; j < img->width; ++j)
    printf("%c", img->data[i*img->width + j] < cfg.threshold ? '#' : '.');
  printf("\n");
}
*/
      if (!ql_print_raster_image(ctx, &status, img, &cfg))
      {
        fprintf(stderr, "Failed to print '%s' (%ux%u)\n",
          argv[i], img->width, img->height);
        return EXIT_FAILURE;
      }
      alarm(timeout);
      do {
        if (!ql_read_status(ctx, &status))
        {
          if (!timed_out) // try again, soon
          {
            usleep(50);
            continue;
          }
          fprintf(stderr, "Printer stopped responding!\n");
          return EXIT_FAILURE;
        }
        if (status.err_info_1 || status.err_info_2)
        {
          fprintf(stderr, "Printer reported error(s): %s\n",
            ql_decode_errors(&status));
          return EXIT_FAILURE;
        }
      } while (status.status_type != QL_STATUS_TYPE_PRINTING_DONE);
      alarm(0);

      printf("%s (%ux%u) OK\n", argv[i], img->width, img->height);

      free(img);
      cfg.first_page = false;
    }
  }

  ql_close(ctx);

  return EXIT_SUCCESS;
}
