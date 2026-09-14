#ifndef	__BEEP_H_
#define	__BEEP_H_

#include "main.h"
#include "music.h"

extern const uint8_t* music;

void beep(uint16_t time);
void beeping(void);
void beep_play_music(const uint8_t* music);
void play_music(const uint8_t* music_list);

#endif
