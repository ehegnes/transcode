/*
 * configs.c
 * a more generic approach to config files.
 * Copyright 12/10/01 Chris C. Hoover
 */

/*
         1         2         3         4         5         6         7         8         9         10        11        12        
12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678

*/

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "configs.h"

#ifdef DEBUG
#include "memtrack.h"
#endif

/*
 * get the section from the root.
 */
CF_SECTION_TYPE *
cf_get_section( CF_ROOT_TYPE * pRoot )
{
	/*
	 * root must exist.
	 */
	if( pRoot == NULL )
		return NULL;
	/*
	 * return the section.
	 * if pRoot->section is NULL then
	 * who cares? return NULL.
	 */
	return pRoot->section;
}

/*
 * get the next section.
 */
CF_SECTION_TYPE *
cf_get_next_section( CF_ROOT_TYPE * pRoot, CF_SECTION_TYPE * pSection )
{
	CF_SECTION_TYPE * next;

	/*
	 * root must exist.
	 */
	if( pRoot == NULL )
		return NULL;
	/*
	 * get a pointer to the section.
	 */
	next = pRoot->section;

	while( next != NULL ) {
		/*
		 * this the section?
		 */
		if( next == pSection )
			return next->next;
		/*
		 * no? then get the next section.
		 */
		next = next->next;
	}
	/*
	 * no can find.
	 */
	return NULL;
}

/*
 * get the named section.
 */
CF_SECTION_TYPE *
cf_get_named_section( CF_ROOT_TYPE * pRoot, char * pName )
{
	CF_SECTION_TYPE * next;

	/*
	 * root and name must exist.
	 */
	if( pRoot == NULL || pName == NULL )
		return NULL;
	/*
	 * get a pointer to the section.
	 */
	next = pRoot->section;

	while( next != NULL ) {
		/*
		 * section name must exist.
		 */
		if( next->name != NULL )
			/*
			 * this the named section?
			 */
			if( strcmp( next->name, pName ) == 0 )
				return next;
		/*
		 * no? then get the next section.
		 */
		next = next->next;
	}
	/*
	 * no can find.
	 */
	return NULL;
}

/*
 * get the key-value pair of the named section.
 * this routine only returns the first key-value
 * pair in the named section.
 */
CF_KEYVALUE_TYPE *
cf_get_named_section_keyvalue( CF_ROOT_TYPE * pRoot, char * pName )
{
	CF_SECTION_TYPE * pSection;

	/*
	 * root and name must exist.
	 */
	if( pRoot == NULL || pName == NULL )
		return NULL;
	/*
	 * find the named section.
	 */
	if( ( pSection = cf_get_named_section( pRoot, pName ) ) != NULL )
		/*
		 * return its' key-value pair.
		 */
		return pSection->keyvalue;
	/*
	 * no can find.
	 */
	return NULL;
}

/*
 * get the next key-value pair of the named section,
 * given the previous key-value pair.
 */
CF_KEYVALUE_TYPE *
cf_get_named_section_next_keyvalue( CF_ROOT_TYPE * pRoot, char * pName, CF_KEYVALUE_TYPE * pKeyvalue )
{
	CF_KEYVALUE_TYPE * pNext;

	/*
	 * root and name must exist.
	 */
	if( pRoot == NULL || pName == NULL )
		return NULL;
	/*
	 * find the named sections first key-value pair.
	 */
	if( ( pNext = cf_get_named_section_keyvalue( pRoot, pName ) ) != NULL ) {
		/*
		 * find the specific key-value pair in the section.
		 */
		while( pNext != NULL && pNext != pKeyvalue )
			pNext = pNext->next;

		if( pNext != NULL )
			/*
			 * return its' next pointer.
			 */
			return pNext->next;
	}
	/*
	 * no can find.
	 */
	return NULL;
}

/*
 * get the key of the named section.
 * this routine only returns the first key in the named section.
 * this is what you want if you only use keyless values or only
 * one key-value pair per section.
 */
char *
cf_get_named_section_key( CF_ROOT_TYPE * pRoot, char * pName )
{
	CF_KEYVALUE_TYPE * pKeyvalue;

	/*
	 * root and name must exist.
	 */
	if( pRoot == NULL || pName == NULL )
		return NULL;
	/*
	 * find the named section.
	 */
	if( ( pKeyvalue = cf_get_named_section_keyvalue( pRoot, pName ) ) != NULL )
		/*
		 * return its' key.
		 */
		return cf_skip_frontwhite( cf_skip_backwhite( pKeyvalue->key ) );
	/*
	 * no can find.
	 */
	return NULL;
}

/*
 * get the value of the named section.
 * this routine only returns the first value in the named section.
 * this is what you want if you only use keyless values or only
 * one key-value pair per section.
 */
char *
cf_get_named_section_value( CF_ROOT_TYPE * pRoot, char * pName )
{
	CF_KEYVALUE_TYPE * pKeyvalue;

	/*
	 * root and name must exist.
	 */
	if( pRoot == NULL || pName == NULL )
		return NULL;
	/*
	 * find the named section.
	 */
	if( ( pKeyvalue = cf_get_named_section_keyvalue( pRoot, pName ) ) != NULL )
		/*
		 * return its' value.
		 */
		return cf_skip_frontwhite( cf_skip_backwhite( pKeyvalue->value ) );
	/*
	 * no can find.
	 */
	return NULL;
}

/*
 * get the value of the named key from the named section.
 */
char *
cf_get_named_section_value_of_key( CF_ROOT_TYPE * pRoot, char * pName, char * pKey )
{
	CF_KEYVALUE_TYPE * pKeyvalue;

	/*
	 * root and name must exist.
	 */
	if( pRoot == NULL || pName == NULL )
		return NULL;
	/*
	 * find the named section
	 * and key-value pair.
	 */
	if( ( pKeyvalue = cf_get_named_section_keyvalue( pRoot, pName ) ) != NULL ) {
		while( pKeyvalue != NULL && strcmp( pKeyvalue->key, pKey ) != 0 )
			pKeyvalue = pKeyvalue->next;

		if( pKeyvalue != NULL )
			return cf_skip_frontwhite( cf_skip_backwhite( pKeyvalue->value ) );
	}
	/*
	 * no can find.
	 */
	return NULL;
}

/*
 * put the value of the named key
 * into the named section.
 *
 * an optimization here might be to
 * just replace the old value with
 * the new one if it will fit. this
 * would prevent fragging memory to
 * some extent but it looks an awfull
 * lot like work to me so maybe later.
 */
int
cf_put_named_section_value_of_key( CF_ROOT_TYPE * pRoot, char * pName, char * pKey, 
	char * pVal, CF_VALUE_TYPE vtype, char * pComment )
{
	CF_KEYVALUE_TYPE * pKeyvalue, * pNew;
	CF_SECTION_TYPE * pSection;

	/*
	 * root and name must exist.
	 */
	if( pRoot == NULL || pName == NULL )
		return -1;
	/*
	 * make sure we can instantiate a 
	 * new key-value pair.
	 */
	if( ( pNew = CF_NEW_KEYVALUE( pKey, pVal, vtype, pComment ) ) == NULL )
		return -1;
	/*
	 * find the named section.
	 */
	if( ( pSection = cf_get_named_section( pRoot, pName ) ) == NULL )
		return -1;
	/*
	 * find the named key-value pair.
	 */
	if( ( pKeyvalue = cf_get_named_section_keyvalue( pRoot, pName ) ) != NULL ) {
		while( pKeyvalue != NULL && strcmp( pKeyvalue->key, pKey ) != 0 )
			pKeyvalue = pKeyvalue->next;

		if( pKeyvalue != NULL ) {
			/*
			 * remove the old key-value pair from the section.
			 */
			if( CF_REMSEC_KEYVALUE( pKeyvalue, pSection ) < 0 )
				return -1;
			/*
			 * add the new key-value pair to the section.
			 */
			if( CF_ADDSEC_KEYVALUE( pNew, pSection ) < 0 )
				return -1;
			/*
			 * move the comments from the old
			 * key-value pair to the new one.
			 */
			pNew->comment = pKeyvalue->comment;

			pKeyvalue->comment = NULL;
			/*
			 * make sure we only free the old key-value pair.
			 */
			pKeyvalue->next = NULL;
			/*
			 * free the old key-value pair.
			 */
			CF_FREE_KEYVALUE( pKeyvalue );

			return 0;
		}
	}
	/*
	 * no can find.
	 */
	return -1;
}

