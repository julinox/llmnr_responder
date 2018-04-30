/** *********************************************
 * Interface to set the overall signal handling *
 ************************************************/

#ifndef LLMNR_SIGNALS_H
#define LLMNR_SIGNALS_H

typedef void (*sighandler)(int);
PUBLIC void sendSignal(const pthread_t tid);
PUBLIC void handleSignals();
PUBLIC void handleSpecSignals(int signo, sighandler funcHandler);

#endif
