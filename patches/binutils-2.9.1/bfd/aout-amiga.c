/* BFD backend for Amiga style a.out with flags set to 0
   Copyright (C) 1990, 91, 92, 93, 1994 Free Software Foundation, Inc.
   Written by Stephan Thesing.

This file is part of BFD, the Binary File Descriptor library.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

#define TARGETNAME "a.out-amiga"
#define MY(OP) CAT(aout_amiga_,OP)

#include "bfd.h"

#define MY_exec_hdr_flags 0

extern boolean
amiga_final_link(bfd *,struct bfd_link_info *);
#define MY_bfd_final_link amiga_final_link

extern bfd_byte *
get_relocated_section_contents(bfd *, struct bfd_link_info *,
			       struct bfd_link_order *, bfd_byte *,
			       boolean , asymbol **);
#define MY_bfd_get_relocated_section_contents get_relocated_section_contents

/* Include the usual a.out support.  */
#include "aoutf1.h"

/* Final link routine.  We need to use a call back to get the correct
   offsets in the output file.  */

boolean
amiga_aout_bfd_final_link (abfd, info)
     bfd *abfd;
     struct bfd_link_info *info;
{
  return NAME(aout,final_link) (abfd, info, MY_final_link_callback);
}
