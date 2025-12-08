/*
Example of:
  1) using a buffer to store a large RGBA2222 colour bitmap 
  2) load in chunks from file
  3) used buffer commands to split a buffer horizontally
  4) use screen capture to bitmap function
  5) use transformations to scale and rotate icons
*/

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <agon/vdp.h>
#include <agon/timer.h>
#include "labels.h"
#include "woosh.h"
#include "completed.h"

// experimental use of inline escape codes to change text colour
// all BRIGHT versions of the colour
#define TEXT_RED "\x11\x09"
#define TEXT_GREEN "\x11\x0A"
#define TEXT_BLUE "\x11\x0C"
#define TEXT_YELLOW "\x11\x0B"
#define TEXT_MAGENTA "\x11\x0D"
#define TEXT_CYAN "\x11\x0E"
#define TEXT_WHITE "\x11\x0F"

// constants used
const uint8_t  screen_mode = 8;
const uint8_t  RGBA2222_format = 1;
const uint8_t  captureBitmapID = 30;
const uint16_t startBitmapID = 40;
const uint16_t startBigBitmapID = 20;
const uint16_t startIconBitmapID = 100;
const uint16_t spinIconID = 200;
const uint16_t transformID = 98;
const uint16_t spinTransformID = 97;
const uint16_t tempBigBitmapID = 99;
const uint16_t bitmapWidth = 320;
const uint16_t bitmapHeight = 240;
const uint16_t chunksPerLine = 4;
const uint16_t chunkSizeW = 80;
const uint16_t chunkSizeH = 60;
const uint16_t label_sprite_start_ID = 0;
const uint16_t scrollJump = 5;
const uint16_t scrollSpeed = 8;
const uint16_t numCells = 16;
const uint16_t hCells = 4;
const uint16_t vCells = 4;
const uint16_t numSprites = 8;
const uint16_t maxPuzzles = 12;


// for file handling
DIR handle;
FILINFO file;

// some global variables used
char dirpath[256];
char myFiles[16][32];
char fname[32];
char directoryName[] = "/puzzles/";
char buff[320*60];
//char bigBuff[320*240];
uint8_t arrayBitmaps[4][4];           // which bitmap is in each cell 0-15
uint8_t arrayOriginal[4][4];           // which bitmap is in each cell 0-15
uint8_t numPuzzles = 0;
uint8_t currentPuzzleNum = 0;

// functions in this file
void loadBitmaps(char bitmapName[]);
uint8_t menuScreen(void);
void initGame(uint8_t level);
uint8_t gameScreen(void);
void completedScreen(void);
void doExit(void);
void redrawBitmaps(void);
void scrollH(uint8_t hNum);
void scrollV(uint8_t vNum);
void scrollHrev(uint8_t hNum);
void scrollVrev(uint8_t vNum);
void my_vdp_capture_bitmap(uint16_t top, uint16_t left, uint16_t bottom, uint16_t right, uint8_t bitmapID);
void makeLabel(uint16_t id, char name[],uint16_t xxx, uint16_t yyy );
void hideSprites(void);
void showSprites(void);
void setupUDG(void);
void shufflePic(uint8_t level);
void loadLabels(void);
uint8_t load_big_puzzles(void);
void drawRect(uint8_t rectNum);
uint8_t imagePicker(int8_t curImage);
void putWord(uint16_t theWord);
void spinOut(uint8_t icon);

// now the main program
int main(void) {

  // setup
  vdp_mode(screen_mode);            // set to mode 8, 320x240 62 colours
  vdp_cursor_enable(false);         // stop flashing cursor
  vdp_clear_screen();               // clear screen
  vdp_set_pixel_coordinates();      // set to pixel coord format
  vdp_reset_sprites();              // clear any sprites previously on the system
  setupUDG();                       // create UDG chars
  srand(time(NULL));                // set random seed

  // load audio samples
  vdp_audio_enable_channel(3);      // for woosh sound
  vdp_audio_enable_channel(4);      // for completion sound
  
  vdp_audio_load_sample( -1, 1971, woosh);  // Load the audio sample
  vdp_audio_set_waveform( 3,  -1);          // set sample in bufferID 64256 (-1) to channel 3
  
  vdp_audio_load_sample( -2, 14632, completed); // Load the audio sample                               
  vdp_audio_set_waveform( 4,  -2);              // set sample in bufferID 64255 (-2) to channel 4
      
  numPuzzles = load_big_puzzles();

  if(numPuzzles == 0){
    printf("No puzzles found");
    doExit();
  }
  printf("%d puzzles found",numPuzzles);

  // get first default file loaded
  char thisFile[32];
  strcpy(thisFile, directoryName);              // directory name 'puzzles/'
  strcat(thisFile, myFiles[currentPuzzleNum]);  // add current file name
  strcpy(fname, thisFile);                      // put into fname
  loadBitmaps(fname);                           // load this bitmap
  loadLabels();

  // loop whole game here
  while(true) {
    uint8_t level = menuScreen();               // do menu and return with chosen level
    initGame(level);                            // setup level chose
    if (gameScreen() == 1)  completedScreen();  // play game and return with give up (0) or completed (1)
  }                                             // show 'completed' message if 1 returned

  return 0;     // exit to MOS
}