/*
 * get the named subsection.
 */
CF_SUBSECTION_TYPE *
cf_get_named_subsection( CF_ROOT_TYPE * pRoot, char * pName )
{
	CF_SUBSECTION_TYPE * pSsection;
	CF_SECTION_TYPE * pNextSection;

	/*
	 * root and name must exist.
	 */
	if( pRoot == NULL || pName == NULL )
		return NULL;
	/*
	 * get a pointer to the section.
	 */
	pNextSection = pRoot->section;

	while( pNextSection != NULL ) {
		/*
		 * get a pointer to the subsection.
		 */
		pSsection = pNextSection->subsection;

		while( pSsection != NULL ) {
			/*
			 * subsection name must exist.
			 */
			if( pSsection->name != NULL )
				/*
				 * this the named section?
				 */
				if( strcmp( pSsection->name, pName ) == 0 )
					return pSsection;
			/*
			 * no? then get the next subsection.
			 */
			pSsection = pSsection->next;
		}
		/*
		 * no? then get the next section.
		 */
		pNextSection = pNextSection->next;
	}
	/*
	 * no can find.
	 */
	return NULL;
}

/*
 * get the key-value pair of the named subsection.
 * this routine only returns the first key-value
 * pair in the named subsection.
 */
CF_KEYVALUE_TYPE *
cf_get_named_subsection_keyvalue( CF_ROOT_TYPE * pRoot, char * pName )
{
	CF_SUBSECTION_TYPE * pSubsection;

	/*
	 * root and name must exist.
	 */
	if( pRoot == NULL || pName == NULL )
		return NULL;
	/*
	 * find the named subsection.
	 */
	if( ( pSubsection = cf_get_named_subsection( pRoot, pName ) ) != NULL )
		/*
		 * return its' key-value pair.
		 */
		return pSubsection->keyvalue;
	/*
	 * no can find.
	 */
	return NULL;
}

/*
 * get the next key-value pair of the named subsection,
 * given the previous keyvalue.
 */
CF_KEYVALUE_TYPE *
cf_get_named_subsection_next_keyvalue( CF_ROOT_TYPE * pRoot, char * pName, CF_KEYVALUE_TYPE * pKeyvalue )
{
	CF_KEYVALUE_TYPE * pNext;

	/*
	 * root and name must exist.
	 */
	if( pRoot == NULL || pName == NULL )
		return NULL;
	/*
	 * find the named subsections first key-value pair.
	 */
	if( ( pNext = cf_get_named_subsection_keyvalue( pRoot, pName ) ) != NULL ) {
		/*
		 * find the specific key-value pair in the subsection.
		 */
		while( pNext != NULL && pNext != pKeyvalue )
			pNext = pNext->next;

		if( pNext != NULL )
			/*
			 * return its' next pointer.
			 */
			return pNext->next;
	}
	/*
	 * no can find.
	 */
	return NULL;
}

/*
 * write root to file.
 */
int
cf_write( CF_ROOT_TYPE * pRoot, char * filename )
{
	FILE * fp;

	/*
	 * root must exist.
	 */
	if( pRoot == NULL )
		return -1;
	/*
	 * check for a NULL filename.
	 */
	if( filename == NULL )
		fp = stdout;

	else {
		/*
		 * open the file for writing.
		 */
		if( ( fp = cf_wopen( filename ) ) == NULL )
			return -1;
	}
	/*
	 * send the root to fp.
	 */
	if( CF_PRINT_ROOT( fp, pRoot ) < 0 ) {
		fclose( fp );
		return -1;
	}
	/*
	 * and remember to close the output file
	 * but don't close the standard output.
	 */
	if( fp != stdout )
		fclose( fp );

	return 0;
}

/*
 * read the root.
 */
