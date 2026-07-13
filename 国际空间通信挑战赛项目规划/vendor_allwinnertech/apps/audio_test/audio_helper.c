/****************************************************************************
 * vendor/allwinnertech/apps/audio_test/audio_helper.c
 * Audio helper - Direct FIFO approach for loopback
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 ****************************************************************************/

#include <nuttx/config.h>
#include <nuttx/audio/audio.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>

#include <audioutils/nxaudio.h>
#include "system/nxrecorder.h"
#include "system/nxplayer.h"
#include "audio_helper.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define RECORD_FILE     "/tmp/audio_test.pcm"
#define LOOPBACK_FIFO   "/var/pipe/loopback"

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct audio_context_s
{
  /* Mode */
  enum audio_mode_e mode;

  /* Audio parameters */
  uint8_t channels;
  uint8_t bps;
  uint32_t sample_rate;
  uint8_t chmap;

  /* Devices */
  FAR struct nxrecorder_s *recorder;
  FAR struct nxplayer_s *player;
  FAR struct nxaudio_s nxaudio;

  /* Loopback thread */
  pthread_t player_thread;
  int player_ret;
  bool player_thread_running;

  /* State */
  bool initialized;
  bool nxaudio_inited;
};

/****************************************************************************
 * Private Data
 ****************************************************************************/

static struct audio_context_s g_audio_ctx;

/****************************************************************************
 * Private Functions - Record Mode
 ****************************************************************************/

static int record_write_callback(int fd, FAR struct ap_buffer_s *apb)
{
  int ret;

  ret = write(fd, apb->samp, apb->nbytes);
  if (ret < 0)
    {
      printf("[record] ERROR: write failed: %d\n", errno);
      return ret;
    }

  return ret;
}

static struct nxrecorder_enc_ops_s g_record_ops =
{
  AUDIO_FMT_PCM,
  NULL,
  record_write_callback,
};

/****************************************************************************
 * Private Functions - Loopback Mode
 ****************************************************************************/

struct player_init_args_s
{
  FAR struct nxplayer_s *player;
  const char *filepath;
  uint8_t channels;
  uint8_t bps;
  uint32_t sample_rate;
  uint8_t chmap;
};

