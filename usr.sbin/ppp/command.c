/*
 *		PPP User command processing module
 *
 *	    Written by Toshiharu OHNO (tony-o@iij.ad.jp)
 *
 *   Copyright (C) 1993, Internet Initiative Japan, Inc. All rights reserverd.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the Internet Initiative Japan, Inc.  The name of the
 * IIJ may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * $Id: command.c,v 1.102 1997/11/13 14:43:14 brian Exp $
 *
 */
#include <sys/param.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <net/route.h>
#include <netdb.h>

#include <alias.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "mbuf.h"
#include "log.h"
#include "defs.h"
#include "timer.h"
#include "fsm.h"
#include "phase.h"
#include "lcp.h"
#include "ipcp.h"
#include "modem.h"
#include "command.h"
#include "filter.h"
#include "alias_cmd.h"
#include "hdlc.h"
#include "loadalias.h"
#include "vars.h"
#include "systems.h"
#include "chat.h"
#include "os.h"
#include "server.h"
#include "main.h"
#include "route.h"
#include "ccp.h"
#include "ip.h"
#include "slcompress.h"
#include "auth.h"

struct in_addr ifnetmask;

static int ShowCommand(struct cmdtab const *, int, char **);
static int TerminalCommand(struct cmdtab const *, int, char **);
static int QuitCommand(struct cmdtab const *, int, char **);
static int CloseCommand(struct cmdtab const *, int, char **);
static int DialCommand(struct cmdtab const *, int, char **);
static int DownCommand(struct cmdtab const *, int, char **);
static int AllowCommand(struct cmdtab const *, int, char **);
static int SetCommand(struct cmdtab const *, int, char **);
static int AddCommand(struct cmdtab const *, int, char **);
static int DeleteCommand(struct cmdtab const *, int, char **);
static int BgShellCommand(struct cmdtab const *, int, char **);
static int FgShellCommand(struct cmdtab const *, int, char **);
static int ShellCommand(struct cmdtab const *, int, char **, int);
static int AliasCommand(struct cmdtab const *, int, char **);
static int AliasEnable(struct cmdtab const *, int, char **);
static int AliasOption(struct cmdtab const *, int, char **, void *);

static int
HelpCommand(struct cmdtab const * list,
	    int argc,
	    char **argv,
	    struct cmdtab const * plist)
{
  struct cmdtab const *cmd;
  int n;

  if (!VarTerm)
    return 0;

  if (argc > 0) {
    for (cmd = plist; cmd->name; cmd++)
      if (strcasecmp(cmd->name, *argv) == 0 && (cmd->lauth & VarLocalAuth)) {
	fprintf(VarTerm, "%s\n", cmd->syntax);
	return 0;
      }
    return -1;
  }
  n = 0;
  for (cmd = plist; cmd->func; cmd++)
    if (cmd->name && (cmd->lauth & VarLocalAuth)) {
      fprintf(VarTerm, "  %-9s: %-20s\n", cmd->name, cmd->helpmes);
      n++;
    }
  if (n & 1)
    fprintf(VarTerm, "\n");

  return 0;
}

int
IsInteractive(int Display)
{
  char *mes = NULL;

  if (mode & MODE_DDIAL)
    mes = "Working in dedicated dial mode.";
  else if (mode & MODE_BACKGROUND)
    mes = "Working in background mode.";
  else if (mode & MODE_AUTO)
    mes = "Working in auto mode.";
  else if (mode & MODE_DIRECT)
    mes = "Working in direct mode.";
  else if (mode & MODE_DEDICATED)
    mes = "Working in dedicated mode.";
  if (mes) {
    if (Display && VarTerm)
      fprintf(VarTerm, "%s\n", mes);
    return 0;
  }
  return 1;
}

static int
DialCommand(struct cmdtab const * cmdlist, int argc, char **argv)
{
  int tries;
  int res;

  if (LcpFsm.state > ST_CLOSED) {
    if (VarTerm)
      fprintf(VarTerm, "LCP state is [%s]\n", StateNames[LcpFsm.state]);
    return 0;
  }

  if (argc > 0 && (res = LoadCommand(cmdlist, argc, argv)) != 0)
    return res;

  tries = 0;
  do {
    if (VarTerm)
      fprintf(VarTerm, "Dial attempt %u of %d\n", ++tries, VarDialTries);
    if (OpenModem() < 0) {
      if (VarTerm)
	fprintf(VarTerm, "Failed to open modem.\n");
      break;
    }
    if ((res = DialModem()) == EX_DONE) {
      nointr_sleep(1);
      ModemTimeout();
      PacketMode();
      break;
    } else if (res == EX_SIG)
      return 1;
  } while (VarDialTries == 0 || tries < VarDialTries);

  return 0;
}

static int
SetLoopback(struct cmdtab const * cmdlist, int argc, char **argv)
{
  if (argc == 1)
    if (!strcasecmp(*argv, "on")) {
      VarLoopback = 1;
      return 0;
    }
    else if (!strcasecmp(*argv, "off")) {
      VarLoopback = 0;
      return 0;
    }
  return -1;
}

static int
BgShellCommand(struct cmdtab const * cmdlist, int argc, char **argv)
{
  if (argc == 0)
    return -1;
  return ShellCommand(cmdlist, argc, argv, 1);
}

static int
FgShellCommand(struct cmdtab const * cmdlist, int argc, char **argv)
{
  return ShellCommand(cmdlist, argc, argv, 0);
}