uint8_t load_big_puzzles(void){
  // count number puzzles
  // load big pics to make icons

   // check for folder of puzzle images
  if(ffs_getcwd(dirpath, 256) != 0) {
    printf("Unable to get current directory\r\n");
    return 0;
  }

  strcat(dirpath, "/puzzles/");        // look in sub-directory 'puzzles/'
  if(ffs_dopen(&handle, dirpath) != 0) {
    printf("Unable to open '%s'\r\n", dirpath);
    return 0;
  }

  uint8_t fileCount=0;
  while(1) {
    uint8_t result = ffs_dread(&handle, &file); 
    if((result != 0) || (strlen(file.fname) == 0)) break;   // end of list
    if(fileCount > maxPuzzles) break;                       // we already have enough files
    if(file.fattrib & 0x10) {
      // dir not a file
    }
    else if(file.fname[0]=='.'){
      // hidden mac file
    } else {
      strcpy(myFiles[fileCount], file.fname);             // add this file
      fileCount++;
    }
    numPuzzles = fileCount ;
  }

    
  vdp_adv_clear_buffer(98);     // clear just in case

  vdp_set_variable(1,1);        // enable fancy scaling buffer commands

  // create the transform matrix to be used to scale icon images
  // this is not in vdp.h yet

  // Commands 32 and 33: Create or manipulate a 2D or 3D affine transformation matrix
  // VDU 23, 0, &A0, bufferId; 32, operation, [<format>, <arguments...>]

  putchar(23);      // vdu buffer command
  putchar(0);
  putchar(0xA0);

  putchar(98);      // transform ID word 98
  putchar(0);

  putchar(32);      // command no 32

  putchar(5);       // operation 5=scale

  putchar(0x03 | 0x40 | 0x80);    // format fixed point, 16 bit, shift point x 3.

  putchar(0x02);       // arguments x scale - should be 1/8 scale
  putchar(0x00);    // arguments

  putchar(0x02);    // arguments y scale
  putchar(0x00);    // arguments

 


  // iterate through all images
  for (uint8_t pc=0; pc <numPuzzles ; pc++){

  char thisFile[32];
  strcpy(thisFile, directoryName);              // directory name 'puzzles/'
  strcat(thisFile, myFiles[pc]);  // add current file name
  strcpy(fname, thisFile);                      // put into fname
  // loadBitmaps(fname);                           // load this bitmap


    vdp_adv_clear_buffer(tempBigBitmapID);                         // clear the buffer
    printf("Scanning file %s\r\n", fname); // dir not a file



    FILE *filePointer = fopen(fname, "r");                        // open the RGBA2222 image file
    uint16_t count = 0;

    if (filePointer == NULL){
      printf("Error opening file %s\r\n", fname); // dir not a file
    }
      
    // load and create big bitmap in small chuncks

    fread(buff, 1,320*60 ,filePointer);                               // read 80 byte chunk of data from file
    vdp_adv_write_block_data(tempBigBitmapID, 320*60, buff);        // send chunk into VDP buffer block 20 +
    fread(buff, 1,320*60 ,filePointer);                               // read 80 byte chunk of data from file
    vdp_adv_write_block_data(tempBigBitmapID, 320*60, buff);        // send chunk into VDP buffer block 20 +
    fread(buff, 1,320*60 ,filePointer);                               // read 80 byte chunk of data from file
    vdp_adv_write_block_data(tempBigBitmapID, 320*60, buff);        // send chunk into VDP buffer block 20 +
    fread(buff, 1,320*60 ,filePointer);                               // read 80 byte chunk of data from file
    vdp_adv_write_block_data(tempBigBitmapID, 320*60, buff);        // send chunk into VDP buffer block 20 +
   
    vdp_adv_consolidate(tempBigBitmapID);                          // consolidate all uploaded chunks into single buffer
    vdp_adv_select_bitmap(tempBigBitmapID);                             // select bitmap ID
    vdp_adv_bitmap_from_buffer(320 , 240, RGBA2222_format);             // make a bitmap 

    vdp_adv_select_bitmap(tempBigBitmapID);                        // select bitmap ID      

    fclose(filePointer);    // close the file as we are done with it

    // create icon version and store in a buffer
    // not in vdp.h yet
    // Command 40: Create a transformed bitmap
    // VDU 23, 0, &A0, bufferId; 40, options, transformBufferId; sourceBitmapId; [width; height;]

    putch(23);      // vdu buffer command
    putch(0);
    putch(0xA0);

    putch(startIconBitmapID + pc);      // minibitmap ID
    putch(0);       // word

    putch(40);      // command

    putch(1);      // options; resize 1

    putch(98);      // transform matrix buffer ID
    putch(0);       // word

    putch(tempBigBitmapID);      // source bitmap ID
    putch(0);       // word

    // clear original data as no longer needed
    vdp_adv_clear_buffer(tempBigBitmapID);                         // clear the buffer

        vdp_audio_play_note(0,127,400 + (pc * 24),60);
  }

  return numPuzzles;

}

