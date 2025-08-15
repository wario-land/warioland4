#ifndef MAIN_H
#define MAIN_H

typedef void (*IntrFunc)(void);
typedef void (*MainInterruptHandlerFunc)(void);

extern const IntrFunc gInterruptVectorTable[];

struct InterruptHandler
{
    MainInterruptHandlerFunc gVBlankInterruptHandlerFunc;
    MainInterruptHandlerFunc gHBlankInterruptHandlerFunc;
    MainInterruptHandlerFunc gVCountInterruptHandlerFunc;
};

extern struct InterruptHandler gInterruptHandler;

void AgbMain(void);
void AllReset(void);

#endif // MAIN_H
