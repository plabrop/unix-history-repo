/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of California, Berkeley.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef lint
static char sccsid[] = "@(#)words.c	5.2 (Berkeley) %G%";
#endif /* not lint */

#include "externs.h"

struct wlist wlist[] = {
	{ "knife",	KNIFE,		OBJECT },
	{ "sword",	SWORD,		NOUNS },
	{ "scabbard",	SWORD,		OBJECT },
	{ "fine",	SWORD,		OBJECT },
	{ "two-handed",	TWO_HANDED,	OBJECT },
	{ "cleaver",	CLEAVER,	OBJECT },
	{ "broadsword",	BROAD,		OBJECT },
	{ "mail",	MAIL,		OBJECT },
	{ "coat",	MAIL,		OBJECT },
	{ "helmet",	HELM,		OBJECT },
	{ "shield",	SHIELD,		OBJECT },
	{ "maid",	MAID,		OBJECT },
	{ "maid's",	MAID,		OBJECT },
	{ "body",	BODY,		NOUNS },
	{ "viper",	VIPER,		OBJECT },
	{ "lamp",	LAMPON,		OBJECT },
	{ "lantern",	LAMPON,		OBJECT },
	{ "shoes",	SHOES,		OBJECT },
	{ "pajamas",	PAJAMAS,	OBJECT },
	{ "robe",	ROBE,		OBJECT },
	{ "amulet",	AMULET,		NOUNS },
	{ "medallion",	MEDALION,	NOUNS },
	{ "talisman",	TALISMAN,	NOUNS },
	{ "woodsman",	DEADWOOD,	OBJECT },
	{ "woodsman's",	DEADWOOD,	OBJECT },
	{ "mallet",	MALLET,		OBJECT },
	{ "laser",	LASER,		OBJECT },
	{ "pistol",	LASER,		OBJECT },
	{ "blaster",	LASER,		OBJECT },
	{ "gun",	LASER,		OBJECT },
	{ "goddess",	NORMGOD,	NOUNS },
	{ "grenade",	GRENADE,	OBJECT },
	{ "chain",	CHAIN,		OBJECT },
	{ "rope",	ROPE,		OBJECT },
	{ "levis",	LEVIS,		OBJECT },
	{ "pants",	LEVIS,		OBJECT },
	{ "mace",	MACE,		OBJECT },
	{ "shovel",	SHOVEL,		OBJECT },
	{ "halberd",	HALBERD,	OBJECT },
	{ "compass",	COMPASS,	OBJECT },
	{ "elf",	ELF,		OBJECT },
	{ "coins",	COINS,		OBJECT },
	{ "matches",	MATCHES,	OBJECT },
	{ "match",	MATCHES,	OBJECT },
	{ "book",	MATCHES,	OBJECT },
	{ "man",	MAN,		NOUNS },
	{ "papayas",	PAPAYAS,	OBJECT },
	{ "pineapple",	PINEAPPLE,	OBJECT },
	{ "kiwi",	KIWI,		OBJECT },
	{ "coconuts",	COCONUTS,	OBJECT },
	{ "mango",	MANGO,		OBJECT },
	{ "ring",	RING,		OBJECT },
	{ "potion",	POTION,		OBJECT },
	{ "bracelet",	BRACELET,	OBJECT },
	{ "timer",	TIMER,		NOUNS },
	{ "bomb",	BOMB,		OBJECT },
	{ "warhead",	BOMB,		OBJECT },
	{ "girl",	NATIVE,		NOUNS },
	{ "native",	NATIVE,		NOUNS },
	{ "horse",	HORSE,		OBJECT },
	{ "stallion",	HORSE,		OBJECT },
	{ "car",	CAR,		OBJECT },
	{ "volare",	CAR,		OBJECT },
	{ "pot",	POT,		OBJECT },
	{ "jewels",	POT,		OBJECT },
	{ "bar",	BAR,		OBJECT },
	{ "diamond",	BLOCK,		OBJECT },
	{ "block",	BLOCK,		OBJECT },
	{ "up",		UP,		VERB },
	{ "u",		UP,		VERB },
	{ "down",	DOWN,		VERB },
	{ "d",		DOWN,		VERB },
	{ "ahead",	AHEAD,		VERB },
	{ "a",		AHEAD,		VERB },
	{ "back",	BACK,		VERB },
	{ "b",		BACK,		VERB },
	{ "right",	RIGHT,		VERB },
	{ "r",		RIGHT,		VERB },
	{ "left",	LEFT,		VERB },
	{ "l",		LEFT,		VERB },
	{ "take",	TAKE,		VERB },
	{ "get",	TAKE,		VERB },
	{ "use",	USE,		VERB },
	{ "look",	LOOK,		VERB },
	{ "lo",		LOOK,		VERB },
	{ "quit",	QUIT,		VERB },
	{ "q",		QUIT,		VERB },
	{ "su",		SU,		VERB },
	{ "drop",	DROP,		VERB },
	{ "draw",	DRAW,		VERB },
	{ "pull",	DRAW,		VERB },
	{ "carry",	DRAW,		VERB },
	{ "wear",	WEARIT,		VERB },
	{ "sheathe",	WEARIT,		VERB },
	{ "put",	PUT,		VERB },
	{ "buckle",	PUT,		VERB },
	{ "strap",	PUT,		VERB },
	{ "tie",	PUT,		VERB },
	{ "inven",	INVEN,		VERB },
	{ "i",		INVEN,		VERB },
	{ "everything",	EVERYTHING,	OBJECT },
	{ "all",	EVERYTHING,	OBJECT },
	{ "and",	AND,		CONJ },
	{ "kill",	KILL,		VERB },
	{ "fight",	KILL,		VERB },
	{ "ravage",	RAVAGE,		VERB },
	{ "rape",	RAVAGE,		VERB },
	{ "undress",	UNDRESS,	VERB },
	{ "throw",	THROW,		VERB },
	{ "launch",	LAUNCH,		VERB },
	{ "land",	LANDIT,		VERB },
	{ "light",	LIGHT,		VERB },
	{ "strike",	LIGHT,		VERB },
	{ "follow",	FOLLOW,		VERB },
	{ "chase",	FOLLOW,		VERB },
	{ "kiss",	KISS,		VERB },
	{ "love",	LOVE,		VERB },
	{ "fuck",	LOVE,		VERB },
	{ "give",	GIVE,		VERB },
	{ "smite",	SMITE,		VERB },
	{ "attack",	SMITE,		VERB },
	{ "swing",	SMITE,		VERB },
	{ "stab",	SMITE,		VERB },
	{ "slice",	SMITE,		VERB },
	{ "cut",	SMITE,		VERB },
	{ "hack",	SMITE,		VERB },
	{ "shoot",	SHOOT,		VERB },
	{ "blast",	SHOOT,		VERB },
	{ "on",		ON,		PREPS },
	{ "off",	OFF,		PREPS },
	{ "time",	TIME,		VERB },
	{ "sleep",	SLEEP,		VERB },
	{ "dig",	DIG,		VERB },
	{ "eat",	EAT,		VERB },
	{ "swim",	SWIM,		VERB },
	{ "drink",	DRINK,		VERB },
	{ "door",	DOOR,		NOUNS },
	{ "save",	SAVE,		VERB },
	{ "ride",	RIDE,		VERB },
	{ "mount",	RIDE,		VERB },
	{ "drive",	DRIVE,		VERB },
	{ "start",	DRIVE,		VERB },
	{ "score",	SCORE,		VERB },
	{ "points",	SCORE,		VERB },
	{ "bury",	BURY,		VERB },
	{ "jump",	JUMP,		VERB },
	{ "kick",	KICK,		VERB },
	{ "kerosene",	0,		ADJS },
	{ "plumed",	0,		ADJS },
	{ "ancient",	0,		ADJS },
	{ "golden",	0,		ADJS },
	{ "gold",	0,		ADJS },
	{ "ostrich",	0,		ADJS },
	{ "rusty",	0,		ADJS },
	{ "old",	0,		ADJS },
	{ "dented",	0,		ADJS },
	{ "blue",	0,		ADJS },
	{ "purple",	0,		ADJS },
	{ "kingly",	0,		ADJS },
	{ "the",	0,		ADJS },
	{ "climb",	0,		ADJS },
	{ "move",	0,		ADJS },
	{ "make",	0,		ADJS },
	{ "to",		0,		ADJS },
	0
};