void putWord(uint16_t theWord){
  uint8_t lsb = theWord %256;
  uint8_t msb = theWord >> 8;
  putch(lsb);       // LSB byte
  putch(msb);       // MSB byte
}


// -----------------------------------------------------------------------
//
// menu screen
//
// -----------------------------------------------------------------------

uint8_t menuScreen(void){

  hideSprites();
  vdp_clear_screen();
  vdp_cursor_tab(0,1);
  vdp_set_text_colour(BRIGHT_RED);
  printf("            S L I D E R\n\n\n");
  vdp_set_text_colour(BRIGHT_WHITE);
  printf("     Press:");
  vdp_set_text_colour(BRIGHT_YELLOW);
  printf(" 1 - 9 for level\n\n");
  printf("            S to Swap puzzle\n\n");

  vdp_set_text_colour(BRIGHT_WHITE);
  printf("   In game: ");
  vdp_set_text_colour(BRIGHT_YELLOW);
  printf("Q to Give up\n\n");
  printf("            ESC to exit program\n\n");
  printf("            1-4 & A-B to scroll\n\n");
  printf("            SHIFT reverse direction\n\n");

  vdp_cursor_tab(0,17);

  // experimental inline text colour change
  printf("             " TEXT_CYAN "  1 2 3 4\n");
  printf("             " TEXT_CYAN "  %c %c %c %c \n\n", 129, 129, 129, 129);
  printf("            " TEXT_CYAN "A%c " TEXT_BLUE "X X X X\n\n", 128);
  printf("            " TEXT_CYAN "B%c " TEXT_BLUE "X X X X\n\n", 128);
  printf("            " TEXT_CYAN "C%c " TEXT_BLUE "X X X X\n\n", 128);
  printf("            " TEXT_CYAN "D%c " TEXT_BLUE "X X X X\n\n", 128);

  vdp_set_text_colour(BRIGHT_BLACK);
  vdp_cursor_tab(8,29);
  //printf("Puzzle: %s             " ,myFiles[currentPuzzleNum]);
  printf("\xA9 Richard Turnnidge 2025");  // \u00A9 is ©
  vdp_set_text_colour(BRIGHT_WHITE); 

  while(true) {
    if(vdp_getKeyCode() == 27) doExit();   // exit if ESC pressed
    if(vdp_getKeyCode() == '1') return 1;   // play level
    if(vdp_getKeyCode() == '2') return 2;   
    if(vdp_getKeyCode() == '3') return 3;   
    if(vdp_getKeyCode() == '4') return 4;   
    if(vdp_getKeyCode() == '5') return 5;   
    if(vdp_getKeyCode() == '6') return 6;   
    if(vdp_getKeyCode() == '7') return 7;   
    if(vdp_getKeyCode() == '8') return 8;   
    if(vdp_getKeyCode() == '9') return 9;   
    // if(vdp_getKeyCode() == 's') {           // find next bitmap file
    //   char thisFile[32];
    //   currentPuzzleNum ++;
    //   if (currentPuzzleNum > numPuzzles-1) currentPuzzleNum = 0;
    //   strcpy(thisFile, directoryName);
    //   strcat(thisFile, myFiles[currentPuzzleNum]);
    //   strcpy(fname, thisFile);
    //   vdp_cursor_tab(0,29);
    //   printf("Puzzle: %s            " ,myFiles[currentPuzzleNum]);
    //   loadBitmaps(fname);         // reload data
    //   vdp_waitKeyUp();
    // };   
     if(vdp_getKeyCode() == 's') {
      char thisFile[32];
      currentPuzzleNum = imagePicker(currentPuzzleNum);  
      strcpy(thisFile, directoryName);
      strcat(thisFile, myFiles[currentPuzzleNum]);
      strcpy(fname, thisFile);
      vdp_cursor_tab(0,29);
      printf("Puzzle: %s            " ,myFiles[currentPuzzleNum]);
      loadBitmaps(fname);         // reload data

      vdp_clear_screen();

      vdp_cursor_tab(0,1);
      vdp_set_text_colour(BRIGHT_RED);
      printf("            S L I D E R\n\n\n");
      vdp_set_text_colour(BRIGHT_WHITE);
      printf("     Press:");
      vdp_set_text_colour(BRIGHT_YELLOW);
      printf(" 1 - 9 for level\n\n");
      printf("            S to Swap puzzle\n\n");

      vdp_set_text_colour(BRIGHT_WHITE);
      printf("   In game: ");
      vdp_set_text_colour(BRIGHT_YELLOW);
      printf("Q to Give up\n\n");
      printf("            ESC to exit program\n\n");
      printf("            1-4 & A-B to scroll\n\n");
      printf("            SHIFT reverse direction\n\n");

      vdp_cursor_tab(0,17);

      // experimental inline text colour change
      printf("             " TEXT_CYAN "  1 2 3 4\n");
      printf("             " TEXT_CYAN "  %c %c %c %c \n\n", 129, 129, 129, 129);
      printf("            " TEXT_CYAN "A%c " TEXT_BLUE "X X X X\n\n", 128);
      printf("            " TEXT_CYAN "B%c " TEXT_BLUE "X X X X\n\n", 128);
      printf("            " TEXT_CYAN "C%c " TEXT_BLUE "X X X X\n\n", 128);
      printf("            " TEXT_CYAN "D%c " TEXT_BLUE "X X X X\n\n", 128);

      vdp_cursor_tab(8,29);
      vdp_set_text_colour(BRIGHT_BLACK); 
            //printf("Puzzle: %s             " ,myFiles[currentPuzzleNum]);
      printf("\xA9 Richard Turnnidge 2025");  // \u00A9 is ©
      vdp_set_text_colour(BRIGHT_WHITE); 

     }

  }
}

