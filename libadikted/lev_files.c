/******************************************************************************/
// lev_files.c - Another Dungeon Keeper Map Editor.
/******************************************************************************/
// Author:   Jon Skeet
// Created:  14 Oct 1997
// Modified: Tomasz Lis

// Purpose:
//   Defines functions for loading and writing levels from/to disk.

// Comment:
//   None.

//Copying and copyrights:
//   This program is free software; you can redistribute it and/or modify
//   it under the terms of the GNU General Public License as published by
//   the Free Software Foundation; either version 2 of the License, or
//   (at your option) any later version.
/******************************************************************************/

#include "lev_files.h"

#include "globals.h"
#include "arr_utils.h"
#include "memfile.h"
#include "obj_column_def.h"
#include "obj_slabs.h"
#include "obj_things.h"
#include "bulcommn.h"
//#include "scr_actn.h"
#include "obj_column.h"
#include "lev_data.h"
#include "lev_script.h"

char *load_error(int errcode)
{
    //static char str[LINEMSG_SIZE];
    static char *const errors[] = {
	"File too small",
	"Bad data",
	"Cant alocate mem",
	"Internal error",
	"Map rejected",
    };
    static char *const warnings[] = {
	"Bad items count",
	"Unknown problem",
    };
    // Note: remember that errors are negative, warnings positive
    if ((errcode>ERR_FILE_TOOSMLL)&&(errcode<WARN_BAD_COUNT))
    {
        return read_file_error(errcode);
    }
    if (errcode<=ERR_FILE_TOOSMLL)
    {
        errcode = ERR_FILE_TOOSMLL-errcode;
        //sprintf(str,"Error code %d",errcode);
        //return str;
        const int max_code=(sizeof(errors)/sizeof(*errors) - 1);
        if ((errcode < 0) || (errcode > max_code) )
          errcode = max_code;
        return errors[errcode];
   }
    if (errcode>=WARN_BAD_COUNT)
    {
        errcode = WARN_BAD_COUNT-errcode;
        const int max_code=(sizeof(warnings)/sizeof(*warnings) - 1);
        //sprintf(str,"Warning code %d",errcode);
        //return str;
        if ((errcode < 0) || (errcode > max_code) )
          errcode = max_code;
        return warnings[errcode];
   }
}

/*
 * Old way of reading various files; not used anymore
 */
short load_subtile(unsigned char **dest,
                        char *fname, int length, int x, int y,
                        int linesize, int nlines, int lineoffset,
                        int mbytes, int byteoffset)
{
    struct MEMORY_FILE mem;
    int i, j;
    long addr=0;

    mem = read_file(fname);
    if (mem.errcode!=MFILE_OK)
      return false;
    if ((mem.len<mbytes*nlines*x*y) || (mem.len!=length))
    {
      free(mem.content);
      return false;
    }
    for (i=0; i < y; i++)
    {
      addr += linesize*lineoffset;
      for (j=0; j < x; j++)
          dest[j][i]=mem.content[addr+j*mbytes+byteoffset];
      addr += (nlines-lineoffset)*linesize;
    }
    free(mem.content);
    return true;
}

/*
 * Old way of reading various files; not used anymore
 */
unsigned char **load_subtile_malloc(char *fname, int length, 
                        int x, int y,
                        int linesize, int nlines, int lineoffset,
                        int mbytes, int byteoffset)
{
    // Allocating mem
    unsigned char **dest;
    dest = (unsigned char **)malloc(x*sizeof(char *));
    if (dest==NULL)
    {
        message_error("load_subtile: Out of memory");
        return NULL;
    }
    int i;
    for (i=0; i < x; i++)
    {
      dest[i]=(unsigned char *)malloc(y*sizeof(char));
      if (dest[i]==NULL)
      {
        message_error("load_subtile: Out of memory");
        return NULL;
      }
    }
    // Loading
    short result=load_subtile(dest,fname,length,x,y,
        linesize,nlines,lineoffset,mbytes,byteoffset);
    if (result)
      return dest;
    // Freeing on error
    for (i=0; i < x; i++)
      free(dest[i]);
    free(dest);
    return NULL;
}


short load_tng(struct LEVEL *lvl,char *fname)
{
    message_log("  load_tng: started");
    struct MEMORY_FILE mem;
    int tng_num;
    int i, j;
    unsigned char *thing;
    if (lvl==NULL) return ERR_INTERNAL;
    mem = read_file (fname);
    if (mem.errcode!=MFILE_OK) return mem.errcode;
    if (mem.len<SIZEOF_DK_TNG_HEADER)
    { free(mem.content); return ERR_FILE_TOOSMLL; }
    short result=ERR_NONE;
    //Read the header
    tng_num = read_short_le_buf(mem.content);
    // Check everything's cushty
    unsigned long expect_size=tng_num*SIZEOF_DK_TNG_REC+SIZEOF_DK_TNG_HEADER;
    if (mem.len != expect_size)
    {
        message_log("  load_tng: File length %d, expected %lu (%d things)",mem.len,expect_size,tng_num);
        // Fixing the problem
        if (((lvl->optns.load_redundant_objects&EXLD_THING)==EXLD_THING)||(mem.len < expect_size))
          tng_num=(mem.len-SIZEOF_DK_TNG_HEADER)/SIZEOF_DK_TNG_REC;
        result=WARN_BAD_COUNT;
    }
    //Read tng entries
    for (i=0; i < tng_num; i++)
    {
      unsigned char *thing = create_thing_empty();
      int offs=SIZEOF_DK_TNG_REC*i+SIZEOF_DK_TNG_HEADER;
      memcpy(thing, mem.content+offs, SIZEOF_DK_TNG_REC);
      thing_add(lvl,thing);
    }
    if (tng_num != lvl->tng_total_count)
    {
        message_error("Internal error in load_tng: tng_num=%d tng_total=%d", tng_num, lvl->tng_total_count);
        return ERR_INTERNAL;
    }
    free (mem.content);
    return result;
}

short load_clm(struct LEVEL *lvl,char *fname)
{
    message_log("  load_clm: started");
    struct MEMORY_FILE mem;
    int i, j;
    if ((lvl==NULL)||(lvl->clm==NULL)) return ERR_INTERNAL;
    mem = read_file(fname);
    if (mem.errcode!=MFILE_OK) return mem.errcode;
    if (mem.len < SIZEOF_DK_CLM_HEADER)
    { free(mem.content); return ERR_FILE_TOOSMLL; }
    memcpy(lvl->clm_hdr, mem.content+0, SIZEOF_DK_CLM_HEADER);
    int num_clms=read_long_le_buf(mem.content+0);
    if (mem.len != SIZEOF_DK_CLM_REC*num_clms+SIZEOF_DK_CLM_HEADER)
    { free(mem.content); return ERR_FILE_BADDATA; }
    if (num_clms>COLUMN_ENTRIES)
    { free(mem.content); return ERR_FILE_BADDATA; }
    for (i=0; i<num_clms; i++)
    {
      int offs=SIZEOF_DK_CLM_REC*i+SIZEOF_DK_CLM_HEADER;
      memcpy(lvl->clm[i], mem.content+offs, SIZEOF_DK_CLM_REC);
    }
    free (mem.content);
    return ERR_NONE;
}

