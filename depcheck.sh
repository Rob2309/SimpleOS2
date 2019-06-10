#!/bin/bash

red="\e[31m"
green="\e[32m"
reset="\e[0m"

code=0

function check_cmd {
    printf "Checking wether $1 is installed... "
    if type $1 1>/dev/zero 2>/dev/zero; then
        echo -e "${green}yes${reset}"
    else
        echo -e "${red}no${reset}"
        echo -e "${red}$2${reset}"
        code=1
    fi
}

check_cmd ${PE_GCC} "SimpleOS2 requires any 64-Bit mingw gcc to be installed. Set the PE_GCC variable accordingly in the Makefile"
check_cmd ${ELF_GCC} "SimpleOS2 requires any 64-Bit gcc to be installed. Set the ELF_GCC variable accordingly in the Makefile"
check_cmd ${NASM} "SimpleOS2 requires nasm to be installed"

check_cmd mmd "SimpleOS2 requires mtools to be installed"
check_cmd mcopy "SimpleOS2 requires mtools to be installed"

check_cmd debugfs "SimpleOS2 requires debugfs in order to create its ramdisk"

if [ $code = 1 ]; then
    echo -e "${red}Not all dependencies are fullfilled${reset}"
else
    echo -e "${green}All dependencies required by SimpleOS2 are installed${reset}"
fi

exit $code