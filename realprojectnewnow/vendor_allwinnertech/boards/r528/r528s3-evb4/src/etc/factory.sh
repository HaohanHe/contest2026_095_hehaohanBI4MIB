set +e

echo "umount /data"
umount /data
echo "force format /dev/data"
mount -t yaffs -o forceformat /dev/usrdata /data