/*
 * Loads the APT file fname, fills LEVEL apt entries
 * This _must_ be called _after_ tng_* are set up
 */
short load_apt(struct LEVEL *lvl,char *fname)
{
    message_log("  load_apt: started");
    struct MEMORY_FILE mem;
    int i;
    unsigned char *actnpt;
    
    if ((lvl==NULL)||(lvl->apt_lookup==NULL)) return ERR_INTERNAL;
    mem = read_file (fname);
    if (mem.errcode!=MFILE_OK) return mem.errcode;
    if (mem.len < SIZEOF_DK_APT_HEADER)
    { free(mem.content); return ERR_FILE_TOOSMLL; }
    short result=ERR_NONE;
    long apt_num;
    apt_num = read_long_le_buf(mem.content+0);
    // Check everything's cushty
    unsigned long expect_size=apt_num*SIZEOF_DK_APT_REC+SIZEOF_DK_APT_HEADER;
    if (mem.len != expect_size)
    {
        message_log("  load_apt: File length %d, expected %lu (%d items)",mem.len,expect_size,apt_num);
        // Fixing the problem
        if (((lvl->optns.load_redundant_objects&EXLD_ACTNPT)==EXLD_ACTNPT)||(mem.len < expect_size))
          apt_num=(mem.len-SIZEOF_DK_APT_HEADER)/SIZEOF_DK_APT_REC;
        result=WARN_BAD_COUNT;
    }
    for (i=0; i < apt_num; i++)
    {
      actnpt=(unsigned char *)malloc(SIZEOF_DK_APT_REC);
      if (actnpt==NULL)
      {
        message_error("Cannot allocate mem for loading action points");
        return ERR_CANT_MALLOC;
      }
      memcpy (actnpt, mem.content+SIZEOF_DK_APT_REC*i+SIZEOF_DK_APT_HEADER, SIZEOF_DK_APT_REC);
      actnpt_add(lvl,actnpt);
    }
    if (apt_num != lvl->apt_total_count)
    {
        message_error("Internal error in load_apt: apt_num=%d apt_total=%d", apt_num, lvl->apt_total_count);
        return ERR_INTERNAL;
    }
    free (mem.content);
    return result;
}

short load_inf(struct LEVEL *lvl,char *fname)
{
    message_log("  load_inf: started");
    struct MEMORY_FILE mem;
    mem = read_file(fname);
    //If wrong filesize - pannic
    if (mem.errcode!=MFILE_OK) return mem.errcode;
    if (mem.len != 1)
    { free(mem.content); return ERR_FILE_BADDATA; }
    lvl->inf=mem.content[0];
    free (mem.content);
    return ERR_NONE;
}

short load_wib(struct LEVEL *lvl,char *fname)
{
    message_log("  load_wib: started");
    //Preparing array bounds
    const int dat_entries_x=MAP_SIZE_X*MAP_SUBNUM_X+1;
    const int dat_entries_y=MAP_SIZE_Y*MAP_SUBNUM_Y+1;
    //Loading the file
    struct MEMORY_FILE mem;
    mem = read_file(fname);
    if (mem.errcode!=MFILE_OK) return mem.errcode;
    if ((mem.len!=dat_entries_x*dat_entries_y))
    { free(mem.content); return ERR_FILE_BADDATA; }
    //Reading WIB entries
    int sx, sy;
    unsigned long addr;
    for (sy=0; sy<dat_entries_y; sy++)
    {
      addr = sy*dat_entries_x;
      for (sx=0; sx<dat_entries_x; sx++)
      {
          set_subtl_wib(lvl,sx,sy,mem.content[addr+sx]);
      }
    }
    free(mem.content);
    return ERR_NONE;
  // Old way
  //return load_subtile(lvl->wib, fname, 65536, arr_entries_x, arr_entries_y,256, 1, 0, 1, 0);
}

short load_slb(struct LEVEL *lvl,char *fname)
{
    message_log("  load_slb: started");
    //Reading file
    struct MEMORY_FILE mem;
    mem = read_file(fname);
    if (mem.errcode!=MFILE_OK) return mem.errcode;
    if ((mem.len!=2*MAP_SIZE_X*MAP_SIZE_Y))
    { free(mem.content); return ERR_FILE_BADDATA; }
    //Loading the entries
    int i, k;
    unsigned long addr=0;
    for (i=0; i<MAP_SIZE_Y; i++)
    {
      addr = 2*MAP_SIZE_X*i;
      for (k=0; k<MAP_SIZE_X; k++)
          set_tile_slab(lvl,k,i,read_short_le_buf(mem.content+addr+k*2));
    }
    free(mem.content);
    return ERR_NONE;
    //The old way - left as an antic
    //return load_subtile(, fname, 14450, MAP_SIZE_Y, MAP_SIZE_X,170, 1, 0, 2, 0);
}

short load_own(struct LEVEL *lvl,char *fname)
{
    message_log("  load_own: started");
    //Preparing array bounds
    int dat_entries_x=MAP_SIZE_X*MAP_SUBNUM_X+1;
    int dat_entries_y=MAP_SIZE_Y*MAP_SUBNUM_Y+1;
    //Loading the file
    struct MEMORY_FILE mem;
    mem = read_file(fname);
    if (mem.errcode!=MFILE_OK) return mem.errcode;
    if ((mem.len!=dat_entries_x*dat_entries_y))
    { free(mem.content); return ERR_FILE_BADDATA; }
    //Reading entries
    int sx, sy;
    unsigned long addr;
    for (sy=0; sy<dat_entries_y; sy++)
    {
      addr = sy*dat_entries_x;
      for (sx=0; sx<dat_entries_x; sx++)
      {
          set_subtl_owner(lvl,sx,sy,mem.content[addr+sx]);
      }
    }
    free(mem.content);
    return ERR_NONE;
    //Old way
    //return load_subtile(lvl->own, fname, 65536, MAP_SIZE_Y, MAP_SIZE_X,256, 3, 0, 3, 0); 
}

