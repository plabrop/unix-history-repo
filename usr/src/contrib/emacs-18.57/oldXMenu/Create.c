/* $Header: Create.c,v 1.14 88/02/02 19:02:31 jim Exp $ */
/* Copyright    Massachusetts Institute of Technology    1985	*/

#include <X11/copyright.h>

/*
 * XMenu:	MIT Project Athena, X Window system menu package
 *
 * 	XMenuCreate -	Creates an X window system menu object.
 *
 *	Author:		Tony Della Fera, DEC
 *			January 23, 1986
 *
 */

#include "XMenuInt.h"


#include <X11/bitmaps/dimple1>
#include <X11/bitmaps/dimple3>
#include <X11/bitmaps/gray1>
#include <X11/bitmaps/gray3>
#include <X11/bitmaps/cross_weave>

#include <X11/bitmaps/left_ptr>
#include <X11/bitmaps/left_ptrmsk>
#include <X11/bitmaps/right_ptr>
#include <X11/bitmaps/right_ptrmsk>
#include <X11/bitmaps/cntr_ptr>
#include <X11/bitmaps/cntr_ptrmsk>
#include <X11/bitmaps/stipple>


#define DEF_FREEZE		0
#define DEF_REVERSE		0
#define DEF_MENU_STYLE		LEFT
#define DEF_MENU_MODE		BOX
#define DEF_INACT_PNUM		3
#define MAX_INACT_PNUM		4

#define DEF_P_STYLE		CENTER

#define DEF_P_EVENTS		(EnterWindowMask | ExposureMask)
#define DEF_P_FNT_NAME		"fixed"
#define DEF_P_SPREAD		0.5
#define DEF_P_BDR_WIDTH		2

#define DEF_S_STYLE		LEFT
#define DEF_S_EVENTS		(EnterWindowMask | LeaveWindowMask)
#define DEF_S_FNT_NAME		"fixed"
#define DEF_S_SPREAD		0.10
#define DEF_S_BDR_WIDTH		1

#define XASSOC_TABLE_SIZE	64

#define TILE_BUF_SIZE		5

int atoi();
double atof();