// -----------------------------------------------------------------------
//
// image picker - choose puzzle picture
//
// -----------------------------------------------------------------------

uint8_t imagePicker(int8_t curImage){
  // arrive with current image number highlighted
  hideSprites();
  vdp_clear_screen();
  vdp_cursor_tab(0,1);
  vdp_set_text_colour(BRIGHT_WHITE);
  printf("Select Picture %c %c then ENTER\n\n", 130, 131);

  // draw thumbnails
  for (uint8_t xx = 0; xx < numPuzzles ; xx++){
      uint8_t xpos = xx % 4;
      uint8_t ypos = xx / 4;

      vdp_adv_select_bitmap(startIconBitmapID + xx);
      vdp_plot_bitmap(xpos * 80, 24 +(ypos * 60));

  }
  drawRect(curImage);
  vdp_cursor_tab(0,29);
  printf("Puzzle: %s             " ,myFiles[curImage]);
  while(true) {

    if(vdp_getKeyCode() == 27) doExit();   // exit if ESC pressed
    if(vdp_getKeyCode() == 8) {         // prev image
        vdp_audio_play_note(0,127,500,50);
        curImage --;
        if (curImage < 0) curImage = numPuzzles -1;
        drawRect(curImage);
          vdp_set_text_colour(BRIGHT_WHITE);
          vdp_cursor_tab(0,29);
          printf("Puzzle: %s             " ,myFiles[curImage]);
        vdp_waitKeyUp();
    }
    if(vdp_getKeyCode() == 21) {         // next image
      vdp_audio_play_note(0,127,600,50);
        curImage ++;
        if (curImage > numPuzzles -1) curImage = 0;
        drawRect(curImage);
          vdp_set_text_colour(BRIGHT_WHITE);
          vdp_cursor_tab(0,29);
          printf("Puzzle: %s             " ,myFiles[curImage]);
         vdp_waitKeyUp();
    }
    //vdp_cursor_tab(0,10);
    //printf("code=%d   ",vdp_getKeyCode());
    if(vdp_getKeyCode() == 13) {
      vdp_audio_play_note(0,127,800,80);
      break;        // ENTER
    }
  }

  spinOut(curImage);
  return curImage;

}

// -----------------------------------------------------------------------
// just draw the rectangles around the icons

void drawRect(uint8_t rectNum){
  uint8_t xpos ;
  uint8_t ypos ;

  // clear old rect
  vdp_set_graphics_fg_colour(0,0);
  for (uint8_t xx = 0; xx < numPuzzles ; xx++){
    xpos = xx % 4;
    ypos = xx / 4;
    vdp_rectangle( xpos * 80 ,24 +(ypos * 60), (xpos * 80 ) + 80 , 60 + 24 +(ypos * 60));
    vdp_rectangle( (xpos * 80) + 1 ,24 -1 +(ypos * 60), (xpos * 80) - 1 + 80 , 60 + 24 -1  +(ypos * 60));
  }
  //plot new single rect
  vdp_set_graphics_fg_colour(0,BRIGHT_RED);
  xpos = rectNum % 4;
  ypos = rectNum / 4;
  vdp_rectangle( (xpos * 80) + 1 ,24 -1 +(ypos * 60), (xpos * 80) - 1 + 80 , 60 + 24 -1  +(ypos * 60));
  vdp_rectangle( (xpos * 80)  ,24  +(ypos * 60), (xpos * 80)  + 80 , 60 + 24   +(ypos * 60));

}

// -----------------------------------------------------------------------
// spin out selected icon