CF_ROOT_TYPE *
cf_read( char * filename )
{
	FILE * fp;
	char * pString = NULL, * pKey, * pVal, * pCmt;

	CF_ROOT_TYPE       * pRoot           = NULL;
	CF_SECTION_TYPE    * pSection        = NULL;
	CF_SUBSECTION_TYPE * pSsection       = NULL;
	CF_KEYVALUE_TYPE   * pKeyvalue       = NULL;
	CF_KEYVALUE_TYPE   * pParentKeyvalue = NULL;
	CF_COMMENT_TYPE    * pComment        = NULL;
	CF_COMMENT_TYPE    * pParentComment  = NULL;

	CF_VALUE_TYPE vtype = CF_CHR;
	CF_STATE_TYPE stype = CF_EMPTY;

	/*
	 * why must i do this??
	 */
	pRoot = NULL;
	/*
	 * check for a NULL filename.
	 */
	if( filename == NULL )
		fp = stdin;

	else {
		/*
		 * open the file for reading.
		 */
		if( ( fp = cf_ropen( filename ) ) == NULL )
			return NULL;
	}
	/*
	 * allocate a ROOT struct.
	 */
	if( ( pRoot = CF_NEW_ROOT( cf_skip_backwhite( filename ), NULL, NULL) ) == NULL ) {
		fclose( fp );
		FREE( pString );
		return NULL;
	}
	/*
	 * set the state to ROOT.
	 */
	stype = CF_ROOT;
	/*
	 * read the file, one line at a time.
	 * cf_readline skips whitespace at the beginning of the line
	 * and zaps the newline at the end.
	 */
	while( ( pString = cf_readline( fp ) ) != NULL ) {
		/*
		 * the first charater on the line tells us
		 * what type of line we've got.
		 */
		switch( * pString ) {
		/*
		 * it's a comment.
		 */
		case '\0':
		case '#':
			/*
			 * this may be needed if we get two or more comments in a row.
			 */
			if( pComment != NULL )
				pParentComment = pComment;
			/*
			 * allocate a COMMENT struct.
			 */
			if( ( pComment = CF_NEW_COMMENT( cf_skip_backwhite( pString ) ) ) == NULL ) {
				fclose( fp );
				CF_FREE_ROOT( pRoot );
				return NULL;
			}
			/*
			 * store the comment depending on the current state.
			 * only the CF_ROOT, the CF_SECTIONS, the CF_SUBSECTIONS
			 * and the CF_KEYVALUE pairs can have comments.
			 */
			switch( stype ) {
			/*
			 * ROOT state.
			 */
			case CF_ROOT:
				/*
				 * root must exist.
				 */
				if( pRoot == NULL ) {
					fclose( fp );
					CF_FREE_ROOT( pRoot );
					return NULL;
				}
				/*
				 * first comment in the ROOT?
				 */
				if( pRoot->comment == NULL )
					pRoot->comment = pComment;
				/*
				 * no? then add a new comment to the list.
				 */
				else
					CF_ADDROOT_COMMENT( pComment, pRoot );

				break;
			/*
			 * CF_SECTION state.
			 */
			case CF_SECTION:
				/*
				 * section must exist.
				 */
				if( pSection == NULL ) {
					fclose( fp );
					CF_FREE_ROOT( pRoot );
					return NULL;
				}
				/*
				 * first comment in the SECTION?
				 */
				if( pSection->comment == NULL )
					pSection->comment = pComment;
				/*
				 * no? then add a new comment to the list.
				 */
				else
					CF_ADDSEC_COMMENT( pComment, pSection );

				break;
			/*
			 * SUBSECTION state.
			 */
			case CF_SUBSECTION:
				/*
				 * subsection must exist.
				 */
				if( pSsection == NULL ) {
					fclose( fp );
					CF_FREE_ROOT( pRoot );
					return NULL;
				}
				/*
				 * first comment in the SUBSECTION?
				 */
				if( pSsection->comment == NULL )
					pSsection->comment = pComment;
				/*
				 * no? then add a new comment to the list.
				 */
				else
					CF_ADDSUB_COMMENT( pComment, pSsection );

				break;
			/*
			 * KEYVALUE state.
			 */
			case CF_KEYVALUE:
				/*
				 * key-value pair must exist.
				 */
				if( pKeyvalue == NULL ) {
					fclose( fp );
					CF_FREE_ROOT( pRoot );
					return NULL;
				}
				/*
				 * first comment in the KEYVALUE?
				 */
				if( pKeyvalue->comment == NULL )
					pKeyvalue->comment = pComment;
				/*
				 * no? then add a new comment to the list.
				 */
				else
					CF_ADDKV_COMMENT( pComment, pKeyvalue );

				break;
			/*
			 * it's not the ROOT, a SECTION, a SUBSECTION or a KEYVALUE 
			 * so what is it? better bail out.
			 */
			default:
				fclose( fp );
				CF_FREE_COMMENT( pComment );
				CF_FREE_ROOT( pRoot );
				return NULL;
			}

			break;
		/*
		 * it's a section.
		 */
		case '[':
			/*
			 * change state to SECTION.
			 */
			stype = CF_SECTION;
			/*
			 * extract the section name from the line.
			 */
			pCmt = NULL;
			pString = cf_isolate( stype, pString, &pCmt );
			/*
			 * create a new section with the given name.
			 */
			if( ( pSection = CF_NEW_SECTION( cf_skip_backwhite( pString ), NULL, NULL, cf_skip_backwhite( pCmt ) ) ) == NULL ) {
				fclose( fp );
				CF_FREE_ROOT( pRoot );
				return NULL;
			}
			/*
			 * sections live in the root so it must exist.
			 */
			if( pRoot == NULL ) {
				fclose( fp );
				CF_FREE_ROOT( pRoot );
				return NULL;
			}
			/*
			 * first section in the root?
			 */
			if( pRoot->section == NULL )
				pRoot->section = pSection;
			/*
			 * no? then add a section to the list.
			 */
			else
				CF_ADD_SECTION( pSection, pRoot );

			break;
		/*
		 * it's a subsection.
		 */
		case '(':
			/*
			 * change state to SUBSECTION.
			 */
			stype = CF_SUBSECTION;
			/*
			 * extract the subsection name from the line.
			 */
			pCmt = NULL;
			pString = cf_isolate( stype, pString, &pCmt );
			/*
			 * create a new subsection with the given name.
			 */
			if( ( pSsection = CF_NEW_SUBSECTION( cf_skip_backwhite( pString ), NULL, cf_skip_backwhite( pCmt ) ) ) == NULL ) {
				fclose( fp );
				CF_FREE_ROOT( pRoot );
				return NULL;
			}
			/*
			 * subsections live in sections so a section must exist.
			 */
			if( pSection == NULL ) {
				fclose( fp );
				CF_FREE_ROOT( pRoot );
				return NULL;
			}
			/*
			 * first subsection in the section?
			 */
			if( pSection->subsection == NULL )
				pSection->subsection = pSsection;
			/*
			 * no? then add a subsection to the list.
			 */
			else
				CF_ADD_SUBSECTION( pSsection, pSection );

			break;
		/*
		 * must be a key-value pair.
		 */
		default:
			/*
			 * this may be needed if we get two or more key-value pairs in a row.
			 */
			if( pKeyvalue != NULL )
				pParentKeyvalue = pKeyvalue;
			/*
			 * extract the key and value from the line.
			 */
			pKey = pVal = pCmt = NULL;
			vtype = CF_NONE;
			if( ( pString = cf_split( pString, &pKey, &pVal, &vtype, &pCmt ) ) == NULL ) {
				fclose( fp );
				CF_FREE_ROOT( pRoot );
				return NULL;
			}
			/*
			 * create a new key-value pair with the given key and value.
			 */
			if( ( pKeyvalue = CF_NEW_KEYVALUE( cf_skip_backwhite( pKey ), cf_skip_backwhite( pVal ), vtype, cf_skip_backwhite( pCmt ) ) ) == NULL ) {
				fclose( fp );
				CF_FREE_ROOT( pRoot );
				return NULL;
			}
			/*
			 * add the key-value pair to the section or subsection or key-value pair.
			 */
			switch( stype ) {
			/*
			 * no key-value pairs for root.
			 */
			case CF_ROOT:
				fclose( fp );
				CF_FREE_KEYVALUE( pKeyvalue );
				CF_FREE_ROOT( pRoot );
				return NULL;
			/*
			 * add a key-value pair to the section.
			 */
			case CF_SECTION:
				/*
				 * the section must exist.
				 */
				if( pSection == NULL ) {
					fclose( fp );
					CF_FREE_ROOT( pRoot );
					return NULL;
				}
				/*
				 * first key-value pair in the section?
				 */
				if( pSection->keyvalue == NULL )
					pSection->keyvalue = pKeyvalue;
				/*
				 * no? then add the key-value pair to the list.
				 */
				else
					CF_ADDSEC_KEYVALUE( pKeyvalue, pSection );

				break;
			/*
			 * add a key-value pair to the subsection.
			 */
			case CF_SUBSECTION:
				/*
				 * the subsection must exist.
				 */
				if( pSsection == NULL ) {
					fclose( fp );
					CF_FREE_ROOT( pRoot );
					return NULL;
				}
				/*
				 * first key-value pair in the subsection?
				 */
				if( pSsection->keyvalue == NULL )
					pSsection->keyvalue = pKeyvalue;
				/*
				 * no? then add the key-value pair to the list.
				 */
				else
					CF_ADDSUB_KEYVALUE( pKeyvalue, pSsection );

				break;
			/*
			 * add a key-value pair to the list of key-value pairs.
			 */
			case CF_KEYVALUE:
				/*
				 * the parent key-value pair must exist.
				 */
				if( pParentKeyvalue == NULL ) {
					fclose( fp );
					CF_FREE_ROOT( pRoot );
					return NULL;
				}
				/*
				 * just add the key-value pair to the existing list.
				 */
				CF_ADDKV_KEYVALUE( pKeyvalue, pParentKeyvalue );

				break;
			/*
			 * don't know what to do so bail out.
			 */
			default:
				fclose( fp );
				CF_FREE_KEYVALUE( pKeyvalue );
				CF_FREE_ROOT( pRoot );
				return NULL;
			}
			/*
			 * set the state to KEYVALUE. this is a special case
			 * because the KEYVALUE must be attached to a SECTION
			 * or SUBSECTION before the state can change. once
			 * that is done then key-value pairs can just be
			 * added to the list if there are more of them.
			 */
			stype = CF_KEYVALUE;

			break;
		}
	}
	/*
	 * remember to close the file but
	 * don't close the standard input.
	 */
	if( fp != stdin )
		fclose( fp );
	/*
	 * brand new root.
	 */
	return pRoot;
}

/*
 * new root.
 */
CF_ROOT_TYPE *
CF_NEW_ROOT( char * name, CF_SECTION_TYPE * pSection, CF_COMMENT_TYPE * pComment )
{
	CF_ROOT_TYPE * pCfr;

	 if( ( pCfr = MALLOC( sizeof( CF_ROOT_TYPE ) ) ) != NULL ) {
		pCfr->name = NULL;
		pCfr->section = NULL;
		pCfr->comment = NULL;

		if( name != NULL ) {
			if( ( pCfr->name = MALLOC( strlen( name ) + 1 ) ) == NULL ) {
				CF_FREE_ROOT( pCfr );
				return NULL;
			}

			if( strncpy( pCfr->name, name, strlen( name ) + 1 ) != pCfr->name ) {
				CF_FREE_ROOT( pCfr );
				return NULL;
			}
		}

		if( pSection != NULL ) {
			if( pCfr->section == NULL )
				pCfr->section = pSection;

			else
				CF_ADD_SECTION( pSection, pCfr );
		}

		if( pComment != NULL ) {
			if( pCfr->comment == NULL )
				pCfr->comment = pComment;

			else
				CF_ADDROOT_COMMENT( pComment, pCfr );
		}
	}

	return pCfr;
}

