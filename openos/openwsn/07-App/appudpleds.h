/**
\brief UDP LEDs application

\author Ankur Mehta <mehtank@eecs.berkeley.edu>, September 2010
*/

#ifndef __APPUDPLEDS_H
#define __APPUDPLEDS_H

//=========================== define ==========================================

//=========================== typedef =========================================

//=========================== variables =======================================

//=========================== prototypes ======================================

void appudpleds_init();
void appudpleds_trigger();
void appudpleds_sendDone(OpenQueueEntry_t* msg, error_t error);
void appudpleds_receive(OpenQueueEntry_t* msg);
bool appudpleds_debugPrint();

#endif
