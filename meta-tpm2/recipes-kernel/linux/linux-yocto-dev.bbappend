require ${@bb.utils.contains('COMBINED_FEATURES', 'tpm2', 'linux-yocto-tpm2.inc', '', d)}
