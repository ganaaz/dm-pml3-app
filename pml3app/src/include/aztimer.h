#ifndef TIME_H
#define TIME_H

#include <stdlib.h>
#include <sys/timerfd.h>

typedef enum
{
    TIMER_SINGLE_SHOT = 0, /*Periodic Timer*/
    TIMER_PERIODIC         /*Single Shot Timer*/
} t_timer;

/**
 * Event Handler
 */
typedef void (*time_handler)(size_t timer_id, void *user_data);

/**
 * Timer data
 **/
struct timer_node
{
    int fd;
    time_handler callback;
    void *user_data;
    unsigned int interval;
    t_timer type;
    struct timer_node *next;
};

/**
 * Initialize the timer
 **/
int initializeTimer();

/**
 * Start the timer with the call back handler
 **/
size_t startTimer(unsigned int interval, time_handler handler, t_timer type, void *user_data);

/**
 * Stop the timer
 **/
void stopTimer(size_t timer_id);

/**
 * Cleanup the timer
 **/
void finalizeTimer();

#endif