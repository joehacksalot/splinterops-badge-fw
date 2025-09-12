
#ifndef INTERACTIVE_GAME_H_
#define INTERACTIVE_GAME_H_

typedef struct InteractiveGameBits_t
{
  uint16_t feather1 : 1;
  uint16_t feather2 : 1;
  uint16_t feather3 : 1;
  uint16_t feather4 : 1;
  uint16_t feather5 : 1;
  uint16_t feather6 : 1;
  uint16_t feather7 : 1;
  uint16_t feather8 : 1;
  uint16_t feather9 : 1;
  uint16_t unused   : 5;
  uint16_t lastFailed : 1;
  uint16_t active   : 1;
} InteractiveGameBits;

typedef union
{
  InteractiveGameBits s;
  uint16_t u;
} InteractiveGameData;

#endif // INTERACTIVE_GAME_H_
