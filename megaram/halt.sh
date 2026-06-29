#!/bin/bash
ST_OCD="${HOME}/bin/openocd"
ST_SCRIPTS="${HOME}/share/openocd/scripts"
OCD_CFG="openocd/stm32h5e5zj.cfg"

"${ST_OCD}" -s "${ST_SCRIPTS}" -f "${OCD_CFG}" \
    -c "init; targets; reset init; wait_halt; poll; shutdown"
