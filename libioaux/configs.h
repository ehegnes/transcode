/*
 * configs.h
 * a more generic approach to config files.
 * Copyright 12/10/01 Chris C. Hoover
 */

#ifndef _CONFIGFILES_H_
#define _CONFIGFILES_H_

//#define DEBUG

#define MALLOC(a) malloc(a)
#define FREE(a) free(a)

/*
 * configfiles have the following format.
 * only the first 256 characters of the line are significant.
 *
 * # comment lines are ignored.
 * [section name]		# this is a comment at the end of the line.
 * key=value~type		# sections can have
 * key=value~type		# key value pairs.
 * value~type			# key value pairs without '=' are just
 *						# values and are legal.
 *
 * (subsection name)	# sections can also have subsections.
 * key=value~type		# subsections can only have key value pairs.
 * key=value			# type is optional and can be CHR or INT.
 *						# type defaults to CHR.
 * (subsection name)
 * key=value
 * key=value~type
 *
 *						# whitespace ('\t',' ',^'\n') is ignored.
 * [section name]
 * key=value~type
 *
 * (subsection name)
 * key=value~type
 */

#define CF_BUFSIZE 256

/*
 * these defines are for convenience only
 * since all these struct names look so
 * much alike.
 */
#define CF_ROOT_TYPE        cfr_t
#define CF_SECTION_TYPE     cfs_t
#define CF_SUBSECTION_TYPE  cfu_t
#define CF_KEYVALUE_TYPE    cfk_t
#define CF_VALUE_TYPE       cf_t
#define CF_COMMENT_TYPE     cfc_t

#define CF_NEW_ROOT         new_cfr
#define CF_FREE_ROOT        free_cfr
#define CF_PRINT_ROOT       fprint_cfr

#define CF_NEW_SECTION      new_cfs
#define CF_FREE_SECTION     free_cfs
#define CF_ADD_SECTION      add_cfs
#define CF_REM_SECTION      rem_cfs
#define CF_PRINT_SECTION    fprint_cfs

#define CF_NEW_SUBSECTION   new_cfu
#define CF_FREE_SUBSECTION  free_cfu
#define CF_ADD_SUBSECTION   add_cfu
#define CF_REM_SUBSECTION   rem_cfu
#define CF_PRINT_SUBSECTION fprint_cfu

#define CF_NEW_KEYVALUE     new_cfk
#define CF_FREE_KEYVALUE    free_cfk
#define CF_ADDSEC_KEYVALUE  adds_cfk
#define CF_ADDSUB_KEYVALUE  addu_cfk
#define CF_ADDKV_KEYVALUE   addk_cfk
#define CF_REMSEC_KEYVALUE  rems_cfk
#define CF_REMSUB_KEYVALUE  remu_cfk
#define CF_PRINT_KEYVALUE   fprint_cfk

#define CF_NEW_COMMENT      new_cfc
#define CF_FREE_COMMENT     free_cfc
#define CF_ADDROOT_COMMENT  addr_cfc
#define CF_ADDSEC_COMMENT   adds_cfc
#define CF_ADDSUB_COMMENT   addu_cfc
#define CF_ADDKV_COMMENT    addk_cfc
#define CF_REMROOT_COMMENT  remr_cfc
#define CF_REMSEC_COMMENT   rems_cfc
#define CF_REMSUB_COMMENT   remu_cfc
#define CF_PRINT_COMMENT    fprint_cfc

#define CF_STATE_TYPE       cft_t
//#define CF_VALUE_TYPE		cfv_t

/*
 * api lighteners.
 */
#define CF_NGETS    cf_get_named_section
#define CF_NGETSK   cf_get_named_section_key
#define CF_NGETSV   cf_get_named_section_value
#define CF_NGETSKV  cf_get_named_section_keyvalue
#define CF_NGETSNKV cf_get_named_section_next_keyvalue
#define CF_NGETSVOK cf_get_named_section_value_of_key

#define CF_NPUTSVOK cf_put_named_section_value_of_key
/*
 * the states enum.
 */
enum cf_state {
	CF_EMPTY,
	CF_ROOT,
	CF_SECTION,
	CF_SUBSECTION,
	CF_KEYVALUE,
	CF_COMMENT
};
typedef enum cf_state cft_t;
/*
 * the types enum.
 */
enum cf_type {
	CF_NONE,
	CF_CHR,
	CF_INT
};
typedef enum cf_type cf_t;
/*
 * the comment type.
 */
struct cf_comment {
	char * comment;
	struct cf_comment * next;
};
typedef struct cf_comment cfc_t;
/*
 * the keyvalue type.
 */
struct cf_keyvaluepair {
	char  * key;
	char  * value;
	cf_t    vtype;
	cfc_t * comment;
	struct cf_keyvaluepair * next;
};
typedef struct cf_keyvaluepair cfk_t;
/*
 * the subsection type.
 */
struct cf_subsection {
	char  * name;
	cfk_t * keyvalue;
	cfc_t * comment;
	struct cf_subsection * next;
};
typedef struct cf_subsection cfu_t;
/*
 * the section type.
 */
struct cf_section {
	char  * name;
	cfk_t * keyvalue;
	cfu_t * subsection;
	cfc_t * comment;
	struct cf_section * next;
};
typedef struct cf_section cfs_t;
/*
 * the root type.
 */
struct cf_root {
	char  * name;
	cfs_t * section;
	cfc_t * comment;
};
typedef struct cf_root cfr_t;
/*
 * the value_type type.
 */
/*
union cf_value {
	char * val_chr;
	int    val_int;
};
typedef union cf_value cfv_t;
*/

/*
 * the top top level api ;)
 * merged in from export_ffmpeg
 */
