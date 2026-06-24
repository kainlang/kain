#ifndef STORM_UI_H
#define STORM_UI_H

int storm_init(void);
int storm_poll(void);
void storm_frame(int, int, int, int, int, int, int, int, int, int);
void storm_exit(void);

#endif