short load_dat(struct LEVEL *lvl,char *fname)
{
    message_log("  load_dat: started");
    //Preparing array bounds
    const int dat_entries_x=MAP_SIZE_X*MAP_SUBNUM_X+1;
    const int dat_entries_y=MAP_SIZE_Y*MAP_SUBNUM_Y+1;
    const unsigned int line_len=2*dat_entries_x;
    //Loading the file
    struct MEMORY_FILE mem;
    mem = read_file(fname);
    if (mem.errcode!=MFILE_OK) return mem.errcode;
    if ((mem.len!=line_len*dat_entries_y))
    { free(mem.content); return ERR_FILE_BADDATA; }
    //Reading DAT entries
    int sx, sy;
    unsigned long addr;
    for (sy=0; sy<dat_entries_y; sy++)
    {
      addr = sy*line_len;
      for (sx=0; sx<dat_entries_x; sx++)
      {
          set_dat_val(lvl,sx,sy,read_short_le_buf(mem.content+addr+sx*2));
      }
    }
    free(mem.content);
    return ERR_NONE;
}

short load_txt(struct LEVEL *lvl,char *fname)
{
    message_log("  load_txt: started");
    struct MEMORY_FILE mem;
    mem = read_file(fname);
//    message_log("  load_txt: file readed");
    //If filesize too small - pannic
    lvl->script.lines_count=0;
    if (mem.errcode!=MFILE_OK) return mem.errcode;
    if (mem.len < 2)
    { free(mem.content); return ERR_FILE_TOOSMLL; }
    unsigned char *content=mem.content;
    unsigned char *ptr=mem.content;
    unsigned char *ptr_end=mem.content+mem.len;
    int lines_count=0;
//    message_log("  load_txt: counting lines");
    while (ptr>=content)
    {
      ptr=memchr(ptr, 0x0a, (char *)ptr_end-(char *)ptr );
      lines_count++;
      if (ptr!=NULL) ptr++;
    }
    lvl->script.txt=(char **)malloc(lines_count*sizeof(unsigned char *));
    lvl->script.list=(struct DK_SCRIPT_COMMAND **)malloc(lines_count*sizeof(struct DK_SCRIPT_COMMAND *));
    ptr=mem.content;
    int currline;
    currline=0;
//    message_log("  load_txt: breaking text into %d lines",lines_count);
    while (currline<lines_count)
    {
      if (ptr>=ptr_end) ptr=ptr_end-1;
      unsigned char *nptr=memchr(ptr, 0x0a, ptr_end-ptr );
      //Skip control characters (but leave spaces and TABs)
      while ((ptr<nptr)&&((unsigned char)ptr[0]<0x20)&&((unsigned char)ptr[0]!=0x09)) ptr++;
      if (nptr==NULL)
        nptr=ptr_end;
      int linelen=(char *)nptr-(char *)ptr;
      //At end, skip control characters and spaces too
      while ((linelen>0)&&((unsigned char)ptr[linelen-1]<=0x20)) linelen--;
      lvl->script.txt[currline]=(unsigned char *)malloc((linelen+1)*sizeof(unsigned char));
      lvl->script.list[currline]=NULL; // decompose_script() will allocate memory for it
      memcpy(lvl->script.txt[currline],ptr,linelen);
      lvl->script.txt[currline][linelen]='\0';
      ptr=nptr+1;
      currline++;
    }
//    message_log("  load_txt: deleting empty lines");
    int nonempty_lines=lines_count-1;
    // Delete empty lines at end
    while ((nonempty_lines>=0)&&((lvl->script.txt[nonempty_lines][0])=='\0'))
      nonempty_lines--;
    currline=lines_count-1;
    while (currline>nonempty_lines)
    {
      free(lvl->script.txt[currline]);
      currline--;
    }
    lines_count=nonempty_lines+1;
    lvl->script.txt=(char **)realloc(lvl->script.txt,lines_count*sizeof(unsigned char *));
    lvl->script.list=(struct DK_SCRIPT_COMMAND **)realloc(lvl->script.list,lines_count*sizeof(struct DK_SCRIPT_COMMAND *));
    lvl->script.lines_count=lines_count;
    free (mem.content);
    decompose_script(&(lvl->script),&(lvl->optns.script));
    script_decomposed_to_params(&(lvl->script),&(lvl->optns.script));
    return ERR_NONE;
}

/*
 * Loads the LGT file fname, fills LEVEL light entries
 * This _must_ be called _after_ tng_* are set up
 */
short load_lgt(struct LEVEL *lvl,char *fname)
{
    message_log("  load_lgt: started");
    struct MEMORY_FILE mem;
    unsigned char *stlight;
    
    if ((lvl==NULL)||(lvl->lgt_lookup==NULL))
      return ERR_INTERNAL;
    mem = read_file (fname);
    if (mem.errcode!=MFILE_OK) return mem.errcode;
    if (mem.len < SIZEOF_DK_LGT_HEADER)
    { free(mem.content); return ERR_FILE_TOOSMLL; }

    short result=ERR_NONE;
    lvl->lgt_total_count=0;
    long lgt_num;
    lgt_num = read_long_le_buf(mem.content+0);
    unsigned long expect_size=lgt_num*SIZEOF_DK_LGT_REC+SIZEOF_DK_LGT_HEADER;
    // Check everything's cushty
    if (mem.len != expect_size)
    {
        message_log("  load_lgt: File length %d, expected %lu (%d items)",mem.len,expect_size,lgt_num);
        // Fixing the problem
        if (((lvl->optns.load_redundant_objects&EXLD_STLGHT)==EXLD_STLGHT)||(mem.len < expect_size))
          lgt_num=(mem.len-SIZEOF_DK_LGT_HEADER)/SIZEOF_DK_LGT_REC;
        result=WARN_BAD_COUNT;
    }
    int i;
    for (i=0; i<lgt_num; i++)
    {
      stlight=(unsigned char *)malloc(SIZEOF_DK_LGT_REC);
      if (stlight==NULL)
      {
        message_error("Cannot allocate mem for loading static lights");
        return ERR_CANT_MALLOC;
      }
      memcpy (stlight, mem.content+SIZEOF_DK_LGT_REC*i+SIZEOF_DK_LGT_HEADER, SIZEOF_DK_LGT_REC);
      stlight_add(lvl,stlight);
    }
    if (lgt_num != lvl->lgt_total_count)
    {
        message_error("Internal error in load_lgt: lgt_num=%d lgt_total=%d", lgt_num, lvl->lgt_total_count);
        return ERR_INTERNAL;
    }
    free (mem.content);
    return result;
}

/*
 * Loads WLB file.
 * WLB seems to be unused by the game, but are always written by BF editor.
 * DK loads this file when starting a level.
 */