static void *player_init_thread(void *arg)
{
  struct player_init_args_s *args = (struct player_init_args_s *)arg;
  int ret;

  printf("[loopback] Player thread: opening FIFO (will block)...\n");
  fflush(stdout);

  /* This will block until recorder opens write-end */
  ret = nxplayer_playraw(args->player,
                        args->filepath,
                        AUDIO_FMT_PCM,
                        0,
                        args->channels,
                        args->bps,
                        args->sample_rate,
                        args->chmap);

  g_audio_ctx.player_ret = ret;

  if (ret == OK)
    {
      printf("[loopback] Player thread: playback started\n");
      fflush(stdout);
    }
  else
    {
      printf("[loopback] Player thread: ERROR %d\n", ret);
      fflush(stdout);
    }

  free(args);
  return NULL;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int audio_init(enum audio_mode_e mode, uint8_t channels, uint8_t bps,
               uint32_t sample_rate, uint8_t chmap)
{
  struct stat st;
  int ret;

  if (g_audio_ctx.initialized)
    {
      printf("[audio] ERROR: Already initialized\n");
      return -EBUSY;
    }

  printf("[audio] Initializing mode=%d, %luHz, %ubit, %uch\n",
         mode, (unsigned long)sample_rate, bps, channels);

  memset(&g_audio_ctx, 0, sizeof(g_audio_ctx));
  g_audio_ctx.mode = mode;
  g_audio_ctx.channels = channels;
  g_audio_ctx.bps = bps;
  g_audio_ctx.sample_rate = sample_rate;
  g_audio_ctx.chmap = chmap;

  /* Create recorder */
  g_audio_ctx.recorder = nxrecorder_create();
  if (!g_audio_ctx.recorder)
    {
      printf("[audio] ERROR: Failed to create recorder\n");
      return -ENOMEM;
    }

  /* Set callback for record mode only */
  if (mode == AUDIO_MODE_RECORD)
    {
      g_audio_ctx.recorder->ops = &g_record_ops;
    }
  else /* AUDIO_MODE_LOOPBACK */
    {
      /* Check /var/pipe exists */
      if (stat("/var/pipe", &st) < 0)
        {
          printf("[audio] ERROR: /var/pipe not found\n");
          nxrecorder_release(g_audio_ctx.recorder);
          return -ENOENT;
        }

      /* Create FIFO - recorder will write to it directly */
      unlink(LOOPBACK_FIFO);
      ret = mkfifo(LOOPBACK_FIFO, 0666);
      if (ret < 0 && errno != EEXIST)
        {
          printf("[audio] ERROR: mkfifo failed: %d\n", errno);
          nxrecorder_release(g_audio_ctx.recorder);
          return -errno;
        }
      printf("[audio] FIFO created: %s\n", LOOPBACK_FIFO);

      /* Create player */
      g_audio_ctx.player = nxplayer_create();
      if (!g_audio_ctx.player)
        {
          printf("[audio] ERROR: Failed to create player\n");
          nxrecorder_release(g_audio_ctx.recorder);
          unlink(LOOPBACK_FIFO);
          return -ENOMEM;
        }

      nxplayer_setdevice(g_audio_ctx.player, "/dev/audio/pcm0p");

      /* Start player in background thread (will block on FIFO open) */
      struct player_init_args_s *args = malloc(sizeof(*args));
      if (!args)
        {
          printf("[audio] ERROR: malloc failed\n");
          nxplayer_release(g_audio_ctx.player);
          g_audio_ctx.player = NULL;
          nxrecorder_release(g_audio_ctx.recorder);
          unlink(LOOPBACK_FIFO);
          return -ENOMEM;
        }

      args->player = g_audio_ctx.player;
      args->filepath = LOOPBACK_FIFO;
      args->channels = channels;
      args->bps = bps;
      args->sample_rate = sample_rate;
      args->chmap = chmap;

      g_audio_ctx.player_thread_running = true;
      ret = pthread_create(&g_audio_ctx.player_thread, NULL,
                          player_init_thread, args);
      if (ret != 0)
        {
          printf("[audio] ERROR: pthread_create failed: %d\n", ret);
          free(args);
          g_audio_ctx.player_thread_running = false;
          nxplayer_release(g_audio_ctx.player);
          g_audio_ctx.player = NULL;
          nxrecorder_release(g_audio_ctx.recorder);
          unlink(LOOPBACK_FIFO);
          return -ret;
        }

      printf("[audio] Player thread started (waiting for recorder)\n");

      /* Give thread time to enter blocking state */
      usleep(100000);  /* 100ms */
    }

  g_audio_ctx.initialized = true;
  printf("[audio] Initialization complete\n");
  return OK;
}

int audio_start(void)
{
  const char *filename;
  int ret;

  if (!g_audio_ctx.initialized)
    {
      printf("[audio] ERROR: Not initialized\n");
      return -EINVAL;
    }

  /* For loopback: recorder opens write-end, unblocking player thread */
  if (g_audio_ctx.mode == AUDIO_MODE_LOOPBACK)
    {
      filename = LOOPBACK_FIFO;
      printf("[audio] Starting recorder (will unblock player thread)...\n");
      fflush(stdout);

      nxrecorder_setdevice(g_audio_ctx.recorder, "/dev/audio/pcm0c");
      ret = nxrecorder_recordinternal(g_audio_ctx.recorder, filename,
                                      AUDIO_FMT_PCM,
                                      g_audio_ctx.channels,
                                      g_audio_ctx.bps,
                                      g_audio_ctx.sample_rate,
                                      g_audio_ctx.chmap);
      if (ret != OK)
        {
          printf("[audio] ERROR: Failed to start recorder: %d\n", ret);
          return ret;
        }
      printf("[audio] Recorder started\n");
      fflush(stdout);

      /* Wait for player thread to complete initialization */
      printf("[audio] Waiting for player thread...\n");
      fflush(stdout);

      /* Simple join - player should start quickly once recorder opens FIFO */
      ret = pthread_join(g_audio_ctx.player_thread, NULL);
      if (ret != 0)
        {
          printf("[audio] ERROR: pthread_join failed: %d\n", ret);
          nxrecorder_stop(g_audio_ctx.recorder);
          g_audio_ctx.player_thread_running = false;
          return -ret;
        }

      g_audio_ctx.player_thread_running = false;      if (g_audio_ctx.player_ret != OK)
        {
          printf("[audio] ERROR: Player failed: %d\n", g_audio_ctx.player_ret);
          nxrecorder_stop(g_audio_ctx.recorder);
          return g_audio_ctx.player_ret;
        }

      printf("[audio] Loopback active (mic -> speaker)\n");
      fflush(stdout);
      return OK;
    }
  else
    {
      filename = RECORD_FILE;
      printf("[audio] Starting recorder -> file with callback\n");

      nxrecorder_setdevice(g_audio_ctx.recorder, "/dev/audio/pcm0c");
      ret = nxrecorder_recordinternal(g_audio_ctx.recorder, filename,
                                      AUDIO_FMT_PCM,
                                      g_audio_ctx.channels,
                                      g_audio_ctx.bps,
                                      g_audio_ctx.sample_rate,
                                      g_audio_ctx.chmap);
      if (ret != OK)
        {
          printf("[audio] ERROR: Failed to start recording: %d\n", ret);
          return ret;
        }

      printf("[audio] Recording started\n");
      return OK;
    }
}

void audio_stop(void)
{
  if (!g_audio_ctx.initialized)
    {
      return;
    }

  printf("[audio] Stopping...\n");

  if (g_audio_ctx.recorder)
    {
      nxrecorder_stop(g_audio_ctx.recorder);
      printf("[audio] Recorder stopped\n");
    }
}

int audio_playback(void)
{
  FAR struct nxplayer_s *player;
  int ret;

  if (!g_audio_ctx.initialized)
    {
      printf("[audio] ERROR: Not initialized\n");
      return -EINVAL;
    }

  if (g_audio_ctx.mode != AUDIO_MODE_RECORD)
    {
      printf("[audio] ERROR: Playback only available in record mode\n");
      return -EINVAL;
    }

  printf("[audio] Starting playback...\n");

  player = nxplayer_create();
  if (!player)
    {
      printf("[audio] ERROR: Failed to create player\n");
      return -ENOMEM;
    }

  nxplayer_setdevice(player, "/dev/audio/pcm0p");
  ret = nxplayer_playraw(player, RECORD_FILE,
                        AUDIO_FMT_PCM,
                        0,
                        g_audio_ctx.channels,
                        g_audio_ctx.bps,
                        g_audio_ctx.sample_rate,
                        g_audio_ctx.chmap);
  if (ret != OK)
    {
      printf("[audio] ERROR: Failed to start playback: %d\n", ret);
      nxplayer_release(player);
      return ret;
    }

  printf("[audio] Playback started\n");

  /* Store player for cleanup */
  g_audio_ctx.player = player;
  return OK;
}

void audio_cleanup(void)
{
  if (!g_audio_ctx.initialized)
    {
      return;
    }

  printf("[audio] Cleaning up...\n");
  fflush(stdout);

  /* For loopback: wait for player thread if still running */
  if (g_audio_ctx.mode == AUDIO_MODE_LOOPBACK && g_audio_ctx.player_thread_running)
    {
      printf("[audio] Waiting for player thread to exit...\n");
      fflush(stdout);

      /* Player thread will exit when FIFO is closed or receives EOF */
      pthread_join(g_audio_ctx.player_thread, NULL);
      g_audio_ctx.player_thread_running = false;

      printf("[audio] Player thread exited\n");
      fflush(stdout);
    }

  /* Stop and release player (both modes) */
  if (g_audio_ctx.player)
    {
      printf("[audio] Stopping player...\n");
      fflush(stdout);
#ifndef CONFIG_AUDIO_EXCLUDE_STOP
      nxplayer_stop(g_audio_ctx.player);
#endif
      printf("[audio] Player stopped\n");
      fflush(stdout);

      printf("[audio] Releasing player...\n");
      fflush(stdout);
      nxplayer_release(g_audio_ctx.player);
      g_audio_ctx.player = NULL;
      printf("[audio] Player released\n");
      fflush(stdout);
    }

  /* Stop and release recorder */
  if (g_audio_ctx.recorder)
    {
      printf("[audio] Stopping recorder...\n");
      fflush(stdout);
      nxrecorder_stop(g_audio_ctx.recorder);
      printf("[audio] Recorder stopped\n");
      fflush(stdout);

      printf("[audio] Releasing recorder...\n");
      fflush(stdout);
      nxrecorder_release(g_audio_ctx.recorder);
      g_audio_ctx.recorder = NULL;
      printf("[audio] Recorder released\n");
      fflush(stdout);
    }

  /* Remove FIFO for loopback mode */
  if (g_audio_ctx.mode == AUDIO_MODE_LOOPBACK)
    {
      unlink(LOOPBACK_FIFO);
      printf("[audio] FIFO removed\n");
      fflush(stdout);
    }

  /* Reset state */
  g_audio_ctx.initialized = false;
  printf("[audio] Cleanup complete\n");
  fflush(stdout);
}

void audio_set_volume(int vol)
{
  if (!g_audio_ctx.nxaudio_inited)
    {
      init_nxaudio(&g_audio_ctx.nxaudio, 16000, 16, 1);
      g_audio_ctx.nxaudio_inited = true;
    }

  nxaudio_setvolume(&g_audio_ctx.nxaudio, vol);
}