/*
 * free root.
 */
void
CF_FREE_ROOT( CF_ROOT_TYPE * pCfr )
{
	if( pCfr != NULL ) {
		if( pCfr->name != NULL )
			FREE( pCfr->name );

		if( pCfr->section != NULL )
			CF_FREE_SECTION( pCfr->section );

		if( pCfr->comment != NULL )
			CF_FREE_COMMENT( pCfr->comment );

		FREE( pCfr );
	}
}

/*
 * print the root.
 */
int
CF_PRINT_ROOT( FILE * fp, CF_ROOT_TYPE * pRoot )
{
	if( pRoot == NULL )
		return -1;

	if( fp == NULL )
		fp = stdout;

	if( pRoot->comment != NULL )
		CF_PRINT_COMMENT( fp, pRoot->comment );

	if( pRoot->section != NULL )
		CF_PRINT_SECTION( fp, pRoot->section );

	return 0;
}

/*
 * new section.
 */
CF_SECTION_TYPE *
CF_NEW_SECTION( char * name, CF_KEYVALUE_TYPE * pKeyvalue, CF_SUBSECTION_TYPE * pSsection, char * pComment )
{
	CF_SECTION_TYPE * pCfs;

	if( ( pCfs = MALLOC( sizeof( CF_SECTION_TYPE ) ) ) != NULL ) {
		pCfs->name = NULL;
		pCfs->keyvalue = NULL;
		pCfs->subsection = NULL;
		pCfs->comment = NULL;
		pCfs->next = NULL;

		if( name != NULL ) {
			if( ( pCfs->name = MALLOC( strlen( name ) + 1 ) ) == NULL ) {
				CF_FREE_SECTION( pCfs );
				return NULL;
			}

			if( strncpy( pCfs->name, name, strlen( name ) + 1 ) != pCfs->name ) {
				CF_FREE_SECTION( pCfs );
				return NULL;
			}
		}

		if( pKeyvalue != NULL )
			pCfs->keyvalue = pKeyvalue;

		if( pSsection != NULL )
			pCfs->subsection = pSsection;

		if( pComment == NULL )
			pCfs->comment = NULL;
		/*
		 * stuff the comment.
		 */
		else {
			/*
			 * get a new comment struct.
			 */
			if( ( pCfs->comment = CF_NEW_COMMENT( cf_skip_backwhite( pComment ) ) ) == NULL )
				return NULL;
		}
	}

	return pCfs;
}

/*
 * free section.
 */
void
CF_FREE_SECTION( CF_SECTION_TYPE * pCfs )
{
	CF_SECTION_TYPE * temp = NULL;

	while( pCfs != NULL ) {
		temp = pCfs;

		if( pCfs->name != NULL )
			FREE( pCfs->name );

		if( pCfs->keyvalue != NULL )
			CF_FREE_KEYVALUE( pCfs->keyvalue );

		if( pCfs->subsection != NULL )
			CF_FREE_SUBSECTION( pCfs->subsection );

		if( pCfs->comment != NULL )
			CF_FREE_COMMENT( pCfs->comment );

		pCfs = pCfs->next;

		FREE( temp );
	}
}

/*
 * add section.
 */
int
CF_ADD_SECTION( CF_SECTION_TYPE * from, CF_ROOT_TYPE * to )
{
	CF_SECTION_TYPE * pSection;

	if( from == NULL || to == NULL )
		return -1;

	if( ( pSection = to->section ) == NULL )
		to->section = from;

	else {
		while( pSection->next != NULL )
			pSection = pSection->next;

		pSection->next = from;
	}

	return 0;
}

/*
 * remove section.
 */
int
CF_REM_SECTION( CF_SECTION_TYPE * rem, CF_ROOT_TYPE * from )
{
	CF_SECTION_TYPE * pSection;

	if( rem == NULL || from == NULL )
		return -1;

	if( ( pSection = from->section ) == NULL )
		return -1;

	if( pSection == rem )
		from->section = rem->next;

	else {
		while( pSection->next != NULL && pSection->next != rem )
			pSection = pSection->next;

		if( pSection->next == NULL )
			return -1;

		pSection->next = rem->next;
	}

	return 0;
}

/*
 * print the section.
 */
int
CF_PRINT_SECTION( FILE * fp, CF_SECTION_TYPE * pSection )
{
	if( pSection == NULL )
		return -1;

	if( fp == NULL )
		fp = stdout;

	if( pSection->name != NULL )
		fprintf( fp, "[%s]", pSection->name );

	if( pSection->comment != NULL ) {
		/*
		 * inline comment.
		 */
		if( * pSection->comment->comment == '#' )
			fprintf( fp, " " );

		else
			fprintf( fp, "\n" );

		CF_PRINT_COMMENT( fp, pSection->comment );
	}

	else
		fprintf( fp, "\n" );

	if( pSection->keyvalue != NULL )
		CF_PRINT_KEYVALUE( fp, pSection->keyvalue );

	if( pSection->subsection != NULL )
		CF_PRINT_SUBSECTION( fp, pSection->subsection );

	while( ( pSection = pSection->next ) != NULL ) {
		if( pSection->name != NULL )
			fprintf( fp, "[%s]", pSection->name );

		if( pSection->comment != NULL ) {
			/*
			 * inline comment.
			 */
			if( * pSection->comment->comment == '#' )
				fprintf( fp, " " );

			else
				fprintf( fp, "\n" );

			CF_PRINT_COMMENT( fp, pSection->comment );
		}

		else
			fprintf( fp, "\n" );

		if( pSection->keyvalue != NULL )
			CF_PRINT_KEYVALUE( fp, pSection->keyvalue );

		if( pSection->subsection != NULL )
			CF_PRINT_SUBSECTION( fp, pSection->subsection );
	}

	return 0;
}

/*
 * new subsection.
 */
CF_SUBSECTION_TYPE *
CF_NEW_SUBSECTION( char * name, CF_KEYVALUE_TYPE * pKeyvalue, char * pComment )
{
	CF_SUBSECTION_TYPE * pCfu;

	if( ( pCfu = MALLOC( sizeof( CF_SUBSECTION_TYPE ) ) ) != NULL ) {
		pCfu->name = NULL;
		pCfu->keyvalue = NULL;
		pCfu->comment = NULL;
		pCfu->next = NULL;

		if( name != NULL ) {
			if( ( pCfu->name = MALLOC( strlen( name ) + 1 ) ) == NULL ) {
				CF_FREE_SUBSECTION( pCfu );
				return NULL;
			}

			if( strncpy( pCfu->name, name, strlen( name ) + 1 ) != pCfu->name ) {
				CF_FREE_SUBSECTION( pCfu );
				return NULL;
			}
		}

		if( pKeyvalue != NULL )
			pCfu->keyvalue = pKeyvalue;

		if( pComment == NULL )
			pCfu->comment = NULL;
		/*
		 * stuff the comment.
		 */
		else {
			/*
			 * get a new comment struct.
			 */
			if( ( pCfu->comment = CF_NEW_COMMENT( cf_skip_backwhite( pComment ) ) ) == NULL )
				return NULL;
		}
	}

	return pCfu;
}

/*
 * free subsection.
 */
void
CF_FREE_SUBSECTION( CF_SUBSECTION_TYPE * pCfu )
{
	CF_SUBSECTION_TYPE * temp = NULL;

	while( pCfu != NULL ) {
		temp = pCfu;

		if( pCfu->name != NULL )
			FREE( pCfu->name );

		if( pCfu->keyvalue != NULL )
			CF_FREE_KEYVALUE( pCfu->keyvalue );

		if( pCfu->comment != NULL )
			CF_FREE_COMMENT( pCfu->comment );

		pCfu = pCfu->next;

		FREE( temp );
	}
}

/*
 * add subsection.
 */
