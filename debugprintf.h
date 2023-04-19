/*
 * dlnatunnel
 * Copyright (C) 2023 Stefan Wildemann
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef __MESSENGER_H
#define __MESSENGER_H

#define HLINE "-------------------------------------------------------------------------------\n"


#include <stdio.h>
#include <string.h>

#define newlineprintf()        fprintf(stdout,"\n")

#ifdef DEBUG
#define debugprintf(x, y...) fprintf(  stderr,"DEBUG(%s, #%5u): " x "\n",(__FILE__),(__LINE__), ##y )
#else
#define debugprintf(x, y...) while(0)
#endif

#define errorprintf(x, y...) fprintf(  stderr,"ERROR(%s, #%5u): " x "\n",(__FILE__),(__LINE__), ##y )

#endif
