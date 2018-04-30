/* Macros */

/* Includes */
#include <signal.h>
#include <string.h>

/* Own includes */
#include "../include/llmnr_defs.h"
#include "../include/llmnr_signals.h"

/* Enums & Structs */

/* Private functions prototypes */
PRIVATE void ignoreSignal(int signo);
PRIVATE sighandler mySignal(int signo, sighandler functionHandler);

/* Glocal variables */

/* Functions definitions */
PUBLIC void handleSignals()
{
    /*
     * Set a handler for all possible signals. In this case
     * the handling is just ignore them. However some signals
     * receive a "special" treatment
     */
        int signo;

        for (signo = 1; signo <= 31; signo++) {
                if (signo != SIGKILL && signo != SIGSTOP &&
                    signo != SIGTERM && signo != SIGUSR2)
                mySignal(signo,ignoreSignal);
        }
}

PUBLIC void handleSpecSignals(int signo, sighandler funcHandler)
{
    /*
     * Handle "special" signals. Special means use another
     * function handler for this signals
     */
        if (signo == SIGTERM || signo == SIGUSR2)
                mySignal(signo,funcHandler);
}

PRIVATE sighandler mySignal(int signo, sighandler functionHandler)
{
    /*
     * Set the function handler for a given signal.
     * If the signal is "SIGUSR1" then set SA_INTERRUPT flag
     * For any other signal set SA_RESTART flag (restart syscalls)
     */
        struct sigaction act, oact;

        act.sa_handler = functionHandler;
        sigemptyset(&act.sa_mask);
        act.sa_flags = 0;
        if (signo == SIGUSR1)
                act.sa_flags |= SA_INTERRUPT;
        else
                act.sa_flags |= SA_RESTART;
        if (sigaction(signo,&act,&oact) < 0)
                return SIG_ERR;
        return oact.sa_handler;
}

PUBLIC void sendSignal(const pthread_t tid)
{
    /*
     * Send 'SIGUSR1' signal to thread 'tid'
     * This is used to wake up the main thread
     * and check for conflicts
     */
        pthread_kill(tid,SIGUSR1);
}

PRIVATE void ignoreSignal(int signo)
{
    /*
     * Do nothing
     */
        signo = signo;
        return;
}

/*PRIVATE int isntSignal(int signo)
{
        switch (signo) {
        case SIGHUP:
                break;
        case SIGINT:
                break;
        case SIGQUIT:
                break;
        case SIGILL:
                break;
        case SIGABRT:
                break;
        case SIGFPE:
                break;
        case SIGKILL:
                break;
        case SIGSEGV:
                break;
        case SIGPIPE:
                break;
        case SIGALRM:
                break;
        case SIGTERM:
                break;
        case SIGUSR1:
                break;
        case SIGUSR2:
                break;
        case SIGCHLD:
                break;
        case SIGCONT:
                break;
        case SIGSTOP:
                break;
        case SIGTSTP:
                break;
        case SIGTTIN:
                break;
        case SIGTTOU:
                break;
        default:
                return FAILURE;
        }
        return SUCCESS;
}*/
