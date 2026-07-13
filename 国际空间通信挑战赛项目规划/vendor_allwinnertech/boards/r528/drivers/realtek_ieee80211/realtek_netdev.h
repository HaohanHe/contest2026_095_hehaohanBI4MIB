/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "realtek_driver.h"

int  realtek_netdev_init(void);
void realtek_netdev_notify_receive(FAR struct realtek_dev_s *priv,
                                  int index, unsigned int len);
int   realtek_netdev_deinit(void);



