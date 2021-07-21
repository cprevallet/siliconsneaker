/* 
 * This program provides a GTK graphical user interface to the 
 * astronomical almanac (aa) program written by S. L. Moshier, November, 1987.
 *
 * aa MUST be installed in order for this one to function.
 * On Debian systems it may be installed by "apt install astronomical-almanac"
 *
 * aa is Copyright (c) 2005 by Stephen L. Moshier <steve@moshier.net>.  
 * aa-gtk is Copyright (c) 2020 by Craig S. Prevallet <penguintx@hotmail.com>. 
 *
 * License:
 *
 * Permission is granted to copy, use, and distribute for any commercial
 * or noncommercial purpose in accordance with the requirements of
 * version 2.0 of the GNU General Public license.
 *
 * This package is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this package; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 *
 * On Debian systems, the complete text of the GNU General
 * Public License can be found in `/usr/share/common-licenses/GPL-2'.
 * 
 * - Craig S. Prevallet, July, 2020  
 */


#include <gtk/gtk.h>
#include <gdk/gdk.h>

/*
 * PLPlot
 */
#include <cairo.h>
#include <cairo-ps.h>
#include <plplot.h>


/* 
 * make UI elements globals (ick)
 */  
GtkTextBuffer *textbuffer1;
GtkSpinButton *sb_hr;
GtkSpinButton *sb_min;
GtkSpinButton *sb_sec;
GtkSpinButton *sb_int;
GtkSpinButton *sb_numint;
GtkSpinButton *sb_lat;
GtkSpinButton *sb_lng;
GtkSpinButton *sb_height;
GtkSpinButton *sb_temp;
GtkSpinButton *sb_press;
GtkSpinButton *sb_dt;
GtkCalendar *cal;
GtkComboBox *cb_planet;
GtkRadioButton *rb_TTUT;
GtkRadioButton *rb_TT;
GtkRadioButton *rb_UT;
GtkRadioButton *rb_planet;
GtkComboBox *cb_star;
GtkRadioButton *rb_star;
GtkEntry *ent_starcat;
GtkDrawingArea *da;

static char starnam[80] = "/usr/share/aa/star.cat";
/* static char starnam[80] = "/usr/share/aa/messier.cat"; */

/* Drawing area callback. */

   gboolean _da_draw_cb (GtkWidget *widget,
                    GdkEventExpose *event,
                    gpointer data) 
    {

        // "convert" the G*t*kWidget to G*d*kWindow (no, it's not a GtkWindow!)
        GdkWindow* window = gtk_widget_get_window(widget);  

        cairo_region_t * cairoRegion = cairo_region_create();

        GdkDrawingContext * drawingContext;
        drawingContext = gdk_window_begin_draw_frame (window,cairoRegion);

        { 
            // say: "I want to start drawing"
            cairo_t * cr = gdk_drawing_context_get_cairo_context (drawingContext);

            { // do your drawing
                plsdev( "extcairo" );
                plinit();
                pl_cmd( PLESC_DEVINIT, cr );
                plenv( 0.0, 1.0, 0.0, 1.0, 1, 0 );
                pllab( "x", "y", "title" );
                plend();

//                cairo_move_to(cr, 30, 30);
//                cairo_set_font_size(cr,15);
//                cairo_show_text(cr, "hello world");
            }

            // say: "I'm finished drawing
            gdk_window_end_draw_frame(window,drawingContext);
        }

        // cleanup
        cairo_region_destroy(cairoRegion);

        return FALSE;
    }

/* Read a star catalog. */
void populate_star() 
{
  FILE *f, *fopen();
  GtkListStore *liststore;
  char star[128];
  char buf[1024];
  int line = 0;

  /* Clear out any existing model first. */
  gtk_combo_box_set_model(cb_star, NULL);
  if ((f = fopen(gtk_entry_get_text(ent_starcat), "r"))) {
    /* Allocate space for an empty linked list. */
    liststore = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_INT);
    /* Read from the file and set the combobox "model" to the linked list. */
    while (fgets(buf, sizeof buf, f)) {
      if (sscanf(buf, "%*s %*s %*s %*s %*s %*s %*s %*s %*s %*s %*s %*s %s",
                 star) == 1) {
        line++;
        gtk_list_store_insert_with_values(liststore, NULL, -1, 0, star, 1, line,
                                          -1);
      } else {
      }
    }
    gtk_combo_box_set_model(cb_star, GTK_TREE_MODEL(liststore));
    /* liststore is now owned by combo, so the initial reference can
     * be dropped. 
     */
    g_object_unref(liststore);
  }
}

/* Store the GUI variables in user's home directory as ana.ini for use
 * with aa. 
 */