short load_wlb(struct LEVEL *lvl,char *fname)
{
    message_log("  load_wlb: started");
    struct MEMORY_FILE mem;
    mem = read_file(fname);
    //If wrong filesize - don't load
    if (mem.errcode!=MFILE_OK) return mem.errcode;
    if (mem.len != MAP_SIZE_X*MAP_SIZE_Y)
    { free(mem.content); return ERR_FILE_BADDATA; }
    int i,j;
    for (i=0;i<MAP_SIZE_Y;i++)
      for (j=0;j<MAP_SIZE_X;j++)
      {
        int mempos=i*MAP_SIZE_X+j;
        lvl->wlb[j][i]=mem.content[mempos];
      }
    free (mem.content);
    return ERR_NONE;
}

/*
 * Loads the FLG file.
 * These seems to have small priority, but DK loads it when starting a level.
 */
short load_flg(struct LEVEL *lvl,char *fname)
{
    message_log("  load_flg: started");
    //Preparing array bounds
    const int dat_entries_x=MAP_SIZE_X*MAP_SUBNUM_X+1;
    const int dat_entries_y=MAP_SIZE_Y*MAP_SUBNUM_Y+1;
    const unsigned int line_len=2*dat_entries_x;
    //Loading the file
    struct MEMORY_FILE mem;
    mem = read_file(fname);
    if (mem.errcode!=MFILE_OK) return mem.errcode;
    if ((mem.len!=line_len*dat_entries_y))
    { free(mem.content); return ERR_FILE_BADDATA; }
    //Reading entries
    int sx, sy;
    unsigned long addr;
    for (sy=0; sy<dat_entries_y; sy++)
    {
      addr = sy*line_len;
      for (sx=0; sx<dat_entries_x; sx++)
      {
          set_subtl_flg(lvl,sx,sy,read_short_le_buf(mem.content+addr+sx*2));
      }
    }
    free(mem.content);
    return ERR_NONE;
}

/*
 * Loads ADiKtEd script and executes all commands
 */
unsigned short script_load_and_execute(struct LEVEL *lvl,char *fname,char *err_msg)
{
    message_log(" script_load_and_execute: started");
    sprintf(err_msg,"No error");
    struct MEMORY_FILE mem;
    mem = read_file(fname);
    if (mem.errcode!=MFILE_OK) return mem.errcode;
    if (mem.len < 2)
    { free(mem.content); return ERR_FILE_TOOSMLL; }
    unsigned char *content=mem.content;
    unsigned char *ptr=mem.content;
    unsigned char *ptr_end=mem.content+mem.len;
    int lines_count=0;
    while (ptr>=content)
    {
      ptr=memchr(ptr, 0x0a, (char *)ptr_end-(char *)ptr );
      lines_count++;
      if (ptr!=NULL) ptr++;
    }
    ptr=mem.content;
    int currline=0;
    int result=ERR_NONE;
    char *line;
    while (currline<lines_count)
    {
      if (ptr>=ptr_end) ptr=ptr_end-1;
      unsigned char *nptr=memchr(ptr, 0x0a, ptr_end-ptr );
      //Skip control characters (but leave spaces and TABs)
      while ((ptr<nptr)&&((unsigned char)ptr[0]<0x20)&&((unsigned char)ptr[0]!=0x09)) ptr++;
      if (nptr==NULL)
        nptr=ptr_end;
      int linelen=(char *)nptr-(char *)ptr;
      //At end, skip control characters and spaces too
      while ((linelen>0)&&((unsigned char)ptr[linelen-1]<=0x20)) linelen--;

      line=(unsigned char *)malloc((linelen+1)*sizeof(unsigned char *));
      memcpy(line,ptr,linelen);
      line[linelen]='\0';
      short res=execute_script_line(lvl,line,err_msg);
      if (!res)
      {
        sprintf(err_msg+strlen(err_msg),", line %d",currline+1);
        result=ERR_FILE_BADDATA;
      }
      free(line);
      ptr=nptr+1;
      currline++;
    }
    free (mem.content);
    return result;
}

short write_slb(struct LEVEL *lvl,char *fname)
{
    message_log(" write_slb: starting");
    FILE *fp;
    int i, k;
    fp = fopen (fname, "wb");
    if (!fp)
    {
      message_error("Can't open \"%s\" for writing", fname);
      return false;
    }
    for (k=0; k < MAP_SIZE_Y; k++)
    {
      for (i=0; i < MAP_SIZE_X; i++)
      {
          write_short_le_file(fp,get_tile_slab(lvl,i,k));
      }
    }
    fclose (fp);
    return true;
}

short write_own(struct LEVEL *lvl,char *fname)
{
    message_log(" write_own: starting");
    //Preparing array bounds
    const int dat_entries_x=MAP_SIZE_X*MAP_SUBNUM_X+1;
    const int dat_entries_y=MAP_SIZE_Y*MAP_SUBNUM_Y+1;
    //Opening
    FILE *fp;
    fp = fopen (fname, "wb");
    if (!fp)
    {
      message_error("Can't open \"%s\" for writing", fname);
      return false;
    }
    //Writing data
    int sx,sy;
    for (sy=0; sy<dat_entries_y; sy++)
    {
      for (sx=0; sx<dat_entries_x; sx++)
      {
          fputc (get_subtl_owner(lvl,sx,sy), fp);
      }
    }
    fclose (fp);
    return true;
}

short write_dat(struct LEVEL *lvl,char *fname)
{
    message_log(" write_dat: starting");
    //Preparing array bounds
    const int dat_entries_x=MAP_SIZE_X*MAP_SUBNUM_X+1;
    const int dat_entries_y=MAP_SIZE_Y*MAP_SUBNUM_Y+1;
    const unsigned int line_len=2*dat_entries_x;

    FILE *fp;
    fp = fopen (fname, "wb");
    if (!fp)
    {
      message_error("Can't open \"%s\" for writing", fname);
      return false;
    }
    //Writing data
    int sx,sy;
    for (sy=0; sy<dat_entries_y; sy++)
    {
      for (sx=0; sx<dat_entries_x; sx++)
      {
          write_short_le_file(fp,get_dat_val(lvl,sx,sy));
      }
    }
    fclose (fp);
    return true;
}

short write_flg(struct LEVEL *lvl,char *fname)
{
    message_log(" write_flg: starting");
    //Preparing array bounds
    const int dat_entries_x=MAP_SIZE_X*MAP_SUBNUM_X+1;
    const int dat_entries_y=MAP_SIZE_Y*MAP_SUBNUM_Y+1;
    const unsigned int line_len=2*dat_entries_x;

    FILE *fp;
    fp = fopen (fname, "wb");
    if (!fp)
    {
      message_error("Can't open \"%s\" for writing", fname);
      return false;
    }
    //Writing data
    int sx,sy;
    for (sy=0; sy<dat_entries_y; sy++)
    {
      for (sx=0; sx<dat_entries_x; sx++)
      {
          write_short_le_file(fp,get_subtl_flg(lvl,sx,sy));
      }
    }
    fclose (fp);
    return true;
}

