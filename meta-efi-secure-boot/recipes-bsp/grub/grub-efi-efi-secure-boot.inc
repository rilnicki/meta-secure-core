RDEPENDS:${PN} += "efitools \
                   ${@'seloader' if d.getVar('UEFI_SELOADER') == '1' else ''} \
                   ${@'mokutil shim' if d.getVar('MOK_SB') == '1' else ''} \
                   "
DEPENDS += "openssl-native"
FILESEXTRAPATHS:prepend := "${THISDIR}/grub-efi:"

GRUB_SIGN_VERIFY_STRICT ?= "1"

GRUB_MOKVERIFY_PATCH = " \
    file://0003-mok2verify-support-to-verify-non-PE-file-with-PKCS-7.patch \
    file://0004-support-mok2-verify-in-multiboot2-protocol.patch \
    file://0005-Verify-all-buffiles.patch \
    file://0006-grub-efi-support-passing-a-buffer-to-be-verified.patch \
    file://0007-efi-fallback-to-legacy-mode-if-mok2verify-is-loaded.patch \
    file://0008-mok2verify-remove-unused-parameter-from-grub_efi_mok.patch \
"

SRC_URI:append:class-target = " \
    file://0001-Add-efivar-module-to-read-EFI-variables.patch \
    file://0002-grub-verify-Add-strict_security-variable.patch \
    ${@d.getVar('GRUB_MOKVERIFY_PATCH') if d.getVar('UEFI_SELOADER') == '1' else ''} \
    file://sbat.csv \
"

COMPATIBLE_HOST:aarch64 = 'null'

GRUB_PREFIX_DIR ?= "/EFI/BOOT"

GRUB_SECURE_BOOT_MODULES += "efivar password_pbkdf2 "

GRUB_SIGNING_MODULES += "${@'pgp gcry_rsa gcry_sha256 gcry_sha512 --pubkey %s ' % d.getVar('GRUB_PUB_KEY') \
    if d.getVar('GRUB_SIGN_VERIFY') == '1' else ''}"

GRUB_SELOADER_MODULES += "${@'mok2verify ' if d.getVar('UEFI_SELOADER') == '1' else ''}"

GRUB_TPM_MODULES += "${@bb.utils.contains('COMBINED_FEATURES', 'tpm2', 'tpm', '', d)}"

GRUB_SECURE_BUILDIN ??= ""
GRUB_SECURE_BUILDIN:append:class-target = " \
    tftp reboot chain \
    ${GRUB_SECURE_BOOT_MODULES} \
    ${GRUB_SIGNING_MODULES} \
    ${GRUB_SELOADER_MODULES} \
    ${GRUB_TPM_MODULES} \
    --sbat ${WORKDIR}/sbat.csv \
    ${@'--disable-shim-lock' if d.getVar('UEFI_SELOADER') == '1' or d.getVar('MOK_SB') != "1" else ''} \
"

# Set a default root specifier.
inherit user-key-store

python __anonymous () {
    if d.getVar('GRUB_SIGN_VERIFY') == '1' and d.getVar('MOK_SB') != "1":
        return

    # Override the default filename if efi-secure-boot enabled.
    # grub-efi must be renamed as grub${arch}.efi for working with shim
    # or SELoader.
    import re

    target = d.getVar('TARGET_ARCH')
    if target == "x86_64":
        grubimage = "grubx64.efi"
    elif re.match('i.86', target):
        grubimage = "grubia32.efi"
    else:
        raise bb.parse.SkipPackage("grub-efi is incompatible with target %s" % target)

    d.setVar("GRUB_IMAGE", grubimage)
}

do_configure:prepend:class-target() {
    sed -i -e 's,@VERSION@,${PV},g' ${WORKDIR}/sbat.csv
}

do_compile:append:class-target() {
    if [ "${GRUB_SIGN_VERIFY}" = "1" -a "${GRUB_SIGN_VERIFY_STRICT}" = "1" ] ; then
        cat<<EOF>${WORKDIR}/cfg
set strict_security=1
EOF
    else
        > ${WORKDIR}/cfg
    fi
    cat<<EOF>>${WORKDIR}/cfg
search.file (\$cmdpath)${GRUB_PREFIX_DIR}/grub.cfg root
set prefix=(\$root)${GRUB_PREFIX_DIR}
EOF
}

do_compile:append:class-native() {
    make grub-editenv
}

do_install:append:class-native() {
    install -m 0755 grub-editenv "${D}${bindir}"
}

do_install:append:class-target() {
    # Create the initial environment block with empty item.
    grub-editenv "${D}${EFI_FILES_PATH}/grubenv" create

    install -d "${D}${EFI_FILES_PATH}/${GRUB_TARGET}-efi"
    grub-mkimage -c ../cfg -p "${GRUB_PREFIX_DIR}" -d "./grub-core" \
        -O "${GRUB_TARGET}-efi" -o "${B}/${GRUB_IMAGE}" \
        ${GRUB_BUILDIN} ${GRUB_SECURE_BUILDIN}

    install -m 0644 "${B}/${GRUB_IMAGE}" "${D}${EFI_FILES_PATH}/${GRUB_IMAGE}"

    # Install the modules to grub-efi's search path
    oe_runmake -C grub-core install DESTDIR="${D}${EFI_FILES_PATH}" pkglibdir=""

    # Remove build host references
    find "${D}${EFI_FILES_PATH}" -name modinfo.sh -type f -exec \
        sed -i \
        -e 's,--sysroot=${STAGING_DIR_TARGET},,g' \
        -e 's|${DEBUG_PREFIX_MAP}||g' \
        -e 's:${RECIPE_SYSROOT_NATIVE}::g' \
        {} +

    # Remove .module
    rm -f ${D}${EFI_FILES_PATH}/${GRUB_TARGET}-efi/*.module
}

python do_sign:prepend:class-target() {
    bb.build.exec_func("check_deploy_keys", d)
    if d.getVar('GRUB_SIGN_VERIFY') == '1':
        bb.build.exec_func("check_boot_public_key", d)
}

fakeroot python do_sign:class-target() {
    image_dir = d.getVar('D')
    efi_files_path = d.getVar('EFI_FILES_PATH')
    grub_image = d.getVar('GRUB_IMAGE')
    dir = image_dir + efi_files_path + '/'

    sb_sign(dir + grub_image, dir + grub_image, d)
}

python do_sign() {
}

addtask sign after do_install before do_deploy do_package

# Append the do_deploy() in oe-core.
do_deploy:append:class-target() {
    install -m 0644 "${D}${EFI_FILES_PATH}/${GRUB_IMAGE}" "${DEPLOYDIR}"

    # Deploy the stacked grub configs.
    install -m 0600 "${D}${EFI_FILES_PATH}/grubenv" "${DEPLOYDIR}"
 
    install -d "${DEPLOYDIR}/efi-unsigned"
    install -m 0644 "${B}/${GRUB_IMAGE}" "${DEPLOYDIR}/efi-unsigned"
    PSEUDO_DISABLED=1 cp -af "${D}${EFI_FILES_PATH}/${GRUB_TARGET}-efi" "${DEPLOYDIR}/efi-unsigned"
}

FILES:${PN} += "${EFI_FILES_PATH}"

CONFFILES:${PN} += "\
    ${EFI_FILES_PATH}/grubenv \
"
