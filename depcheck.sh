#!/bin/bash

red="\e[31m"
green="\e[32m"
yellow="\e[33m"
reset="\e[0m"

code=0

function check_cmd {
    printf "Checking wether $1 is installed... "
    if type $1 1>/dev/zero 2>/dev/zero; then
        printf "${green}yes${reset}\n"
    else
        printf "${red}no${reset}\n"
        printf "${red}$2${reset}\n"
        code=1
    fi
}

function check_cmd_warn {
    printf "Checking wether $1 is installed... "
    if type $1 1>/dev/zero 2>/dev/zero; then
        printf "${green}yes${reset}\n"
    else
        printf "${yellow}no${reset}\n"
        printf "${yellow}$2${reset}\n"
    fi
}

check_cmd ${PE_GCC} "SimpleOS2 requires any 64-Bit mingw gcc to be installed. Set the PE_GCC variable accordingly in the Makefile"
check_cmd ${ELF_GCC} "SimpleOS2 requires any 64-Bit gcc to be installed. Set the ELF_GCC variable accordingly in the Makefile"
check_cmd ${NASM} "SimpleOS2 requires nasm to be installed"

check_cmd ${MTOOLS_MMD} "SimpleOS2 requires mtools to be installed"
check_cmd ${MTOOLS_MCOPY} "SimpleOS2 requires mtools to be installed"

check_cmd ${MKFS_FAT} "SimpleOS2 requires mkfs.fat to be installed"

check_cmd ${DEBUGFS} "SimpleOS2 requires debugfs in order to create its ramdisk"

check_cmd ${PARTED} "SimpleOS2 requires parted to create its disk image"

check_cmd_warn ${QEMU_X64} "SimpleOS2 requires any x64 qemu if you want to run it in qemu"

check_cmd_warn ${VBOX_MANAGE} "SimpleOS2 requires VBoxManage if you want to create a virtualbox disk image"

if [ $code = 1 ]; then
    printf "${red}Not all dependencies are fullfilled${reset}\n"
else
    printf "${green}All dependencies required by SimpleOS2 are installed${reset}\n"
fi

exit $code