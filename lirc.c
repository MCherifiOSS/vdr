/*
 * lirc.c: LIRC remote control
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * LIRC support added by Carsten Koch <Carsten.Koch@icem.de>  2000-06-16.
 *
 * $Id: lirc.c 1.1 2002/09/29 13:16:33 kls Exp $
 */

#include "lirc.h"
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/un.h>

#define REPEATLIMIT  20 // ms
#define REPEATDELAY 350 // ms

cLircRemote::cLircRemote(char *DeviceName)
:cRemote("LIRC")
{
  struct sockaddr_un addr;
  addr.sun_family = AF_UNIX;
  strcpy(addr.sun_path, DeviceName);
  if ((f = socket(AF_UNIX, SOCK_STREAM, 0)) >= 0) {
     if (connect(f, (struct sockaddr *)&addr, sizeof(addr)) >= 0) {
        Start();
        return;
        }
     LOG_ERROR_STR(DeviceName);
     close(f);
     }
  else
     LOG_ERROR_STR(DeviceName);
  f = -1;
}

cLircRemote::~cLircRemote()
{
  Cancel();
}

void cLircRemote::Action(void)
{
  dsyslog("LIRC remote control thread started (pid=%d)", getpid());

  int FirstTime = 0;
  int LastTime = 0;
  char buf[LIRC_BUFFER_SIZE];
  char LastKeyName[LIRC_KEY_BUF] = "";
  bool repeat = false;

  for (; f >= 0;) {

      LOCK_THREAD;

      if (cFile::FileReady(f, REPEATLIMIT) && safe_read(f, buf, sizeof(buf)) > 21) {
         int count;
         char KeyName[LIRC_KEY_BUF];
         sscanf(buf, "%*x %x %29s", &count, KeyName); // '29' in '%29s' is LIRC_KEY_BUF-1!
         int Now = time_ms();
         if (count == 0) {
            strcpy(LastKeyName, KeyName);
            repeat = false;
            FirstTime = Now;
            }
         else {
            if (Now - FirstTime < REPEATDELAY)
               continue; // repeat function kicks in after a short delay
            repeat = true;
            }
         LastTime = Now;
         Put(KeyName, repeat);
         }
      else if (repeat) { // the last one was a repeat, so let's generate a release
         if (time_ms() - LastTime > REPEATDELAY) {
            Put(LastKeyName, false, true);
            repeat = false;
            *LastKeyName = 0;
            }
         }
      }
}