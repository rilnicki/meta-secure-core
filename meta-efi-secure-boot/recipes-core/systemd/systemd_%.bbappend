require ${@bb.utils.contains('COMBINED_FEATURES', 'efi-secure-boot', 'systemd-efi-secure-boot.inc', '', d)}