static int
ShellCommand(struct cmdtab const * cmdlist, int argc, char **argv, int bg)
{
  const char *shell;
  pid_t shpid;
  FILE *oVarTerm;

#ifdef SHELL_ONLY_INTERACTIVELY
  /* we're only allowed to shell when we run ppp interactively */
  if (mode != MODE_INTER) {
    LogPrintf(LogWARN, "Can only start a shell in interactive mode\n");
    return 1;
  }
#endif
#ifdef NO_SHELL_IN_AUTO_INTERACTIVE

  /*
   * we want to stop shell commands when we've got a telnet connection to an
   * auto mode ppp
   */
  if (VarTerm && !(mode & MODE_INTER)) {
    LogPrintf(LogWARN, "Shell is not allowed interactively in auto mode\n");
    return 1;
  }
#endif

  if (argc == 0)
    if (!(mode & MODE_INTER)) {
      if (VarTerm)
        LogPrintf(LogWARN, "Can't start an interactive shell from"
		  " a telnet session\n");
      else
        LogPrintf(LogWARN, "Can only start an interactive shell in"
		  " interactive mode\n");
      return 1;
    } else if (bg) {
      LogPrintf(LogWARN, "Can only start an interactive shell in"
		" the foreground mode\n");
      return 1;
    }
  if ((shell = getenv("SHELL")) == 0)
    shell = _PATH_BSHELL;

  if ((shpid = fork()) == 0) {
    int dtablesize, i, fd;

    if (VarTerm)
      fd = fileno(VarTerm);
    else if ((fd = open("/dev/null", O_RDWR)) == -1) {
      LogPrintf(LogALERT, "Failed to open /dev/null: %s\n", strerror(errno));
      exit(1);
    }
    for (i = 0; i < 3; i++)
      dup2(fd, i);

    if (fd > 2)
      if (VarTerm) {
	oVarTerm = VarTerm;
	VarTerm = 0;
	if (oVarTerm && oVarTerm != stdout)
	  fclose(oVarTerm);
      } else
	close(fd);

    for (dtablesize = getdtablesize(), i = 3; i < dtablesize; i++)
      (void) close(i);

    TtyOldMode();
    setuid(geteuid());
    if (argc > 0) {
      /* substitute pseudo args */
      for (i = 1; i < argc; i++)
	if (strcasecmp(argv[i], "HISADDR") == 0)
	  argv[i] = strdup(inet_ntoa(IpcpInfo.his_ipaddr));
	else if (strcasecmp(argv[i], "INTERFACE") == 0)
	  argv[i] = strdup(IfDevName);
	else if (strcasecmp(argv[i], "MYADDR") == 0)
	  argv[i] = strdup(inet_ntoa(IpcpInfo.want_ipaddr));
      if (bg) {
	pid_t p;

	p = getpid();
	if (daemon(1, 1) == -1) {
	  LogPrintf(LogERROR, "%d: daemon: %s\n", p, strerror(errno));
	  exit(1);
	}
      } else if (VarTerm)
        fprintf(VarTerm, "ppp: Pausing until %s finishes\n", argv[0]);
      (void) execvp(argv[0], argv);
    } else {
      if (VarTerm)
        fprintf(VarTerm, "ppp: Pausing until %s finishes\n", shell);
      (void) execl(shell, shell, NULL);
    }

    LogPrintf(LogWARN, "exec() of %s failed\n", argc > 0 ? argv[0] : shell);
    exit(255);
  }
  if (shpid == (pid_t) - 1) {
    LogPrintf(LogERROR, "Fork failed: %s\n", strerror(errno));
  } else {
    int status;

    (void) waitpid(shpid, &status, 0);
  }

  TtyCommandMode(1);

  return (0);
}

static struct cmdtab const Commands[] = {
  {"accept", NULL, AcceptCommand, LOCAL_AUTH,
  "accept option request", "accept option .."},
  {"add", NULL, AddCommand, LOCAL_AUTH,
  "add route", "add dest mask gateway"},
  {"allow", "auth", AllowCommand, LOCAL_AUTH,
  "Allow ppp access", "allow users|modes ...."},
  {"bg", "!bg", BgShellCommand, LOCAL_AUTH,
  "Run a command in the background", "[!]bg command"},
  {"close", NULL, CloseCommand, LOCAL_AUTH,
  "Close connection", "close"},
  {"delete", NULL, DeleteCommand, LOCAL_AUTH,
  "delete route", "delete ALL | dest [gateway [mask]]"},
  {"deny", NULL, DenyCommand, LOCAL_AUTH,
  "Deny option request", "deny option .."},
  {"dial", "call", DialCommand, LOCAL_AUTH,
  "Dial and login", "dial|call [remote]"},
  {"disable", NULL, DisableCommand, LOCAL_AUTH,
  "Disable option", "disable option .."},
  {"display", NULL, DisplayCommand, LOCAL_AUTH,
  "Display option configs", "display"},
  {"enable", NULL, EnableCommand, LOCAL_AUTH,
  "Enable option", "enable option .."},
  {"passwd", NULL, LocalAuthCommand, LOCAL_NO_AUTH,
  "Password for manipulation", "passwd LocalPassword"},
  {"load", NULL, LoadCommand, LOCAL_AUTH,
  "Load settings", "load [remote]"},
  {"save", NULL, SaveCommand, LOCAL_AUTH,
  "Save settings", "save"},
  {"set", "setup", SetCommand, LOCAL_AUTH,
  "Set parameters", "set[up] var value"},
  {"shell", "!", FgShellCommand, LOCAL_AUTH,
  "Run a subshell", "shell|! [sh command]"},
  {"show", NULL, ShowCommand, LOCAL_AUTH,
  "Show status and statistics", "show var"},
  {"term", NULL, TerminalCommand, LOCAL_AUTH,
  "Enter to terminal mode", "term"},
  {"alias", NULL, AliasCommand, LOCAL_AUTH,
  "alias control", "alias option [yes|no]"},
  {"quit", "bye", QuitCommand, LOCAL_AUTH | LOCAL_NO_AUTH,
  "Quit PPP program", "quit|bye [all]"},
  {"help", "?", HelpCommand, LOCAL_AUTH | LOCAL_NO_AUTH,
  "Display this message", "help|? [command]", (void *) Commands},
  {NULL, "down", DownCommand, LOCAL_AUTH,
  "Generate down event", "down"},
  {NULL, NULL, NULL},
};

static int
ShowLoopback()
{
  if (VarTerm)
    fprintf(VarTerm, "Local loopback is %s\n", VarLoopback ? "on" : "off");

  return 0;
}

static int
ShowLogLevel()
{
  int i;

  if (!VarTerm)
    return 0;

  fprintf(VarTerm, "Log:  ");
  for (i = LogMIN; i <= LogMAX; i++)
    if (LogIsKept(i) & LOG_KEPT_SYSLOG)
      fprintf(VarTerm, " %s", LogName(i));

  fprintf(VarTerm, "\nLocal:");
  for (i = LogMIN; i <= LogMAX; i++)
    if (LogIsKept(i) & LOG_KEPT_LOCAL)
      fprintf(VarTerm, " %s", LogName(i));

  fprintf(VarTerm, "\n");

  return 0;
}

static int
ShowEscape()
{
  int code, bit;

  if (!VarTerm)
    return 0;
  if (EscMap[32]) {
    for (code = 0; code < 32; code++)
      if (EscMap[code])
	for (bit = 0; bit < 8; bit++)
	  if (EscMap[code] & (1 << bit))
	    fprintf(VarTerm, " 0x%02x", (code << 3) + bit);
    fprintf(VarTerm, "\n");
  }
  return 0;
}