XMenu *
XMenuCreate(display, parent, def_env)
    Display *display;           /* ID of previously opened display */
    Window parent;		/* Window ID of the menu's parent window. */
    register char *def_env;	/* X Defaults program environment name. */
{
    register int i;		/* Loop counter. */
    register int j;		/* Loop counter. */
    register char *def_val;	/* X Default value temp variable. */

    register XMenu *menu;	/* Pointer to the new menu. */
    XMStyle menu_style;		/* Menu display style. */
    XMMode menu_mode;		/* Menu display mode. */
    XMPane *pane;		/* Pane list header. */
    XAssocTable *assoc_tab;     /* XAssocTable pointer. */

    int freeze;			/* Freeze server mode. */
    int reverse;		/* Reverse video mode. */

    XMStyle p_style;		/* Pane display style. */
    char *p_fnt_name;		/* Flag font name. */
    XFontStruct *p_fnt_info;    /* Flag font structure */
    int p_fnt_pad;		/* Flag font padding in pixels. */
    double p_spread;		/* Pane spread in flag height fractions. */
    int p_fnt_height;           /* Pane character height. */
    int p_bdr_width;		/* Pane border width. */
    int flag_height;		/* Flag window height. */
    int p_height;		/* Pane window height. */
    int p_x_off;		/* Pane X offset. */
    int p_y_off;		/* Pane Y offset. */
    GC pane_GC;			/* Pane graphics context. */

    XMStyle s_style;		/* Selection display style. */
    char *s_fnt_name;		/* Selection font name. */
    XFontStruct *s_fnt_info;	/* Selection font structure. */
    int s_fnt_pad;		/* Selection font padding in pixels. */
    int s_fnt_height;           /* Selection font character height */
    double s_spread;		/* Select spread in line height fractions. */
    int s_bdr_width;		/* Highlight border width. */
    int s_height;		/* Selection window height. */
    int s_x_off;		/* Selection window X offset. */
    int s_y_off;		/* Selection window Y offset. */
    GC normal_select_GC;	/* GC used for normal video selection. */
    GC inverse_select_GC;	/* GC used for inverse video selection. */
    GC inact_GC;		/* GC for inactive pane header and */
				/* selections. */
    GC inact_GC_noexpose;

    XColor color_def;		/* Temp color definition holder. */
    XColor screen_def;		/* Temp screen color defintion holder */
    XColor p_bdr_color;		/* Color of border. */
    XColor s_bdr_color;		/* Color of highlight. */
    XColor p_frg_color;		/* Color of pane foreground color. */
    XColor s_frg_color;		/* Color of selection foreground. */
    XColor bkgnd_color;		/* Color of background.. */
    XColor mouse_color;		/* Color of mouse cursor. */
    Cursor mouse_cursor;	/* Mouse cursor. */
    Pixmap inact_bitmap;        /* Menu inactive pixmap. */

    int inact_pnum;		/* Inactive background pattern number. */

    Pixel p_bdr_pixel;	        /* Pane border pixel. */
    Pixel s_bdr_pixel;	        /* Selection border pixel. */
    Pixel p_frg_pixel;	        /* Pane forground pixel. */
    Pixel s_frg_pixel;	        /* Selection forground pixel. */
    Pixel bkgnd_pixel;	        /* Menu background pixel. */

    int *width, *height;
    Pixmap *bitmap;
    int *x_hot, *y_hot;
    int status;                 /* Return code from XReadBitmapFile. */

    Pixmap cursor;              /* Cursor pixmap holder. */
    Pixmap cursor_mask;         /* Cursor mask pixmap holder. */
    Pixmap stipple_pixmap;	/* Stippple mask for half-tone text. */
    unsigned long valuemask;
    XGCValues *values;
    

    /*
     * Calloc the XMenu structure and the initial pane.
     */
    menu = (XMenu *)calloc(1, sizeof(XMenu));
    if (menu == NULL) {
	_XMErrorCode = XME_CALLOC;
	return(NULL);
    }
    pane = (XMPane *)calloc(1, sizeof(XMPane));
    if (pane == NULL) {
	_XMErrorCode = XME_CALLOC;
	return(NULL);
    }
    
    /* 
     * Create the XAssocTable
     */
    assoc_tab = (XAssocTable *)XCreateAssocTable(XASSOC_TABLE_SIZE);
    if(assoc_tab == NULL) {
	_XMErrorCode= XME_CREATE_ASSOC;
	return(NULL);
    }

    /*
     * Set up the default environment name.
     */
    if (def_env == NULL || *def_env == '\0') def_env = "XMenu";

    /*
     * Set up internal fail-safe defaults.
     */
    freeze = DEF_FREEZE;
    reverse = DEF_REVERSE;
    menu_style = DEF_MENU_STYLE;
    menu_mode = DEF_MENU_MODE;
    inact_pnum = DEF_INACT_PNUM;

    p_style = DEF_P_STYLE;
    p_spread = DEF_P_SPREAD;
    p_fnt_name = DEF_P_FNT_NAME;
    p_bdr_width = DEF_P_BDR_WIDTH;

    s_style = DEF_S_STYLE;
    s_spread = DEF_S_SPREAD;
    s_fnt_name = DEF_S_FNT_NAME;
    s_bdr_width = DEF_S_BDR_WIDTH;

    /*
     * Get default values from X.
     */
    def_val = XGetDefault(display, def_env, "MenuFreeze");
    if (def_val != NULL) {
	if (strcmp(def_val, "on") == 0) freeze = 1;
	else if (strcmp(def_val, "off") == 0) freeze = 0;
    }

    def_val = XGetDefault(display, def_env, "MenuReverseVideo");
    if (def_val != NULL) {
	if (strcmp(def_val, "on") == 0) reverse = 1;
	else if (strcmp(def_val, "off") == 0) reverse = 0;
    }

    def_val = XGetDefault(display, def_env, "MenuStyle");
    if (def_val != NULL) {
	if (strcmp(def_val, "right_hand") == 0) menu_style = RIGHT;
	else if (strcmp(def_val, "left_hand") == 0) menu_style = LEFT;
	else if (strcmp(def_val, "center") == 0) menu_style = CENTER;
    }

    def_val = XGetDefault(display, def_env, "MenuMode");
    if (def_val != NULL) {
	if (strcmp(def_val, "box") == 0) menu_mode = BOX;
	else if (strcmp(def_val, "invert") == 0) menu_mode = INVERT;
    }
    
    def_val = XGetDefault(display, def_env, "MenuMouse");
    if (
	def_val != NULL &&
	DisplayCells(display, DefaultScreen(display)) > 2 &&
	XAllocNamedColor(display, 
			 DefaultColormap(display, DefaultScreen(display)), 
			 def_val, 
			 &mouse_color, &color_def)
	);
    else if (reverse &&
	     XAllocNamedColor(display,
			      DefaultColormap(display, DefaultScreen(display)),
			      "white",
			      &mouse_color, &color_def)
	     );
    
    else if (XAllocNamedColor(display,
			      DefaultColormap(display, DefaultScreen(display)),
			      "black", 
			      &mouse_color, &color_def)
	     );
    
    else ;

    def_val = XGetDefault(display, def_env, "MenuBackground");
    if (
	def_val != NULL &&
	DisplayCells(display, DefaultScreen(display)) > 2 &&
	XAllocNamedColor(display,
			 DefaultColormap(display, DefaultScreen(display)),
			 def_val,
			 &bkgnd_color, &color_def)
	);
    else if (reverse &&
	     XAllocNamedColor(display,
			      DefaultColormap(display, DefaultScreen(display)),
			      "black",
			      &bkgnd_color, &color_def)
	     );
    else if (XAllocNamedColor(display,
			      DefaultColormap(display, DefaultScreen(display)),
			      "white",
			      &bkgnd_color, &color_def)
	     );
    else;

    def_val = XGetDefault(display, def_env, "MenuInactivePattern");
    if (def_val != NULL) {
	if (strcmp(def_val, "dimple1") == 0) inact_pnum = 0;
	else if (strcmp(def_val, "dimple3") == 0) inact_pnum = 1;
	else if (strcmp(def_val, "gray1") == 0) inact_pnum = 2;
	else if (strcmp(def_val, "gray3") == 0) inact_pnum = 3;
	else if (strcmp(def_val, "cross_weave") == 0) inact_pnum = 4;
    }

    def_val = XGetDefault(display, def_env, "PaneStyle");
    if (def_val != NULL) {
	if (strcmp(def_val, "flush_left") == 0) p_style = LEFT;
	else if (strcmp(def_val, "flush_right") == 0) p_style = RIGHT;
	else if (strcmp(def_val, "center") == 0) p_style = CENTER;
    }

    def_val = XGetDefault(display, def_env, "PaneFont");
    if (def_val != NULL) p_fnt_name = def_val;

    def_val = XGetDefault(display, def_env, "PaneForeground");
    if (
	def_val != NULL &&
	DisplayCells(display, DefaultScreen(display)) > 2 
	)
      XAllocNamedColor(display, DefaultColormap(display,
						DefaultScreen(display)),
		       def_val,
		       &p_frg_color, &color_def);
	  
    else if (reverse) XAllocNamedColor(display,
				       DefaultColormap(display, 
						       DefaultScreen(display)),
				       "white",
				       &p_frg_color, &color_def);
    else XAllocNamedColor(display,
			  DefaultColormap(display, DefaultScreen(display)),
			  "black",
			  &p_frg_color, &color_def);

    def_val = XGetDefault(display, def_env, "PaneBorder");
    if (
	def_val != NULL &&
	DisplayCells(display, DefaultScreen(display)) > 2 &&
	XAllocNamedColor(display,
			 DefaultColormap(display, DefaultScreen(display)),
			 def_val,
			 &p_bdr_color, &color_def)
	);
    else if (reverse &&
	     XAllocNamedColor(display, 
			      DefaultColormap(display, DefaultScreen(display)),
			      "white",
			      &p_bdr_color, &color_def)
	     );
    else XAllocNamedColor(display, 
			  DefaultColormap(display, DefaultScreen(display)),
			  "black",
			  &p_bdr_color, &color_def);
    
    def_val = XGetDefault(display, def_env, "PaneBorderWidth");
    if (def_val != NULL) p_bdr_width = atoi(def_val);
    
    def_val = XGetDefault(display, def_env, "PaneSpread");
    if (def_val != NULL) p_spread = atof(def_val);

    def_val = XGetDefault(display, def_env, "SelectionStyle");
    if (def_val != NULL) {
	if (strcmp(def_val, "flush_left") == 0) s_style = LEFT;
	else if (strcmp(def_val, "flush_right") == 0) s_style = RIGHT;
	else if (strcmp(def_val, "center") == 0) s_style = CENTER;
    }

    def_val = XGetDefault(display, def_env, "SelectionFont");
    if (def_val != NULL) s_fnt_name = def_val;

    def_val = XGetDefault(display, def_env, "SelectionForeground");
    if (
	def_val != NULL &&
	DisplayCells(display, DefaultScreen(display)) > 2 &&
	XAllocNamedColor(display,
			 DefaultColormap(display, DefaultScreen(display)),
			 def_val,
			 &s_frg_color, &color_def)
	); 
    else if (reverse &&
	     XAllocNamedColor(display,
			      DefaultColormap(display, DefaultScreen(display)),
			      "white",
			      &s_frg_color, &color_def)
	     ) ;
    else if (XAllocNamedColor(display,
			      DefaultColormap(display, DefaultScreen(display)),
			      "black",
			      &s_frg_color, &color_def)
	     ) ;
    else ;
    

    def_val = XGetDefault(display, def_env, "SelectionBorder");
    if (
	def_val != NULL &&
	DisplayCells(display, DefaultScreen(display)) > 2 &&
	XAllocNamedColor(display,
			 DefaultColormap(display, DefaultScreen(display)),
			 def_val,
			 &s_bdr_color, &color_def)
	) ;
    else if (reverse &&
	     XAllocNamedColor(display, 
			      DefaultColormap(display, DefaultScreen(display)),
			      "white",
			      &s_bdr_color, &color_def)
	     ) ;
    else if (XAllocNamedColor(display,
			      DefaultColormap(display, DefaultScreen(display)),
			      "black",
			      &s_bdr_color, &color_def)
	     ) ;
    else ;

    def_val = XGetDefault(display, def_env, "SelectionBorderWidth");
    if (def_val != NULL) s_bdr_width = atoi(def_val);
    
    def_val = XGetDefault(display, def_env, "SelectionSpread");
    if (def_val != NULL) s_spread = atof(def_val);

    /*
     * Create and store the inactive pattern pixmap.
     */
    switch (inact_pnum) 
      {
  case 0:
	  inact_bitmap = XCreateBitmapFromData(
					       display,
					       RootWindow(display, DefaultScreen(display)), 
					       dimple1_bits, 
					       16, 16
					       );
	  break;
  case 1:
	  inact_bitmap = XCreateBitmapFromData(
					       display, 
					       RootWindow(display, DefaultScreen(display)), 
					       dimple3_bits, 
					       16,16);
	  break;
  case 2:
	  inact_bitmap = XCreateBitmapFromData(
					       display, RootWindow(display, DefaultScreen(display)), 
					       gray1_bits, 16, 16);
	  break;
  case 3:
	  inact_bitmap = XCreateBitmapFromData(
					       display, 
					       RootWindow(display, DefaultScreen(display)), 
					       gray3_bits, 16, 16);
	  break;
  case 4:
	  inact_bitmap = XCreateBitmapFromData(
					       display, 
					       RootWindow(display, DefaultScreen(display)), 
					       cross_weave_bits, 16, 16);
	  break;
  default:
	  break;
	  
      }
    if (inact_bitmap == NULL) {
	_XMErrorCode = XME_STORE_BITMAP;
	return(NULL);
    }

    /*
     * Load the mouse cursor.
     */
	  
    switch (menu_style) {
        case LEFT:
	    cursor = XCreateBitmapFromData(display,
					   RootWindow(display,
						      DefaultScreen(display)),
					   left_ptr_bits,
					   left_ptr_width,
					   left_ptr_height);
	    cursor_mask = XCreateBitmapFromData(display,
					        RootWindow(display,
						      DefaultScreen(display)),
					        left_ptrmsk_bits,
					        left_ptrmsk_width,
					        left_ptrmsk_height);
	    mouse_cursor = XCreatePixmapCursor(
					       display,
					       cursor, cursor_mask, 
					       &mouse_color, &bkgnd_color,
					       left_ptr_x_hot,
					       left_ptr_y_hot
					       );
	    XFreePixmap(display, cursor);
	    XFreePixmap(display, cursor_mask);	    
	    break;
        case RIGHT:
	    cursor = XCreateBitmapFromData(display,
					   RootWindow(display,
						      DefaultScreen(display)),
					   right_ptr_bits,
					   right_ptr_width,
					   right_ptr_height);
	    cursor_mask = XCreateBitmapFromData(display,
					        RootWindow(display,
						      DefaultScreen(display)),
					        right_ptrmsk_bits,
					        right_ptrmsk_width,
					        right_ptrmsk_height);
	    mouse_cursor = XCreatePixmapCursor(
					       display,
					       cursor, cursor_mask,
					       &mouse_color, &bkgnd_color,
					       right_ptr_x_hot,
					       right_ptr_y_hot
					       );
	    XFreePixmap(display, cursor);
	    XFreePixmap(display, cursor_mask);	    
	    break;
        case CENTER:
	    cursor = XCreateBitmapFromData(display,
					   RootWindow(display,
						      DefaultScreen(display)),
					   cntr_ptr_bits,
					   cntr_ptr_width,
					   cntr_ptr_height);
	    cursor_mask = XCreateBitmapFromData(display,
					        RootWindow(display,
						      DefaultScreen(display)),
					        cntr_ptrmsk_bits,
					        cntr_ptrmsk_width,
					        cntr_ptrmsk_height);
	    mouse_cursor = XCreatePixmapCursor(
					       display,
					       cursor, cursor_mask,
					       &mouse_color, &bkgnd_color,
					       cntr_ptr_x_hot,
					       cntr_ptr_y_hot
					       );
	    XFreePixmap(display, cursor);
	    XFreePixmap(display, cursor_mask);	    
	break;
 default:
	/* Error! Invalid style parameter. */
	_XMErrorCode = XME_STYLE_PARAM;
	return(NULL);
    }
    if (mouse_cursor == _X_FAILURE) {
	_XMErrorCode = XME_CREATE_CURSOR;
	return(NULL);
    }

    /*
     * Open the pane and selection fonts.
     */
    
    p_fnt_info = XLoadQueryFont(display, p_fnt_name);
    if (p_fnt_info == NULL) {
	_XMErrorCode = XME_OPEN_FONT;
	return(NULL);
	
    }

    s_fnt_info = XLoadQueryFont(display, s_fnt_name);
    if (s_fnt_info == NULL) {
	_XMErrorCode = XME_OPEN_FONT;
	return(NULL);
    }
    /*
     * Calculate the fixed padding value in pixels for each font.
     */
    p_fnt_height = p_fnt_info->max_bounds.ascent + p_fnt_info->max_bounds.descent;
    s_fnt_height = s_fnt_info->max_bounds.ascent + s_fnt_info->max_bounds.descent;
    p_fnt_pad = s_spread * p_fnt_height;
    s_fnt_pad = s_spread * s_fnt_height;

    /*
     * Calculate fixed height and offset requirements.
     */
    flag_height = p_fnt_height + (p_fnt_pad << 1);

    p_height = 0;
    p_y_off = flag_height + p_bdr_width;
    p_x_off = p_y_off * p_spread;

    s_height = s_fnt_height + (s_fnt_pad << 1) + (s_bdr_width << 1);
    s_y_off = s_height;
    s_x_off = p_x_off;

    /*
     * Set up the pane list header.
     */
    pane->next = pane;
    pane->prev = pane;
    pane->type = PL_HEADER;
    pane->serial = -1;

    /*
     * Initialize the internal pane and selection creation queues.
     */
    _XMWinQueInit();
    
    /*
     * Create pane, active, and inactive GC's.
     */
    values = (XGCValues *)malloc(sizeof(XGCValues));
    valuemask = (GCForeground | GCBackground | GCFont | GCLineWidth);

    /*
     * First, pane.
     */

    values->foreground = p_frg_color.pixel;
    values->background = bkgnd_color.pixel;
    values->font = p_fnt_info->fid;
    values->line_width = p_bdr_width;
    
    pane_GC = XCreateGC(
			display, 
			RootWindow(display, DefaultScreen(display)),
			valuemask,
			values);
    /*
     * Then normal video selection.
     */

    values->foreground = s_frg_color.pixel;
    values->background = bkgnd_color.pixel;
    values->font = s_fnt_info->fid;
    values->line_width = s_bdr_width;
    normal_select_GC = XCreateGC(display, 
				 RootWindow(display, DefaultScreen(display)),
				 valuemask,
				 values);
    /*
     * Inverse video selection.
     */

    values->foreground = bkgnd_color.pixel;		
    values->background = s_frg_color.pixel;
    values->font = s_fnt_info->fid;
    values->line_width = s_bdr_width;
    inverse_select_GC = XCreateGC(display, 
				  RootWindow(display, DefaultScreen(display)),
				  valuemask,
				  values);
    stipple_pixmap = XCreateBitmapFromData(display,
					   RootWindow(display,
						      DefaultScreen(display)),
					   stipple_bits,		
					   stipple_width,		
					   stipple_height);	
    
    /*
     * Finally, inactive pane header and selections
     */
    valuemask |= (GCFillStyle | GCStipple);
    values->foreground = s_frg_color.pixel;
    values->background = bkgnd_color.pixel;
    values->font = s_fnt_info->fid;
    values->line_width = s_bdr_width;
    values->fill_style = FillStippled;
    values->stipple = stipple_pixmap;
	
    inact_GC = XCreateGC(display, 
			 RootWindow(display, DefaultScreen(display)),
			 valuemask,
			 values);

    valuemask |= (GCGraphicsExposures);
    values->graphics_exposures = False;
    inact_GC_noexpose = XCreateGC (display, 
    				   RootWindow (display, 
					       DefaultScreen (display)),
				   valuemask, values);


    /*
     * Construct the XMenu object.
     */
    /* -------------------- Menu data -------------------- */
    menu->menu_style = menu_style;
    menu->menu_mode = menu_mode;
    menu->freeze = freeze;
    menu->aeq = 0;
    menu->recompute = 1;
    menu->parent = parent;
    menu->height = 0;
    menu->width = 0;
    menu->mouse_cursor = mouse_cursor;
    menu->assoc_tab = assoc_tab;
    menu->p_list = pane;
    /* -------------------- Pane window data -------------------- */
    menu->p_style = p_style;
    menu->p_events = DEF_P_EVENTS;
    menu->p_fnt_info = p_fnt_info;
    menu->p_fnt_pad = p_fnt_pad;
    menu->p_spread = p_spread;
    menu->p_bdr_width = p_bdr_width;
    menu->flag_height = flag_height;
    menu->p_width = 0;
    menu->p_height = p_height;
    menu->p_x_off = p_x_off;
    menu->p_y_off = p_y_off;
    menu->p_count = 0;
    menu->pane_GC = pane_GC;
    menu->x_pos = 0;
    menu->y_pos = 0;
    /* -------------------- Selection window data -------------------- */
    menu->s_style = s_style;
    menu->s_events = DEF_S_EVENTS;
    menu->s_fnt_info = s_fnt_info;
    menu->s_fnt_pad = s_fnt_pad;
    menu->s_spread = s_spread;
    menu->s_bdr_width = s_bdr_width;		/* unnecessary */
    menu->s_width = 0;
    menu->s_height = s_height;
    menu->s_x_off = s_x_off;
    menu->s_y_off = s_y_off;
    menu->s_count = 0;
    menu->normal_select_GC = normal_select_GC;
    menu->inverse_select_GC = inverse_select_GC;
    menu->inact_GC = inact_GC;
    /* -------------------- Color data -------------------- */
    menu->p_bdr_color = p_bdr_color.pixel;
    menu->s_bdr_color = s_bdr_color.pixel;
    menu->p_frg_color = p_frg_color.pixel;
    menu->s_frg_color = s_frg_color.pixel;
    menu->bkgnd_color = bkgnd_color.pixel;
    /* -------------------- Pixmap data -------------------- */
    menu->p_bdr_pixmap = NULL;
    menu->s_bdr_pixmap = NULL;
    menu->p_frg_pixmap = NULL;
    menu->s_frg_pixmap = NULL;
    menu->bkgnd_pixmap = NULL;
    if (DisplayPlanes (display, DefaultScreen (display)) == 1)
	menu->inact_pixmap = inact_bitmap;
    else
    {
	int loop;
	
	menu->inact_pixmap = XCreatePixmap (display,
					    parent,
					    16,
					    16,
					    DisplayPlanes (display,
							   DefaultScreen (display)));
	if (menu->inact_pixmap == NULL)
	{
	    _XMErrorCode = XME_STORE_BITMAP;
	    return(NULL);
	}

	XCopyPlane (display, inact_bitmap, menu->inact_pixmap, 
		    inact_GC_noexpose, 0, 0, 16, 16, 0, 0, 1);

/*
	for (loop = 0;
	     loop < DisplayPlanes (display, DefaultScreen (display));
	     loop++)
	{
	    XCopyPlane (display,
			inact_bitmap,
			menu->inact_pixmap,
			inact_GC,
			0,
			0,
			16,
			16,
			0,
			0,
			(1 << loop));
	}
*/
    }

    /*
     * Return the completed XMenu.
     */
    _XMErrorCode = XME_NO_ERROR;
    return(menu);
}