void spinOut(uint8_t icon){

  uint16_t startX = icon % 4;
  uint16_t startY = icon / 4;

  vdp_adv_select_bitmap(startIconBitmapID + icon);
  // vdp_plot_bitmap(xpos * 80, 24 +(ypos * 60));

  for(uint8_t n=1; n<10; n++){              // spin out here

    vdp_adv_clear_buffer(spinIconID);         // clear the transform buffer
    vdp_adv_clear_buffer(97);         // clear the transform buffer


        // create spinout transformation- part 1: rotation
// Commands 32 and 33: Create or manipulate a 2D or 3D affine transformation matrix
  // VDU 23, 0, &A0, bufferId; 32, operation, [<format>, <arguments...>]

  putchar(23);      // vdu buffer command
  putchar(0);
  putchar(0xA0);

  putchar(97);      // transform ID word 97 spinout
  putchar(0);

  putchar(32);      // command no 32

  putchar(2);       // operation 3=rotate rads 2=deg

  putchar(0x00 | 0x40 | 0x80);    // format fixed point, 16 bit, shift point x 0.

  uint8_t a = (n * 40) % 256;   // try to do 1 full rotation
  uint8_t b = (n * 40) / 256;

  putchar(a);       // arguments angle 1 deg
  putchar(b);    // arguments


  // create spinout transformation- part 2: scale
  // Commands 32 and 33: Create or manipulate a 2D or 3D affine transformation matrix
    // VDU 23, 0, &A0, bufferId; 32, operation, [<format>, <arguments...>]

    putchar(23);      // vdu buffer command
    putchar(0);
    putchar(0xA0);

    putchar(97);      // transform ID word 98
    putchar(0);

    putchar(32);      // command no 32

    putchar(5);       // operation 5=scale

    putchar(0x02 | 0x40 | 0x80);    // format fixed point, 16 bit, shift point x 1.

    putchar(n + 3);       // arguments x scale - should be 1.5 x n
    putchar(0x00);    // arguments

    putchar(n +3);    // arguments y scale
    putchar(0x00);    // arguments







    // create the bitmap

  putch(23);      // vdu buffer command
  putch(0);
  putch(0xA0);

  putch(spinIconID);      // minibitmap ID
  putch(0);       // word

  putch(40);      // command

  putch(5);      // options; resize 1

  putch(97);      // transform matrix buffer ID
  putch(0);       // word

  putch(startIconBitmapID + icon);      // source bitmap ID
  putch(0);       // word

  // putch(80 + (n * 20));      // x size
  // putch(0);       // word

  // putch(60 + (n * 20));      // y size
  // putch(0);       // word


  vdp_adv_select_bitmap(spinIconID);

  //vdp_plot_bitmap(20 + ((10-n) * startX * 80), 44 +((10-n) * startY * 60));
  // where it starts
  uint16_t initx = 20 + ( startX * 80 );
  uint16_t inity = 24 + ( startY * 60 );

  // going from 1-9 positions so 10-n is inverse
  uint16_t newx = (initx * (10-n)) / 10;
  uint16_t newy = (inity * (10-n)) / 10;

  vdp_plot_bitmap(newx, newy);

  delay(80);


  }

}


// -----------------------------------------------------------------------
//
// init game - this will do variable setups, etc
//
// -----------------------------------------------------------------------

void initGame(uint8_t level){

  // set initial position of all bitmaps, 0->15
  uint16_t thisBitmap = 0;
  for (uint16_t yy = 0; yy < hCells ; yy++){
    for (uint16_t xx = 0; xx < vCells ; xx++){
      arrayBitmaps[xx][yy] = thisBitmap;
      arrayOriginal[xx][yy] = thisBitmap;
      thisBitmap ++;
    }
  }

  vdp_clear_screen();                     // clear screen
  redrawBitmaps();                        // show current bitmap
  shufflePic(level);                      // ix up 'level' times
  vdp_refresh_sprites();                  // put controls in corrrect psition
  vdp_activate_sprites(numSprites);       // activate control sprites
  showSprites();                          // display controls
}

// -----------------------------------------------------------------------
//
// game screen
//
// -----------------------------------------------------------------------