static int
ShowTimeout()
{
  if (VarTerm)
    fprintf(VarTerm, " Idle Timer: %d secs   LQR Timer: %d secs"
	    "   Retry Timer: %d secs\n", VarIdleTimeout, VarLqrTimeout,
	    VarRetryTimeout);
  return 0;
}

static int
ShowStopped()
{
  if (!VarTerm)
    return 0;

  fprintf(VarTerm, " Stopped Timer:  LCP: ");
  if (!LcpFsm.StoppedTimer.load)
    fprintf(VarTerm, "Disabled");
  else
    fprintf(VarTerm, "%ld secs", LcpFsm.StoppedTimer.load / SECTICKS);

  fprintf(VarTerm, ", IPCP: ");
  if (!IpcpFsm.StoppedTimer.load)
    fprintf(VarTerm, "Disabled");
  else
    fprintf(VarTerm, "%ld secs", IpcpFsm.StoppedTimer.load / SECTICKS);

  fprintf(VarTerm, ", CCP: ");
  if (!CcpFsm.StoppedTimer.load)
    fprintf(VarTerm, "Disabled");
  else
    fprintf(VarTerm, "%ld secs", CcpFsm.StoppedTimer.load / SECTICKS);

  fprintf(VarTerm, "\n");

  return 0;
}

static int
ShowAuthKey()
{
  if (!VarTerm)
    return 0;
  fprintf(VarTerm, "AuthName = %s\n", VarAuthName);
  fprintf(VarTerm, "AuthKey  = %s\n", VarAuthKey);
#ifdef HAVE_DES
  fprintf(VarTerm, "Encrypt  = %s\n", VarMSChap ? "MSChap" : "MD5" );
#endif
  return 0;
}

static int
ShowVersion()
{
  if (VarTerm)
    fprintf(VarTerm, "%s - %s \n", VarVersion, VarLocalVersion);
  return 0;
}

static int
ShowInitialMRU()
{
  if (VarTerm)
    fprintf(VarTerm, " Initial MRU: %ld\n", VarMRU);
  return 0;
}

static int
ShowPreferredMTU()
{
  if (VarTerm)
    if (VarPrefMTU)
      fprintf(VarTerm, " Preferred MTU: %ld\n", VarPrefMTU);
    else
      fprintf(VarTerm, " Preferred MTU: unspecified\n");
  return 0;
}

static int
ShowReconnect()
{
  if (VarTerm)
    fprintf(VarTerm, " Reconnect Timer:  %d,  %d tries\n",
	    VarReconnectTimer, VarReconnectTries);
  return 0;
}

static int
ShowRedial()
{
  if (!VarTerm)
    return 0;
  fprintf(VarTerm, " Redial Timer: ");

  if (VarRedialTimeout >= 0) {
    fprintf(VarTerm, " %d seconds, ", VarRedialTimeout);
  } else {
    fprintf(VarTerm, " Random 0 - %d seconds, ", REDIAL_PERIOD);
  }

  fprintf(VarTerm, " Redial Next Timer: ");

  if (VarRedialNextTimeout >= 0) {
    fprintf(VarTerm, " %d seconds, ", VarRedialNextTimeout);
  } else {
    fprintf(VarTerm, " Random 0 - %d seconds, ", REDIAL_PERIOD);
  }

  if (VarDialTries)
    fprintf(VarTerm, "%d dial tries", VarDialTries);

  fprintf(VarTerm, "\n");

  return 0;
}

#ifndef NOMSEXT
static int
ShowMSExt()
{
  if (VarTerm) {
    fprintf(VarTerm, " MS PPP extention values \n");
    fprintf(VarTerm, "   Primary NS     : %s\n", inet_ntoa(ns_entries[0]));
    fprintf(VarTerm, "   Secondary NS   : %s\n", inet_ntoa(ns_entries[1]));
    fprintf(VarTerm, "   Primary NBNS   : %s\n", inet_ntoa(nbns_entries[0]));
    fprintf(VarTerm, "   Secondary NBNS : %s\n", inet_ntoa(nbns_entries[1]));
  }
  return 0;
}

#endif

static struct cmdtab const ShowCommands[] = {
  {"afilter", NULL, ShowAfilter, LOCAL_AUTH,
  "Show keep Alive filters", "show afilter option .."},
  {"auth", NULL, ShowAuthKey, LOCAL_AUTH,
  "Show auth name, key and algorithm", "show auth"},
  {"ccp", NULL, ReportCcpStatus, LOCAL_AUTH,
  "Show CCP status", "show cpp"},
  {"compress", NULL, ReportCompress, LOCAL_AUTH,
  "Show compression statistics", "show compress"},
  {"dfilter", NULL, ShowDfilter, LOCAL_AUTH,
  "Show Demand filters", "show dfilteroption .."},
  {"escape", NULL, ShowEscape, LOCAL_AUTH,
  "Show escape characters", "show escape"},
  {"hdlc", NULL, ReportHdlcStatus, LOCAL_AUTH,
  "Show HDLC error summary", "show hdlc"},
  {"ifilter", NULL, ShowIfilter, LOCAL_AUTH,
  "Show Input filters", "show ifilter option .."},
  {"ipcp", NULL, ReportIpcpStatus, LOCAL_AUTH,
  "Show IPCP status", "show ipcp"},
  {"lcp", NULL, ReportLcpStatus, LOCAL_AUTH,
  "Show LCP status", "show lcp"},
  {"loopback", NULL, ShowLoopback, LOCAL_AUTH,
  "Show current loopback setting", "show loopback"},
  {"log", NULL, ShowLogLevel, LOCAL_AUTH,
  "Show current log level", "show log"},
  {"mem", NULL, ShowMemMap, LOCAL_AUTH,
  "Show memory map", "show mem"},
  {"modem", NULL, ShowModemStatus, LOCAL_AUTH,
  "Show modem setups", "show modem"},
  {"mru", NULL, ShowInitialMRU, LOCAL_AUTH,
  "Show Initial MRU", "show mru"},
  {"mtu", NULL, ShowPreferredMTU, LOCAL_AUTH,
  "Show Preferred MTU", "show mtu"},
  {"ofilter", NULL, ShowOfilter, LOCAL_AUTH,
  "Show Output filters", "show ofilter option .."},
  {"proto", NULL, ReportProtStatus, LOCAL_AUTH,
  "Show protocol summary", "show proto"},
  {"reconnect", NULL, ShowReconnect, LOCAL_AUTH,
  "Show Reconnect timer,tries", "show reconnect"},
  {"redial", NULL, ShowRedial, LOCAL_AUTH,
  "Show Redial timeout value", "show redial"},
  {"route", NULL, ShowRoute, LOCAL_AUTH,
  "Show routing table", "show route"},
  {"timeout", NULL, ShowTimeout, LOCAL_AUTH,
  "Show Idle timeout value", "show timeout"},
  {"stopped", NULL, ShowStopped, LOCAL_AUTH,
  "Show STOPPED timeout value", "show stopped"},
#ifndef NOMSEXT
  {"msext", NULL, ShowMSExt, LOCAL_AUTH,
  "Show MS PPP extentions", "show msext"},
#endif
  {"version", NULL, ShowVersion, LOCAL_NO_AUTH | LOCAL_AUTH,
  "Show version string", "show version"},
  {"help", "?", HelpCommand, LOCAL_NO_AUTH | LOCAL_AUTH,
  "Display this message", "show help|? [command]", (void *) ShowCommands},
  {NULL, NULL, NULL},
};

