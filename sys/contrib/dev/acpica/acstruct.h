/******************************************************************************
 *
 * Name: acstruct.h - Internal structs
 *       $Revision: 3 $
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999, 2000, 2001, Intel Corp.
 * All rights reserved.
 *
 * 2. License
 *
 * 2.1. This is your license from Intel Corp. under its intellectual property
 * rights.  You may have additional license terms from the party that provided
 * you this software, covering your right to use that party's intellectual
 * property rights.
 *
 * 2.2. Intel grants, free of charge, to any person ("Licensee") obtaining a
 * copy of the source code appearing in this file ("Covered Code") an
 * irrevocable, perpetual, worldwide license under Intel's copyrights in the
 * base code distributed originally by Intel ("Original Intel Code") to copy,
 * make derivatives, distribute, use and display any portion of the Covered
 * Code in any form, with the right to sublicense such rights; and
 *
 * 2.3. Intel grants Licensee a non-exclusive and non-transferable patent
 * license (with the right to sublicense), under only those claims of Intel
 * patents that are infringed by the Original Intel Code, to make, use, sell,
 * offer to sell, and import the Covered Code and derivative works thereof
 * solely to the minimum extent necessary to exercise the above copyright
 * license, and in no event shall the patent license extend to any additions
 * to or modifications of the Original Intel Code.  No other license or right
 * is granted directly or by implication, estoppel or otherwise;
 *
 * The above copyright and patent license is granted only if the following
 * conditions are met:
 *
 * 3. Conditions
 *
 * 3.1. Redistribution of Source with Rights to Further Distribute Source.
 * Redistribution of source code of any substantial portion of the Covered
 * Code or modification with rights to further distribute source must include
 * the above Copyright Notice, the above License, this list of Conditions,
 * and the following Disclaimer and Export Compliance provision.  In addition,
 * Licensee must cause all Covered Code to which Licensee contributes to
 * contain a file documenting the changes Licensee made to create that Covered
 * Code and the date of any change.  Licensee must include in that file the
 * documentation of any changes made by any predecessor Licensee.  Licensee
 * must include a prominent statement that the modification is derived,
 * directly or indirectly, from Original Intel Code.
 *
 * 3.2. Redistribution of Source with no Rights to Further Distribute Source.
 * Redistribution of source code of any substantial portion of the Covered
 * Code or modification without rights to further distribute source must
 * include the following Disclaimer and Export Compliance provision in the
 * documentation and/or other materials provided with distribution.  In
 * addition, Licensee may not authorize further sublicense of source of any
 * portion of the Covered Code, and must include terms to the effect that the
 * license from Licensee to its licensee is limited to the intellectual
 * property embodied in the software Licensee provides to its licensee, and
 * not to intellectual property embodied in modifications its licensee may
 * make.
 *
 * 3.3. Redistribution of Executable. Redistribution in executable form of any
 * substantial portion of the Covered Code or modification must reproduce the
 * above Copyright Notice, and the following Disclaimer and Export Compliance
 * provision in the documentation and/or other materials provided with the
 * distribution.
 *
 * 3.4. Intel retains all right, title, and interest in and to the Original
 * Intel Code.
 *
 * 3.5. Neither the name Intel nor any other trademark owned or controlled by
 * Intel shall be used in advertising or otherwise to promote the sale, use or
 * other dealings in products derived from or relating to the Covered Code
 * without prior written authorization from Intel.
 *
 * 4. Disclaimer and Export Compliance
 *
 * 4.1. INTEL MAKES NO WARRANTY OF ANY KIND REGARDING ANY SOFTWARE PROVIDED
 * HERE.  ANY SOFTWARE ORIGINATING FROM INTEL OR DERIVED FROM INTEL SOFTWARE
 * IS PROVIDED "AS IS," AND INTEL WILL NOT PROVIDE ANY SUPPORT,  ASSISTANCE,
 * INSTALLATION, TRAINING OR OTHER SERVICES.  INTEL WILL NOT PROVIDE ANY
 * UPDATES, ENHANCEMENTS OR EXTENSIONS.  INTEL SPECIFICALLY DISCLAIMS ANY
 * IMPLIED WARRANTIES OF MERCHANTABILITY, NONINFRINGEMENT AND FITNESS FOR A
 * PARTICULAR PURPOSE.
 *
 * 4.2. IN NO EVENT SHALL INTEL HAVE ANY LIABILITY TO LICENSEE, ITS LICENSEES
 * OR ANY OTHER THIRD PARTY, FOR ANY LOST PROFITS, LOST DATA, LOSS OF USE OR
 * COSTS OF PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES, OR FOR ANY INDIRECT,
 * SPECIAL OR CONSEQUENTIAL DAMAGES ARISING OUT OF THIS AGREEMENT, UNDER ANY
 * CAUSE OF ACTION OR THEORY OF LIABILITY, AND IRRESPECTIVE OF WHETHER INTEL
 * HAS ADVANCE NOTICE OF THE POSSIBILITY OF SUCH DAMAGES.  THESE LIMITATIONS
 * SHALL APPLY NOTWITHSTANDING THE FAILURE OF THE ESSENTIAL PURPOSE OF ANY
 * LIMITED REMEDY.
 *
 * 4.3. Licensee shall not export, either directly or indirectly, any of this
 * software or system incorporating such software without first obtaining any
 * required license or other approval from the U. S. Department of Commerce or
 * any other agency or department of the United States Government.  In the
 * event Licensee exports any such software from the United States or
 * re-exports any such software from a foreign destination, Licensee shall
 * ensure that the distribution and export/re-export of the software is in
 * compliance with all laws, regulations, orders, or other restrictions of the
 * U.S. Export Administration Regulations. Licensee agrees that neither it nor
 * any of its subsidiaries will export/re-export any technical data, process,
 * software, or service, directly or indirectly, to any country for which the
 * United States government or any agency thereof requires an export license,
 * other governmental approval, or letter of assurance, without first obtaining
 * such license, approval or letter.
 *
 *****************************************************************************/

