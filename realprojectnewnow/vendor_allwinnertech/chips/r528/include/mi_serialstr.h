/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

/****************************************************************************
 * Public Types
 ****************************************************************************/

#undef EXTERN
#if defined(__cplusplus)
#define EXTERN extern "C"
extern "C"
{
#else
#define EXTERN extern
#endif

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: usbdev_serialstr_init
 *
 * Description:
 *   Read sn from secure storage.
 *   Initialize the g_serialstr, assigned by sn.
 *
 * Returned Values:
 *   Zero is returned on success; a negated errno value is returned on
 *   any failure.
 *
 ****************************************************************************/

#ifdef CONFIG_BOARD_USBDEV_SERIALSTR
int usbdev_serialstr_init(void);
#endif

#undef EXTERN
#if defined(__cplusplus)
}
#endif