static struct cmdtab const *
FindCommand(struct cmdtab const * cmds, char *str, int *pmatch)
{
  int nmatch;
  int len;
  struct cmdtab const *found;

  found = NULL;
  len = strlen(str);
  nmatch = 0;
  while (cmds->func) {
    if (cmds->name && strncasecmp(str, cmds->name, len) == 0) {
      if (cmds->name[len] == '\0') {
	*pmatch = 1;
	return cmds;
      }
      nmatch++;
      found = cmds;
    } else if (cmds->alias && strncasecmp(str, cmds->alias, len) == 0) {
      if (cmds->alias[len] == '\0') {
	*pmatch = 1;
	return cmds;
      }
      nmatch++;
      found = cmds;
    }
    cmds++;
  }
  *pmatch = nmatch;
  return found;
}

static int
FindExec(struct cmdtab const * cmdlist, int argc, char **argv)
{
  struct cmdtab const *cmd;
  int val = 1;
  int nmatch;

  cmd = FindCommand(cmdlist, *argv, &nmatch);
  if (nmatch > 1)
    LogPrintf(LogWARN, "%s: Ambiguous command\n", *argv);
  else if (cmd && (cmd->lauth & VarLocalAuth))
    val = (cmd->func) (cmd, argc-1, argv+1, cmd->args);
  else
    LogPrintf(LogWARN, "%s: Invalid command\n", *argv);

  if (val == -1)
    LogPrintf(LogWARN, "Usage: %s\n", cmd->syntax);
  else if (val)
    LogPrintf(LogCOMMAND, "%s: Failed %d\n", *argv, val);

  return val;
}

int aft_cmd = 1;

void
Prompt()
{
  char *pconnect, *pauth;

  if (!VarTerm || TermMode)
    return;

  if (!aft_cmd)
    fprintf(VarTerm, "\n");
  else
    aft_cmd = 0;

  if (VarLocalAuth == LOCAL_AUTH)
    pauth = " ON ";
  else
    pauth = " on ";
  if (IpcpFsm.state == ST_OPENED && phase == PHASE_NETWORK)
    pconnect = "PPP";
  else
    pconnect = "ppp";
  fprintf(VarTerm, "%s%s%s> ", pconnect, pauth, VarShortHost);
  fflush(VarTerm);
}

void
InterpretCommand(char *buff, int nb, int *argc, char ***argv)
{
  static char *vector[40];
  char *cp;

  if (nb > 0) {
    cp = buff + strcspn(buff, "\r\n");
    if (cp)
      *cp = '\0';
    *argc = MakeArgs(buff, vector, VECSIZE(vector));
    *argv = vector;
  } else
    *argc = 0;
}

void
RunCommand(int argc, char **argv, const char *label)
{
  if (argc > 0) {
    if (LogIsKept(LogCOMMAND)) {
      static char buf[LINE_LEN];
      int f, n;

      *buf = '\0';
      if (label) {
        strcpy(buf, label);
        strcat(buf, ": ");
      }
      n = strlen(buf);
      for (f = 0; f < argc; f++) {
        if (n < sizeof(buf)-1 && f)
          buf[n++] = ' ';
        strncpy(buf+n, argv[f], sizeof(buf)-n-1);
        n += strlen(buf+n);
      }
      LogPrintf(LogCOMMAND, "%s\n", buf);
    }
    FindExec(Commands, argc, argv);
  }
}

void
DecodeCommand(char *buff, int nb, const char *label)
{
  int argc;
  char **argv;

  InterpretCommand(buff, nb, &argc, &argv);
  RunCommand(argc, argv, label);
}

static int
ShowCommand(struct cmdtab const * list, int argc, char **argv)
{
  if (argc > 0)
    FindExec(ShowCommands, argc, argv);
  else if (VarTerm)
    fprintf(VarTerm, "Use ``show ?'' to get a list.\n");
  else
    LogPrintf(LogWARN, "show command must have arguments\n");

  return 0;
}

static int
TerminalCommand(struct cmdtab const * list, int argc, char **argv)
{
  if (LcpFsm.state > ST_CLOSED) {
    if (VarTerm)
      fprintf(VarTerm, "LCP state is [%s]\n", StateNames[LcpFsm.state]);
    return 1;
  }
  if (!IsInteractive(1))
    return (1);
  if (OpenModem() < 0) {
    if (VarTerm)
      fprintf(VarTerm, "Failed to open modem.\n");
    return (1);
  }
  if (VarTerm) {
    fprintf(VarTerm, "Enter to terminal mode.\n");
    fprintf(VarTerm, "Type `~?' for help.\n");
  }
  TtyTermMode();
  return (0);
}

static int
QuitCommand(struct cmdtab const * list, int argc, char **argv)
{
  if (VarTerm) {
    DropClient();
    if (mode & MODE_INTER)
      Cleanup(EX_NORMAL);
    else if (argc > 0 && !strcasecmp(*argv, "all") && VarLocalAuth&LOCAL_AUTH)
      Cleanup(EX_NORMAL);
  }

  return 0;
}

static int
CloseCommand(struct cmdtab const * list, int argc, char **argv)
{
  reconnect(RECON_FALSE);
  LcpClose();
  return 0;
}

static int
DownCommand(struct cmdtab const * list, int argc, char **argv)
{
  LcpDown();
  return 0;
}

static int
SetModemSpeed(struct cmdtab const * list, int argc, char **argv)
{
  int speed;

  if (argc > 0) {
    if (strcasecmp(*argv, "sync") == 0) {
      VarSpeed = 0;
      return 0;
    }
    speed = atoi(*argv);
    if (IntToSpeed(speed) != B0) {
      VarSpeed = speed;
      return 0;
    }
    LogPrintf(LogWARN, "%s: Invalid speed\n", *argv);
  }
  return -1;
}

