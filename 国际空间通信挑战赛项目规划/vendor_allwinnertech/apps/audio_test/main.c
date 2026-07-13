/****************************************************************************
 * apps/vendor/allwinnertech/audio/voicedetect_main.c
 *
 * Voice detection and audio test application
 ****************************************************************************/

#include <nuttx/config.h>

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#include "audio_helper.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define DEFAULT_DURATION    5
#define DEFAULT_SAMPLE_RATE 16000
#define DEFAULT_BPS         16
#define DEFAULT_CHANNELS    1
#define DEFAULT_CHMAP       1

/****************************************************************************
 * Private Data
 ****************************************************************************/

static volatile bool g_interrupted = false;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static void signal_handler(int signo)
{
  printf("\n[audio] Caught signal %d, cleaning up...\n", signo);
  fflush(stdout);
  g_interrupted = true;

  /* Don't cleanup here - let main flow handle it */
  /* This just sets the flag to break out of sleep loops */
}static unsigned int parse_uint_arg(FAR const char *str, unsigned int def,
                                   unsigned int min, unsigned int max)
{
  unsigned long val;
  char *endptr;

  if (!str)
    {
      return def;
    }

  val = strtoul(str, &endptr, 10);

  if (*endptr != '\0' || val < min || val > max)
    {
      return def;
    }

  return (unsigned int)val;
}

static void print_usage(void)
{
  printf("\nVoice Detection Test Utility\n");
  printf("Usage:\n");
  printf("  audio_test record <seconds> [rate bps ch chmap]\n");
  printf("             Record audio then play it back\n\n");
  printf("  audio_test loopback <seconds> [rate bps ch chmap]\n");
  printf("          Real-time audio loopback (mic to speaker)\n\n");
  printf("Arguments:\n");
  printf("  seconds  - Duration in seconds (default: %d)\n", DEFAULT_DURATION);
  printf("  rate     - Sample rate in Hz (default: %d)\n", DEFAULT_SAMPLE_RATE);
  printf("  bps      - Bits per sample (default: %d)\n", DEFAULT_BPS);
  printf("  ch       - Number of channels (default: %d)\n", DEFAULT_CHANNELS);
  printf("  chmap    - Channel map (default: %d)\n\n", DEFAULT_CHMAP);
  printf("Examples:\n");
  printf("  audio_test record 5\n");
  printf("  audio_test loopback 10 16000 16 1 1\n\n");
}

static int cmd_record(int argc, FAR char *argv[])
{
  unsigned int seconds;
  uint32_t rate;
  uint8_t bps;
  uint8_t channels;
  uint8_t chmap;
  int ret;
  unsigned int elapsed;

  /* Parse arguments */
  seconds  = parse_uint_arg(argc > 2 ? argv[2] : NULL, DEFAULT_DURATION, 1, 3600);
  rate     = parse_uint_arg(argc > 3 ? argv[3] : NULL, DEFAULT_SAMPLE_RATE, 8000, 48000);
  bps      = parse_uint_arg(argc > 4 ? argv[4] : NULL, DEFAULT_BPS, 8, 32);
  channels = parse_uint_arg(argc > 5 ? argv[5] : NULL, DEFAULT_CHANNELS, 1, 2);
  chmap    = parse_uint_arg(argc > 6 ? argv[6] : NULL, DEFAULT_CHMAP, 0, 255);

  printf("[audio] Record mode: %us, %luHz, %ubit, %uch\n",
         seconds, (unsigned long)rate, bps, channels);

  /* Register signal handlers */
  g_interrupted = false;
  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);

  /* Initialize */
  ret = audio_init(AUDIO_MODE_RECORD, channels, bps, rate, chmap);
  if (ret < 0)
    {
      printf("[audio] ERROR: Init failed: %d\n", ret);
      signal(SIGINT, SIG_DFL);
      signal(SIGTERM, SIG_DFL);
      return ret;
    }

  /* Start recording */
  ret = audio_start();
  if (ret < 0)
    {
      printf("[audio] ERROR: Start failed: %d\n", ret);
      audio_cleanup();
      signal(SIGINT, SIG_DFL);
      signal(SIGTERM, SIG_DFL);
      return ret;
    }

  printf("[audio] Recording for %u seconds (Ctrl+C to stop)...\n", seconds);
  fflush(stdout);

  /* Sleep with interrupt checking */
  for (elapsed = 0; elapsed < seconds && !g_interrupted; elapsed++)
    {
      sleep(1);
    }

  if (g_interrupted)
    {
      printf("\n[audio] Interrupted by user\n");
      audio_stop();
      audio_cleanup();
      signal(SIGINT, SIG_DFL);
      signal(SIGTERM, SIG_DFL);
      return OK;
    }

  /* Stop recording */
  audio_stop();
  printf("[audio] Recording complete\n");

  /* Playback */
  ret = audio_playback();
  if (ret < 0)
    {
      printf("[audio] ERROR: Playback failed: %d\n", ret);
      audio_cleanup();
      signal(SIGINT, SIG_DFL);
      signal(SIGTERM, SIG_DFL);
      return ret;
    }

  printf("[audio] Playing back for %u seconds...\n", seconds);
  fflush(stdout);

  /* Sleep with interrupt checking */
  for (elapsed = 0; elapsed < seconds && !g_interrupted; elapsed++)
    {
      sleep(1);
    }

  if (g_interrupted)
    {
      printf("\n[audio] Interrupted during playback\n");
    }

  /* Cleanup */
  audio_cleanup();
  printf("[audio] Done\n");

  /* Restore default signal handlers */
  signal(SIGINT, SIG_DFL);
  signal(SIGTERM, SIG_DFL);

  return OK;
}

