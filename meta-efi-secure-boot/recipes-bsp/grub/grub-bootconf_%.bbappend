FILESEXTRAPATHS:prepend := "${THISDIR}/grub-bootconf:"

SRC_URI:append:class-target = " \
    file://grub-efi.cfg \
    file://boot-menu.inc \
"

inherit deploy
require ${@bb.utils.contains('COMBINED_FEATURES', 'efi-secure-boot', 'grub-bootconf-efi-secure-boot.inc', '', d)}

do_install:append:class-target() {
    rm ${D}${EFI_FILES_PATH}/grub.cfg
    install -m 0600 ${WORKDIR}/grub-efi.cfg ${D}${EFI_FILES_PATH}/grub.cfg

    local menu="${WORKDIR}/boot-menu.inc"

    # Enable the default IMA rules if IMA is enabled and luks is disabled.
    # This is because unseal operation will fail when any PCR is extended
    # due to updating the aggregate integrity value by the default IMA rules.
    [ x"${IMA}" = x"1" -a x"${@bb.utils.contains('DISTRO_FEATURES', 'luks', '1', '0', d)}" != x"1" ] && {
        ! grep -q "ima_policy=tcb" $menu &&
            sed -i 's/^\s*linux\s\+.*bzImage.*/& ima_policy=tcb/g' $menu
    }

    # Replace the root parameter in boot command line with BOOT_CMD_ROOT,
    # which can be configured. It is helpful when secure boot is enabled.
    [ -n "${BOOT_CMD_ROOT}" ] && {
        sed -i "s,root=LABEL=root,root=${BOOT_CMD_ROOT},g" $menu
    }

    # Install the stacked grub configs.
    install -m 0600 $menu ${D}${EFI_FILES_PATH}
}

do_deploy() {
    # Deploy the stacked grub configs.
    install -m 0600 ${D}${EFI_FILES_PATH}/grub.cfg ${DEPLOYDIR}
    install -m 0600 ${D}${EFI_FILES_PATH}/boot-menu.inc ${DEPLOYDIR}
}

FILES:${PN}:append = " ${EFI_FILES_PATH}/boot-menu.inc"

addtask deploy after do_install before do_package
