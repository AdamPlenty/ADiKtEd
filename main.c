/*
 * main.c   contains main() and initialization functions
 */

#include "globals.h"
#include "action.h"
#include "scr_help.h"
#include "var_utils.h"
#include "internal.h"

static void read_init(void);

/*
 * Main function; initializes variables, loads map from commandline
 * parameters and enters the main program loop.
 */
int main(int argc, char **argv)
{
    // variable that informs if main program loop should finish
    finished=false;
    // say the clipboard is empty to start with
    tngclipboard[2]=0;
    // create object for storing map
    level_init();
    // initialize slbkey array
    init_slbkey();
    // read help file
    init_help();
    // initialize keyboard shortcuts
    init_keys();
    // read configuration file
    read_init();
    // random num gen seed selection
    srand(time(0));
    // Interpreting command line parameters
    if (argc>=2)
    {
        //Loading map
        if (format_map_fname(lvl->fname,argv[1]))
        {
          load_map(lvl);
        } else
          start_new_map(lvl);
    } else
        start_new_map(lvl);
    input_init();
    screen_init();
    create_default_clm();
    check_doors();
    do 
    {
      draw_levscr();
      proc_key();
    } while (!finished);
    done();
    return 0;
}

static void read_init(void)
{
    // Do something else here
#if defined(unix) && !defined(GO32)
    filebase="levels";
#else
    filebase="levels";
#endif
    char buffer[READ_BUFSIZE];
    const char config_filename[]="map.ini";
    char *p;
    int l;
    
    FILE *fp;
    fp = fopen (config_filename, "rb");
    // If we can't get anything, warn but don't die
    if (!fp)
    {
      message_info_force("Couldn't open %s, defaults loaded.",config_filename);
      return;
    } else
    {
      message_info_force("Welcome to Adikted. Press F1 for help.");
    }
    while (fgets(buffer, READ_BUFSIZE-1, fp))
    {
      //Searchinf for a line containing equal sign
      p = strchr (buffer, '=');
      if (p==NULL)
          continue;
      strip_crlf(buffer);
      (*p)=0;
      p++;
      while ((*p==' ')||(*p=='\t')) p++;
      if (strchr (buffer, ' '))
          *strchr (buffer, ' ')=0;
      if (!strcmp(buffer, "datmode"))
          datmode=atoi(p);
      if (!strcmp(buffer, "filebase"))
      {
          filebase=strdup(p);
          if (strchr(filebase,' '))
            *strchr(filebase,' ')=0;
          l = strlen (filebase);
          if (l)
            if (filebase[l-1]==SEPARATOR[0])
                filebase[l-1]=0;
      }
    }
}
