#!/bin/bash

red="\e[31m"
green="\e[32m"
reset="\e[0m"

function check_cmd {
    printf "Checking wether $2 is installed... "
    if type $1 1>/dev/zero 2>/dev/zero; then
        echo -e "${green}yes${reset}"
    else
        echo -e "${red}no${reset}"
        echo -e "${red}SimpleOS2 requires $2 to be installed${reset}"
        exit 1
    fi
}

check_cmd ${PE_GCC} "mingw"
check_cmd ${ELF_GCC} "gcc"
check_cmd ${NASM} "nasm"
check_cmd mcopy "mtools"

exit