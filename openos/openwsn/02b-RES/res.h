/**
\brief Implementation of the IEEE802.15.4e RES layer

\author Thomas Watteyne <watteyne@eecs.berkeley.edu>, August 2011
*/

#ifndef __RES_H
#define __RES_H

/**
\addtogroup MAChigh
\{
\addtogroup RES
\{
*/

//=========================== define ==========================================

//=========================== typedef =========================================

//=========================== variables =======================================

//=========================== prototypes ======================================

void    res_init();
bool    res_debugPrint();
// from upper layer
error_t res_send(OpenQueueEntry_t *msg);
// from lower layer
void    task_resNotifSendDone();
void    res_sendDone(OpenQueueEntry_t* msg, error_t error);
void    task_resNotifReceive();
void    res_receive(OpenQueueEntry_t* msg);

/**
\}
\}
*/

#endif