int
CF_ADD_SUBSECTION( CF_SUBSECTION_TYPE * from, CF_SECTION_TYPE * to )
{
	CF_SUBSECTION_TYPE * pSsection;

	if( from == NULL || to == NULL )
		return -1;

	if( ( pSsection = to->subsection ) == NULL )
		to->subsection = from;

	else {
		while( pSsection->next != NULL )
			pSsection = pSsection->next;

		pSsection->next = from;
	}

	return 0;
}

/*
 * remove subsection.
 */
int
CF_REM_SUBSECTION( CF_SUBSECTION_TYPE * rem, CF_SECTION_TYPE * from )
{
	CF_SUBSECTION_TYPE * pSsection;

	if( rem == NULL || from == NULL )
		return -1;

	if( ( pSsection = from->subsection ) == NULL )
		return -1;

	if( pSsection == rem )
		from->subsection = rem->next;

	else {
		while( pSsection->next != NULL && pSsection->next != rem )
			pSsection = pSsection->next;

		if( pSsection->next == NULL )
			return -1;

		pSsection->next = rem->next;
	}

	return 0;
}

/*
 * print the subsection.
 */
int
CF_PRINT_SUBSECTION( FILE * fp, CF_SUBSECTION_TYPE * pSsection )
{
	if( pSsection == NULL )
		return -1;

	if( fp == NULL )
		fp = stdout;

	if( pSsection->name != NULL )
		fprintf( fp, "(%s)", pSsection->name );

	if( pSsection->comment != NULL ) {
		/*
		 * inline comment.
		 */
		if( * pSsection->comment->comment == '#' )
			fprintf( fp, " " );

		else
			fprintf( fp, "\n" );

		CF_PRINT_COMMENT( fp, pSsection->comment );
	}

	else
		fprintf( fp, "\n" );

	if( pSsection->keyvalue != NULL )
		CF_PRINT_KEYVALUE( fp, pSsection->keyvalue );

	while( ( pSsection = pSsection->next ) != NULL ) {
		if( pSsection->name != NULL )
			fprintf( fp, "(%s)", pSsection->name );

		if( pSsection->comment != NULL ) {
			/*
			 * inline comment.
			 */
			if( * pSsection->comment->comment == '#' )
				fprintf( fp, " " );

			else
				fprintf( fp, "\n" );

			CF_PRINT_COMMENT( fp, pSsection->comment );
		}

		else
			fprintf( fp, "\n" );

		if( pSsection->keyvalue != NULL )
			CF_PRINT_KEYVALUE( fp, pSsection->keyvalue );
	}

	return 0;
}

/*
 * new key-value pair.
 */
CF_KEYVALUE_TYPE *
CF_NEW_KEYVALUE( char * pKey, char * pVal, CF_VALUE_TYPE vtype, char * pComment )
{
	CF_KEYVALUE_TYPE * pCfk = NULL;
	char * pK = NULL, * pV = NULL;

	/*
	 * allocate a new key-value pair struct.
	 */
	if( ( pCfk = MALLOC( sizeof( CF_KEYVALUE_TYPE ) ) ) != NULL ) {
		/*
		 * caller doesn't have a key to insert.
		 */
		if( pKey == NULL )
			pCfk->key = NULL;
		/*
		 * stuff the key.
		 */
		else {
			/*
			 * allocate the key string.
			 */
			if( ( pK = MALLOC( strlen( pKey ) + 1 ) ) == NULL )
				return NULL;
			/*
			 * stuff the string.
			 */
			if( strncpy( pK, pKey, strlen( pKey ) + 1 ) != pK )
				return NULL;
			/*
			 * fill in the key part.
			 */
			pCfk->key = pK;
		}
		/*
		 * caller doesn't have a value to insert.
		 */
		if( pVal == NULL )
			pCfk->value = NULL;
		/*
		 * stuff the value.
		 */
		else {
			/*
			 * allocate the value string.
			 */
			if( ( pV = MALLOC( strlen( pVal ) + 1 ) ) == NULL )
				return NULL;
			/*
			 * stuff the string.
			 */
			if( strncpy( pV, pVal, strlen( pVal ) + 1 ) != pV )
				return NULL;
			/*
			 * fill in the value part.
			 */
			pCfk->value = pV;
		}
		/*
		 * caller doesn't have a type to insert.
		 */
		if( vtype == CF_NONE )
			pCfk->vtype = CF_CHR;
		/*
		 * stuff the type.
		 */
		else
			pCfk->vtype = vtype;
		/*
		 * caller doesn't have a comment to insert.
		 */
		if( pComment == NULL )
			pCfk->comment = NULL;
		/*
		 * stuff the comment.
		 */
		else {
			/*
			 * get a new comment struct.
			 */
			if( ( pCfk->comment = CF_NEW_COMMENT( cf_skip_backwhite( pComment ) ) ) == NULL )
				return NULL;
		}
		/*
		 * this has been done but let's make sure.
		 */
		pCfk->next = NULL;
	}

	return pCfk;
}

/*
 * free key-value pair.
 */
void
CF_FREE_KEYVALUE( CF_KEYVALUE_TYPE * pCfk )
{
	CF_KEYVALUE_TYPE * temp = NULL;

	while( pCfk != NULL ) {
		temp = pCfk;

		if( pCfk->key != NULL )
			FREE( pCfk->key );

		if( pCfk->value != NULL )
			FREE( pCfk->value );

		if( pCfk->comment != NULL )
			CF_FREE_COMMENT( pCfk->comment );

		pCfk = pCfk->next;

		FREE( temp );
	}
}

/*
 * add key-value pair
 * to the section.
 */
int
CF_ADDSEC_KEYVALUE( CF_KEYVALUE_TYPE * from, CF_SECTION_TYPE * to )
{
	CF_KEYVALUE_TYPE * pKeyvalue;

	if( from == NULL || to == NULL )
		return -1;

	if( ( pKeyvalue = to->keyvalue ) == NULL )
		to->keyvalue = from;

	else {
		while( pKeyvalue->next != NULL )
			pKeyvalue = pKeyvalue->next;

		pKeyvalue->next = from;
	}

	return 0;
}

/*
 * add key-value pair
 * to the subsection.
 */
int
CF_ADDSUB_KEYVALUE( CF_KEYVALUE_TYPE * from, CF_SUBSECTION_TYPE * to )
{
	CF_KEYVALUE_TYPE * pKeyvalue;

	if( from == NULL || to == NULL )
		return -1;

	if( ( pKeyvalue = to->keyvalue ) == NULL )
		to->keyvalue = from;

	else {
		while( pKeyvalue->next != NULL )
			pKeyvalue = pKeyvalue->next;

		pKeyvalue->next = from;
	}

	return 0;
}

/*
 * add key-value pair to the
 * previous key-value pair.
 */
int
CF_ADDKV_KEYVALUE( CF_KEYVALUE_TYPE * from, CF_KEYVALUE_TYPE * to )
{
	if( from == NULL || to == NULL || from == to )
		return -1;

	if( to->next == NULL )
		to->next = from;

	else {
		while( to->next != NULL )
			to = to->next;

		to->next = from;
	}

	return 0;
}

/*
 * remove key-value pair
 * from the section.
 */
int
CF_REMSEC_KEYVALUE( CF_KEYVALUE_TYPE * rem, CF_SECTION_TYPE * from )
{
	CF_KEYVALUE_TYPE * pKeyvalue;

	if( rem == NULL || from == NULL )
		return -1;

	if( ( pKeyvalue = from->keyvalue ) == NULL )
		return -1;

	if( pKeyvalue == rem )
		from->keyvalue = rem->next;

	else {
		while( pKeyvalue->next != NULL && pKeyvalue->next != rem )
			pKeyvalue = pKeyvalue->next;

		if( pKeyvalue->next == NULL )
			return -1;

		pKeyvalue->next = rem->next;
	}

	return 0;
}

/*
 * remove key-value pair
 * from the subsection.
 */
