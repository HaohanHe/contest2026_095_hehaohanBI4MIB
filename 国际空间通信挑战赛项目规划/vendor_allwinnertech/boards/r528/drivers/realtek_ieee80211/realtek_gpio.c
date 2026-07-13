#include <sys/ioctl.h>
#include <nuttx/ioexpander/gpio.h>
#include <nuttx/ioexpander/ioexpander.h>
#include <fcntl.h>
#include <stdio.h>
#include <osdep_service.h>
/****************************************************************************
 * Public Functions
 ****************************************************************************/
#define WL_DIS_PIN_GPIO "/dev/gpio3"
/****************************************************************************
 * Name: realtek_wl_set_gpio
 ****************************************************************************/

int realtek_wl_set_gpio(char * dev ,bool value)
{
    int fd_a;
    int ret;

    fd_a = open(dev, O_RDWR);

    if (fd_a == -1)
    {
        DBG_INFO("can not open WL_DIS_PIN_GPIO %s", dev);
        return false;
    }
    ret = ioctl(fd_a, GPIOC_SETPINTYPE, (unsigned long) GPIO_OUTPUT_PIN);
    if (ret == -1)
    {
        DBG_INFO("can not control WL_DIS_PIN_GPIO %s, errno %d, %s", dev, errno,
                   strerror(errno));
        close(fd_a);
        return false;
    }
    ret = ioctl(fd_a, GPIOC_WRITE, (unsigned long) value);
    if (ret == -1)
    {
        DBG_INFO("can not set WL_DIS_PIN_GPIO %s, errno %d, %s", dev, errno, strerror(errno));
        close(fd_a);
        return false;
    }

    close(fd_a);
    return 0;
}

/****************************************************************************
 * Name: bcmf_get_gpio
 ****************************************************************************/

int realtek_wl_get_gpio(char *dev)
{   
    /*int fd_a;
    int ret;
    fd_a = file_open(dev, O_RDWR);
    bool value;
    //gplh_read(gpio, &value);
    ret = ioctl(fd_a, GPIOC_READ, (unsigned long)((uintptr_t)&value));
    file_close(dev);
    return value;*/
    return 0;
}