static int
SetReconnect(struct cmdtab const * list, int argc, char **argv)
{
  if (argc == 2) {
    VarReconnectTimer = atoi(argv[0]);
    VarReconnectTries = atoi(argv[1]);
    return 0;
  }
  return -1;
}

static int
SetRedialTimeout(struct cmdtab const * list, int argc, char **argv)
{
  int timeout;
  int tries;
  char *dot;

  if (argc == 1 || argc == 2) {
    if (strncasecmp(argv[0], "random", 6) == 0 &&
	(argv[0][6] == '\0' || argv[0][6] == '.')) {
      VarRedialTimeout = -1;
      randinit();
    } else {
      timeout = atoi(argv[0]);

      if (timeout >= 0)
	VarRedialTimeout = timeout;
      else {
	LogPrintf(LogWARN, "Invalid redial timeout\n");
	return -1;
      }
    }

    dot = strchr(argv[0], '.');
    if (dot) {
      if (strcasecmp(++dot, "random") == 0) {
	VarRedialNextTimeout = -1;
	randinit();
      } else {
	timeout = atoi(dot);
	if (timeout >= 0)
	  VarRedialNextTimeout = timeout;
	else {
	  LogPrintf(LogWARN, "Invalid next redial timeout\n");
	  return -1;
	}
      }
    } else
      VarRedialNextTimeout = NEXT_REDIAL_PERIOD;	/* Default next timeout */

    if (argc == 2) {
      tries = atoi(argv[1]);

      if (tries >= 0) {
	VarDialTries = tries;
      } else {
	LogPrintf(LogWARN, "Invalid retry value\n");
	return 1;
      }
    }
    return 0;
  }
  return -1;
}

static int
SetStoppedTimeout(struct cmdtab const * list, int argc, char **argv)
{
  LcpFsm.StoppedTimer.load = 0;
  IpcpFsm.StoppedTimer.load = 0;
  CcpFsm.StoppedTimer.load = 0;
  if (argc <= 3) {
    if (argc > 0) {
      LcpFsm.StoppedTimer.load = atoi(argv[0]) * SECTICKS;
      if (argc > 1) {
	IpcpFsm.StoppedTimer.load = atoi(argv[1]) * SECTICKS;
	if (argc > 2)
	  CcpFsm.StoppedTimer.load = atoi(argv[2]) * SECTICKS;
      }
    }
    return 0;
  }
  return -1;
}

#define ismask(x) \
  (*x == '0' && strlen(x) == 4 && strspn(x+1, "0123456789.") == 3)

static int
SetServer(struct cmdtab const * list, int argc, char **argv)
{
  int res = -1;

  if (argc > 0 && argc < 4) {
    const char *port, *passwd, *mask;

    /* What's what ? */
    port = argv[0];
    if (argc == 2)
      if (ismask(argv[1])) {
        passwd = NULL;
        mask = argv[1];
      } else {
        passwd = argv[1];
        mask = NULL;
      }
    else if (argc == 3) {
      passwd = argv[1];
      mask = argv[2];
      if (!ismask(mask))
        return -1;
    } else
      passwd = mask = NULL;

    if (passwd == NULL)
      VarHaveLocalAuthKey = 0;
    else {
      strncpy(VarLocalAuthKey, passwd, sizeof VarLocalAuthKey);
      VarLocalAuthKey[sizeof VarLocalAuthKey - 1] = '\0';
      VarHaveLocalAuthKey = 1;
    }
    LocalAuthInit();

    if (strcasecmp(port, "none") == 0) {
      int oserver;

      if (mask != NULL || passwd != NULL)
        return -1;
      oserver = server;
      ServerClose();
      if (oserver != -1)
        LogPrintf(LogPHASE, "Disabling server port.\n");
      res = 0;
    } else if (*port == '/') {
      mode_t imask;

      if (mask != NULL) {
	unsigned m;

	if (sscanf(mask, "%o", &m) == 1)
	  imask = m;
        else
          return -1;
      } else
        imask = (mode_t)-1;
      res = ServerLocalOpen(port, imask);
    } else {
      int iport;

      if (mask != NULL)
        return -1;

      if (strspn(port, "0123456789") != strlen(port)) {
        struct servent *s;

        if ((s = getservbyname(port, "tcp")) == NULL) {
	  iport = 0;
	  LogPrintf(LogWARN, "%s: Invalid port or service\n", port);
	} else
	  iport = ntohs(s->s_port);
      } else
        iport = atoi(port);
      res = iport ? ServerTcpOpen(iport) : -1;
    }
  }

  return res;
}

static int
SetModemParity(struct cmdtab const * list, int argc, char **argv)
{
  return argc > 0 ? ChangeParity(*argv) : -1;
}

static int
SetLogLevel(struct cmdtab const * list, int argc, char **argv)
{
  int i;
  int res;
  char *arg;
  void (*Discard)(int), (*Keep)(int);
  void (*DiscardAll)(void);

  res = 0;
  if (strcasecmp(argv[0], "local")) {
    Discard = LogDiscard;
    Keep = LogKeep;
    DiscardAll = LogDiscardAll;
  } else {
    argc--;
    argv++;
    Discard = LogDiscardLocal;
    Keep = LogKeepLocal;
    DiscardAll = LogDiscardAllLocal;
  }

  if (argc == 0 || (argv[0][0] != '+' && argv[0][0] != '-'))
    (*DiscardAll)();
  while (argc--) {
    arg = **argv == '+' || **argv == '-' ? *argv + 1 : *argv;
    for (i = LogMIN; i <= LogMAX; i++)
      if (strcasecmp(arg, LogName(i)) == 0) {
	if (**argv == '-')
	  (*Discard)(i);
	else
	  (*Keep)(i);
	break;
      }
    if (i > LogMAX) {
      LogPrintf(LogWARN, "%s: Invalid log value\n", arg);
      res = -1;
    }
    argv++;
  }
  return res;
}

static int
SetEscape(struct cmdtab const * list, int argc, char **argv)
{
  int code;

  for (code = 0; code < 33; code++)
    EscMap[code] = 0;
  while (argc-- > 0) {
    sscanf(*argv++, "%x", &code);
    code &= 0xff;
    EscMap[code >> 3] |= (1 << (code & 7));
    EscMap[32] = 1;
  }
  return 0;
}