uint8_t gameScreen(void){

  while(true) {

    vdp_waitKeyDown();  // wait until something is pressed, then check what
    uint8_t kCode = vdp_getKeyCode();   // get code of key just pressed

    if(kCode == 'a') scrollH(0);   // scroll a line
    if(kCode == 'b') scrollH(1);   // scroll a line
    if(kCode == 'c') scrollH(2);   // scroll a line
    if(kCode == 'd') scrollH(3);   // scroll a line

    if(kCode == 'A') scrollHrev(0);   // scroll a line
    if(kCode == 'B') scrollHrev(1);   // scroll a line
    if(kCode == 'C') scrollHrev(2);   // scroll a line
    if(kCode == 'D') scrollHrev(3);   // scroll a line

    if(kCode == '1') scrollV(0);   // scroll a column
    if(kCode == '2') scrollV(1);   // scroll a column
    if(kCode == '3') scrollV(2);   // scroll a column
    if(kCode == '4') scrollV(3);   // scroll a column

    if(kCode == '!') scrollVrev(0);   // scroll a column
    if(kCode == '@') scrollVrev(1);   // scroll a column
    if(kCode == 34) scrollVrev(1);    // scroll a column " char non-international keyboards may need this
    if(kCode == 163) scrollVrev(2);   // scroll a column £ char
    if(kCode == '#') scrollVrev(2);   // scroll a column
    if(kCode == '$') scrollVrev(3);   // scroll a column

    if(kCode == 27) doExit();   // exit if ESC pressed
    if(kCode == 'q') return 0;   // end game, go to menu screen

      // check if game completed
    if(memcmp(arrayBitmaps, arrayOriginal, sizeof(arrayBitmaps)) == 0){
        // both the same, so image matches, player completed level
        return 1;
    };
  }
}

// -----------------------------------------------------------------------
//
// game over screen - Come here when puzzle completed
//
// -----------------------------------------------------------------------

void completedScreen(void){
  // draw message over screen
  vdp_cursor_tab(10,18);
  printf("+++++++++++++++++++\n");
  vdp_cursor_tab(10,19);
  printf("+                 +\n");
  vdp_cursor_tab(10,20);
  printf("+    Well Done    +\n");
  vdp_cursor_tab(10,21);
  printf("+                 +\n");
  vdp_cursor_tab(10,22);
  printf("+  Press Any Key  +\n");
  vdp_cursor_tab(10,23);
  printf("+                 +\n");
  vdp_cursor_tab(10,24);
  printf("+++++++++++++++++++\n");

  vdp_audio_play_sample(4,127);         // completed cound

  vdp_waitKeyDown();                    // wait for key down/up to continue
  if(vdp_getKeyCode() == 27) doExit();   // exit if ESC pressed
    vdp_waitKeyUp();

}

// -----------------------------------------------------------------------
//
// End of main screens
//
// -----------------------------------------------------------------------

// randomly slide x or y 'level' no. of times

void shufflePic(uint8_t level){
  delay(1000);
  for(uint8_t LL = 0; LL < level; LL++){
    uint8_t r = rand() % 8;
    switch (r) {
    case 0:
      scrollH(0);
      break;
    case 1:
      scrollH(1);
      break;
    case 2:
      scrollH(2);
      break;
    case 3:
      scrollH(3);
      break;
    case 4:
      scrollV(0);
      break;
    case 5:
      scrollV(1);
      break;
    case 6:
      scrollV(2);
      break;
    case 7:
      scrollV(3);
      break;
    }
    delay(500);
  }
}

// -----------------------------------------------------------------------
// capture screen area and stores in a bitmap
// horizontal capture

void captureBitmapH(uint8_t hNum){
  uint16_t topPos = hNum * chunkSizeH;
  uint16_t bottomPos = ((hNum + 1) * chunkSizeH) - 1;
  
  my_vdp_capture_bitmap(topPos,0,bottomPos, 319,captureBitmapID );                 // capture image
}

// -----------------------------------------------------------------------
// vertical capture

void captureBitmapV(uint8_t vNum){
  uint16_t leftPos = vNum * chunkSizeW;
  uint16_t rightPos = ((vNum + 1) * chunkSizeW) - 1;
  
  my_vdp_capture_bitmap(0, leftPos, 239,rightPos, captureBitmapID );                 // capture image
}

// -----------------------------------------------------------------------

void my_vdp_capture_bitmap(uint16_t top, uint16_t left, uint16_t bottom, uint16_t right, uint8_t bmID){

  vdp_move_to( left, top );            // define top left point
  vdp_move_to( right, bottom );         // define bottom right point
  
  // VDU 23, 27, 1, n, 0, 0;: Capture screen data into bitmap n *
  // not yet defined in vdp.h so do it with putch()

  putch(23);
  putch(27);
  putch(1);
  putch(bmID);           // defined as constant
  putch(0);
  putch(0);
  putch(0);
}

// -----------------------------------------------------------------------

void scrollH(uint8_t hNum){
  vdp_waitKeyUp();

 // capture bitmap
  captureBitmapH( hNum);
  vdp_select_bitmap(captureBitmapID);

// play sound
  vdp_audio_play_sample(3,127);
 
// animate bitmap
  for (uint16_t xx = 0; xx <= chunkSizeW ; xx += scrollJump){
      vdp_plot_bitmap( xx , hNum * chunkSizeH);          // plot the bitmap at 0,0
      vdp_plot_bitmap( xx - bitmapWidth , hNum * chunkSizeH);          // plot the bitmap at 0,0
      delay(scrollSpeed);
  }

  // grab id of each bitmap in row and transfer to next position
  // we get column number from 0-3
  uint8_t temp = arrayBitmaps[3][hNum];
  arrayBitmaps[3][hNum] = arrayBitmaps[2][hNum];
  arrayBitmaps[2][hNum] = arrayBitmaps[1][hNum];
  arrayBitmaps[1][hNum] = arrayBitmaps[0][hNum];
  arrayBitmaps[0][hNum] = temp;
  redrawBitmaps();

}
// -----------------------------------------------------------------------

