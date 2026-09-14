#include <nuttx/config.h>
#ifdef CONFIG_VELAEVB1_NSH

#if !(defined(CONFIG_BOARDCTL_RESET_CAUSE) && !defined(CONFIG_NSH_DISABLE_RESET_CAUSE))
#error "Deps: defined(CONFIG_BOARDCTL_RESET_CAUSE) && !defined(CONFIG_NSH_DISABLE_RESET_CAUSE)"
#endif
#if !defined(CONFIG_KVDB)
#error "Deps: defined(CONFIG_KVDB)"
#endif

set -x
echo "You're running an nsh image."

mount -t romfs /dev/res /resource
mount -t yaffs /dev/usrdata /data
set errcode $?
if [ $errcode -ne 0 ]
then
  echo "/dev/usrdata mount failed, errcode is" $errcode
fi

set resetcause `resetcause`
echo $resetcause

if [ "$resetcause" == "cpu_soft_reset(restore)" ]
then
  echo "recovery reset system"
  sh /etc/factory.sh
  echo "reboot system now"
  reboot
fi

#ifdef CONFIG_SHOW_LOGO
/* Do not show logo if system crashed or during silient OTA. */
if [ "$resetcause" == "cpu_soft_reset(panic)" ]
then
  echo "System crashed, hide logo"
else
  set silent_reboot_flag `getprop persist.silent_reboot.flag`

  /* Show logo firstly before reset system, if rst_btn is pressed.*/
  if [ -z $silent_reboot_flag -o $silent_reboot_flag == false ]
  then
    showlogo -n /resource/logo/logo1.bin
  fi
fi
#endif

echo "Boot nsh ok"

#endif
