/**
\brief UDP Helicopter application

\author Thomas Watteyne <watteyne@eecs.berkeley.edu>, September 2010
*/

#ifndef __APPUDPHELI_H
#define __APPUDPHELI_H

//=========================== define ==========================================

//=========================== typedef =========================================

//=========================== variables =======================================

//=========================== prototypes ======================================

void appudpheli_init();
void appudpheli_trigger();
void appudpheli_sendDone(OpenQueueEntry_t* msg, error_t error);
void appudpheli_receive(OpenQueueEntry_t* msg);
bool appudpheli_debugPrint();

#endif
