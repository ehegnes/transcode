/*
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
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "subtitler.h"


int hash(s)/* form hash value for string s */
char *s;
{
int hashval;

//for(hashval = 0; *s != '\0';) hashval += *s++;
/* sum of ascii value of characters divided by tablesize */

/* vector into structure list, a row for each frame number */
hashval = atoi(s);
return(hashval % FRAME_HASH_SIZE);
}


char *strsave(char *s) /*save char array s somewhere*/
{
char *p, *malloc();
if(p = malloc( strlen(s) +  1) ) strcpy(p, s);
return(p);
}


struct frame *lookup_frame(char *name)
{
struct frame *pa;

for(pa = frametab[hash(name)]; pa != 0; pa = pa -> nxtentr)
	{
	if(strcmp(pa -> name, name) == 0) return(pa);
	}

return 0; /* not found */
}/* end function lookup_frame */


struct frame *install_frame(char *name)
{
struct frame *pa, *pnew, *pnext, *lookupframe();
int hashval;

if(debug_flag)
	{
	fprintf(stdout, "installframe(): arg name=%s\n", name);
	}
	
/* allow multiple entries with the same name */
//pa = lookup_frame(name);
pa = 0;

if(!pa) /* not found */
	{
	/* create new structure */
	pnew = (struct frame *) calloc(1, sizeof(*pnew) );
	if(! pnew) return 0;
	pnew -> name = strsave(name);
	if(! pnew -> name) return 0;

	/* get next structure */
	hashval = hash(name);
	pnext = frametab[hashval];/* may be zero, if there was nothing */

	/* insert before next structure (if any, else at start) */
	frametab[hashval] = pnew;

	/* set pointers for next structure */
	if(pnext) pnext -> prventr = pnew;
	
	/* set pointers for new structure */
	pnew -> nxtentr = pnext;
	pnew -> prventr = 0;/* always inserting at start of chain of structures */
	
	return pnew;/* pointer to new structure */
	}/* end if not found */

return pa;
}/* end function install_frame */


int delete_all_frames()
{
struct frame *pa;
int i;

for(i = 0; i < FRAME_HASH_SIZE; i++)/* for all structures at this position */
	{
	while(1)
		{
		pa = frametab[i];
		if(! pa) break;
		frametab[i] = pa -> nxtentr;
					/* frametab entry points to next one,
					this could be 0
					*/
		free(pa -> name);/* free name */
		free(pa -> data);
		free(pa);/* free structure */
		}/* end while all structures hashing to this value */ 
	}/* end for all entries in frametab */

return(0);/* not found */
}/* end function delete_all_frames */


int add_frame(\
	char *name, char *data, int object_type,\
	int xsize, int ysize, int zsize, int id)
{
struct frame *pa;
char *ptr;

if(debug_flag)
	{
	fprintf(stdout,\
	"add_frame(): arg name=%s\n\
	data=%lu\n\
	object_type=%d\n\
	xsize=%d ysize=%d zsize=%d\n\
	id=%d\n",\
	name,\
	data,\
	object_type,\
	xsize, ysize, zsize,\
	id);
	}

/* argument check */
if(! name) return 0;
if(! data) return 0;

pa = install_frame(name);
if(! pa) return(0);

pa -> data = strsave(data);
if(! pa -> data) return(0);

pa -> type = object_type;

pa -> xsize = xsize;
pa -> ysize = ysize;
pa -> zsize = zsize;

pa -> id = id;

pa -> pfd = vo_font;

pa -> status = NEW_ENTRY;

return 1;
}/* end function add_frame */


int process_frame_number(int frame_nr)
{
struct frame *pa;
char temp[80];

if(debug_flag)
	{
	printf("subtitler(): process_frame_number(): arg frame_nr=%d\n",\
	frame_nr); 
	}

sprintf(temp, "%d", frame_nr);
for(pa = frametab[hash(temp)]; pa != 0; pa = pa -> nxtentr)
	{
	if(strcmp(pa -> name, temp) == 0)
		{
		/* parse data here */
		parse_frame_entry(pa);
		} /* end if frame number matches */
	} /* end for all entries that hash to this frame number */

return 1;
} /* end function process_frame_number */


int set_end_frame(int frame_nr, int end_frame)
{
struct frame *pa;
char temp[80];

if(debug_flag)
	{
	printf("set_end_frame(): frame_nr=%d end_frame=%d\n",\
	frame_nr, end_frame);
	}

sprintf(temp, "%d", frame_nr);
for(pa = frametab[hash(temp)]; pa != 0; pa = pa -> nxtentr)
	{
//printf("WAS pa->type=%d pa->name=%s frame_nr=%d end_frame=%d\n",\
//pa -> type, pa -> name, frame_nr, end_frame);

	if(pa -> type == FORMATTED_TEXT)
		{
		if(atoi(pa -> name) == frame_nr)
			{
			pa -> end_frame = end_frame;
			
			return 1;
			}
		} /* end if type FORMATTED_TEXT */
	} /* end for all entries that hash to this frame number */

/* not found */
return 0;
} /* end function set_end_frame */