void scrollHrev(uint8_t hNum){
  vdp_waitKeyUp();

 // capture bitmap
  captureBitmapH( hNum);
  vdp_select_bitmap(captureBitmapID);

// play sound
  vdp_audio_play_sample(3,127);
 
// animate bitmap
  for (uint16_t xx = 0; xx <= chunkSizeW ; xx += scrollJump){
      vdp_plot_bitmap( 0 - xx , hNum * chunkSizeH);          // plot the bitmap at 0,0
      vdp_plot_bitmap( 320 -xx , hNum * chunkSizeH);          // plot the bitmap at 0,0
      delay(scrollSpeed);
  }

  // grab id of each bitmap in row and transfer to next position
  // we get column number from 0-3
  uint8_t temp = arrayBitmaps[0][hNum];
  arrayBitmaps[0][hNum] = arrayBitmaps[1][hNum];
  arrayBitmaps[1][hNum] = arrayBitmaps[2][hNum];
  arrayBitmaps[2][hNum] = arrayBitmaps[3][hNum];
  arrayBitmaps[3][hNum] = temp;
  redrawBitmaps();

}

// -----------------------------------------------------------------------

void scrollV(uint8_t vNum){
  vdp_waitKeyUp();

   // capture bitmap
  captureBitmapV( vNum);
  vdp_select_bitmap(captureBitmapID);

  // play sound
  vdp_audio_play_sample(3,127);

// animate bitmap
  for (uint16_t yy = 0; yy <= chunkSizeH ; yy += scrollJump){
      vdp_plot_bitmap(vNum * chunkSizeW,  yy );          // plot the bitmap at 0,0
      vdp_plot_bitmap(vNum * chunkSizeW,  yy - bitmapHeight );          // plot the bitmap at 0,0
      delay(scrollSpeed);
  }

  // grab id of each itmap in row and transfer to next position
  // we get row number from 0-3

  uint8_t temp = arrayBitmaps[vNum][3];
  arrayBitmaps[vNum][3] = arrayBitmaps[vNum][2];
  arrayBitmaps[vNum][2] = arrayBitmaps[vNum][1];
  arrayBitmaps[vNum][1] = arrayBitmaps[vNum][0];
  arrayBitmaps[vNum][0] = temp;
  redrawBitmaps();

}

// -----------------------------------------------------------------------

void scrollVrev(uint8_t vNum){
  vdp_waitKeyUp();

   // capture bitmap
  captureBitmapV( vNum);
  vdp_select_bitmap(captureBitmapID);

  // play sound
  vdp_audio_play_sample(3,127);

// animate bitmap
  for (uint16_t yy = 0; yy <= chunkSizeH ; yy += scrollJump){
      vdp_plot_bitmap(vNum * chunkSizeW,  0 - yy );          // plot the bitmap at 0,0
      vdp_plot_bitmap(vNum * chunkSizeW,  240 - yy );          // plot the bitmap at 0,0
      delay(scrollSpeed);
  }

  // grab id of each itmap in row and transfer to next position
  // we get row number from 0-3

  uint8_t temp = arrayBitmaps[vNum][0];
  arrayBitmaps[vNum][0] = arrayBitmaps[vNum][1];
  arrayBitmaps[vNum][1] = arrayBitmaps[vNum][2];
  arrayBitmaps[vNum][2] = arrayBitmaps[vNum][3];
  arrayBitmaps[vNum][3] = temp;
  redrawBitmaps();

}

// -----------------------------------------------------------------------
// pre-load all the bitmap data needed and create bitmap buffers