#ifndef __ACSTRUCT_H__
#define __ACSTRUCT_H__


/*****************************************************************************
 *
 * Tree walking typedefs and structs
 *
 ****************************************************************************/


/*
 * Walk state - current state of a parse tree walk.  Used for both a leisurely stroll through
 * the tree (for whatever reason), and for control method execution.
 */

#define NEXT_OP_DOWNWARD    1
#define NEXT_OP_UPWARD      2

#define WALK_NON_METHOD     0
#define WALK_METHOD         1
#define WALK_METHOD_RESTART 2

typedef struct acpi_walk_state
{
    UINT8                   DataType;                           /* To differentiate various internal objs */\
    ACPI_OWNER_ID           OwnerId;                            /* Owner of objects created during the walk */
    BOOLEAN                 LastPredicate;                      /* Result of last predicate */
    UINT8                   NextOpInfo;                         /* Info about NextOp */
    UINT8                   NumOperands;                        /* Stack pointer for Operands[] array */
    UINT8                   CurrentResult;                      /* */

    struct acpi_walk_state  *Next;                              /* Next WalkState in list */
    ACPI_PARSE_OBJECT       *Origin;                            /* Start of walk [Obsolete] */

/* TBD: Obsolete with removal of WALK procedure ? */
    ACPI_PARSE_OBJECT       *PrevOp;                            /* Last op that was processed */
    ACPI_PARSE_OBJECT       *NextOp;                            /* next op to be processed */


    ACPI_GENERIC_STATE      *Results;                           /* Stack of accumulated results */
    ACPI_GENERIC_STATE      *ControlState;                      /* List of control states (nested IFs) */
    ACPI_GENERIC_STATE      *ScopeInfo;                         /* Stack of nested scopes */
    ACPI_PARSE_STATE        *ParserState;                       /* Current state of parser */
    UINT8                   *AmlLastWhile;
    ACPI_OPCODE_INFO        *OpInfo;                            /* Info on current opcode */
    ACPI_PARSE_DOWNWARDS    DescendingCallback;
    ACPI_PARSE_UPWARDS      AscendingCallback;

    union acpi_operand_obj  *ReturnDesc;                        /* Return object, if any */
    union acpi_operand_obj  *MethodDesc;                        /* Method descriptor if running a method */
    struct acpi_node        *MethodNode;                        /* Method Node if running a method */
    ACPI_PARSE_OBJECT       *MethodCallOp;                      /* MethodCall Op if running a method */
    struct acpi_node        *MethodCallNode;                    /* Called method Node*/
    union acpi_operand_obj  *Operands[OBJ_NUM_OPERANDS];        /* Operands passed to the interpreter */
    struct acpi_node        Arguments[MTH_NUM_ARGS];            /* Control method arguments */
    struct acpi_node        LocalVariables[MTH_NUM_LOCALS];     /* Control method locals */
    struct acpi_walk_list   *WalkList;
    UINT32                  ParseFlags;
    UINT8                   WalkType;
    UINT8                   ReturnUsed;
    UINT16                  Opcode;                             /* Current AML opcode */
    UINT32                  PrevArgTypes;
    UINT16                  CurrentSyncLevel;                   /* Mutex Sync (nested acquire) level */

    /* Debug support */

    UINT32                  MethodBreakpoint;


} ACPI_WALK_STATE;


/*
 * Walk list - head of a tree of walk states.  Multiple walk states are created when there
 * are nested control methods executing.
 */
typedef struct acpi_walk_list
{

    ACPI_WALK_STATE         *WalkState;
    ACPI_OBJECT_MUTEX       AcquiredMutexList;                 /* List of all currently acquired mutexes */

} ACPI_WALK_LIST;


/* Info used by AcpiPsInitObjects */

typedef struct acpi_init_walk_info
{
    UINT16                  MethodCount;
    UINT16                  OpRegionCount;
    UINT16                  FieldCount;
    UINT16                  OpRegionInit;
    UINT16                  FieldInit;
    UINT16                  ObjectCount;
    ACPI_TABLE_DESC         *TableDesc;

} ACPI_INIT_WALK_INFO;


/* Info used by TBD */

typedef struct acpi_device_walk_info
{
    UINT16                  DeviceCount;
    UINT16                  Num_STA;
    UINT16                  Num_INI;
    ACPI_TABLE_DESC         *TableDesc;

} ACPI_DEVICE_WALK_INFO;


/* TBD: [Restructure] Merge with struct above */

typedef struct acpi_walk_info
{
    UINT32                  DebugLevel;
    UINT32                  OwnerId;

} ACPI_WALK_INFO;

typedef struct acpi_get_devices_info
{
    ACPI_WALK_CALLBACK      UserFunction;
    void                    *Context;
    NATIVE_CHAR             *Hid;

} ACPI_GET_DEVICES_INFO;


#endif
