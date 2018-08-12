/******************************************************************************
 *
 * Project:  OpenCPN
 * Purpose:  Climatology Plugin
 * Author:   Sean D'Epagnier
 *
 ***************************************************************************
 *   Copyright (C) 2016 by Sean D'Epagnier                                 *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,  USA.             *
 ***************************************************************************
 */

/* This program takes the slp data from:

   http://www.jisao.washington.edu/data/coads_climatologies/slpcoadsclim5079.nc
   to produce a much condensed binary file to be compressed
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>

#include <string.h>

#include <netcdfcpp.h>

const char* slppath = "slpcoadsclim5079.nc";

int main(int argc, char *argv[])
{
    NcFile slp(slppath, NcFile::ReadOnly);
    if(!slp.is_valid() || slp.num_dims() != 3 || slp.num_vars() != 4) {
        fprintf(stderr, "failed reading file: %s\n", slppath);
        return 1;
    }

    NcVar* data = slp.get_var("data");
    if(!data->is_valid() || data->num_dims() != 3) {
        fprintf(stderr, "slp has incorrect dimensions");
        return 1;
    }

    int16_t slpd[12][90][180];
    data->get(&slpd[0][0][0], 12, 90, 180);
    fwrite(slpd, sizeof slpd, 1, stdout);
    return 0;
}