int store_ini() 
{
  FILE *fp, *fopen();
  char s[84];

  char *t = getenv("HOME");
  strcpy(s, "aa.ini");
  if (t && strlen(t) < 70) {
    strcpy(s, t);
    strcat(s, "/.aa.ini");
  }
  if ((fp = fopen(s, "w"))) {
    fprintf(fp, "%f ;Terrestrial east longitude of observer, degrees \n",
            gtk_spin_button_get_value(sb_lng));
    fprintf(fp, "%f ;Geodetic latitude, degrees\n",
            gtk_spin_button_get_value(sb_lat));
    fprintf(fp, "%f ;Height above sea level, meters\n",
            gtk_spin_button_get_value(sb_height));
    fprintf(fp, "%f ;Atmospheric temperature, deg C\n",
            gtk_spin_button_get_value(sb_temp));
    fprintf(fp, "%f ;Atmospheric pressure, millibars\n",
            gtk_spin_button_get_value(sb_press));
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(rb_TTUT))) 
      fprintf(fp, "%d ; 0 - TDT=UT, 1 - input=TDT, 2 - input=UT\n", 0);
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(rb_TT))) 
      fprintf(fp, "%d ; 0 - TDT=UT, 1 - input=TDT, 2 - input=UT\n", 1);
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(rb_UT))) 
      fprintf(fp, "%d ; 0 - TDT=UT, 1 - input=TDT, 2 - input=UT\n", 2);
    fprintf(fp, "%f ; Use this deltaT (sec) if nonzero, else compute it.\n",
            gtk_spin_button_get_value(sb_dt));
    fclose(fp);
    return 0;
  } else {
    return -1;
  }
  
}

/* Store other values aa expects in a temp file to stream to aa. */
int store_values() 
{
  guint year;
  guint mon;
  guint day;
  gint active_item = 0;
  FILE *fp;
  int file_line = 0;

  /* Retrieve values from various GUI widgets. */
  int hr = gtk_spin_button_get_value_as_int(sb_hr);
  int min = gtk_spin_button_get_value_as_int(sb_min);
  int sec = gtk_spin_button_get_value_as_int(sb_sec);
  float interval = gtk_spin_button_get_value(sb_int);
  int num_intervals = gtk_spin_button_get_value_as_int(sb_numint);
  gtk_calendar_get_date(cal, &year, &mon, &day);
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(rb_planet))) 
    active_item = gtk_combo_box_get_active(cb_planet);
  
  /* Entries from the star combobox, are way more complicated! */
  const gchar *catalog;
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(rb_star))) {
    active_item = 88;
    catalog = gtk_entry_get_text(ent_starcat);
    GtkTreeModel *tree_model = gtk_combo_box_get_model(cb_star);
    GtkTreeIter iter;
    gtk_combo_box_get_active_iter(cb_star, &iter);
    GValue linenum = G_VALUE_INIT;
    gtk_tree_model_get_value(tree_model, &iter, 1, &linenum);
    file_line = g_value_get_int(&linenum);
  }

  /* Create the file containing the values for aa to run. */
  fp = fopen("/tmp/aa.txt", "w+");
  if (fp) {
    fprintf(fp, "%d\n", year);
    fprintf(fp, "%d\n", mon + 1);
    fprintf(fp, "%d\n", day);
    fprintf(fp, "%d\n", hr);
    fprintf(fp, "%d\n", min);
    fprintf(fp, "%d\n", sec);
    fprintf(fp, "%f\n", interval);
    fprintf(fp, "%d\n", num_intervals);
    fprintf(fp, "%d\n", active_item);
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(rb_star))) {
      fprintf(fp, "%s\n", catalog);
      fprintf(fp, "%d\n", file_line);
    }
    fclose(fp);
    return 0;
  } else {
    return -1;
  }
}

/* Load the values stored in the ini file as initial widget values.
 * Initialize the calendar and time widgets with the current UTC values.
 */
int initialize_widgets() 
{
  time_t rawtime;
  struct tm *utc;
  FILE *f, *fopen();
  char s[84]; /* This is oddly specific??? */
  double tlong, glat, height, attemp, atpress, dtgiven;
  int jdflag;

  /* Set default time and date to GMT time. */
  time(&rawtime);
  utc = gmtime(&rawtime);
  gtk_spin_button_set_value(sb_hr, utc->tm_hour);
  gtk_spin_button_set_value(sb_min, utc->tm_min);
  gtk_spin_button_set_value(sb_sec, utc->tm_sec);
  gtk_calendar_select_month(cal, utc->tm_mon, utc->tm_year + 1900);
  gtk_calendar_select_day(cal, utc->tm_mday);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(rb_planet), TRUE);
  gtk_entry_set_text(ent_starcat, starnam);
  populate_star(); /* Read from star catalog. */
  
  char *t = getenv("HOME");
  strcpy(s, "aa.ini");
  if (t && strlen(t) < 70) {
    strcpy(s, t);
    strcat(s, "/.aa.ini");
  }
  if ((f = fopen(s, "r"))) {
    fgets(s, 80, f);
    sscanf(s, "%lf", &tlong);
    gtk_spin_button_set_value(sb_lng, tlong);
    fgets(s, 80, f);
    sscanf(s, "%lf", &glat);
    gtk_spin_button_set_value(sb_lat, glat);
    fgets(s, 80, f);
    sscanf(s, "%lf", &height);
    gtk_spin_button_set_value(sb_height, height);
    fgets(s, 80, f);
    sscanf(s, "%lf", &attemp);
    gtk_spin_button_set_value(sb_temp, attemp);
    fgets(s, 80, f);
    sscanf(s, "%lf", &atpress);
    gtk_spin_button_set_value(sb_press, atpress);
    fgets(s, 80, f);
    sscanf(s, "%d", &jdflag);
    switch (jdflag) {
    case 0:
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(rb_TTUT), TRUE);
      break;
    case 1:
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(rb_TT), TRUE);
      break;
    case 2:
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(rb_UT), TRUE);
      break;
    default:
      printf("Illegal jdflag\n");
      exit(2);
    }
    fgets(s, 80, f);
    sscanf(s, "%lf", &dtgiven);
    gtk_spin_button_set_value(sb_dt, dtgiven);
    fclose(f);
    return 0;
  }
  return -1; /* We should only get here if bad file read. */
}

