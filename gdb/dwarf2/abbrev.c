/* DWARF 2 abbreviations

   Copyright (C) 1994-2020 Free Software Foundation, Inc.

   Adapted by Gary Funck (gary@intrepid.com), Intrepid Technology,
   Inc.  with support from Florida State University (under contract
   with the Ada Joint Program Office), and Silicon Graphics, Inc.
   Initial contribution by Brent Benson, Harris Computer Systems, Inc.,
   based on Fred Fish's (Cygnus Support) implementation of DWARF 1
   support.

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#include "defs.h"
#include "dwarf2/read.h"
#include "dwarf2/abbrev.h"
#include "dwarf2/leb.h"
#include "bfd.h"

/* Abbreviation tables.

   In DWARF version 2, the description of the debugging information is
   stored in a separate .debug_abbrev section.  Before we read any
   dies from a section we read in all abbreviations and install them
   in a hash table.  */

/* Allocate space for a struct abbrev_info object in ABBREV_TABLE.  */

struct abbrev_info *
abbrev_table::alloc_abbrev ()
{
  struct abbrev_info *abbrev;

  abbrev = XOBNEW (&abbrev_obstack, struct abbrev_info);
  memset (abbrev, 0, sizeof (struct abbrev_info));

  return abbrev;
}

/* Add an abbreviation to the table.  */

void
abbrev_table::add_abbrev (unsigned int abbrev_number,
			  struct abbrev_info *abbrev)
{
  unsigned int hash_number;

  hash_number = abbrev_number % ABBREV_HASH_SIZE;
  abbrev->next = m_abbrevs[hash_number];
  m_abbrevs[hash_number] = abbrev;
}

/* Look up an abbrev in the table.
   Returns NULL if the abbrev is not found.  */

struct abbrev_info *
abbrev_table::lookup_abbrev (unsigned int abbrev_number)
{
  unsigned int hash_number;
  struct abbrev_info *abbrev;

  hash_number = abbrev_number % ABBREV_HASH_SIZE;
  abbrev = m_abbrevs[hash_number];

  while (abbrev)
    {
      if (abbrev->number == abbrev_number)
	return abbrev;
      abbrev = abbrev->next;
    }
  return NULL;
}

/* Read in an abbrev table.  */

abbrev_table_up
abbrev_table_read_table (struct objfile *objfile,
			 struct dwarf2_section_info *section,
			 sect_offset sect_off)
{
  bfd *abfd = section->get_bfd_owner ();
  const gdb_byte *abbrev_ptr;
  struct abbrev_info *cur_abbrev;
  unsigned int abbrev_number, bytes_read, abbrev_name;
  unsigned int abbrev_form;
  std::vector<struct attr_abbrev> cur_attrs;

  abbrev_table_up abbrev_table (new struct abbrev_table (sect_off));

  section->read (objfile);
  abbrev_ptr = section->buffer + to_underlying (sect_off);
  abbrev_number = read_unsigned_leb128 (abfd, abbrev_ptr, &bytes_read);
  abbrev_ptr += bytes_read;

  /* Loop until we reach an abbrev number of 0.  */
  while (abbrev_number)
    {
      cur_attrs.clear ();
      cur_abbrev = abbrev_table->alloc_abbrev ();

      /* read in abbrev header */
      cur_abbrev->number = abbrev_number;
      cur_abbrev->tag
	= (enum dwarf_tag) read_unsigned_leb128 (abfd, abbrev_ptr, &bytes_read);
      abbrev_ptr += bytes_read;
      cur_abbrev->has_children = read_1_byte (abfd, abbrev_ptr);
      abbrev_ptr += 1;

      /* now read in declarations */
      for (;;)
	{
	  LONGEST implicit_const;

	  abbrev_name = read_unsigned_leb128 (abfd, abbrev_ptr, &bytes_read);
	  abbrev_ptr += bytes_read;
	  abbrev_form = read_unsigned_leb128 (abfd, abbrev_ptr, &bytes_read);
	  abbrev_ptr += bytes_read;
	  if (abbrev_form == DW_FORM_implicit_const)
	    {
	      implicit_const = read_signed_leb128 (abfd, abbrev_ptr,
						   &bytes_read);
	      abbrev_ptr += bytes_read;
	    }
	  else
	    {
	      /* Initialize it due to a false compiler warning.  */
	      implicit_const = -1;
	    }

	  if (abbrev_name == 0)
	    break;

	  cur_attrs.emplace_back ();
	  struct attr_abbrev &cur_attr = cur_attrs.back ();
	  cur_attr.name = (enum dwarf_attribute) abbrev_name;
	  cur_attr.form = (enum dwarf_form) abbrev_form;
	  cur_attr.implicit_const = implicit_const;
	  ++cur_abbrev->num_attrs;
	}

      cur_abbrev->attrs =
	XOBNEWVEC (&abbrev_table->abbrev_obstack, struct attr_abbrev,
		   cur_abbrev->num_attrs);
      memcpy (cur_abbrev->attrs, cur_attrs.data (),
	      cur_abbrev->num_attrs * sizeof (struct attr_abbrev));

      abbrev_table->add_abbrev (abbrev_number, cur_abbrev);

      /* Get next abbreviation.
         Under Irix6 the abbreviations for a compilation unit are not
         always properly terminated with an abbrev number of 0.
         Exit loop if we encounter an abbreviation which we have
         already read (which means we are about to read the abbreviations
         for the next compile unit) or if the end of the abbreviation
         table is reached.  */
      if ((unsigned int) (abbrev_ptr - section->buffer) >= section->size)
	break;
      abbrev_number = read_unsigned_leb128 (abfd, abbrev_ptr, &bytes_read);
      abbrev_ptr += bytes_read;
      if (abbrev_table->lookup_abbrev (abbrev_number) != NULL)
	break;
    }

  return abbrev_table;
}
