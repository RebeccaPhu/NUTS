#pragma once

#include "opencbm-plugin.h"

extern opencbm_plugin_init_t         *opencbm_init;
extern opencbm_plugin_uninit_t       *opencbm_uninit;
extern opencbm_plugin_driver_open_t  *opencbm_driver_open;
extern opencbm_plugin_driver_close_t *opencbm_driver_close;
extern opencbm_plugin_raw_write_t    *opencbm_raw_write;
extern opencbm_plugin_raw_read_t     *opencbm_raw_read;
extern opencbm_plugin_open_t         *opencbm_open;
extern opencbm_plugin_close_t        *opencbm_close;
extern opencbm_plugin_listen_t       *opencbm_listen;
extern opencbm_plugin_talk_t         *opencbm_talk;
extern opencbm_plugin_unlisten_t     *opencbm_unlisten;
extern opencbm_plugin_untalk_t       *opencbm_untalk;

extern bool OpenCBMLoaded;

void LoadOpenCBM();