static int
SetInitialMRU(struct cmdtab const * list, int argc, char **argv)
{
  long mru;
  char *err;

  if (argc > 0) {
    mru = atol(*argv);
    if (mru < MIN_MRU)
      err = "Given MRU value (%ld) is too small.\n";
    else if (mru > MAX_MRU)
      err = "Given MRU value (%ld) is too big.\n";
    else {
      VarMRU = mru;
      return 0;
    }
    LogPrintf(LogWARN, err, mru);
  }
  return -1;
}

static int
SetPreferredMTU(struct cmdtab const * list, int argc, char **argv)
{
  long mtu;
  char *err;

  if (argc > 0) {
    mtu = atol(*argv);
    if (mtu == 0) {
      VarPrefMTU = 0;
      return 0;
    } else if (mtu < MIN_MTU)
      err = "Given MTU value (%ld) is too small.\n";
    else if (mtu > MAX_MTU)
      err = "Given MTU value (%ld) is too big.\n";
    else {
      VarPrefMTU = mtu;
      return 0;
    }
    LogPrintf(LogWARN, err, mtu);
  }
  return -1;
}

static int
SetIdleTimeout(struct cmdtab const * list, int argc, char **argv)
{
  if (argc-- > 0) {
    VarIdleTimeout = atoi(*argv++);
    UpdateIdleTimer();		/* If we're connected, restart the idle timer */
    if (argc-- > 0) {
      VarLqrTimeout = atoi(*argv++);
      if (VarLqrTimeout < 1)
	VarLqrTimeout = 30;
      if (argc > 0) {
	VarRetryTimeout = atoi(*argv);
	if (VarRetryTimeout < 1 || VarRetryTimeout > 10)
	  VarRetryTimeout = 3;
      }
    }
    return 0;
  }
  return -1;
}

static struct in_addr
GetIpAddr(char *cp)
{
  struct hostent *hp;
  struct in_addr ipaddr;

  hp = gethostbyname(cp);
  if (hp && hp->h_addrtype == AF_INET)
    memcpy(&ipaddr, hp->h_addr, hp->h_length);
  else if (inet_aton(cp, &ipaddr) == 0)
    ipaddr.s_addr = 0;
  return (ipaddr);
}

static int
SetInterfaceAddr(struct cmdtab const * list, int argc, char **argv)
{
  DefMyAddress.ipaddr.s_addr = DefHisAddress.ipaddr.s_addr = 0L;

  if (argc > 4)
    return -1;

  HaveTriggerAddress = 0;
  ifnetmask.s_addr = 0;

  if (argc > 0) {
    if (ParseAddr(argc, argv++,
		  &DefMyAddress.ipaddr,
		  &DefMyAddress.mask,
		  &DefMyAddress.width) == 0)
      return 1;
    if (--argc > 0) {
      if (ParseAddr(argc, argv++,
		    &DefHisAddress.ipaddr,
		    &DefHisAddress.mask,
		    &DefHisAddress.width) == 0)
	return 2;
      if (--argc > 0) {
	ifnetmask = GetIpAddr(*argv);
	if (--argc > 0) {
	  TriggerAddress = GetIpAddr(*argv);
	  HaveTriggerAddress = 1;
	}
      }
    }
  }

  /*
   * For backwards compatibility, 0.0.0.0 means any address.
   */
  if (DefMyAddress.ipaddr.s_addr == 0) {
    DefMyAddress.mask.s_addr = 0;
    DefMyAddress.width = 0;
  }
  if (DefHisAddress.ipaddr.s_addr == 0) {
    DefHisAddress.mask.s_addr = 0;
    DefHisAddress.width = 0;
  }
  IpcpInfo.want_ipaddr.s_addr = DefMyAddress.ipaddr.s_addr;
  IpcpInfo.his_ipaddr.s_addr = DefHisAddress.ipaddr.s_addr;

  if ((mode & MODE_AUTO) &&
      OsSetIpaddress(DefMyAddress.ipaddr, DefHisAddress.ipaddr, ifnetmask) < 0)
    return 4;

  return 0;
}

#ifndef NOMSEXT

static void
SetMSEXT(struct in_addr * pri_addr,
	 struct in_addr * sec_addr,
	 int argc,
	 char **argv)
{
  int dummyint;
  struct in_addr dummyaddr;

  pri_addr->s_addr = sec_addr->s_addr = 0L;

  if (argc > 0) {
    ParseAddr(argc, argv++, pri_addr, &dummyaddr, &dummyint);
    if (--argc > 0)
      ParseAddr(argc, argv++, sec_addr, &dummyaddr, &dummyint);
    else
      sec_addr->s_addr = pri_addr->s_addr;
  }

  /*
   * if the primary/secondary ns entries are 0.0.0.0 we should set them to
   * either the localhost's ip, or the values in /etc/resolv.conf ??
   * 
   * up to you if you want to implement this...
   */

}

static int
SetNS(struct cmdtab const * list, int argc, char **argv)
{
  SetMSEXT(&ns_entries[0], &ns_entries[1], argc, argv);
  return 0;
}

static int
SetNBNS(struct cmdtab const * list, int argc, char **argv)
{
  SetMSEXT(&nbns_entries[0], &nbns_entries[1], argc, argv);
  return 0;
}

#endif				/* MS_EXT */

int
SetVariable(struct cmdtab const * list, int argc, char **argv, int param)
{
  u_long map;
  char *arg;

  if (argc > 0)
    arg = *argv;
  else
    arg = "";

  switch (param) {
  case VAR_AUTHKEY:
    strncpy(VarAuthKey, arg, sizeof(VarAuthKey) - 1);
    VarAuthKey[sizeof(VarAuthKey) - 1] = '\0';
    break;
  case VAR_AUTHNAME:
    strncpy(VarAuthName, arg, sizeof(VarAuthName) - 1);
    VarAuthName[sizeof(VarAuthName) - 1] = '\0';
    break;
  case VAR_DIAL:
    strncpy(VarDialScript, arg, sizeof(VarDialScript) - 1);
    VarDialScript[sizeof(VarDialScript) - 1] = '\0';
    break;
  case VAR_LOGIN:
    strncpy(VarLoginScript, arg, sizeof(VarLoginScript) - 1);
    VarLoginScript[sizeof(VarLoginScript) - 1] = '\0';
    break;
  case VAR_DEVICE:
    if (modem != -1)
      LogPrintf(LogWARN, "Cannot change device to \"%s\" when \"%s\" is open\n",
                arg, VarDevice);
    else {
      strncpy(VarDevice, arg, sizeof(VarDevice) - 1);
      VarDevice[sizeof(VarDevice) - 1] = '\0';
      VarBaseDevice = strrchr(VarDevice, '/');
      VarBaseDevice = VarBaseDevice ? VarBaseDevice + 1 : "";
    }
    break;
  case VAR_ACCMAP:
    sscanf(arg, "%lx", &map);
    VarAccmap = map;
    break;
  case VAR_PHONE:
    strncpy(VarPhoneList, arg, sizeof(VarPhoneList) - 1);
    VarPhoneList[sizeof(VarPhoneList) - 1] = '\0';
    strcpy(VarPhoneCopy, VarPhoneList);
    VarNextPhone = VarPhoneCopy;
    VarAltPhone = NULL;
    break;
  case VAR_HANGUP:
    strncpy(VarHangupScript, arg, sizeof(VarHangupScript) - 1);
    VarHangupScript[sizeof(VarHangupScript) - 1] = '\0';
    break;
#ifdef HAVE_DES
  case VAR_ENC:
    VarMSChap = !strcasecmp(arg, "mschap");
    break;
#endif
  }
  return 0;
}

