#include <debug.h>
#include <errno.h>
#include <stdint.h>
#include <nuttx/mmcsd.h>
#include <nuttx/sdio.h>
#include <nuttx/wqueue.h>
#include <sys/mount.h>

/* Automounter defaults */
#ifndef CONFIG_R528_SDHC_AUTOMOUNT_FSTYPE
#define CONFIG_R528_SDHC_AUTOMOUNT_FSTYPE "vfat"
#endif

#ifndef CONFIG_R528_SDHC_AUTOMOUNT_BLKDEV
#define CONFIG_R528_SDHC_AUTOMOUNT_BLKDEV "/dev/mmcsd1"
#endif

#ifndef CONFIG_R528_SDHC_AUTOMOUNT_MOUNTPOINT
#define CONFIG_R528_SDHC_AUTOMOUNT_MOUNTPOINT "/sdcard/"
#endif


struct sdio_dev_s *mrcio_sd_host = NULL;
static volatile int mrcio_state = 0;
static volatile int mount_state = 0;
static struct work_s sd_work;

/* worker that performs slot init and mount/unmount in thread context */
static void sd_worker(FAR void *arg)
{
  int present = (int)(uintptr_t)arg;
  int ret;

  if (present == 1) {
    if (mrcio_sd_host == NULL) {
      _info("micro_sd: sdio host not initialized in worker\n");
      return;
    }

    ret = mmcsd_slotinitialize(1, mrcio_sd_host);
    if (ret < 0) {
      _info("micro_sd: mmcsd_slotinitialize failed: %d\n", ret);
      return;
    }

    ret = mount(CONFIG_R528_SDHC_AUTOMOUNT_BLKDEV,
                CONFIG_R528_SDHC_AUTOMOUNT_MOUNTPOINT,
                CONFIG_R528_SDHC_AUTOMOUNT_FSTYPE, MS_NOSUID|MS_SYNCHRONOUS, NULL);
    if (ret >= 0) {
      mount_state = 1;
    } else {
      _info("micro_sd: mount failed: %d\n", ret);
    }

    mrcio_state = 1;
  } else {
    if (mount_state == 1) {
      //UMOUNT_NOFOLLOW MNT_EXPIRE MNT_DETACH MNT_FORCE
      // int uret = umount(CONFIG_R528_SDHC_AUTOMOUNT_MOUNTPOINT);
      int uret = umount2(CONFIG_R528_SDHC_AUTOMOUNT_MOUNTPOINT,UMOUNT_NOFOLLOW);
      if (uret >= 0) {
        mount_state = 0;
      } else {
        _info("micro_sd: umount failed: %d\n", uret);
      }
    }
    if (mrcio_state == 1) {
      if (mrcio_sd_host && mrcio_sd_host->callbackenable) {
        mrcio_sd_host->callbackenable(mrcio_sd_host, SDIOMEDIA_EJECTED);
      } else {
        _info("micro_sd: no host/callback to notify eject in worker\n");
      }
      mrcio_state = 0;
    }
  }
}
struct sdio_dev_s *sdio_initialize(int sdcno);
void set_sdio_param(int sdcno, int cd_mode,
                    void (*card_detected_cb)(uint32_t present,
                                             uint16_t sdc_id));

void card_detected(uint32_t present, uint16_t sdc_id)
{
  int ret;

  /* Schedule worker to perform mount/unmount in thread context. Do minimal
   * work in this callback (may be in interrupt context).
   */
  ret = work_queue(HPWORK, &sd_work, sd_worker, (FAR void *)(uintptr_t)present, 0);
  if (ret < 0) {
    _info("micro_sd: work_queue failed: %d\n", ret);
  }
}

int micro_sd_initialize(void) {
  set_sdio_param(0, 2, card_detected);
  mrcio_sd_host = sdio_initialize(0);
  if (mrcio_sd_host == NULL) {
    _info("micro_sd: sdio_initialize failed\n");
    return -ENODEV;
  }
  return 0;
}