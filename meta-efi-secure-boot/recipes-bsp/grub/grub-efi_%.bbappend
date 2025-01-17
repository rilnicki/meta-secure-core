require ${@bb.utils.contains('COMBINED_FEATURES', 'efi-secure-boot', 'grub-efi-efi-secure-boot.inc', '', d)}