short write_clm(struct LEVEL *lvl,char *fname)
{
    message_log(" write_clm: starting");
    FILE *fp;
    int i;
    fp = fopen (fname, "wb");
    if (!fp)
    {
      message_error("Can't open \"%s\" for writing", fname);
      return false;
    }
    write_long_le_buf(lvl->clm_hdr+0,COLUMN_ENTRIES);
    fwrite(lvl->clm_hdr, SIZEOF_DK_CLM_HEADER, 1, fp);
    for (i=0; i<COLUMN_ENTRIES; i++)
      fwrite(lvl->clm[i], SIZEOF_DK_CLM_REC, 1, fp);
    fclose (fp);
    return true;
}

/*
 * Saves WIB file.
 */
short write_wib(struct LEVEL *lvl,char *fname)
{
    message_log(" write_wib: starting");
    //Preparing array bounds
    int dat_entries_x=MAP_SIZE_X*MAP_SUBNUM_X+1;
    int dat_entries_y=MAP_SIZE_Y*MAP_SUBNUM_Y+1;

    FILE *fp;
    fp = fopen (fname, "wb");
    if (!fp)
    {
      message_error("Can't open \"%s\" for writing", fname);
      return false;
    }
    int i, j;
    for (i=0; i < dat_entries_y; i++)
    {
      for (j=0; j<dat_entries_x; j++)
          fputc (get_subtl_wib(lvl,j,i), fp);
    }
    fclose (fp);
    return true;
}

/*
 * Saves APT file.
 */
short write_apt(struct LEVEL *lvl,char *fname)
{
    message_log(" write_apt: starting");
    //Preparing array bounds
    int arr_entries_x=MAP_SIZE_X*MAP_SUBNUM_X;
    int arr_entries_y=MAP_SIZE_Y*MAP_SUBNUM_Y;

    FILE *fp;
    fp = fopen (fname, "wb");
    if (!fp)
    {
      message_error("Can't open \"%s\" for writing", fname);
      return false;
    }
    write_long_le_file (fp,lvl->apt_total_count);
    int cy, cx, k;
    for (cy=0; cy<arr_entries_y; cy++)
    {
      for (cx=0; cx<arr_entries_x; cx++)
      {
          int num_subs=get_actnpt_subnums(lvl,cx,cy);
          for (k=0; k<num_subs; k++)
          {
                char *actnpt=get_actnpt(lvl,cx,cy,k);
                fwrite (actnpt, SIZEOF_DK_APT_REC, 1, fp);
          }
      }
    }
    fclose (fp);
    return true;
}

/*
 * Saves TNG file.
 */
short write_tng(struct LEVEL *lvl,char *fname)
{
    message_log(" write_tng: starting");
    //Preparing array bounds
    int arr_entries_x=MAP_SIZE_X*MAP_SUBNUM_X;
    int arr_entries_y=MAP_SIZE_Y*MAP_SUBNUM_Y;

    FILE *fp;
    int cx, cy, k;
    fp = fopen (fname, "wb");
    if (!fp)
    {
      message_error("Can't open \"%s\" for writing", fname);
      return false;
    }
    //Header
    write_short_le_file(fp, lvl->tng_total_count);
    //Entries
    for (cy=0; cy < arr_entries_y; cy++)
      for (cx=0; cx < arr_entries_x; cx++)
          for (k=0; k < get_thing_subnums(lvl,cx,cy); k++)
                fwrite (get_thing(lvl,cx,cy,k), SIZEOF_DK_TNG_REC, 1, fp);
    fclose (fp);
    return true;
}

/*
 * Saves INF file. One byte file - the easy one.
 */
short write_inf(struct LEVEL *lvl,char *fname)
{
    message_log(" write_inf: starting");
    FILE *fp;
    int i, j, k;
    fp = fopen (fname, "wb");
    if (!fp)
    {
      message_error("Can't open \"%s\" for writing", fname);
      return false;
    }
    fputc ((lvl->inf) & 255, fp);
    fclose (fp);
    return true;
}

/*
 * Saves TXT file.
 */
short write_txt(struct LEVEL *lvl,char *fname)
{
    message_log(" write_txt: starting");
    return write_text_file(lvl->script.txt,lvl->script.lines_count,fname);
}

/*
 * Saves LGT file.
 */
short write_lgt(struct LEVEL *lvl,char *fname)
{
    message_log(" write_lgt: starting");
    //Preparing array bounds
    int arr_entries_x=MAP_SIZE_X*MAP_SUBNUM_X;
    int arr_entries_y=MAP_SIZE_Y*MAP_SUBNUM_Y;

    FILE *fp;
    fp = fopen (fname, "wb");
    if (!fp)
    {
      message_error("Can't open \"%s\" for writing", fname);
      return false;
    }
    write_long_le_file (fp,lvl->lgt_total_count);
    int cy, cx, k;
    for (cy=0; cy<arr_entries_y; cy++)
    {
      for (cx=0; cx<arr_entries_x; cx++)
      {
          int num_subs=get_stlight_subnums(lvl,cx,cy);
          for (k=0; k<num_subs; k++)
          {
                char *stlight=get_stlight(lvl,cx,cy,k);
                fwrite (stlight, SIZEOF_DK_LGT_REC, 1, fp);
          }
      }
    }
    fclose (fp);
    return true;
}

/*
 * Saves WLB file.
 * WLB seems to be unused by the game, but are always written by BF editor.
 */
short write_wlb(struct LEVEL *lvl,char *fname)
{
    message_log(" write_wlb: starting");
    FILE *fp;
    fp = fopen (fname, "wb");
    if (!fp)
    {
      message_error("Can't open \"%s\" for writing", fname);
      return false;
    }
    int i, j;
    for (i=0; i < MAP_SIZE_Y; i++)
    {
      for (j=0; j < MAP_SIZE_X; j++)
          fputc(lvl->wlb[j][i], fp);
    }
    fclose(fp);
    return true;
}

/*
 * Saves ADI scripf file. Creates it first.
 */
short write_adi_script(struct LEVEL *lvl,char *fname)
{
    message_log(" write_adi_script: starting");
   //Creating text lines
   char **lines=NULL;
   int lines_count=0;
   add_stats_to_script(&lines,&lines_count,lvl);
   add_graffiti_to_script(&lines,&lines_count,lvl);
   add_custom_clms_to_script(&lines,&lines_count,lvl);
   short result;
   result=write_text_file(lines,lines_count,fname);
   text_file_free(lines,lines_count);
   return result;
}