int
CF_REMSUB_KEYVALUE( CF_KEYVALUE_TYPE * rem, CF_SUBSECTION_TYPE * from )
{
	CF_KEYVALUE_TYPE * pKeyvalue;

	if( rem == NULL || from == NULL )
		return -1;

	if( ( pKeyvalue = from->keyvalue ) == NULL )
		return -1;

	if( pKeyvalue == rem )
		from->keyvalue = rem->next;

	else {
		while( pKeyvalue->next != NULL && pKeyvalue->next != rem )
			pKeyvalue = pKeyvalue->next;

		if( pKeyvalue->next == NULL )
			return -1;

		pKeyvalue->next = rem->next;
	}

	return 0;
}

/*
 * print key-value pairs.
 */
int
CF_PRINT_KEYVALUE( FILE * fp, CF_KEYVALUE_TYPE * pKeyvalue )
{
	if( pKeyvalue == NULL )
		return -1;

	if( fp == NULL )
		fp = stdout;

	if( pKeyvalue->key != NULL && pKeyvalue->value != NULL ) {
		if( strlen( pKeyvalue->value ) == strlen( pKeyvalue->key ) && 
			strcmp( pKeyvalue->value, pKeyvalue->key ) == 0 )
			fprintf( fp, "%s", pKeyvalue->value );

		else
			fprintf( fp, "%s=%s", pKeyvalue->key, pKeyvalue->value );

		if( pKeyvalue->vtype == CF_INT )
			fprintf( fp, "~INT" );

		else
			fprintf( fp, "~CHR" );

		if( pKeyvalue->comment != NULL ) {
			/*
			 * inline comment.
			 */
			if( * pKeyvalue->comment->comment == '#' )
				fprintf( fp, " " );

			else
				fprintf( fp, "\n" );

			CF_PRINT_COMMENT( fp, pKeyvalue->comment );
		}

		else
			fprintf( fp, "\n" );

		while( ( pKeyvalue = pKeyvalue->next ) != NULL ) {
			if( strlen( pKeyvalue->value ) == strlen( pKeyvalue->key ) && 
				strcmp( pKeyvalue->value, pKeyvalue->key ) == 0 )
				fprintf( fp, "%s", pKeyvalue->value );

			else
				fprintf( fp, "%s=%s", pKeyvalue->key, pKeyvalue->value );

			if( pKeyvalue->vtype == CF_INT )
				fprintf( fp, "~INT" );

			else
				fprintf( fp, "~CHR" );

			if( pKeyvalue->comment != NULL ) {
				/*
				 * inline comment.
				 */
				if( * pKeyvalue->comment->comment == '#' )
					fprintf( fp, " " );

				else
					fprintf( fp, "\n" );

				CF_PRINT_COMMENT( fp, pKeyvalue->comment );
			}

			else
				fprintf( fp, "\n" );
		}
	}

	return 0;
}

/*
 * new comment.
 */
CF_COMMENT_TYPE *
CF_NEW_COMMENT( char * pComment )
{
	CF_COMMENT_TYPE * pCfc;

	if( ( pCfc = MALLOC( sizeof( CF_COMMENT_TYPE ) ) ) != NULL ) {
		pCfc->comment = NULL;
		pCfc->next = NULL;

		if( pComment != NULL ) {
			if( ( pCfc->comment = MALLOC( strlen( pComment ) + 1 ) ) == NULL ) {
				CF_FREE_COMMENT( pCfc );
				pCfc = NULL;
			}

			if( strncpy( pCfc->comment, pComment, strlen( pComment ) + 1 ) != pCfc->comment ) {
				CF_FREE_COMMENT( pCfc );
				pCfc = NULL;
			}
		}

	}

	return pCfc;
}

/*
 * free comment.
 */
void
CF_FREE_COMMENT( CF_COMMENT_TYPE * pCfc )
{
	CF_COMMENT_TYPE * temp = NULL;

	while( pCfc != NULL ) {
		temp = pCfc;

		if( pCfc->comment != NULL )
			FREE( pCfc->comment );

		pCfc = pCfc->next;

		FREE( temp );
	}
}

/*
 * add comment to the root.
 */
int
CF_ADDROOT_COMMENT( CF_COMMENT_TYPE * from, CF_ROOT_TYPE * to )
{
	CF_COMMENT_TYPE * pComment;

	if( from == NULL || to == NULL )
		return -1;

	if( ( pComment = to->comment ) == NULL )
		to->comment = from;

	else {
		while( pComment->next != NULL )
			pComment = pComment->next;

		pComment->next = from;
	}

	return 0;
}

/*
 * add comment to the section.
 */
int
CF_ADDSEC_COMMENT( CF_COMMENT_TYPE * from, CF_SECTION_TYPE * to )
{
	CF_COMMENT_TYPE * pComment;

	if( from == NULL || to == NULL )
		return -1;

	if( ( pComment = to->comment ) == NULL )
		to->comment = from;

	else {
		while( pComment->next != NULL )
			pComment = pComment->next;

		pComment->next = from;
	}

	return 0;
}

/*
 * add comment to the subsection.
 */
int
CF_ADDSUB_COMMENT( CF_COMMENT_TYPE * from, CF_SUBSECTION_TYPE * to )
{
	CF_COMMENT_TYPE * pComment;

	if( from == NULL || to == NULL )
		return -1;

	if( ( pComment = to->comment ) == NULL )
		to->comment = from;

	else {
		while( pComment->next != NULL )
			pComment = pComment->next;

		pComment->next = from;
	}

	return 0;
}

/*
 * add comment to the keyvalue.
 */
int
CF_ADDKV_COMMENT( CF_COMMENT_TYPE * from, CF_KEYVALUE_TYPE * to )
{
	CF_COMMENT_TYPE * pComment;

	if( from == NULL || to == NULL )
		return -1;

	if( ( pComment = to->comment ) == NULL )
		to->comment = from;

	else {
		while( pComment->next != NULL )
			pComment = pComment->next;

		pComment->next = from;
	}

	return 0;
}

/*
 * remove comment from
 * the root.
 */
int
CF_REMROOT_COMMENT( CF_COMMENT_TYPE * rem, CF_ROOT_TYPE * from )
{
	CF_COMMENT_TYPE * pComment;

	if( rem == NULL || from == NULL )
		return -1;

	if( ( pComment = from->comment ) == NULL )
		return -1;

	if( pComment == rem )
		from->comment = rem->next;

	else {
		while( pComment->next != NULL && pComment->next != rem )
			pComment = pComment->next;

		if( pComment->next == NULL )
			return -1;

		pComment->next = rem->next;
	}

	return 0;
}

/*
 * remove comment from
 * the section.
 */
int
CF_REMSEC_COMMENT( CF_COMMENT_TYPE * rem, CF_SECTION_TYPE * from )
{
	CF_COMMENT_TYPE * pComment;

	if( rem == NULL || from == NULL )
		return -1;

	if( ( pComment = from->comment ) == NULL )
		return -1;

	if( pComment == rem )
		from->comment = rem->next;

	else {
		while( pComment->next != NULL && pComment->next != rem )
			pComment = pComment->next;

		if( pComment->next == NULL )
			return -1;

		pComment->next = rem->next;
	}

	return 0;
}

/*
 * remove comment from
 * the subsection.
 */
int
CF_REMSUB_COMMENT( CF_COMMENT_TYPE * rem, CF_SUBSECTION_TYPE * from )
{
	CF_COMMENT_TYPE * pComment;

	if( rem == NULL || from == NULL )
		return -1;

	if( ( pComment = from->comment ) == NULL )
		return -1;

	if( pComment == rem )
		from->comment = rem->next;

	else {
		while( pComment->next != NULL && pComment->next != rem )
			pComment = pComment->next;

		if( pComment->next == NULL )
			return -1;

		pComment->next = rem->next;
	}

	return 0;
}

/*
 * print comments.
 */
int
CF_PRINT_COMMENT( FILE * fp, CF_COMMENT_TYPE * pComment )
{
	if( pComment == NULL )
		return -1;

	if( fp == NULL )
		fp = stdout;

	if( pComment->comment != NULL ) {
		fprintf( fp, "%s\n", pComment->comment );

		while( ( pComment = pComment->next ) != NULL ) {
			if( pComment->comment != NULL )
				fprintf( fp, "%s\n", pComment->comment );
		}
	}

	return 0;
}

