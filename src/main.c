/*
Example of:
  1) using a buffer to store a large RGBA2222 colour bitmap 
  2) load in chunks from file
  3) used buffer commands to split a buffer horizontally
  4) use screen capture to bitmap function (currently missing from vdp.h)
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

// constants used
const uint8_t  screen_mode = 8;
const uint8_t  RGBA2222_format = 1;
const uint8_t  captureBitmapID = 30;
const uint16_t startBitmapID = 40;
const uint16_t startBigBitmapID = 20;
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

// for file handling
DIR handle;
FILINFO file;

// some global variables used
char dirpath[256];
char myFiles[10][32];
char fname[32];
char directoryName[] = "puzzles/";
char buff[320*60];
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
void vdp_capture_bitmap(void);
void makeLabel(uint16_t id, char name[],uint16_t xxx, uint16_t yyy );
void hideSprites(void);
void showSprites(void);
void setupUDG(void);
void shufflePic(uint8_t level);
void loadLabels(void);

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
      
  // check for folder of puzzle images
  if(ffs_getcwd(dirpath, 256) != 0) {
    printf("Unable to get current directory\r\n");
    return 0;
  }

  strcat(dirpath, "puzzles/");        // look in subn-directory 'puzzles/'
  if(ffs_dopen(&handle, dirpath) != 0) {
    printf("Unable to open '%s'\r\n", dirpath);
    return 0;
  }
  uint8_t fileCount=0;
  while(1) {
    uint8_t result = ffs_dread(&handle, &file); 
    if((result != 0) || (strlen(file.fname) == 0)) break; // end of list
    if(fileCount > 9) break;                              // we already have enough files
    if(file.fattrib & 0x10) printf("DIR %s\r\n", file.fname); // dir not a file
    else {
      strcpy(myFiles[fileCount], file.fname);             // add this file
      fileCount++;
    }
    numPuzzles = fileCount ;
  }

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

  vdp_cursor_tab(0,20);
  vdp_set_text_colour(BRIGHT_BLUE);

  printf("            A%c X X X X\n\n", 128);
  printf("            B%c X X X X\n\n", 128);
  printf("            C%c X X X X\n\n", 128);
  printf("            D%c X X X X\n\n", 128);


  vdp_cursor_tab(0,17);
  vdp_set_text_colour(BRIGHT_CYAN);

  printf("               1 2 3 4\n");
  printf("               %c %c %c %c \n\n", 129, 129, 129, 129);
  printf("            A%c\n\n",128);
  printf("            B%c\n\n",128);
  printf("            C%c\n\n",128);
  printf("            D%c\n\n",128);

  vdp_set_text_colour(BRIGHT_WHITE);
  vdp_cursor_tab(0,29);
  printf("Puzzle: %s             " ,myFiles[currentPuzzleNum]);

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
    if(vdp_getKeyCode() == 's') {           // find next bitmap file
      char thisFile[32];
      currentPuzzleNum ++;
      if (currentPuzzleNum > numPuzzles-1) currentPuzzleNum = 0;
      strcpy(thisFile, directoryName);
      strcat(thisFile, myFiles[currentPuzzleNum]);
      strcpy(fname, thisFile);
      vdp_cursor_tab(0,29);
      printf("Puzzle: %s            " ,myFiles[currentPuzzleNum]);
      loadBitmaps(fname);         // reload data
      vdp_waitKeyUp();
    };   
    
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
  
  vdp_move_to( 0, topPos );             // define top left point
  vdp_move_to( 319, bottomPos );        // define bottom right point
  vdp_capture_bitmap();                 // capture image
}

// -----------------------------------------------------------------------
// vertical capture

void captureBitmapV(uint8_t vNum){
  uint16_t leftPos = vNum * chunkSizeW;
  uint16_t rightPos = ((vNum + 1) * chunkSizeW) - 1;
  
  vdp_move_to( leftPos, 0 );            // define top left point
  vdp_move_to( rightPos, 239 );         // define bottom right point
  vdp_capture_bitmap();                 // capture image
}

// -----------------------------------------------------------------------

void vdp_capture_bitmap(void){
  // VDU 23, 27, 1, n, 0, 0;: Capture screen data into bitmap n *
  // not yet defined in vdp.h so do it with putch()

  putch(23);
  putch(27);
  putch(1);
  putch(captureBitmapID);           // defined as constant
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
      vdp_plot_bitmap( 320 -xx - bitmapWidth , hNum * chunkSizeH);          // plot the bitmap at 0,0
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
      vdp_plot_bitmap(vNum * chunkSizeW,  240 - yy - bitmapHeight );          // plot the bitmap at 0,0
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
          vdp_adv_write_block_data((20 + rows), 19200, buff);       // send chunk into VDP buffer block 20 +
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

  // enable hardware test flag
  // VDU 23, 0, &F8, 2; 1;
  // not yet defined in vdp.h
  putch(23);
  putch(0);
  putch(0xF8);
  putch(2);
  putch(0);
  putch(1);
  putch(0);
  
// set to be hardware sprite
// VDU 23, 27, 19: Set sprite to be a hardware sprite ***
// not yet defined in vdp.h
  putch(23);
  putch(27);
  putch(19);

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
// sprite routines

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
}

// -----------------------------------------------------------------------
//
// exit
//
// -----------------------------------------------------------------------

void doExit(void){
  vdp_clear_screen();
  vdp_cursor_enable(true);
  exit(0);
}

// -----------------------------------------------------------------------