/* Clear out the text buffer when clear button is pressed. */
void _clear_results(GtkButton *b) 
{
  GtkTextIter start;
  GtkTextIter end;
  gtk_text_buffer_get_bounds(textbuffer1, &start, &end);
  gtk_text_buffer_delete(textbuffer1, &start, &end);
}

/* Execute the aa program via a popen call passing the values it
 * expects via a file (written out by store_values and store_ini).
 */
void _on_clicked(GtkButton *b) 
{
  GtkTextMark *mark;
  GtkTextIter iter;
  FILE *fp;
  char line[1035]; /* Why this big??? */

  /* Save the widget values prior to running aa. */
  store_ini();
  store_values();

  /* Open a new process (as read) and run aa. 
   * popen pipe is by definition one-directional.
   * Passing the file in works around this limitation to pass
   * in program arguments.
   */
  fp = popen("/usr/bin/aa < /tmp/aa.txt", "r");
  if (fp == NULL) {
    printf("Failed to run command\n");
    printf("Astronomical almanac must be installed for this program to "
           "function.\n");
    exit(1);
  }

  /* Read the output from the aa process pipeline at a time and display it.
   * Filter out interactive user prompts from results.
   */
  while (fgets(line, sizeof(line), fp) != NULL) {
      if ( (strcspn(line, "?") == strlen(line)) &&          
           (strcmp(line, "Enter starting date of tabulation\n")) ) {
        mark = gtk_text_buffer_get_insert(textbuffer1);
        gtk_text_buffer_get_iter_at_mark(textbuffer1, &iter, mark);
        gtk_text_buffer_insert(textbuffer1, &iter, line, -1);
      }
  }
  pclose(fp);
}

/* This is the program entry point.  The builder reads an XML file (generated  
 * by the Glade application and instantiate the associated (global) objects.
 */
int main(int argc, char *argv[]) 
{
  GtkBuilder *builder;
  GtkWidget *window;

  gtk_init(&argc, &argv);

  builder = gtk_builder_new_from_file("gtkdraw.glade");

  window = GTK_WIDGET(gtk_builder_get_object(builder, "window1"));
  textbuffer1 = GTK_TEXT_BUFFER(gtk_builder_get_object(builder, "textbuffer1"));
  sb_hr = GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "sb_hr"));
  sb_min = GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "sb_min"));
  sb_sec = GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "sb_sec"));
  sb_int = GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "sb_int"));
  sb_numint = GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "sb_numint"));
  sb_lat = GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "sb_lat"));
  sb_lng = GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "sb_lng"));
  sb_height = GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "sb_height"));
  sb_temp = GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "sb_temp"));
  sb_press = GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "sb_press"));
  sb_dt = GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "sb_dt"));
  cal = GTK_CALENDAR(gtk_builder_get_object(builder, "cal1"));
  cb_planet = GTK_COMBO_BOX(gtk_builder_get_object(builder, "cb_planet"));
  rb_TTUT = GTK_RADIO_BUTTON(gtk_builder_get_object(builder, "rb_TTUT"));
  rb_TT = GTK_RADIO_BUTTON(gtk_builder_get_object(builder, "rb_TT"));
  rb_UT = GTK_RADIO_BUTTON(gtk_builder_get_object(builder, "rb_UT"));
  rb_planet = GTK_RADIO_BUTTON(gtk_builder_get_object(builder, "rb_planet"));
  rb_star = GTK_RADIO_BUTTON(gtk_builder_get_object(builder, "rb_star"));
  cb_star = GTK_COMBO_BOX(gtk_builder_get_object(builder, "cb_star"));
  ent_starcat = GTK_ENTRY(gtk_builder_get_object(builder, "starcat"));
  da = GTK_DRAWING_AREA(gtk_builder_get_object(builder, "da"));

  gtk_builder_connect_signals(builder, NULL);

  /* Retrieve initial values from .ini file.*/ 
  initialize_widgets();

  g_object_unref(builder);

  gtk_widget_show(window);
  gtk_main();

  return 0;
}

/* Call when the window is closed.  Store GUI values before exiting.*/
void on_window1_destroy() 
{
  store_ini();
  gtk_main_quit();
}