/*
 * read a line from the configfile.
 */
char *
cf_readline( FILE * fp )
{
	static char inbuf[ CF_BUFSIZE ];
	char * pString;
	/*
	 * no file pointer, no read.
	 */
	if( fp == NULL )
		return NULL;
	/*
	 * get the line into the buffer.
	 */
	if( ( pString = fgets( inbuf, CF_BUFSIZE, fp ) ) != inbuf )
		return NULL;
	/*
	 * the line must have a newline character at the end.
	 */
	if( cf_zap_newline( pString ) < 0 )
		return NULL;
	/*
	 * return the first non whitespace character as
	 * the beginning of the line.
	 */
	return cf_skip_frontwhite( pString );
}

/*
 * open the named configfile for reading.
 */
FILE * 
cf_ropen( char * filename )
{
	static FILE * fp;
	/*
	 * no filename, no open.
	 */
	if( filename == NULL )
		return NULL;
	/*
	 * open the file for reading.
	 */
	if( ( fp = fopen( filename, "r" ) ) == NULL )
		return NULL;

	return fp;
}

/*
 * open the named configfile for reading.
 */
FILE *
cf_wopen( char * filename )
{
	static FILE * fp;
	/*
	 * no filename, no open.
	 */
	if( filename == NULL )
		return NULL;
	/*
	 * open the file for writing.
	 */
	if( ( fp = fopen( filename, "w" ) ) == NULL )
		return NULL;

	return fp;
}

/*
 * step over white space at the
 * beginning of the line.
 */
char *
cf_skip_frontwhite( char * pWhite )
{
	if( pWhite == NULL )
		return NULL;

	while( * pWhite =='\t' || * pWhite == ' ' )
		pWhite++;

	return pWhite;
}

/*
 * step over white space at the
 * beginning of the line.
 */
char *
cf_skip_backwhite( char * pWhite )
{
	char * pTail = NULL, * pTemp = NULL;

	if( pWhite == NULL )
		return NULL;

	pTail = pWhite + strlen( pWhite ) - 1;

	pTemp = pTail;

	while( * pTail == '\t' || * pTail == ' ' ) {
		pTemp = pTail;
		pTail--;
	}

	if( * pTemp == '\t' || * pTemp == ' ' )
		* pTemp = '\0';

	return pWhite;
}

/*
 * zap newline in string.
 */
int
cf_zap_newline( char * pString )
{
	if( pString == NULL )
		return -1;

	while( * pString != '\0' && * pString != '\n' )
		pString++;

	if( * pString == '\n' )
		* pString = '\0';

	return 0;
}

/*
 * split the key-value pair.
 */
char *
cf_split( char * pString, char ** pKeyP, char ** pValP, cf_t * pType, char ** pCmtP )
{
	char * pSplit;
	static char newString [ CF_BUFSIZE ];
	static char newKey    [ CF_BUFSIZE ];
	static char newVal    [ CF_BUFSIZE ];
	static char newComment[ CF_BUFSIZE ];

	/*
	 * the string to split must exist.
	 */
	if( pString == NULL )
		return NULL;
	/*
	 * the key and value strings, the type and the comment string must also exist.
	 */
	if( pKeyP == NULL || pValP == NULL || pType == NULL || pCmtP == NULL )
		return NULL;
	/*
	 * first copy the String to newString.
	 */
	if( strncpy( newString, pString, CF_BUFSIZE ) != newString )
		return NULL;
	/*
	 * next find the '=' in newString that marks a key-value pair.
	 * if found, copy the key and value to newKey and newValue.
	 */
	if( ( pSplit = strchr( newString, '=' ) ) != NULL ) {
		if( strncpy( newVal, pSplit + 1, CF_BUFSIZE ) != newVal )
			return NULL;

		* pSplit = '\0';

		if( strncpy( newKey, newString, CF_BUFSIZE ) != newKey )
			return NULL;
	}
	/*
	 * if not then copy newString to newKey and newVal.
	 */
	else {
		if( strncpy( newKey, newString, CF_BUFSIZE ) != newKey )
			return NULL;

		if( strncpy( newVal, newString, CF_BUFSIZE ) != newVal )
			return NULL;

		if( strncpy( newComment, newString + 1, CF_BUFSIZE ) != newComment )
			return NULL;
	}
	/*
	 * now search newVal for the '~' that marks the type.
	 * if found, set the type parameter.
	 * the string following the '~' is coppied to newComment
	 * for later use and the end of newVal at the '~' character.
	 * then search the newKey for the '~' and end that string there.
	 * if not found in newKey or newValue then move on to
	 * the comment.
	 */
	if( ( pSplit = strchr( newVal, '~' ) ) != NULL ) {
		if( strncpy( newComment, pSplit + 1, CF_BUFSIZE ) != newComment )
			return NULL;
		/*
		 * this key-value pair has a type.
		 */
		if( strcmp( cf_sntoupper( pSplit + 1, 3 ), "INT") == 0 ) {
			* pType = CF_INT;
		}

		else {
			* pType = CF_CHR;
		}

		* pSplit = '\0';

		if( ( pSplit = strchr( newKey, '~' ) ) != NULL )
			* pSplit = '\0';
	}
	else {
		/*
		 * this key-value pair has no type.
		 * default to CHR.
		 */
		* pType = CF_CHR;
		/*
		 * the string could still have a comment even though it
		 * has no type.
		 */
		if( strncpy( newComment, pString + 1, CF_BUFSIZE ) != newComment )
			return NULL;
	}
	/*
	 * lastly, search newComment for the '#' that marks the comment.
	 * if found then move the '#' to the head of the comment string.
	 */
	if( ( pSplit = strchr( newComment, '#' ) ) != NULL ) {
		/*
		 * this key-value pair has a comment.
		 */
		if( strncpy( newComment, pSplit, CF_BUFSIZE ) != newComment )
			return NULL;

		* pCmtP = newComment;
	}
	else {
		/*
		 * this key-value pair has no comment.
		 */
		* pCmtP = NULL;
	}

	if( ( pSplit = strchr( newKey, '#' ) ) != NULL )
		* pSplit = '\0';

	* pKeyP = newKey;

	if( ( pSplit = strchr( newVal, '#' ) ) != NULL )
		* pSplit = '\0';

	* pValP = newVal;

	if( ( pSplit = strchr( newString, '#' ) ) != NULL )
		* pSplit = '\0';

	return newString;
}

/*
 * isolate the desired name string.
 */
char *
cf_isolate( CF_STATE_TYPE state, char * pString, char ** pCmtP )
{
	char * pSplit, front = '?', back = '?';
	static char newString[ CF_BUFSIZE ];
	static char newComment[ CF_BUFSIZE ];

	/*
	 * the string pointer must exist.
	 */
	if( pString == NULL )
		return NULL;
	/*
	 * prepare front and back characters
	 * based on the state variable.
	 */
	switch( state ) {
	/*
	 * section state.
	 */
	case CF_SECTION:
		front = '[';
		back = ']';
		break;
	/*
	 * subsection state.
	 */
	case CF_SUBSECTION:
		front = '(';
		back = ')';
		break;
	/*
	 * only section and subsection names
	 * can be isolated in this way.
	 */
	default:
		return NULL;
	}
	/*
	 * the first character must be front.
	 */
	if( * pString != front )
		return NULL;
	/*
	 * step over the front character.
	 */
	if( strncpy( newString, pString + 1, CF_BUFSIZE ) != newString )
		return NULL;
	/*
	 * now find and kill the back character.
	 */
	if( ( pSplit = strchr( newString, back ) ) == NULL )
		/*
		 * format failure. no back character.
		 */
		return NULL;
	/*
	 * kill the back character.
	 */
	* pSplit = '\0';

	/*
	 * deal with the comment.
	 */
	if( pCmtP != NULL ) {
		/*
		 * look for the telltail '#' that marks a comment type.
		 */
		if( ( pSplit = strchr( pString, '#' ) ) != NULL ) {
			/*
			 * this section or subsection has a comment.
			 */
			if( strncpy( newComment, pSplit, CF_BUFSIZE ) != newComment )
				return NULL;

			* pCmtP = newComment;
		}
		else {
			/*
			 * this section or subsection has no comment.
			 */
			* pCmtP = NULL;
		}
	}

	return newString;
}

