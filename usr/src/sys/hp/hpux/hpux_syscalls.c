/*
 * System call names.
 *
 * DO NOT EDIT-- this file is automatically generated.
 * created from	%W% (Berkeley) %G%
 */

char *hpuxsyscallnames[] = {
	"#0",			/* 0 = indir or out-of-range */
	"exit",			/* 1 = exit */
	"fork",			/* 2 = fork */
	"read",			/* 3 = read */
	"write",			/* 4 = write */
	"open",			/* 5 = open */
	"close",			/* 6 = close */
	"owait",			/* 7 = owait */
	"ocreat",			/* 8 = ocreat */
	"link",			/* 9 = link */
	"unlink",			/* 10 = unlink */
	"execv",			/* 11 = execv */
	"chdir",			/* 12 = chdir */
	"old.time",		/* 13 = old time */
	"mknod",			/* 14 = mknod */
	"chmod",			/* 15 = chmod */
	"chown",			/* 16 = chown */
	"break",			/* 17 = break */
	"old.stat",		/* 18 = old stat */
	"compat_43_lseek",			/* 19 = compat_43_lseek */
	"getpid",			/* 20 = getpid */
	"mount",			/* 21 = mount (unimplemented) */
	"umount",			/* 22 = umount (unimplemented) */
	"setuid",			/* 23 = setuid */
	"getuid",			/* 24 = getuid */
	"old.stime",		/* 25 = old stime */
	"ptrace",			/* 26 = ptrace */
	"old.alarm",		/* 27 = old alarm */
	"old.fstat",		/* 28 = old fstat */
	"old.pause",		/* 29 = old pause */
	"old.utime",		/* 30 = old utime */
	"old.stty",		/* 31 = old stty */
	"old.gtty",		/* 32 = old gtty */
	"access",			/* 33 = access */
	"old.nice",		/* 34 = old nice */
	"old.ftime",		/* 35 = old ftime */
	"sync",			/* 36 = sync */
	"kill",			/* 37 = kill */
	"stat",			/* 38 = stat */
	"old.setpgrp",		/* 39 = old setpgrp */
	"lstat",			/* 40 = lstat */
	"dup",			/* 41 = dup */
	"pipe",			/* 42 = pipe */
	"old.times",		/* 43 = old times */
	"profil",			/* 44 = profil */
	"ki_syscall",			/* 45 = ki_syscall (unimplemented) */
	"setgid",			/* 46 = setgid */
	"getgid",			/* 47 = getgid */
	"old.ssig",		/* 48 = old ssig */
	"#49",			/* 49 = reserved for USG */
	"#50",			/* 50 = reserved for USG */
	"acct",			/* 51 = acct (unimplemented) */
	"#52",			/* 52 = nosys */
	"#53",			/* 53 = nosys */
	"ioctl",			/* 54 = ioctl */
	"reboot",			/* 55 = reboot (unimplemented) */
	"symlink",			/* 56 = symlink */
	"utssys",			/* 57 = utssys */
	"readlink",			/* 58 = readlink */
	"execve",			/* 59 = execve */
	"umask",			/* 60 = umask */
	"chroot",			/* 61 = chroot */
	"fcntl",			/* 62 = fcntl */
	"ulimit",			/* 63 = ulimit */
	"#64",			/* 64 = nosys */
	"#65",			/* 65 = nosys */
	"vfork",			/* 66 = vfork */
	"vread",			/* 67 = vread */
	"vwrite",			/* 68 = vwrite */
	"#69",			/* 69 = nosys */
	"#70",			/* 70 = nosys */
	"mmap",			/* 71 = mmap */
	"#72",			/* 72 = nosys */
	"munmap",			/* 73 = munmap */
	"mprotect",			/* 74 = mprotect (unimplemented) */
	"#75",			/* 75 = nosys */
	"#76",			/* 76 = nosys */
	"#77",			/* 77 = nosys */
	"#78",			/* 78 = nosys */
	"getgroups",			/* 79 = getgroups */
	"setgroups",			/* 80 = setgroups */
	"getpgrp2",			/* 81 = getpgrp2 */
	"setpgrp2",			/* 82 = setpgrp2 */
	"setitimer",			/* 83 = setitimer */
	"wait3",			/* 84 = wait3 */
	"swapon",			/* 85 = swapon (unimplemented) */
	"getitimer",			/* 86 = getitimer */
	"#87",			/* 87 = nosys */
	"#88",			/* 88 = nosys */
	"#89",			/* 89 = nosys */
	"dup2",			/* 90 = dup2 */
	"#91",			/* 91 = nosys */
	"fstat",			/* 92 = fstat */
	"select",			/* 93 = select */
	"#94",			/* 94 = nosys */
	"fsync",			/* 95 = fsync */
	"#96",			/* 96 = nosys */
	"#97",			/* 97 = nosys */
	"#98",			/* 98 = nosys */
	"#99",			/* 99 = nosys */
	"#100",			/* 100 = nosys */
	"#101",			/* 101 = nosys */
	"#102",			/* 102 = nosys */
	"sigreturn",			/* 103 = sigreturn */
	"#104",			/* 104 = nosys */
	"#105",			/* 105 = nosys */
	"#106",			/* 106 = nosys */
	"#107",			/* 107 = nosys */
	"sigvec",			/* 108 = sigvec */
	"sigblock",			/* 109 = sigblock */
	"sigsetmask",			/* 110 = sigsetmask */
	"sigpause",			/* 111 = sigpause */
	"compat_43_sigstack",			/* 112 = compat_43_sigstack */
	"#113",			/* 113 = nosys */
	"#114",			/* 114 = nosys */
	"#115",			/* 115 = nosys */
	"gettimeofday",			/* 116 = gettimeofday */
	"#117",			/* 117 = nosys */
	"#118",			/* 118 = nosys */
	"hpib_io_stub",			/* 119 = hpib_io_stub (unimplemented) */
	"readv",			/* 120 = readv */
	"writev",			/* 121 = writev */
	"settimeofday",			/* 122 = settimeofday */
	"fchown",			/* 123 = fchown */
	"fchmod",			/* 124 = fchmod */
	"#125",			/* 125 = nosys */
	"setresuid",			/* 126 = setresuid */
	"setresgid",			/* 127 = setresgid */
	"rename",			/* 128 = rename */
	"old.truncate",		/* 129 = old truncate */
	"old.ftruncate",		/* 130 = old ftruncate */
	"#131",			/* 131 = nosys */
	"sysconf",			/* 132 = sysconf */
	"#133",			/* 133 = nosys */
	"#134",			/* 134 = nosys */
	"#135",			/* 135 = nosys */
	"mkdir",			/* 136 = mkdir */
	"rmdir",			/* 137 = rmdir */
	"#138",			/* 138 = nosys */
	"#139",			/* 139 = nosys */
	"#140",			/* 140 = nosys */
	"#141",			/* 141 = nosys */
	"#142",			/* 142 = nosys */
	"#143",			/* 143 = nosys */
	"old.getrlimit",		/* 144 = old getrlimit */
	"old.setrlimit",		/* 145 = old setrlimit */
	"#146",			/* 146 = nosys */
	"#147",			/* 147 = nosys */
	"#148",			/* 148 = nosys */
	"#149",			/* 149 = nosys */
	"#150",			/* 150 = nosys */
	"privgrp",			/* 151 = privgrp (unimplemented) */
	"rtprio",			/* 152 = rtprio */
	"plock",			/* 153 = plock (unimplemented) */
	"netioctl",			/* 154 = netioctl */
	"lockf",			/* 155 = lockf */
	"semget",			/* 156 = semget */
	"semctl",			/* 157 = semctl */
	"semop",			/* 158 = semop */
	"msgget",			/* 159 = msgget (unimplemented) */
	"msgctl",			/* 160 = msgctl (unimplemented) */
	"msgsnd",			/* 161 = msgsnd (unimplemented) */
	"msgrcv",			/* 162 = msgrcv (unimplemented) */
#ifdef SYSVSHM
	"shmget",			/* 163 = shmget */
	"shmctl",			/* 164 = shmctl */
	"shmat",			/* 165 = shmat */
	"shmdt",			/* 166 = shmdt */
#else
	"shmget",			/* 163 = shmget (unimplemented) */
	"shmctl",			/* 164 = shmctl (unimplemented) */
	"shmat",			/* 165 = shmat (unimplemented) */
	"shmdt",			/* 166 = shmdt (unimplemented) */
#endif
	"m68020_advise",			/* 167 = m68020_advise */
	"nsp_init",			/* 168 = nsp_init (unimplemented) */
	"cluster",			/* 169 = cluster (unimplemented) */
	"mkrnod",			/* 170 = mkrnod (unimplemented) */
	"#171",			/* 171 = nosys */
	"unsp_open",			/* 172 = unsp_open (unimplemented) */
	"#173",			/* 173 = nosys */
	"getcontext",			/* 174 = getcontext */
	"#175",			/* 175 = nosys */
	"#176",			/* 176 = nosys */
	"#177",			/* 177 = nosys */
	"lsync",			/* 178 = lsync (unimplemented) */
	"#179",			/* 179 = nosys */
	"mysite",			/* 180 = mysite (unimplemented) */
	"sitels",			/* 181 = sitels (unimplemented) */
	"#182",			/* 182 = nosys */
	"#183",			/* 183 = nosys */
	"dskless_stats",			/* 184 = dskless_stats (unimplemented) */
	"#185",			/* 185 = nosys */
	"setacl",			/* 186 = setacl (unimplemented) */
	"fsetacl",			/* 187 = fsetacl (unimplemented) */
	"getacl",			/* 188 = getacl (unimplemented) */
	"fgetacl",			/* 189 = fgetacl (unimplemented) */
	"getaccess",			/* 190 = getaccess */
	"getaudid",			/* 191 = getaudid (unimplemented) */
	"setaudid",			/* 192 = setaudid (unimplemented) */
	"getaudproc",			/* 193 = getaudproc (unimplemented) */
	"setaudproc",			/* 194 = setaudproc (unimplemented) */
	"getevent",			/* 195 = getevent (unimplemented) */
	"setevent",			/* 196 = setevent (unimplemented) */
	"audwrite",			/* 197 = audwrite (unimplemented) */
	"audswitch",			/* 198 = audswitch (unimplemented) */
	"audctl",			/* 199 = audctl (unimplemented) */
	"waitpid",			/* 200 = waitpid */
	"#201",			/* 201 = nosys */
	"#202",			/* 202 = nosys */
	"#203",			/* 203 = nosys */
	"#204",			/* 204 = nosys */
	"#205",			/* 205 = nosys */
	"#206",			/* 206 = nosys */
	"#207",			/* 207 = nosys */
	"#208",			/* 208 = nosys */
	"#209",			/* 209 = nosys */
	"#210",			/* 210 = nosys */
	"#211",			/* 211 = nosys */
	"#212",			/* 212 = nosys */
	"#213",			/* 213 = nosys */
	"#214",			/* 214 = nosys */
	"#215",			/* 215 = nosys */
	"#216",			/* 216 = nosys */
	"#217",			/* 217 = nosys */
	"#218",			/* 218 = nosys */
	"#219",			/* 219 = nosys */
	"#220",			/* 220 = nosys */
	"#221",			/* 221 = nosys */
	"#222",			/* 222 = nosys */
	"#223",			/* 223 = nosys */
	"#224",			/* 224 = nosys */
	"pathconf",			/* 225 = pathconf (unimplemented) */
	"fpathconf",			/* 226 = fpathconf (unimplemented) */
	"#227",			/* 227 = nosys */
	"#228",			/* 228 = nosys */
	"async_daemon",			/* 229 = async_daemon (unimplemented) */
	"nfs_fcntl",			/* 230 = nfs_fcntl (unimplemented) */
	"getdirentries",			/* 231 = getdirentries */
	"getdomainname",			/* 232 = getdomainname */
	"nfs_getfh",			/* 233 = nfs_getfh (unimplemented) */
	"vfsmount",			/* 234 = vfsmount (unimplemented) */
	"nfs_svc",			/* 235 = nfs_svc (unimplemented) */
	"setdomainname",			/* 236 = setdomainname */
	"statfs",			/* 237 = statfs (unimplemented) */
	"fstatfs",			/* 238 = fstatfs (unimplemented) */
	"sigaction",			/* 239 = sigaction */
	"sigprocmask",			/* 240 = sigprocmask */
	"sigpending",			/* 241 = sigpending */
	"sigsuspend",			/* 242 = sigsuspend */
	"fsctl",			/* 243 = fsctl (unimplemented) */
	"#244",			/* 244 = nosys */
	"pstat",			/* 245 = pstat (unimplemented) */
	"#246",			/* 246 = nosys */
	"#247",			/* 247 = nosys */
	"#248",			/* 248 = nosys */
	"#249",			/* 249 = nosys */
	"#250",			/* 250 = nosys */
	"#251",			/* 251 = nosys */
	"#252",			/* 252 = nosys */
	"#253",			/* 253 = nosys */
	"#254",			/* 254 = nosys */
	"#255",			/* 255 = nosys */
	"#256",			/* 256 = nosys */
	"#257",			/* 257 = nosys */
	"#258",			/* 258 = nosys */
	"#259",			/* 259 = nosys */
	"#260",			/* 260 = nosys */
	"#261",			/* 261 = nosys */
	"#262",			/* 262 = nosys */
	"#263",			/* 263 = nosys */
	"#264",			/* 264 = nosys */
	"#265",			/* 265 = nosys */
	"#266",			/* 266 = nosys */
	"#267",			/* 267 = nosys */
	"getnumfds",			/* 268 = getnumfds */
	"#269",			/* 269 = nosys */
	"#270",			/* 270 = nosys */
	"#271",			/* 271 = nosys */
	"fchdir",			/* 272 = fchdir */
	"#273",			/* 273 = nosys */
	"#274",			/* 274 = nosys */
	"old.accept",		/* 275 = old accept */
	"bind",			/* 276 = bind */
	"connect",			/* 277 = connect */
	"old.getpeername",		/* 278 = old getpeername */
	"old.getsockname",		/* 279 = old getsockname */
	"getsockopt",			/* 280 = getsockopt */
	"listen",			/* 281 = listen */
	"old.recv",		/* 282 = old recv */
	"old.recvfrom",		/* 283 = old recvfrom */
	"old.recvmsg",		/* 284 = old recvmsg */
	"old.send",		/* 285 = old send */
	"old.sendmsg",		/* 286 = old sendmsg */
	"sendto",			/* 287 = sendto */
	"setsockopt2",			/* 288 = setsockopt2 */
	"shutdown",			/* 289 = shutdown */
	"socket",			/* 290 = socket */
	"socketpair",			/* 291 = socketpair */
	"#292",			/* 292 = nosys */
	"#293",			/* 293 = nosys */
	"#294",			/* 294 = nosys */
	"#295",			/* 295 = nosys */
	"#296",			/* 296 = nosys */
	"#297",			/* 297 = nosys */
	"#298",			/* 298 = nosys */
	"#299",			/* 299 = nosys */
	"#300",			/* 300 = nosys */
	"#301",			/* 301 = nosys */
	"#302",			/* 302 = nosys */
	"#303",			/* 303 = nosys */
	"#304",			/* 304 = nosys */
	"#305",			/* 305 = nosys */
	"#306",			/* 306 = nosys */
	"#307",			/* 307 = nosys */
	"#308",			/* 308 = nosys */
	"#309",			/* 309 = nosys */
	"#310",			/* 310 = nosys */
	"#311",			/* 311 = nosys */
	"nsemctl",			/* 312 = nsemctl */
	"msgctl",			/* 313 = msgctl (unimplemented) */
#ifdef SYSVSHM
	"nshmctl",			/* 314 = nshmctl */
#else
	"nshmctl",			/* 314 = nshmctl (unimplemented) */
#endif
};
