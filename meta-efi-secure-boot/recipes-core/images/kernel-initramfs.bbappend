require ${@bb.utils.contains('COMBINED_FEATURES', 'efi-secure-boot', 'kernel-initramfs-efi-secure-boot.inc', '', d)}