/*
 * convert the string to upper case characters.
 *
 * this routine is destructive of the string
 * passed in as the parameter so be careful.
 */
char *
cf_sntoupper( char * pString, int num )
{
	static char upper[ CF_BUFSIZE ];

	if( pString == NULL || num <= 0 || num > CF_BUFSIZE )
		return NULL;

	if( strncpy( upper, pString, CF_BUFSIZE ) != upper )
		return NULL;

	if( num < CF_BUFSIZE )
		upper[ num-- ] = '\0';

	while( num >= 0 && upper[ num ] != '\0' ) {
		upper[ num ] = toupper( upper[ num ] );
		num--;
	}

	return upper;
}

int module_read_config(char *section, char *prefix, char *module, struct config *conf) {
  CF_ROOT_TYPE    *p_root;
  CF_SECTION_TYPE *p_section;
  struct stat      statfile;
  char             buffer[1024];
  char             conffile[256];

  snprintf(conffile, 255, "./%s.cfg", module);
  
// search for the config file called module.cfg
  if (stat(conffile, &statfile) != 0) {
    char *home = getenv("HOME");
    
    if (home != NULL) {
      snprintf(buffer, 1023, "%s/.transcode/%s.cfg", home, module);
      if (stat(buffer, &statfile) != 0) {
        fprintf(stderr, "[%s] Neither './%s.cfg' nor '~/.transcode/%s.cfg' found. Falling back "
                "to hardcoded defaults.\n", prefix, module, module);
        return 0;
      }
    } else {
      return 0;
    }
  } else {
    strcpy(buffer, conffile);
  }

  fprintf(stderr, "[%s] Reading configuration from '%s'\n", prefix, buffer);
  
  if (!S_ISREG(statfile.st_mode)) {
    fprintf(stderr, "[%s] '%s' is not a regular file. Falling back to hardcoded"
            " defaults.\n", prefix, buffer);
    return 0;
  }

  p_root = cf_read(buffer);
  if (p_root == NULL) {
    fprintf(stderr, "[%s] Error reading configuration file '%s'. Falling back "
            "to hardcoded defaults.\n", prefix, buffer);
    return 0;
  }

  p_section = cf_get_section(p_root);
  while (p_section != NULL) {
    if (!strcmp(p_section->name, section)) {
      module_read_values(p_root, p_section, prefix, conf);
      CF_FREE_ROOT(p_root);
      return 1;
    }
    p_section = cf_get_next_section(p_root, p_section);
  }
  
  CF_FREE_ROOT(p_root);
  
  fprintf(stderr, "[%s] No section named '%s' found in '%s'. Falling "
          "back to hardcoded defaults.\n", prefix, section,
          conffile);

  return 0;
}

int module_read_values(CF_ROOT_TYPE *p_root, CF_SECTION_TYPE *p_section,
                        char *prefix, struct config *conf) {
  CF_KEYVALUE_TYPE *kv;
  struct config    *cur_config;
  char             *value, *error;
  int               i;
  float             f;

  cur_config = conf;

  while (cur_config->name != NULL) {
    value = cf_get_named_section_value_of_key(p_root, p_section->name,
                                              cur_config->name);
    if (value != NULL) {
      errno = 0;
      switch (cur_config->type) {
        case CONF_TYPE_INT:
          i = strtol(value, &error, 10);
          if ((errno != 0) || (i == LONG_MIN) || (i == LONG_MAX) ||
              ((error != NULL) && (*error != 0)))
            fprintf(stderr, "[%s] Option '%s' must be an integer.\n",
                    prefix, cur_config->name);
          else if ((cur_config->flags & CONF_MIN) && (i < cur_config->min))
            fprintf(stderr, "[%s] Option '%s' has a value that is too low "
                    "(%d < %d).\n", prefix, cur_config->name, i,
                    (int)cur_config->min);
          else if ((cur_config->flags & CONF_MAX) && (i > cur_config->max))
            fprintf(stderr, "[%s] Option '%s' has a value that is too high "
                    "(%d > %d).\n", prefix, cur_config->name, i,
                    (int)cur_config->max);
          else
            *((int *)cur_config->p) = i;
          break;
        case CONF_TYPE_FLAG:
          i = atoi(value);
          if (errno != 0)
            fprintf(stderr, "[%s] Option '%s' is a flag. The only values "
                    "allowed for it are '0' and '1'.\n", prefix,
                    cur_config->name);
          else if ((i != 1) && (i != 0))
            fprintf(stderr, "[%s] Option '%s' is a flag. The only values "
                    "allowed for it are '0' and '1'.\n", prefix,
                    cur_config->name);
          else
            *((int *)cur_config->p) = (i?(int)cur_config->max:0);
          break;
        case CONF_TYPE_FLOAT:
          f = strtod(value, NULL);
          if (errno != 0)
            fprintf(stderr, "[%s] Option '%s' must be a float.\n",
                    prefix, cur_config->name);
          else if ((cur_config->flags & CONF_MIN) && (f < cur_config->min))
            fprintf(stderr, "[%s] Option '%s' has a value that is too low "
                    "(%f < %f).\n", prefix, cur_config->name, f,
                    cur_config->min);
          else if ((cur_config->flags & CONF_MAX) && (f > cur_config->max))
            fprintf(stderr, "[%s] Option '%s' has a value that is too high "
                    "(%f > %f).\n", prefix, cur_config->name, f,
                    cur_config->max);
          else
            *((float *)cur_config->p) = f;
          break;
        case CONF_TYPE_STRING:
          *((char **)cur_config->p) = strdup(value);
          break;
        default:
          fprintf(stderr, "[%s] Unsupported config type '%d' for '%s'.\n",
                  prefix, cur_config->type, cur_config->name);
      }
    }
  
    cur_config++;
  }
  
  kv = cf_get_named_section_keyvalue(p_root, p_section->name);
  while (kv != NULL) {
    cur_config = conf;
    i = 0;
    while (cur_config->name != NULL) {
      if (!strcmp(kv->key, cur_config->name)) {
        i = 1;
        break;
      }
      cur_config++;
    }
    if (!i)
      fprintf(stderr, "[%s] Key '%s' is not a valid option.\n", prefix,
              kv->key);
    kv = cf_get_named_section_next_keyvalue(p_root, p_section->name, kv);
  }
  
  return 0;
}

int module_print_config(char *prefix, struct config *conf) {
  struct config *cur_config;
  char          *s;
  
  cur_config = conf;
  
  while (cur_config->name != NULL) {
    switch (cur_config->type) {
      case CONF_TYPE_INT:
        fprintf(stderr, "%s%s = %d\n", prefix, cur_config->name, 
                *((int *)cur_config->p));
        break;
      case CONF_TYPE_FLAG:
        fprintf(stderr, "%s%s = %d\n", prefix, cur_config->name, 
                *((int *)cur_config->p) ? 1 : 0);
        break;
      case CONF_TYPE_FLOAT:
        fprintf(stderr, "%s%s = %f\n", prefix, cur_config->name, 
                *((float *)cur_config->p));
        break;
      case CONF_TYPE_STRING:
        s = *((char **)cur_config->p);
        fprintf(stderr, "%s%s%s = %s\n", prefix,
                s == NULL ? "#" : (*s == 0 ? "#" : ""),
                cur_config->name,
                s == NULL ? "" : s);
        break;
      default:
        fprintf(stderr, "%s#%s = <UNSUPPORTED FORMAT>\n", prefix,
                cur_config->name);
    }
    cur_config++;
  }
  
  return 0;
}