static int 
SetCtsRts(struct cmdtab const * list, int argc, char **argv)
{
  if (argc > 0) {
    if (strcmp(*argv, "on") == 0)
      VarCtsRts = 1;
    else if (strcmp(*argv, "off") == 0)
      VarCtsRts = 0;
    else
      return -1;
    return 0;
  }
  return -1;
}


static int 
SetOpenMode(struct cmdtab const * list, int argc, char **argv)
{
  if (argc > 0) {
    if (strcmp(*argv, "active") == 0)
      VarOpenMode = OPEN_ACTIVE;
    else if (strcmp(*argv, "passive") == 0)
      VarOpenMode = OPEN_PASSIVE;
    else
      return -1;
    return 0;
  }
  return -1;
}

static struct cmdtab const SetCommands[] = {
  {"accmap", NULL, SetVariable, LOCAL_AUTH,
  "Set accmap value", "set accmap hex-value", (void *) VAR_ACCMAP},
  {"afilter", NULL, SetAfilter, LOCAL_AUTH,
  "Set keep Alive filter", "set afilter ..."},
  {"authkey", "key", SetVariable, LOCAL_AUTH,
  "Set authentication key", "set authkey|key key", (void *) VAR_AUTHKEY},
  {"authname", NULL, SetVariable, LOCAL_AUTH,
  "Set authentication name", "set authname name", (void *) VAR_AUTHNAME},
  {"ctsrts", NULL, SetCtsRts, LOCAL_AUTH,
  "Use CTS/RTS modem signalling", "set ctsrts [on|off]"},
  {"device", "line", SetVariable, LOCAL_AUTH,
  "Set modem device name", "set device|line device-name", (void *) VAR_DEVICE},
  {"dfilter", NULL, SetDfilter, LOCAL_AUTH,
  "Set demand filter", "set dfilter ..."},
  {"dial", NULL, SetVariable, LOCAL_AUTH,
  "Set dialing script", "set dial chat-script", (void *) VAR_DIAL},
#ifdef HAVE_DES
  {"encrypt", NULL, SetVariable, LOCAL_AUTH,
  "Set CHAP encryption algorithm", "set encrypt MSChap|MD5", (void *) VAR_ENC},
#endif
  {"escape", NULL, SetEscape, LOCAL_AUTH,
  "Set escape characters", "set escape hex-digit ..."},
  {"hangup", NULL, SetVariable, LOCAL_AUTH,
  "Set hangup script", "set hangup chat-script", (void *) VAR_HANGUP},
  {"ifaddr", NULL, SetInterfaceAddr, LOCAL_AUTH,
  "Set destination address", "set ifaddr [src-addr [dst-addr [netmask [trg-addr]]]]"},
  {"ifilter", NULL, SetIfilter, LOCAL_AUTH,
  "Set input filter", "set ifilter ..."},
  {"loopback", NULL, SetLoopback, LOCAL_AUTH,
  "Set loopback facility", "set loopback on|off"},
  {"log", NULL, SetLogLevel, LOCAL_AUTH,
  "Set log level", "set log [local] [+|-]value..."},
  {"login", NULL, SetVariable, LOCAL_AUTH,
  "Set login script", "set login chat-script", (void *) VAR_LOGIN},
  {"mru", NULL, SetInitialMRU, LOCAL_AUTH,
  "Set Initial MRU value", "set mru value"},
  {"mtu", NULL, SetPreferredMTU, LOCAL_AUTH,
  "Set Preferred MTU value", "set mtu value"},
  {"ofilter", NULL, SetOfilter, LOCAL_AUTH,
  "Set output filter", "set ofilter ..."},
  {"openmode", NULL, SetOpenMode, LOCAL_AUTH,
  "Set open mode", "set openmode [active|passive]"},
  {"parity", NULL, SetModemParity, LOCAL_AUTH,
  "Set modem parity", "set parity [odd|even|none]"},
  {"phone", NULL, SetVariable, LOCAL_AUTH,
  "Set telephone number(s)", "set phone phone1[:phone2[...]]", (void *) VAR_PHONE},
  {"reconnect", NULL, SetReconnect, LOCAL_AUTH,
  "Set Reconnect timeout", "set reconnect value ntries"},
  {"redial", NULL, SetRedialTimeout, LOCAL_AUTH,
  "Set Redial timeout", "set redial value|random[.value|random] [dial_attempts]"},
  {"stopped", NULL, SetStoppedTimeout, LOCAL_AUTH,
  "Set STOPPED timeouts", "set stopped [LCPseconds [IPCPseconds [CCPseconds]]]"},
  {"server", "socket", SetServer, LOCAL_AUTH,
  "Set server port", "set server|socket TcpPort|LocalName|none [mask]"},
  {"speed", NULL, SetModemSpeed, LOCAL_AUTH,
  "Set modem speed", "set speed value"},
  {"timeout", NULL, SetIdleTimeout, LOCAL_AUTH,
  "Set Idle timeout", "set timeout value"},
#ifndef NOMSEXT
  {"ns", NULL, SetNS, LOCAL_AUTH,
  "Set NameServer", "set ns pri-addr [sec-addr]"},
  {"nbns", NULL, SetNBNS, LOCAL_AUTH,
  "Set NetBIOS NameServer", "set nbns pri-addr [sec-addr]"},
#endif
  {"help", "?", HelpCommand, LOCAL_AUTH | LOCAL_NO_AUTH,
  "Display this message", "set help|? [command]", (void *) SetCommands},
  {NULL, NULL, NULL},
};