/*
 * Saves any text file.
 */
short write_text_file(char **lines,int lines_count,char *fname)
{
    FILE *fp;
    int i;
    fp = fopen (fname, "wb");
    if (!fp)
    {
      message_error("Can't open \"%s\" for writing", fname);
      return false;
    }
    int last_line=lines_count-1;
    for (i=0;i<last_line;i++)
    {
      fputs(lines[i],fp);
      fputs("\r\n",fp);
    }
    if (last_line>=0)
    {
      fputs(lines[last_line],fp);
      if (lines[last_line][0] != '\0')
        fputs("\r\n",fp);
    }
    fclose(fp);
    return true;
}

/*
 * Saves the whole map. Includes all files editable in Adikted.
 */
short save_map(struct LEVEL *lvl)
{
    message_log(" save_map: started");
    struct IPOINT_2D errpt={-1,-1};
    if (level_verify(lvl,"save",&errpt)==VERIF_ERROR)
      return false;

    //Once there was an CLM/DAT/TNG update function here,
    // but the new way is to minimize file changes - so it's been removed
    update_slab_owners(lvl);

    char *fnames;
    fnames = (char *)malloc(strlen(lvl->savfname)+5);
    if (fnames==NULL)
    {
        message_error("save_map: Out of memory");
        return false;
    }
    short result=true;
    sprintf (fnames, "%s.own", lvl->savfname);
    result&=write_own(lvl,fnames);
    sprintf (fnames, "%s.slb", lvl->savfname);
    result&=write_slb(lvl,fnames);
    sprintf (fnames, "%s.dat", lvl->savfname);
    result&=write_dat(lvl,fnames);
    sprintf (fnames, "%s.clm", lvl->savfname);
    result&=write_clm(lvl,fnames);
    sprintf (fnames, "%s.tng", lvl->savfname);
    result&=write_tng(lvl,fnames);
    sprintf (fnames, "%s.apt", lvl->savfname);
    result&=write_apt(lvl,fnames);
    sprintf (fnames, "%s.wib", lvl->savfname);
    result&=write_wib(lvl,fnames);
    sprintf (fnames, "%s.inf", lvl->savfname);
    result&=write_inf(lvl,fnames);
    sprintf (fnames, "%s.txt", lvl->savfname);
    result&=write_txt(lvl,fnames);
    sprintf (fnames, "%s.lgt", lvl->savfname);
    result&=write_lgt(lvl,fnames);
    sprintf (fnames, "%s.wlb", lvl->savfname);
    result&=write_wlb(lvl,fnames);
    sprintf (fnames, "%s.flg", lvl->savfname);
    result&=write_flg(lvl,fnames);
    sprintf (fnames, "%s.adi", lvl->savfname);
    result&=write_adi_script(lvl,fnames);
    if ((result)||(strlen(lvl->fname)<1))
    {
      strncpy(lvl->fname,lvl->savfname,DISKPATH_SIZE);
      lvl->fname[DISKPATH_SIZE-1]=0;
    }
    free(fnames);
    lvl->stats.saves_count++;
    message_log(" save_map: finished");
    return result;
}

/*
 * Loads the whole map. Tries to open all files editable in Adikted.
 * Returns true on success, on error starts new map and returns false.
 */
short load_map(struct LEVEL *lvl)
{
  message_log(" load_map: started");
  char *fnames;
  char *err_msg;
  char *ifname=NULL;
  level_free(lvl);
  if ((lvl->fname==NULL)||(strlen(lvl->fname)<1))
  {
    start_new_map(lvl);
    return false;
  }
  level_clear(lvl);
  err_msg=(char *)malloc(LINEMSG_SIZE);
  fnames = (char *)malloc(strlen(lvl->fname)+5);
  if ((fnames==NULL)||(err_msg==NULL))
  {
    message_error("load_map: Out of memory");
    return false;
  }
  short result=ERR_NONE;
  short file_result;
  if (result>=ERR_NONE)
  {
    sprintf (fnames, "%s.slb", lvl->fname);
    file_result=load_slb(lvl,fnames);
    if (file_result!=ERR_NONE)
    {
      free(ifname);
      ifname=prepare_short_fname(fnames,24);
      result=file_result;
    }
  }    
  if (result>=ERR_NONE)
  {
    sprintf (fnames, "%s.own", lvl->fname);
    file_result=load_own(lvl,fnames);
    if (file_result!=ERR_NONE)
    {
      free(ifname);
      ifname=prepare_short_fname(fnames,24);
      result=file_result;
    }
  }    
  if (result>=ERR_NONE)
  {
    sprintf (fnames, "%s.dat", lvl->fname);
    file_result=load_dat(lvl,fnames);
    if (file_result!=ERR_NONE)
    {
      free(ifname);
      ifname=prepare_short_fname(fnames,24);
      result=file_result;
    }
  }    
  if (result>=ERR_NONE)
  {
    sprintf (fnames, "%s.tng", lvl->fname);
    file_result=load_tng(lvl, fnames);
    if (file_result!=ERR_NONE)
    {
      free(ifname);
      ifname=prepare_short_fname(fnames,24);
      result=file_result;
    }
  }    
  if (result>=ERR_NONE)
  {
    sprintf (fnames, "%s.apt", lvl->fname);
    file_result=load_apt(lvl, fnames);
    if (file_result!=ERR_NONE)
    {
      free(ifname);
      ifname=prepare_short_fname(fnames,24);
      result=file_result;
    }
  }    
  if (result>=ERR_NONE)
  {
    sprintf (fnames, "%s.lgt", lvl->fname);
    file_result=load_lgt(lvl,fnames);
    if (file_result!=ERR_NONE)
    {
      char *warn_fname;
      warn_fname=prepare_short_fname(fnames,24);
      message_info_force("LGT warning: %s when loading \"%s\" (ignored)",load_error(file_result), warn_fname);
      free(warn_fname);
    }
  }    
  if (result>=ERR_NONE)
  {
    sprintf (fnames, "%s.clm", lvl->fname);
    file_result=load_clm(lvl,fnames);
    if (file_result!=ERR_NONE)
    {
      free(ifname);
      ifname=prepare_short_fname(fnames,24);
      result=file_result;
    }
  }    
  if (result>=ERR_NONE)
  {
    sprintf (fnames, "%s.wib", lvl->fname);
    file_result=load_wib(lvl,fnames);
    if (file_result!=ERR_NONE)
    {
      free(ifname);
      ifname=prepare_short_fname(fnames,24);
      result=file_result;
    }
  }    
  if (result>=ERR_NONE)
  {
    sprintf (fnames, "%s.txt", lvl->fname);
    file_result=load_txt(lvl,fnames);
    if (file_result!=ERR_NONE)
    {
      char *warn_fname;
      warn_fname=prepare_short_fname(fnames,24);
      message_info_force("Script warning: %s in \"%s\" (ignored)", load_error(file_result), warn_fname);
      free(warn_fname);
    }
  }    
  if (result>=ERR_NONE)
  {
    sprintf (fnames, "%s.inf", lvl->fname);
    //INFs contain only texture number, so ignore error on loading them
    file_result=load_inf(lvl,fnames);
  }    
  if (result>=ERR_NONE)
  {
    sprintf (fnames, "%s.wlb", lvl->fname);
    //WLBs are not very importand, and may even not exist,
    // so ignore any error when loading them
    file_result=load_wlb(lvl,fnames);
  }    
  if (result>=ERR_NONE)
  {
    sprintf (fnames, "%s.flg", lvl->fname);
    //FLGs are not very importand, so ignore any error when loading
    file_result=load_flg(lvl,fnames);
  }    
  if (result>=ERR_NONE)
  {
    sprintf (fnames, "%s.adi", lvl->fname);
    file_result=script_load_and_execute(lvl,fnames,err_msg);
    // Ignore ADI load errors other than "bad data" - ADI file don't have to exist
    if (file_result==ERR_FILE_BADDATA)
    {
      char *warn_fname;
      warn_fname=prepare_short_fname(fnames,24);
      message_info_force("ADI script warning: %s in \"%s\" (ignored)", err_msg, warn_fname);
      free(warn_fname);
    }
  }    
  if (result<ERR_NONE)
  {
      message_error("Error: %s when loading \"%s\"",load_error(result), fnames);
      free(ifname);
      free(fnames);
      free(err_msg);
      free_map(lvl);
      start_new_map(lvl);
      return false;
  } else
  if (result>ERR_NONE)
  {
      message_info_force("Warning: %s when loading \"%s\" (ignored)",load_error(result), ifname);
  }
  if ((result>=ERR_NONE)&&(strlen(lvl->savfname)<1))
  {
      strncpy(lvl->savfname,lvl->fname,DISKPATH_SIZE);
      lvl->savfname[DISKPATH_SIZE-1]=0;
  }
  update_level_stats(lvl);
  free(ifname);
  free(fnames);
  free(err_msg);
  message_log(" load_map: finished");
  return true;
}

