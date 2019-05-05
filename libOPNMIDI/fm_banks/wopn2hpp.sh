#!/bin/bash

output="../utils/vlc_codec/gm_opn_bank.h"

out()
{
    printf "${1}" >> $output
}

truncate -s 0 $output

out "/*===============================================================*\n"
out "   This file is automatically generated by wopn2hpp.sh script\n"
out "   PLEASE DON'T EDIT THIS DIRECTLY. Edit the gm.wopn file first,\n"
out "   and then run a wopn2hpp.sh script to generate this file again\n"
out " *===============================================================*/\n\n"
out "static unsigned char g_gm_opn2_bank[] = \n"
out "{\n"
hexdump -ve '12/1 "0x%02x, " "\n"' xg.wopn >> $output
out "0x00\n"
out "};\n"
out "\n"

sed -i "s/0x  /0x00/g" $output