static int cmd_loopback(int argc, FAR char *argv[])
{
  unsigned int seconds;
  uint32_t rate;
  uint8_t bps;
  uint8_t channels;
  uint8_t chmap;
  int ret;
  unsigned int elapsed;

  /* Parse arguments */
  seconds  = parse_uint_arg(argc > 2 ? argv[2] : NULL, DEFAULT_DURATION, 1, 3600);
  rate     = parse_uint_arg(argc > 3 ? argv[3] : NULL, DEFAULT_SAMPLE_RATE, 8000, 48000);
  bps      = parse_uint_arg(argc > 4 ? argv[4] : NULL, DEFAULT_BPS, 8, 32);
  channels = parse_uint_arg(argc > 5 ? argv[5] : NULL, DEFAULT_CHANNELS, 1, 2);
  chmap    = parse_uint_arg(argc > 6 ? argv[6] : NULL, DEFAULT_CHMAP, 0, 255);

  printf("[audio] Loopback mode: %us, %luHz, %ubit, %uch\n",
         seconds, (unsigned long)rate, bps, channels);

  /* Register signal handlers for Ctrl+C */
  g_interrupted = false;
  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);

  /* Initialize */
  ret = audio_init(AUDIO_MODE_LOOPBACK, channels, bps, rate, chmap);
  if (ret < 0)
    {
      printf("[audio] ERROR: Init failed: %d\n", ret);
      return ret;
    }

  /* Start */
  ret = audio_start();
  if (ret < 0)
    {
      printf("[audio] ERROR: Start failed: %d\n", ret);
      audio_cleanup();
      return ret;
    }

  printf("[audio] Loopback running for %u seconds (Ctrl+C to stop)...\n", seconds);
  fflush(stdout);

  /* Sleep with interrupt checking */
  for (elapsed = 0; elapsed < seconds && !g_interrupted; elapsed++)
    {
      sleep(1);
    }

  if (g_interrupted)
    {
      printf("\n[audio] Interrupted by user\n");
    }

  /* Stop */
  audio_stop();

  /* Cleanup */
  audio_cleanup();
  printf("[audio] Done\n");

  /* Restore default signal handlers */
  signal(SIGINT, SIG_DFL);
  signal(SIGTERM, SIG_DFL);

  return OK;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  /* Check for help or no arguments */
  if (argc < 2 ||
      strcmp(argv[1], "-h") == 0 ||
      strcmp(argv[1], "--help") == 0 ||
      strcmp(argv[1], "help") == 0)
    {
      print_usage();
      return OK;
    }

  /* Parse command */
  if (strcmp(argv[1], "record") == 0)
    {
      return cmd_record(argc, argv);
    }
  else if (strcmp(argv[1], "loopback") == 0)
    {
      return cmd_loopback(argc, argv);
    }
  else
    {
      printf("ERROR: Unknown command '%s'\n", argv[1]);
      print_usage();
      return ERROR;
    }
}