/*
 * Loads the map preview. Tries to open only files needed for Slab mode preview.
 * Returns true on success, on error returns false without clearing the structure.
 * (but the data from before the loading is cleared)
 */
short load_map_preview(struct LEVEL *lvl)
{
  char *fnames;
  char *err_msg;
  level_free(lvl);
  if ((lvl->fname==NULL)||(strlen(lvl->fname)<1))
  {
    level_clear(lvl);
    return false;
  }
  level_clear(lvl);
  err_msg=(char *)malloc(LINEMSG_SIZE);
  fnames = (char *)malloc(strlen(lvl->fname)+5);
  if ((fnames==NULL)||(err_msg==NULL))
  {
    message_error("load_map: Out of memory");
    return false;
  }
  short result=ERR_NONE;
  short file_result;
  if (result>=ERR_NONE)
  {
    sprintf (fnames, "%s.slb", lvl->fname);
    file_result=load_slb(lvl,fnames);
    if (file_result!=ERR_NONE)
    {
      result=file_result;
    }
  }    
  if (result>=ERR_NONE)
  {
    sprintf (fnames, "%s.own", lvl->fname);
    file_result=load_own(lvl,fnames);
    if (file_result!=ERR_NONE)
    {
      result=file_result;
    }
  }    
  if (result!=ERR_NONE)
  {
      message_info_force("Preview: %s when loading \"%s\"",load_error(result), fnames);
      if (result<ERR_NONE)
      {
        free(fnames);
        free(err_msg);
        return false;
      }
  }
  if (strlen(lvl->savfname)<1)
  {
      strncpy(lvl->savfname,lvl->fname,DISKPATH_SIZE);
      lvl->savfname[DISKPATH_SIZE-1]=0;
  }
  update_level_stats(lvl);
  free(fnames);
  free(err_msg);
  return true;
}

/*
 * Utility function for reverse engineering the CLM format
 * Used in rework mode.
 */
short write_def_clm_source(struct LEVEL *lvl,char *fname)
{
    FILE *fp;
    int i,k;
    fp = fopen (fname, "w");
    if (!fp)
    {
      message_error("Can't open \"%s\" for writing", fname);
      return false;
    }

    //Preparing array bounds
    int arr_entries_x=MAP_SIZE_X*MAP_SUBNUM_X;
    int arr_entries_y=MAP_SIZE_Y*MAP_SUBNUM_Y;

    unsigned char *clmentry;
    struct COLUMN_REC *clm_rec;
    clm_rec=create_column_rec();
    for (i=0; i<COLUMN_ENTRIES; i++)
      {
        clmentry = (unsigned char *)(lvl->clm[i]);
        get_clm_entry(clm_rec, clmentry);
        int csum=read_short_le_buf(clmentry+2);
/*        clm_rec->c[0]+clm_rec->c[1]+clm_rec->c[2]+clm_rec->c[3]+
            clm_rec->c[4]+clm_rec->c[5]+clm_rec->c[6]+clm_rec->c[7]+
            clm_rec->base;*/
//        if ((lvl->clm_utilize[i])!=(clm_rec->use))
        {
/*          char binstr0[9];
          char binstr1[9];
          char binstr2[9];
          char binstr3[9];
          char binstr4[9];
          char binstr5[9];
          for (k=0;k<8;k++)
            binstr0[k]='0'+((clmentry[0]&(1<<k))!=0);
          binstr0[8]=0;
          for (k=0;k<8;k++)
            binstr1[k]='0'+((clmentry[1]&(1<<k))!=0);
          binstr1[8]=0;
          for (k=0;k<8;k++)
            binstr2[k]='0'+((clmentry[2]&(1<<k))!=0);
          binstr2[8]=0;
          for (k=0;k<8;k++)
            binstr3[k]='0'+((clmentry[3]&(1<<k))!=0);
          binstr3[8]=0;
          for (k=0;k<8;k++)
            binstr4[k]='0'+((clmentry[4]&(1<<k))!=0);
          binstr4[8]=0;
          for (k=0;k<8;k++)
            binstr5[k]='0'+((use_cntr[i]&(1<<k))!=0);
          binstr5[8]=0;
          fprintf(fp,"%4d   %s %s(%5d) %s %s %s (%s=%d)\n",i,binstr0,binstr1,clm_rec->use,binstr2,binstr3,binstr4,binstr5,use_cntr[i]);
*/
          fprintf(fp,"COLUMN(%4d,%5d,%2d,%2d,%2d, 0x%02x, 0x%03x,%2d,",
           i,(unsigned short)(clm_rec->use), clm_rec->permanent, clm_rec->lintel,
           clm_rec->height, clm_rec->solid, clm_rec->base, clm_rec->orientation);
            fprintf(fp,"    0x%03x, 0x%03x, 0x%03x, 0x%03x, 0x%03x, 0x%03x, 0x%03x, 0x%03x) u=%d u0=%d\n",
              clm_rec->c[0],clm_rec->c[1],clm_rec->c[2],clm_rec->c[3],
              clm_rec->c[4],clm_rec->c[5],clm_rec->c[6],clm_rec->c[7],
              lvl->clm_utilize[i],(unsigned short)(clm_rec->use-(lvl->clm_utilize[i])));
/*          fprintf(fp,"  set_clm_ent_idx(lvl,%4d,%5d,%2d,%2d,%2d, 0x%02x, 0x%03x,%2d,\n",
           i,(unsigned short)(clm_rec->use-(lvl->clm_utilize[i])), clm_rec->permanent, clm_rec->lintel,
           clm_rec->height, clm_rec->solid, clm_rec->base, clm_rec->orientation);
        if ((clm_rec->c[5]==0)&&(clm_rec->c[6]==0)&&(clm_rec->c[7]==0))
            fprintf(fp,"    0x%03x, 0x%03x, 0x%03x, 0x%03x, 0x%03x, 0x%x, 0x%x, 0x%x);\n",
              clm_rec->c[0],clm_rec->c[1],clm_rec->c[2],clm_rec->c[3],
              clm_rec->c[4],clm_rec->c[5],clm_rec->c[6],clm_rec->c[7]);
          else
            fprintf(fp,"    0x%03x, 0x%03x, 0x%03x, 0x%03x, 0x%03x, 0x%03x, 0x%03x, 0x%03x);\n",
              clm_rec->c[0],clm_rec->c[1],clm_rec->c[2],clm_rec->c[3],
              clm_rec->c[4],clm_rec->c[5],clm_rec->c[6],clm_rec->c[7]);
*/
        }
      }
    free_column_rec(clm_rec);
    fclose (fp);
    return true;
}