static int
SetCommand(struct cmdtab const * list, int argc, char **argv)
{
  if (argc > 0)
    FindExec(SetCommands, argc, argv);
  else if (VarTerm)
    fprintf(VarTerm, "Use `set ?' to get a list or `set ? <var>' for"
	    " syntax help.\n");
  else
    LogPrintf(LogWARN, "set command must have arguments\n");

  return 0;
}


static int
AddCommand(struct cmdtab const * list, int argc, char **argv)
{
  struct in_addr dest, gateway, netmask;

  if (argc == 3) {
    if (strcasecmp(argv[0], "MYADDR") == 0)
      dest = IpcpInfo.want_ipaddr;
    else
      dest = GetIpAddr(argv[0]);
    netmask = GetIpAddr(argv[1]);
    if (strcasecmp(argv[2], "HISADDR") == 0)
      gateway = IpcpInfo.his_ipaddr;
    else
      gateway = GetIpAddr(argv[2]);
    OsSetRoute(RTM_ADD, dest, gateway, netmask);
    return 0;
  }
  return -1;
}

static int
DeleteCommand(struct cmdtab const * list, int argc, char **argv)
{
  struct in_addr dest, gateway, netmask;

  if (argc == 1 && strcasecmp(argv[0], "all") == 0)
    DeleteIfRoutes(0);
  else if (argc > 0 && argc < 4) {
    if (strcasecmp(argv[0], "MYADDR") == 0)
      dest = IpcpInfo.want_ipaddr;
    else
      dest = GetIpAddr(argv[0]);
    netmask.s_addr = INADDR_ANY;
    if (argc > 1) {
      if (strcasecmp(argv[1], "HISADDR") == 0)
	gateway = IpcpInfo.his_ipaddr;
      else
	gateway = GetIpAddr(argv[1]);
      if (argc == 3) {
	if (inet_aton(argv[2], &netmask) == 0) {
	  LogPrintf(LogWARN, "Bad netmask value.\n");
	  return -1;
	}
      }
    } else
      gateway.s_addr = INADDR_ANY;
    OsSetRoute(RTM_DELETE, dest, gateway, netmask);
  } else
    return -1;

  return 0;
}

static struct cmdtab const AliasCommands[] =
{
  {"enable", NULL, AliasEnable, LOCAL_AUTH,
  "enable IP aliasing", "alias enable [yes|no]"},
  {"port", NULL, AliasRedirectPort, LOCAL_AUTH,
  "port redirection", "alias port [proto addr_local:port_local  port_alias]"},
  {"addr", NULL, AliasRedirectAddr, LOCAL_AUTH,
  "static address translation", "alias addr [addr_local addr_alias]"},
  {"deny_incoming", NULL, AliasOption, LOCAL_AUTH,
    "stop incoming connections", "alias deny_incoming [yes|no]",
  (void *) PKT_ALIAS_DENY_INCOMING},
  {"log", NULL, AliasOption, LOCAL_AUTH,
    "log aliasing link creation", "alias log [yes|no]",
  (void *) PKT_ALIAS_LOG},
  {"same_ports", NULL, AliasOption, LOCAL_AUTH,
    "try to leave port numbers unchanged", "alias same_ports [yes|no]",
  (void *) PKT_ALIAS_SAME_PORTS},
  {"use_sockets", NULL, AliasOption, LOCAL_AUTH,
    "allocate host sockets", "alias use_sockets [yes|no]",
  (void *) PKT_ALIAS_USE_SOCKETS},
  {"unregistered_only", NULL, AliasOption, LOCAL_AUTH,
    "alias unregistered (private) IP address space only",
    "alias unregistered_only [yes|no]",
  (void *) PKT_ALIAS_UNREGISTERED_ONLY},
  {"help", "?", HelpCommand, LOCAL_AUTH | LOCAL_NO_AUTH,
    "Display this message", "alias help|? [command]",
  (void *) AliasCommands},
  {NULL, NULL, NULL},
};


static int
AliasCommand(struct cmdtab const * list, int argc, char **argv)
{
  if (argc > 0)
    FindExec(AliasCommands, argc, argv);
  else if (VarTerm)
    fprintf(VarTerm, "Use `alias help' to get a list or `alias help <option>'"
	    " for syntax help.\n");
  else
    LogPrintf(LogWARN, "alias command must have arguments\n");

  return 0;
}

static int
AliasEnable(struct cmdtab const * list, int argc, char **argv)
{
  if (argc == 1)
    if (strcasecmp(argv[0], "yes") == 0) {
      if (!(mode & MODE_ALIAS)) {
	if (loadAliasHandlers(&VarAliasHandlers) == 0) {
	  mode |= MODE_ALIAS;
	  return 0;
	}
	LogPrintf(LogWARN, "Cannot load alias library\n");
	return 1;
      }
      return 0;
    } else if (strcasecmp(argv[0], "no") == 0) {
      if (mode & MODE_ALIAS) {
	unloadAliasHandlers();
	mode &= ~MODE_ALIAS;
      }
      return 0;
    }
  return -1;
}


static int
AliasOption(struct cmdtab const * list, int argc, char **argv, void *param)
{
  if (argc == 1)
    if (strcasecmp(argv[0], "yes") == 0) {
      if (mode & MODE_ALIAS) {
	VarPacketAliasSetMode((unsigned) param, (unsigned) param);
	return 0;
      }
      LogPrintf(LogWARN, "alias not enabled\n");
    } else if (strcmp(argv[0], "no") == 0) {
      if (mode & MODE_ALIAS) {
	VarPacketAliasSetMode(0, (unsigned) param);
	return 0;
      }
      LogPrintf(LogWARN, "alias not enabled\n");
    }
  return -1;
}

static struct cmdtab const AllowCommands[] = {
  {"users", "user", AllowUsers, LOCAL_AUTH,
  "Allow users access to ppp", "allow users logname..."},
  {"modes", "mode", AllowModes, LOCAL_AUTH,
  "Only allow certain ppp modes", "allow modes mode..."},
  {"help", "?", HelpCommand, LOCAL_AUTH | LOCAL_NO_AUTH,
  "Display this message", "allow help|? [command]", (void *)AllowCommands},
  {NULL, NULL, NULL},
};

static int
AllowCommand(struct cmdtab const *list, int argc, char **argv)
{
  if (argc > 0)
    FindExec(AllowCommands, argc, argv);
  else if (VarTerm)
    fprintf(VarTerm, "Use `allow ?' to get a list or `allow ? <cmd>' for"
	    " syntax help.\n");
  else
    LogPrintf(LogWARN, "allow command must have arguments\n");

  return 0;
}
