extern int chroma_key(int u, int v, double color,\
	double color_window, double saturation);
extern int set_main_movie_properties(struct object *pa);
extern void *movie_routine(char *temp);
extern void adjust_color(int *u, int *v, double degrees, double saturation);
extern int yuv_to_ppm(char *data, int xsize, int ysize, char *filename);
extern char *change_picture_geometry(\
	char *data, int xsize, int ysize,\
	double *new_xsize, double *new_ysize,\
	int keep_aspect,\
	double zrotation,\
	double xshear, double yshear);
extern int sort_objects_by_zaxis();
extern char *ppm_to_yuv_in_char(char *pathfilename, int *xsize, int *ysize);
extern int get_h_pixels(int c, font_desc_t *pfd);
extern char *p_reformat_text(char *text, int max_pixels, font_desc_t *pfd);
extern int p_center_text(char *text, font_desc_t *pfd);
extern int add_text(\
	int x, int y, char *text, int u, int v,\
	double contrast, double transparency, font_desc_t *pfd,\
	int extra_char_space);
extern int draw_char(\
	int x, int y, int c, int u, int v,\
	double contrast, double transparency, font_desc_t *pfd);
extern void draw_alpha(\
	int x0 ,int y0,\
	int w, int h,\
	uint8_t *src, uint8_t *srca, int stride, int u, int v,\
	double contrast, double transparency);
extern void draw_alpha_rgb24(\
	int w, int h,\
	unsigned char* src, unsigned char *srca, int srcstride,\
	unsigned char* dstbase, int dststride);
extern int time_base_corrector(int y, uint8_t *pfm, int hsize, int vsize);
extern int print_options();
extern int hash(char *s);
extern char *strsave(char *s);
extern int readline(FILE *file, char *contents);
extern struct frame *lookup_frame(char *name);
extern struct frame *install_frame(char *name);
extern int delete_all_frames();
extern int add_frame(\
	char *name, char *data, int object_type,\
	int xsize, int ysize, int zsize, int id);
extern int set_end_frame_and_end_sample(int frame_nr, int end_frame);
extern char *get_path(char *filename);
extern raw_file* load_raw(char *name,int verbose);
extern font_desc_t* read_font_desc(char* fname,float factor,int verbose);
extern int readline_msdos(FILE *file, char *contents);
extern int load_ssa_file(char *pathfilename);
extern read_in_ssa_file(FILE *finptr);
extern int load_ppml_file(char *pathfilename);
extern read_in_ppml_file(FILE *finptr);
extern struct object *lookup_object(char *name);
extern struct object *install_object_at_end_of_list(char *name);
extern int delete_object(char *name);
extern int delete_all_objects();
extern int set_object_status(int start_frame_nr, int status);
extern int get_object_status(int start_frame_nr, int *status);
extern int add_subtitle_object(\
	int start_frame_nr, int end_frame_nr, int type,\
	double xpos, double ypos, double zpos,\
	char *data);
extern int process_frame_number(int current_frame_nr);
extern void putimage(int xsize, int ysize);
extern int openwin(int argc, char *argv[], int xsize, int ysize);
extern unsigned char *getbuf(void);
extern void closewin(void);
extern int get_x11_bpp();
extern int resize_window(int xsize, int ysize);