void loadBitmaps(char bitmapName[])
{
  for (uint16_t bitmapNum = 0; bitmapNum < 24 ; bitmapNum++){
    vdp_adv_clear_buffer(startBigBitmapID + bitmapNum);                         // clear all buffers
  }

  FILE *filePointer = fopen(bitmapName, "r");                           // open the RGBA2222 image file
  uint16_t count = 0;

  // Load into 4 big rows first as separate buffers... then split each by width.
  // big buffers will be ID 20-23
  // smaller buffers will be 0-15

  // read all data into 4 bitmap buffers, in 320 byte blocks, ie 60 blocks each
  for (uint16_t rows = 0; rows < 4; rows++){
          fread(buff, 1,19200 ,filePointer);                                 // read 80 byte chunk of data from file
          vdp_adv_write_block_data((startBigBitmapID + rows), 19200, buff);       // send chunk into VDP buffer block 20 +
  }

  // we now have 4 big buffers 320x60 of data
  // Command 20: Split by width into blocks and spread across blocks starting at target buffer ID

  vdp_adv_split_by_width_multiple_from(startBigBitmapID + 0, chunkSizeW, chunksPerLine, startBitmapID + 0);
  vdp_adv_split_by_width_multiple_from(startBigBitmapID + 1, chunkSizeW, chunksPerLine, startBitmapID + 4);
  vdp_adv_split_by_width_multiple_from(startBigBitmapID + 2, chunkSizeW, chunksPerLine, startBitmapID + 8);
  vdp_adv_split_by_width_multiple_from(startBigBitmapID + 3, chunkSizeW, chunksPerLine, startBitmapID + 12);

  // turn all 16 buffers into bitmaps
  for (uint16_t xx = 0; xx < 16 ; xx++){
    vdp_adv_select_bitmap(startBitmapID + xx);
    vdp_adv_bitmap_from_buffer(80 , 60, RGBA2222_format);    // make a bitmap to test
  }

  // clear original data as no longer needed
  for (uint16_t bitmapNum = startBigBitmapID; bitmapNum < startBigBitmapID + 4 ; bitmapNum++){
    vdp_adv_clear_buffer(bitmapNum);                         // clear the buffer
  }
  fclose(filePointer);                                       // close the file
}

// -----------------------------------------------------------------------

void loadLabels(void){
  // now load labels from labels.h and convert to sprites

  makeLabel( label_sprite_start_ID + 0, label1, 32, 220);
  makeLabel( label_sprite_start_ID + 1, label2, 112, 220);
  makeLabel( label_sprite_start_ID + 2, label3, 192, 220);
  makeLabel( label_sprite_start_ID + 3, label4, 273, 220);

  makeLabel( label_sprite_start_ID + 4, labelA, 4, 20);
  makeLabel( label_sprite_start_ID + 5, labelB, 4, 85);
  makeLabel( label_sprite_start_ID + 6, labelC, 4, 142);
  makeLabel( label_sprite_start_ID + 7, labelD, 4, 202);
  
  // activate the sprites
  vdp_activate_sprites(numSprites);
  vdp_refresh_sprites();
}

// -----------------------------------------------------------------------

void makeLabel(uint16_t id, char name[] ,uint16_t xxx, uint16_t yyy ){
  vdp_select_bitmap(id);
  vdp_load_bitmap( 16, 16, (uint8_t *) name );
  vdp_select_sprite(id);
  vdp_clear_sprite();
  vdp_add_sprite_bitmap(id);

  // enable hardware sprite test flag
  // VDU 23, 0, &F8, 2; 1;
  
  vdp_set_variable(2,1);

// set to be hardware sprite
// VDU 23, 27, 19: Set sprite to be a hardware sprite ***

  vdp_set_hardware_sprite();
  vdp_move_sprite_to( xxx, yyy );
}

// -----------------------------------------------------------------------
// plot bitmaps at current positions in array

void redrawBitmaps(void){
uint16_t thisBitmap = 0;

  for (uint16_t xx = 0; xx < hCells ; xx++){
    for (uint16_t yy = 0; yy < vCells ; yy++){
      thisBitmap = arrayBitmaps[xx][yy];
      vdp_adv_select_bitmap(thisBitmap + startBitmapID);
      vdp_plot_bitmap( xx * chunkSizeW, yy * chunkSizeH);          // plot the bitmap at 0,0
    }
  }
}

// -----------------------------------------------------------------------
// hide/show routines

void hideSprites(void){
  for (uint8_t xx = 0; xx < numSprites ; xx++){ 
    vdp_select_sprite(xx);
    vdp_hide_sprite();
  }
  vdp_refresh_sprites();
}

void showSprites(void){
  for (uint8_t xx = 0; xx < numSprites ; xx++){ 
    vdp_select_sprite(xx);
    vdp_show_sprite();
  }
  vdp_refresh_sprites();
}

// -----------------------------------------------------------------------
// UDGs used for menu screen arrows

void setupUDG(void){

  vdp_redefine_character_special(128,
    0b00001000,
    0b00001100,
    0b00001110,
    0b00001111,
    0b00001111,
    0b00001110,
    0b00001100,
    0b00001000);

    vdp_redefine_character_special(129,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b11111111,
    0b01111110,
    0b00111100,
    0b00011000);

    vdp_redefine_character_special(130,
    0b00010000,
    0b00110000,
    0b01110000,
    0b11111111,
    0b11111111,
    0b01110000,
    0b00110000,
    0b00010000);

    vdp_redefine_character_special(131,
    0b00010000,
    0b00011000,
    0b00011100,
    0b11111110,
    0b11111110,
    0b00011100,
    0b00011000,
    0b00010000);
  }

// -----------------------------------------------------------------------
//
// exit
//
// -----------------------------------------------------------------------

void doExit(void){
  //vdp_clear_screen();
  vdp_cursor_enable(true);
  exit(0);
}

// -----------------------------------------------------------------------
