/*
 * Copyright 2017 DiUS Computing Pty Ltd. All rights reserved.
 *
 * Released under GPLv3, see LICENSE for details.
 *
 * @author Johny Mattsson <jmattsson@dius.com.au>
 */
#ifndef _QL_H_
#define _QL_H_

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

/* The QL module is based on an amalgamation of information from:
 *
 *  - Brother QL-500/550/560/570/580N/650TD/700/1050/1060N Command Reference 
 *     http://download.brother.com/welcome/docp000678/cv_qlseries_eng_raster_600.pdf
 * - Software Developer's Manual Raster Command Reference   QL-710W/720NW 
 *    http://download.brother.com/welcome/docp000698/cv_ql710720_eng_raster_100.pdf
 * - Actual experience communicating with a QL-570
 *
 */

typedef struct
{
  uint8_t print_head_mark;
  uint8_t sz;
  uint8_t rsvd_2;  // 'B'
  uint8_t model_class;
  uint8_t model_code;
  uint8_t rsvd_5; // '0'
  uint8_t rsvd_6; // '0'
  uint8_t rsvd_7; // 0x00
  uint8_t err_info_1;
  uint8_t err_info_2;
  uint8_t media_width_mm;
  uint8_t media_type;
  uint8_t rsvd_12; // 0x00
  uint8_t rsvd_13; // 0x00
  uint8_t rsvd_14; // 0x3f
  uint8_t mode;
  uint8_t rsvd_16; // 0x00
  uint8_t media_length_mm;
  uint8_t status_type;
  uint8_t phase_type;
  uint8_t phase_hi;
  uint8_t phase_lo;
  uint8_t notification;
  uint8_t rsvd_23; // 0x00
  uint8_t rsvd_24[8]; // 0x00...
} ql_status_t;

#define QL_ERR_1_NO_MEDIA                   0x01
#define QL_ERR_1_END_OF_MEDIA               0x02
#define QL_ERR_1_CUTTER_JAM                 0x04
// 0x08 not defined
#define QL_ERR_1_PRINTER_IN_USE             0x10
#define QL_ERR_1_PRINTER_TURNED_OFF         0x20
#define QL_ERR_1_HIGH_VOLTAGE_ADAPTER       0x40
#define QL_ERR_1_FAN_MOTOR_ERROR            0x80

#define QL_ERR_2_REPLACE_MEDIA              0x01
#define QL_ERR_2_EXPANSION_BUFFER_FULL      0x02
#define QL_ERR_2_COMMUNICATION_ERROR        0x04
#define QL_ERR_2_COMMUNICATION_BUFFER_FULL  0x08
#define QL_ERR_2_COVER_OPEN                 0x10
#define QL_ERR_2_CANCEL_KEY                 0x20
#define QL_ERR_2_MEDIA_CANNOT_BE_FED        0x40
#define QL_ERR_2_SYSTEM_ERROR               0x80

#define QL_MEDIA_LENGTH_CONTINUOUS  0x00

#define QL_MEDIA_TYPE_NO_MEDIA      0x00
#define QL_MEDIA_TYPE_CONTINUOUS    0x0a
#define QL_MEDIA_TYPE_DIECUT_LABELS 0x0b
// The 710/720 might report these instead
#define QL_MEDIA_TYPE_CONTINUOUS_ALT    0x4a
#define QL_MEDIA_TYPE_DIECUT_LABELS_ALT 0x4b

// Flags for mode
#define QL_MODE_NO_AUTOCUT          0x00
#define QL_MODE_AUTOCUT             0x40

#define QL_STATUS_TYPE_REPLY            0x00
#define QL_STATUS_TYPE_PRINTING_DONE    0x01
#define QL_STATUS_TYPE_ERROR_OCCURRED   0x02
#define QL_STATUS_TYPE_TURNED_OFF       0x04
#define QL_STATUS_TYPE_NOTIFICATION     0x05
#define QL_STATUS_TYPE_PHASE_CHANGE     0x06

#define QL_PHASE_TYPE_RECEIVING         0x00
#define QL_PHASE_TYPE_PRINTING          0x01

#define QL_NOTIFICATION_NONE            0x00
#define QL_NOTIFICATION_COOLING_STARTED 0x03
#define QL_NOTIFICATION_COOLING_DONE    0x04

// Flags for expanded mode
#define QL_EXPANDED_MODE_CUT_AT_END     0x10  /* Gah, 710 doc claims 0x08! */
#define QL_EXPANDED_MODE_HIGH_RES       0x40  /* QL-570/580N/700 */

typedef struct {
  uint16_t width;
  uint16_t height;
  uint8_t data[];
} ql_raster_image_t;

typedef struct {
  uint8_t threshold; // pixel values below threshold deemed black
  uint8_t flags; // QL_PRINT_CFG_xxx flags, indicating which other fields valid
  uint8_t media_type;
  uint8_t media_width;
  uint8_t media_length;
  bool first_page; // used for autocut pagination
} ql_print_cfg_t;

#define QL_PRINT_CFG_MEDIA_TYPE     0x02
#define QL_PRINT_CFG_MEDIA_WIDTH    0x04
#define QL_PRINT_CFG_MEDIA_LENGTH   0x08
#define QL_PRINT_CFG_QUALITY_PRIO   0x40

typedef struct ql_ctx *ql_ctx_t;

ql_ctx_t ql_open(const char *printer);
void ql_close(ql_ctx_t ctx);

bool ql_init(ql_ctx_t ctx); // also cancel
bool ql_request_status(ql_ctx_t ctx);
bool ql_read_status(ql_ctx_t ctx, ql_status_t *status);

bool ql_needs_mode_switch(const ql_status_t *status);
bool ql_switch_to_raster_mode(ql_ctx_t ctx);

bool ql_set_mode(ql_ctx_t ctx, unsigned mode);
bool ql_set_expanded_mode(ql_ctx_t ctx, unsigned mode);
bool ql_set_autocut_every_n(ql_ctx_t ctx, uint8_t n);
bool ql_set_margin(ql_ctx_t ctx, uint16_t dots);

// Note: status needed for 1050/1060N detection to adjust command format
bool ql_print_raster_image(ql_ctx_t ctx, const ql_status_t *status, const ql_raster_image_t *img, const ql_print_cfg_t *cfg);

// Caution: ql_decode_*() are *not* multi-thread safe
const char *ql_decode_mode(const ql_status_t *status);
const char *ql_decode_errors(const ql_status_t *status);
const char *ql_decode_model(const ql_status_t *status);
const char *ql_decode_media_type(const ql_status_t *status);
#define QL_DECODE_MODEL  0x01
#define QL_DECODE_ERROR  0x02
#define QL_DECODE_MEDIA  0x04
#define QL_DECODE_MODE   0x08
void ql_decode_print_status(FILE *out, const ql_status_t *status, unsigned flags);

#endif