/*
 * Utility function for reverse engineering the TNG format.
 * Used in rework mode.
 */
short write_def_tng_source(struct LEVEL *lvl,char *fname)
{
    FILE *fp;
    int i,k;
    fp = fopen (fname, "w");
    if (!fp)
    {
      message_error("Can't open \"%s\" for writing", fname);
      return false;
    }

    //Preparing array bounds
    int arr_entries_x=MAP_SIZE_X*MAP_SUBNUM_X;
    int arr_entries_y=MAP_SIZE_Y*MAP_SUBNUM_Y;
    int cx,cy;
    for (cy=0; cy < arr_entries_y; cy++)
      for (cx=0; cx < arr_entries_x; cx++)
          for (k=0; k < get_thing_subnums(lvl,cx,cy); k++)
          {
            unsigned char *thing=get_thing(lvl,cx,cy,k);
            int spos_x=get_thing_subtile_x(thing);
            int spos_y=get_thing_subtile_y(thing);
            int sen_tl=get_thing_sensitile(thing);
            unsigned short tngtype=get_thing_type(thing);
//            if (tngtype!=THING_TYPE_ITEM) continue;
            if ( (sen_tl!=((spos_x/MAP_SUBNUM_Y-1)+(spos_y/MAP_SUBNUM_Y-1)*MAP_SIZE_X)) &&
                 (sen_tl!=((spos_x/MAP_SUBNUM_Y-1)+(spos_y/MAP_SUBNUM_Y+0)*MAP_SIZE_X)) &&
                 (sen_tl!=((spos_x/MAP_SUBNUM_Y-1)+(spos_y/MAP_SUBNUM_Y+1)*MAP_SIZE_X)) &&
                 (sen_tl!=((spos_x/MAP_SUBNUM_Y+0)+(spos_y/MAP_SUBNUM_Y-1)*MAP_SIZE_X)) &&
                 (sen_tl!=((spos_x/MAP_SUBNUM_Y+0)+(spos_y/MAP_SUBNUM_Y+0)*MAP_SIZE_X)) &&
                 (sen_tl!=((spos_x/MAP_SUBNUM_Y+0)+(spos_y/MAP_SUBNUM_Y+1)*MAP_SIZE_X)) &&
                 (sen_tl!=((spos_x/MAP_SUBNUM_Y+1)+(spos_y/MAP_SUBNUM_Y-1)*MAP_SIZE_X)) &&
                 (sen_tl!=((spos_x/MAP_SUBNUM_Y+1)+(spos_y/MAP_SUBNUM_Y+0)*MAP_SIZE_X)) &&
                 (sen_tl!=((spos_x/MAP_SUBNUM_Y+1)+(spos_y/MAP_SUBNUM_Y+1)*MAP_SIZE_X)) )
            {
              int tl_x=spos_x/MAP_SUBNUM_X;
              int tl_y=spos_y/MAP_SUBNUM_Y;
              fprintf(fp,"stl %3d,%3d tl %2d,%2d", spos_x, spos_y,
                  tl_x, tl_y);
              fprintf(fp," s %d,%d", spos_x-tl_x*MAP_SUBNUM_X, spos_y-tl_y*MAP_SUBNUM_Y);
              fprintf(fp," stlpos %3d,%3d",
              get_thing_subtpos_x(thing), get_thing_subtpos_y(thing));
              fprintf(fp," alt %3d altstl %d",
                get_thing_subtpos_h(thing),get_thing_subtile_h(thing));
              fprintf(fp," typ %5s",get_thing_type_shortname(tngtype));
              switch (tngtype)
              {
              case THING_TYPE_DOOR:
                fprintf(fp," knd %s",get_door_subtype_fullname(get_thing_subtype(thing)));
                break;
              case THING_TYPE_TRAP:
                fprintf(fp," knd %s",get_trap_subtype_fullname(get_thing_subtype(thing)));
                break;
              case THING_TYPE_ITEM:
                fprintf(fp," knd %s",get_item_subtype_fullname(get_thing_subtype(thing)));
                break;
              }
              fprintf(fp,"\n");
              for (i=0; i < SIZEOF_DK_TNG_REC; i++)
              {
                  fprintf(fp,"  %02X", (unsigned int)thing[i]);
              }
              fprintf(fp,"\n");
              for (i=0; i < SIZEOF_DK_TNG_REC; i++)
              {
                  fprintf(fp," %3d", (unsigned int)thing[i]);
              }
              fprintf(fp,"\n\n");
            }
          }
    fclose (fp);
    return true;
}