#define CONF_TYPE_FLAG		0
#define CONF_TYPE_INT		1
#define CONF_TYPE_FLOAT		2
#define CONF_TYPE_STRING	3
#define CONF_TYPE_SECTION	4

#define CONF_MIN		(1<<0)
#define CONF_MAX		(1<<1)
#define CONF_RANGE		(CONF_MIN|CONF_MAX)

struct config {
  char *name;
  void *p;
  unsigned int type, flags;
  float min, max;
  void *dummy;
};

int module_read_config(char *section, char *prefix, char *module, struct config *conf, char *configdir);
int module_read_values(CF_ROOT_TYPE *p_root, CF_SECTION_TYPE *p_section,
                       char *prefix, struct config *conf);
int module_print_config(char *prefix, struct config *conf);

/*
 * fill the config structure in this way:

struct config module_conf[] = {
    {"vcodec", &lavc_param_vcodec, CONF_TYPE_STRING, 0, 0, 0, NULL},
    {"vbitrate", &lavc_param_vbitrate, CONF_TYPE_INT, CONF_RANGE, 4, 24000000, NULL},
    {NULL, NULL, 0, 0, 0, 0, NULL}
  };
  */

/*
 * the top level api.
 */
cfr_t * cf_read( char * filename );
int cf_write( cfr_t * pRoot, char * filename );
cfs_t * cf_get_section( cfr_t * pRoot );
cfs_t * cf_get_next_section( cfr_t * pRoot, cfs_t * pSection );
/*
 * the user has intimate knowledge of the section, subsection
 * and key names in the config file so all the following
 * functions rely on that knowledge.
 */
cfs_t * cf_get_named_section( cfr_t * pRoot, char * pName );
char  * cf_get_named_section_key( cfr_t * pRoot, char * pName );
char  * cf_get_named_section_value( cfr_t * pRoot, char * pName );
cfk_t * cf_get_named_section_keyvalue( cfr_t * pRoot, char * pName );
cfk_t * cf_get_named_section_next_keyvalue( cfr_t * pRoot, char * pName, cfk_t * pKeyvalue );
char  * cf_get_named_section_value_of_key( CF_ROOT_TYPE * pRoot, char * pName, char * pKey );
cfu_t * cf_get_named_subsection( cfr_t * pRoot, char * pName );
cfk_t * cf_get_named_subsection_keyvalue( cfr_t * pRoot, char * pName );
cfk_t * cf_get_named_subsection_next_keyvalue( cfr_t * pRoot, char * pName, cfk_t * pKeyvalue );

int cf_put_named_section_value_of_key( cfr_t * pRoot, char * pName, char * pKey, char * pVal, cf_t vtype, char * pComment );
/*
 * the root creation, destruction and maintenance functions.
 */
cfr_t * new_cfr( char * name, cfs_t * pSection, cfc_t * pComment );
void free_cfr( cfr_t * pCfr );
int fprint_cfr( FILE * fp, cfr_t * pRoot );
/*
 * the section creation, destruction and maintenance functions.
 */
cfs_t * new_cfs( char * name, cfk_t * pKeyvalue, cfu_t * pSsection, char * pComment );
void free_cfs( cfs_t * pCfs );
int add_cfs( cfs_t * from, cfr_t * to );
int rem_cfs( cfs_t * rem, cfr_t * from );
int fprint_cfs( FILE * fp, cfs_t * pSection );
/*
 * the subsection creation, destruction and maintenance functions.
 */
cfu_t * new_cfu( char * name, cfk_t * pKeyvalue, char * pComment );
void free_cfu( cfu_t * pCfu );
int add_cfu( cfu_t * from, cfs_t * to );
int rem_cfu( cfu_t * rem, cfs_t * from );
int fprint_cfu( FILE * fp, cfu_t * pSsection );
/*
 * the key-value creation, destruction and maintenance functions.
 */
cfk_t * new_cfk( char * pKey, char * pVal, cf_t vtype, char * pComment );
void free_cfk( cfk_t * pCfk );
int adds_cfk( cfk_t * from, cfs_t * to );
int addu_cfk( cfk_t * from, cfu_t * to );
int addk_cfk( cfk_t * from, cfk_t * to );
int rems_cfk( cfk_t * rem, cfs_t * from );
int remu_cfk( cfk_t * rem, cfu_t * from );
int fprint_cfk( FILE * fp, cfk_t * pKeyvalue );
/*
 * the comment creation, destruction and maintenance functions.
 */
cfc_t * new_cfc( char * pComment );
void free_cfc( cfc_t * pCfc );
int addr_cfc( cfc_t * from, cfr_t * to );
int adds_cfc( cfc_t * from, cfs_t * to );
int addu_cfc( cfc_t * from, cfu_t * to );
int addk_cfc( cfc_t * from, cfk_t * to );
int remr_cfc( cfc_t * rem, cfr_t * from );
int rems_cfc( cfc_t * rem, cfs_t * from );
int remu_cfc( cfc_t * rem, cfu_t * from );
int fprint_cfc( FILE * fp, cfc_t * pComment );
/*
 * the utilities.
 */
FILE * cf_ropen( char * filename );
FILE * cf_wopen( char * filename );
char * cf_readline( FILE * fp );
char * cf_split( char * pString, char ** pKeyP, char ** pValP, cf_t * pType, char ** pCmtP );
char * cf_isolate( cft_t state, char * pString, char ** pCmtP );
char * cf_skip_frontwhite( char * pWhite );
char * cf_skip_backwhite( char * pWhite );
int cf_zap_newline( char * pString );
char * cf_sntoupper( char * pString, int num );

/*
 * _CONFIGFILES_H_
 */
#endif

