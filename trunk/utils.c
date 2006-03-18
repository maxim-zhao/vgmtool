#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <zlib.h>
#include "vgm.h"
#include "gui.h"
#include "utils.h"

extern HWND hWndMain;

// Buffer for copying (created when needed)
#define BUFFER_SIZE 1024*8

// returns a boolean specifying if the passed filename exists
// also shows an error message if it doesn't
BOOL FileExists(char *filename) {
  BOOL result=FileExistsQuiet(filename);
  if (!result) ShowError("File not found or in use:\n%s",filename);
  return result;
}

BOOL FileExistsQuiet(char *filename) {
  FILE *f;
  BOOL result;
  if(!filename) return FALSE;
  f=fopen(filename,"rb");
  result=(f!=NULL);
  if (f) fclose(f);
  return result;
}

// returns the size of the file in bytes
unsigned long int FileSize(char *filename) {
  HANDLE *f;
  unsigned long int s;
  f=CreateFile(filename,GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
  s=GetFileSize(f,NULL);
  CloseHandle(f);
  return s;
}

// makes a unique temp filename out of src, mallocing the space for the result
// so don't forget to free it when you're done
// it will probably choke with weird parameters, real writable filenames should be OK
char *MakeTempFilename(char *src) {
  int i=0;
  char *p,*dest;
  
  dest=malloc(strlen(src)+4); // just in case it has no extension, some extra space
  strcpy(dest,src);
  p=strrchr(dest,'\\');
  p++;
  do sprintf(p,"%d.tmp",i++);
  while (FileExistsQuiet(dest)); // keep trying integers until one doesn't exist

//  ShowMessage("Made a temp filename:\n%s\nfrom:\n%s",dest,src); // debugging

  return dest;
}

//----------------------------------------------------------------------------------------------
// Helper routine - "filename.ext","suffix" becomes "filename (suffix).ext"
//----------------------------------------------------------------------------------------------
char *MakeSuffixedFilename(char *src,char *suffix) {
  char *p,*dest;

  dest=malloc(strlen(src)+strlen(suffix)+10); // 10 is more than I need to be safe

  strcpy(dest,src);
  p=strrchr(strrchr(dest,'\\'),'.'); // find last dot after last slash
  if(!p) p=dest+strlen(dest); // if no extension, add to the end of the file instead
  sprintf(p," (%s)%s",suffix,src+(p-dest));

  ShowMessage("Made a temp filename:\n%s\nfrom:\n%s",dest,src); // debugging

  return dest;
}


// compresses the file with GZip compression
// to a temp file, then overwrites the original file with the temp
BOOL Compress(char *filename) {
  gzFile *in,*out;
  char *outfilename;
  char *copybuffer;
  int AmtRead;

  if (!FileExists(filename)) return FALSE;

  ShowStatus("Compressing...");

  // Check filesize since big files take ages to compress
  if (
    (FileSize(filename)>1024*1024*1) &&
    (ShowQuestion(
      "This uncompressed VGM is over 1MB so it'll take a while to compress.\n"
      "Do you want to skip compressing it?\n"
      "(You can compress it later using the \"Compress file\" button on the Misc tab.)"
     )==IDYES)
  ) {
    ShowStatus("Compression skipped");
    return FALSE;
  }

  outfilename=MakeTempFilename(filename);

  out=gzopen(outfilename,"wb9");
  in=gzopen(filename,"rb");

  copybuffer=malloc(BUFFER_SIZE);

  do {
    AmtRead=gzread(in,copybuffer,BUFFER_SIZE);
    if (gzwrite(out,copybuffer,AmtRead)!=AmtRead) {
      // Error copying file
      ShowError("Error copying data to temporary file %s!",outfilename);
      free(copybuffer);
      gzclose(in);
      gzclose(out);
      DeleteFile(outfilename);
      return FALSE;
    }
  } while (AmtRead>0);

  free(copybuffer);
  gzclose(in);
  gzclose(out);

  ReplaceFile(filename,outfilename);

  free(outfilename);
  ShowStatus("Compression complete");
  return TRUE;
}

// decompress the file
// to a temp file, then overwrites the original file with the temp file
BOOL Decompress(char *filename) {
  FILE *out;
  gzFile *in;
  char *outfilename;
  char *copybuffer;
  int AmtRead,x;

  if (!FileExists(filename)) return FALSE;

  ShowStatus("Decompressing...");

  outfilename=MakeTempFilename(filename);

  out=fopen(outfilename,"wb");
  in=gzopen(filename,"rb");

  copybuffer=malloc(BUFFER_SIZE);

  do {
    AmtRead=gzread(in,copybuffer,BUFFER_SIZE);
    if((x=fwrite(copybuffer,1,AmtRead,out))!=AmtRead) {
      // Error copying file
      ShowError("Error copying data to temporary file %s!",outfilename);
      free(copybuffer);
      gzclose(in);
      gzclose(out);
      DeleteFile(outfilename);
      return FALSE;
    }
  }while (!gzeof(in));

  free(copybuffer);
  gzclose(in);
  fclose(out);

  ReplaceFile(filename,outfilename);

  free(outfilename);
  ShowStatus("Decompression complete");
  return TRUE;
}

// Assumes filename has space at the end for the extension, if needed
void ChangeExt(char *filename,char *ext) {
  char *p,*q;

  p=strrchr(filename,'\\');
  q=strchr(p,'.');
  if(q==NULL) q=p+strlen(p); // if no ext, point to end of string

  strcpy(q,".");
  strcpy(q+1,ext);
}

#define GZMagic1 0x1f
#define GZMagic2 0x8b
// Changes the file's extension to vgm or vgz depending on whether it's compressed
BOOL FixExt(char *filename) {
  char *newfilename;
  FILE *f;
  int IsCompressed=0;

  if (!FileExists(filename)) return FALSE;

  f=fopen(filename,"rb");
  IsCompressed=((fgetc(f)==GZMagic1) && (fgetc(f)==GZMagic2));
  fclose(f);

  newfilename=malloc(strlen(filename)+10); // plenty of space, can't hurt

  strcpy(newfilename,filename);

  if (IsCompressed) ChangeExt(newfilename,"vgz");
  else ChangeExt(newfilename,"vgm");

  if (strcmp(newfilename,filename)!=0) {
    ReplaceFile(newfilename,filename); // replaces any existing file with the new name, with the existing file
    strcpy(filename,newfilename);
  }

  free(newfilename);
  return TRUE;
}

// Delete filetoreplace, rename with with its name
void ReplaceFile(char *filetoreplace,char *with) {
  if ( strcmp(filetoreplace, with) == 0 )
    return;
  if ( FileExistsQuiet( filetoreplace ) )
    DeleteFile( filetoreplace );
  if ( MoveFile( with, filetoreplace ) == 0 )
    ShowError( "Error replacing old file:\n%s\nUpdated file is called:\n%s", filetoreplace, with);
}
