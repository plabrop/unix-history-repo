!
! BOOTSTRAP ON HK, GOING MULTI USER
!
SET DEF HEX
SET DEF LONG
SET REL:0
HALT
UNJAM
INIT
LOAD BOOT
D R10 2		! DEVICE CHOICE 2=HK
D R11 0		! 0= AUTOBOOT
START 2
