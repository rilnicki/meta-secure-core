require ${@bb.utils.contains('COMBINED_FEATURES', 'efi-secure-boot', '${BPN}-efi-secure-boot.inc', '', d)}
