/*
 * local.h
 *
 * This file was generated by configure. But then HAND-EDITED. It will
 * probably disappear in future revisions once the configuration process
 * will become more like that of standard bsd code.
 * lr 970714
 *
 */

/* build hex2hex /tmp/foo.x trix_boot.h trix_boot */

/*
 * make everything conditioned on NSND>0 so as to detect errors
 * because of missing "controller snd0" statement
 */
#define ALLOW_BUFFER_MAPPING 1

#include "snd.h"
#if NSND > 0
#define CONFIGURE_SOUNDCARD

#define CONFIG_SEQUENCER

#include "gus.h"
#if NGUS != 0 && !defined(CONFIG_GUS)
#define CONFIG_GUS
#define CONFIG_GUSMAX
#endif

#include "sscape.h"
#if NSSCAPE != 0 && !defined(CONFIG_SSCAPE)
#define CONFIG_SSCAPE
#endif

#include "trix.h"
#if NTRIX > 0
#define INCLUDE_TRIX_BOOT
#define CONFIG_TRIX	/* can use NTRIX > 0 instead */
#define CONFIG_YM3812
#endif

#if defined(CONFIG_GUSMAX) || ( NSSCAPE > 0 ) || ( NTRIX > 0 )
#define CONFIG_AD1848
#endif

#if  defined(CONFIG_SEQUENCER) && (NTRIX == 0)
#define CONFIG_MIDI
#endif

#include "sb.h"
#if NSB > 0
#define CONFIG_SB
#endif

#include "mss.h"
#if NMSS != 0
#define CONFIG_AD1848
#define CONFIG_MSS
#undef  CONFIG_CS4232
#endif

#include "sbxvi.h"
#if NSBXVI != 0 && !defined(CONFIG_SB16)
#define CONFIG_SB16
#define CONFIG_SBPRO                /*  FIXME: Also needs to be a sep option */
#endif

#include "sbmidi.h"
#if NSBMIDI != 0 && !defined(CONFIG_SB16MIDI)
#define CONFIG_SB16MIDI
#endif

#include "awe.h"
#if NAWE != 0 && !defined(CONFIG_AWE32)
#define CONFIG_AWE32
#endif

#include "pas.h"
#if NPAS != 0 && !defined(CONFIG_PAS)
#define CONFIG_PAS
#endif

#include "mpu.h"
#if NMPU != 0 && !defined(CONFIG_MPU401)
#define CONFIG_MPU401
#endif
 
#include "opl.h"
#if NOPL != 0 && !defined(CONFIG_YM3812)
#define CONFIG_YM3812
#endif

#define ALLOW_POLL

/* #undef  CONFIG_PAS */
/* #undef  CONFIG_ADLIB */
/* #define CONFIG_GUS */
/* #undef  CONFIG_MPU401 */
#undef  CONFIG_UART6850
#undef  CONFIG_PSS
#undef  CONFIG_GUS16
/* #undef  CONFIG_MSS */
/* #undef  CONFIG_SSCAPE */
#undef  CONFIG_MAD16
/* #undef  CONFIG_CS4232 */
#undef  CONFIG_MAUI
#undef  CONFIG_PNP
/* #undef  CONFIG_SBPRO	*/
/* #undef  CONFIG_SB16	*/
#undef  CONFIG_AEDSP16
#define CONFIG_AUDIO	/* obvious ? */

#define CONFIG_MPU_EMU

#define DSP_BUFFSIZE 32768*2
/* #define SELECTED_SOUND_OPTIONS	0x0188090a */

#ifndef TRIX_SB_BASE
#define TRIX_SB_BASE 0x220
#endif

#ifndef TRIX_SB_IRQ
#define TRIX_SB_IRQ 7
#endif

#ifndef TRIX_SB_DMA
#define TRIX_SB_DMA 1
#endif

#ifndef TRIX_BASE
#define	TRIX_BASE	0x530
#endif

#ifndef TRIX_IRQ
#define TRIX_IRQ	9
#endif

#ifndef TRIX_DMA
#define TRIX_DMA	3
#endif

#ifndef TRIX_DMA2
#define TRIX_DMA2	1
#endif

#ifndef GUS_BASE
#define GUS_BASE	0x220
#endif

#ifndef GUS_IRQ
#define GUS_IRQ		12
#endif

#ifndef GUS_MIDI_IRQ
#define GUS_MIDI_IRQ	GUS_IRQ
#endif

#ifndef GUS_DMA
#define GUS_DMA		4
#endif

#ifndef GUS_DMA2
#define GUS_DMA2	4
#endif

#define SOUND_CONFIG_DATE "Wed Aug  6 22:58:35 PDT 1997"
#define SOUND_CONFIG_BY "Amancio Hasty"
#define SOUND_CONFIG_HOST "rah"
#define SOUND_CONFIG_DOMAIN "star-gate.com"

#else	/* NSND = 0 */
#undef CONFIGURE_SOUNDCARD
#endif
