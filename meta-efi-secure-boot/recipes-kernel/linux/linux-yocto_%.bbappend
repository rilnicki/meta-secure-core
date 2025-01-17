require ${@bb.utils.contains('COMBINED_FEATURES', 'efi-secure-boot', 'linux-yocto-efi-secure-boot.inc', '', d)}
