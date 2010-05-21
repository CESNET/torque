/*
*         OpenPBS (Portable Batch System) v2.3 Software License
*
* Copyright (c) 1999-2000 Veridian Information Solutions, Inc.
* All rights reserved.
*
* ---------------------------------------------------------------------------
* For a license to use or redistribute the OpenPBS software under conditions
* other than those described below, or to purchase support for this software,
* please contact Veridian Systems, PBS Products Department ("Licensor") at:
*
*    www.OpenPBS.org  +1 650 967-4675                  sales@OpenPBS.org
*                        877 902-4PBS (US toll-free)
* ---------------------------------------------------------------------------
*
* This license covers use of the OpenPBS v2.3 software (the "Software") at
* your site or location, and, for certain users, redistribution of the
* Software to other sites and locations.  Use and redistribution of
* OpenPBS v2.3 in source and binary forms, with or without modification,
* are permitted provided that all of the following conditions are met.
* After December 31, 2001, only conditions 3-6 must be met:
*
* 1. Commercial and/or non-commercial use of the Software is permitted
*    provided a current software registration is on file at www.OpenPBS.org.
*    If use of this software contributes to a publication, product, or
*    service, proper attribution must be given; see www.OpenPBS.org/credit.html
*
* 2. Redistribution in any form is only permitted for non-commercial,
*    non-profit purposes.  There can be no charge for the Software or any
*    software incorporating the Software.  Further, there can be no
*    expectation of revenue generated as a consequence of redistributing
*    the Software.
*
* 3. Any Redistribution of source code must retain the above copyright notice
*    and the acknowledgment contained in paragraph 6, this list of conditions
*    and the disclaimer contained in paragraph 7.
*
* 4. Any Redistribution in binary form must reproduce the above copyright
*    notice and the acknowledgment contained in paragraph 6, this list of
*    conditions and the disclaimer contained in paragraph 7 in the
*    documentation and/or other materials provided with the distribution.
*
* 5. Redistributions in any form must be accompanied by information on how to
*    obtain complete source code for the OpenPBS software and any
*    modifications and/or additions to the OpenPBS software.  The source code
*    must either be included in the distribution or be available for no more
*    than the cost of distribution plus a nominal fee, and all modifications
*    and additions to the Software must be freely redistributable by any party
*    (including Licensor) without restriction.
*
* 6. All advertising materials mentioning features or use of the Software must
*    display the following acknowledgment:
*
*     "This product includes software developed by NASA Ames Research Center,
*     Lawrence Livermore National Laboratory, and Veridian Information
*     Solutions, Inc.
*     Visit www.OpenPBS.org for OpenPBS software support,
*     products, and information."
*
* 7. DISCLAIMER OF WARRANTY
*
* THIS SOFTWARE IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND. ANY EXPRESS
* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
* OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, AND NON-INFRINGEMENT
* ARE EXPRESSLY DISCLAIMED.
*
* IN NO EVENT SHALL VERIDIAN CORPORATION, ITS AFFILIATED COMPANIES, OR THE
* U.S. GOVERNMENT OR ANY OF ITS AGENCIES BE LIABLE FOR ANY DIRECT OR INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
* OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
* LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
* NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
* EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
* This license will be governed by the laws of the Commonwealth of Virginia,
* without reference to its choice of law rules.
*/

#include <pbs_config.h>   /* the master config generated by configure */

#ifndef _POSIX_SOURCE
#define _POSIX_SOURCE 1
#endif  /* _POSIX_SOURCE */

#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <setjmp.h>
#include <math.h>
#include "Semantic.h"
#include "Parser.h"
#include "af.h"

static char ident[] = "@(#) $RCSfile$ $Revision$";

extern void
  yyerror(char *ep);

extern int  linenum;
extern char linebuf[];

/* Global Variables */
FILE *SemanticFpOut;

/* File Scope Variables */
static char * SemanticErrors[] =
  {
  "0 no such error msg",
  "1 illegal use of var or expr type",
  "2 expr or sym ptr is NULL",
  "3 illegal combination of types",
  "4 illegal type of LHS in assign",
  "5 str ptr is NULL or malloc problems",
  "6 var name is undefined",
  "7 var name previously declared",
  "8 func name is undefined",
  "9 func arg type mismatch",
  "10 func name is previously declared",
  "11 type returned does not match func return type",
  "12 Duplicate case label in the same switch statement",
  "13 switch variable's type inconsistent with case label's type",
  "14 break stmt must be within a loop body",
  "15 continue stmt must be within a loop body",
  "16 bad constant value: date(1 <= mo <= 12, 1 <= dd <= 31, 0 < yy) or time( 0 <= hh <= 23, 0 <= mm <= 59, 0 <= ss <= 61)",
  "17 bad constant range value: low_value must be < high_value",
  "18 incomplete arguments to function call",
  "19 QueJobFind/QueFilter func arg missing",
  "20 QueJobFind's arg2 return type must match arg4",
  "21 QueJobFind can only have up to 2 args",
  "22 QueJobFind/QueFilter's compare operator must be MAX or MIN",
  "23 QueJobFind/QueFilter's compare operator must be EQ, NEQ, GE, GT, LE, LT",
  "24 QueJobFind/QueFilter's func arg return type is not valid!",
  "25 QueFilter's arg2 return type must match arg4's",
  "26 QueFilter's arg4 is missing",
  "27 Variable cannot be of VOID type",
  "28 QueJobFind/QueFilter's func arg does not accept Job as only parameter type",
  "29 Constant string not allowed as return parameter",
  "30 passed-by-value string cannot be modified",
  "31 Sort object (1st parameter) missing",
  "32 Sort key function (2nd parameter) missing",
  "33 Sort object != {SetJob, SetQue, SetServer, SetCNode}",
  "34 Sort key function's return type is invalid!",
  "35 Sort object is a function or sort key argument is not a function!",
  "36 Sort key function must operate on compatible type!",
  "37 Sort object (1st parameter) is a function!",
  "38 Sort key function argument does not accept only 1 parameter!",
  ""
  };

static int SemanticMaxErrors = 40;

static char *SemanticName = "Semantic";

static int SemanticDF = 0;

void SemanticInit(void)
  {
  SemanticCondPrint("SemanticInit");

  /* SemanticDF = 1; */
  }


void
SemanticPutDF(int df)
  {
  SemanticCondPrint("SemanticPutDF");

  SemanticDF = df;
  }

void
SemanticCondPrint(char *str)
  {
  if (SemanticFpOut == NULL)
    printf("SematicFpOut is NULL -- die\n");

  if (str == NULL)
    SemanticErr(5);

  if (SemanticDF == 1)
    {
    fprintf(SemanticFpOut, "%s\t", SemanticName);
    fprintf(SemanticFpOut, "%s\n", str);
    }
  }

void SemanticPrint(void)
  {
  SemanticCondPrint("SemanticPrint\n");

  }

void
SemanticErr(int e)
  {
  if (SemanticFpOut == NULL)
    printf("SematicFpOut is NULL -- die\n");

  if (e >= SemanticMaxErrors)
    e = 0;

  fprintf(SemanticFpOut, "SemanticErr\n");

  fprintf(SemanticFpOut,
          "%d: Semantic Error %s in this line:\n%s\n",
          linenum, SemanticErrors[e], linebuf);

  /*WARNING*********put it back in after testing*/
  exit(1);
  }



void
SemanticPrintToken2(struct MYTOK sym)
  {
  SemanticCondPrint("SemanticPrintToken2");

  fprintf(SemanticFpOut, "lexem=%s, lin=%d, len=%d, typ=%d, varFlag=%d\n", sym.lexeme, sym.line, sym.len, sym.type, sym.varFlag);
  }


/*used by the SemanticX.c progs -- not by real PseCo*/
/*
void SemanticPutToken(symp, lex, lin, len, typ, varFlag )
struct  MYTOK symp;
char  *lex;
int  lin;
int  len;
int  typ;
int  varFlag;
  {
 SemanticCondPrint("SemanticPutToken");

 if (symp == NULL)
  SemanticErr(2);

 if (lex == NULL)
  SemanticErr(2);

 strcpy((char *) symp->lexeme , lex);
 symp.line = lin;
 symp.len = len;
 symp.type = typ;
 symp.varFlag = varFlag;
  }
*/

void SemanticStatNopCk(void)
  {
  SemanticCondPrint("SemanticStatNopCk\n");

  /*fine*/
  }




void
SemanticStatAssignCk(struct MYTOK var, struct MYTOK expr)
  {
  int varType, exprType;

  SemanticCondPrint("SemanticStatAssignCk\n");

  /*******************/
  /*OK simple type:int, float, dayofweek,
   *OK date, Time, String, Size, Fun of proper return type
   *OK: que, job. cnode, server, resmom,
   *OK: range: int, float, dayofweek,
   *OK: range: date, time, size,
   *OK: set:   server, que, job, cnode
   *NOT: void
   *All must match exactly, excetp int=float ok, float=int ok
   */

  /* assuming at this point that var has already been checked for */
  /* declaration */

  varType = var.type;
  /*if expr is not single var but real combo expression*/
  exprType = expr.type;

  switch (varType)
    {

    case DAYOFWEEKTYPE:

    case DATETIMETYPE:

    case STRINGTYPE:

    case SIZETYPE:

    case QUETYPE:

    case JOBTYPE:

    case CNODETYPE:

    case SERVERTYPE:

    case DAYOFWEEKRANGETYPE:

    case DATETIMERANGETYPE:

    case SIZERANGETYPE:

    case SERVERSETTYPE:

    case QUESETTYPE:

    case JOBSETTYPE:

    case CNODESETTYPE:

    case INTRANGETYPE:

    case FLOATRANGETYPE:
      {

      if (varType != exprType)
        {
        SemanticErr(3);
        }

      break;
      }

    case INTTYPE:

    case FLOATTYPE:
      {
      if ((exprType != INTTYPE) && (exprType != FLOATTYPE))
        {
        SemanticErr(3);
        }

      break;
      }

    default:
      {
      SemanticErr(4);
      }
    }


  }


/*nothing needed*/
void
SemanticStatPlusExprCk(struct MYTOK left_expr, struct MYTOK right_expr)
  {
  int leftType, rightType;

  SemanticCondPrint("SemanticStatPlusExprCk\n");

  leftType  = left_expr.type;
  rightType = right_expr.type;

  switch (leftType)
    {

    case STRINGTYPE:

    case SIZETYPE:
      {
      if (leftType != rightType)
        {
        SemanticErr(3);
        }

      break;
      }

    case INTTYPE:

    case FLOATTYPE:
      {
      if ((rightType != INTTYPE) && (rightType != FLOATTYPE))
        {
        SemanticErr(3);
        }

      break;
      }

    default:
      {
      SemanticErr(1);
      }
    }

  /*fine*/
  }

void
SemanticStatMinusExprCk(struct MYTOK left_expr, struct MYTOK right_expr)
  {
  int leftType, rightType;

  SemanticCondPrint("SemanticStatMinusExprCk\n");

  leftType  = left_expr.type;
  rightType = right_expr.type;

  switch (leftType)
    {

    case SIZETYPE:

      {
      if (leftType != rightType)
        {
        SemanticErr(3);
        }

      break;
      }

    case INTTYPE:

    case FLOATTYPE:
      {
      if ((rightType != INTTYPE) && (rightType != FLOATTYPE))
        {
        SemanticErr(3);
        }

      break;
      }

    default:
      {
      SemanticErr(1);
      }
    }

  /*fine*/
  }

void
SemanticStatMultDivExprCk(struct MYTOK left_expr, struct MYTOK right_expr)
  {
  int leftType, rightType;

  SemanticCondPrint("SemanticStatMultDivExprCk\n");

  leftType  = left_expr.type;
  rightType = right_expr.type;

  switch (leftType)
    {

    case SIZETYPE:
      {
      if (leftType != rightType)
        {
        SemanticErr(3);
        }

      break;
      }

    case INTTYPE:

    case FLOATTYPE:
      {
      if ((rightType != INTTYPE) && (rightType != FLOATTYPE))
        {
        SemanticErr(3);
        }

      break;
      }

    default:
      {
      SemanticErr(1);
      }
    }

  /*fine*/
  }

void
SemanticStatModulusExprCk(struct MYTOK left_expr, struct MYTOK right_expr)
  {
  int leftType, rightType;

  SemanticCondPrint("SemanticStatModulusExprCk\n");

  leftType  = left_expr.type;
  rightType = right_expr.type;

  switch (leftType)
    {

    case INTTYPE:
      {
      if (leftType != rightType)
        {
        SemanticErr(3);
        }

      break;
      }

    default:
      {
      SemanticErr(1);
      }
    }

  /*fine*/
  }

void
SemanticStatCompExprCk(struct MYTOK left_expr, struct MYTOK right_expr)
  {
  int leftType, rightType;

  SemanticCondPrint("SemanticCompExprCk\n");

  leftType  = left_expr.type;
  rightType = right_expr.type;

  switch (leftType)
    {

    case DAYOFWEEKTYPE:

    case DATETIMETYPE:

    case STRINGTYPE:

    case SIZETYPE:

    case SERVERTYPE:

    case QUETYPE:

    case JOBTYPE:

    case CNODETYPE:

    case SERVERSETTYPE:

    case QUESETTYPE:

    case JOBSETTYPE:

    case CNODESETTYPE:
      {
      if (leftType != rightType)
        {
        SemanticErr(3);
        }

      break;
      }

    case INTTYPE:

    case FLOATTYPE:
      {
      if ((rightType != INTTYPE) && (rightType != FLOATTYPE))
        {
        SemanticErr(3);
        }

      break;
      }

    default:
      {
      SemanticErr(1);
      }
    }

  /*fine*/
  }

void
SemanticStatAndOrExprCk(struct MYTOK left_expr, struct MYTOK right_expr)
  {
  int leftType, rightType;

  SemanticCondPrint("SemanticStatAndOrExprCk\n");

  leftType  = left_expr.type;
  rightType = right_expr.type;

  switch (leftType)
    {

    case INTTYPE:

    case FLOATTYPE:
      {
      if ((rightType != INTTYPE) && (rightType != FLOATTYPE))
        {
        SemanticErr(3);
        }

      break;
      }

    default:
      {
      SemanticErr(1);
      }
    }

  /*fine*/
  }

void
SemanticStatNotExprCk(struct MYTOK expr)
  {
  int  exprType;

  SemanticCondPrint("SemanticStatNotExprCk\n");

  exprType  = expr.type;

  if (exprType != INTTYPE && exprType != FLOATTYPE)
    {
    SemanticErr(1);
    }

  /*fine*/
  }

void
SemanticStatPostOpExprCk(struct MYTOK expr)
  {
  int  exprType;

  SemanticCondPrint("SemanticPostOpExprCk\n");

  exprType  = expr.type;

  if (exprType != INTTYPE && exprType != FLOATTYPE)
    {
    SemanticErr(1);
    }

  /*fine*/
  }

void
SemanticStatUnaryExprCk(struct MYTOK expr)
  {
  int  exprType;

  SemanticCondPrint("SemanticStatUnaryExprCk\n");

  exprType  = expr.type;

  if (exprType != INTTYPE && exprType != FLOATTYPE && \
      exprType != SIZETYPE)
    {
    SemanticErr(1);
    }

  /*fine*/
  }

void
SemanticStatPrintTailCk(struct MYTOK exp)
  {
  int exprType;

  SemanticCondPrint("SemanticStatPrintTailCk\n");

  /*******************/
  /*OK simple type:int, float, dayofweek, date, time, string, size, cpr,
   *OK complex: que, job. cnode, server, resmom
   *OK: range of simple:
   *NOT: set
   */

  exprType = exp.type;

  switch (exprType)
    {

    case INTTYPE:

    case FLOATTYPE:

    case DAYOFWEEKTYPE:

    case DATETIMETYPE:

    case STRINGTYPE:

    case SIZETYPE:

    case QUETYPE:

    case JOBTYPE:

    case CNODETYPE:

    case SERVERTYPE:

    case INTRANGETYPE:

    case FLOATRANGETYPE:

    case DAYOFWEEKRANGETYPE:

    case DATETIMERANGETYPE:

    case SIZERANGETYPE:
      {
      break;
      }

    default:
      {
      SemanticErr(1);
      }
    }
  }

void
SemanticStatWhileHeadCk(struct MYTOK exp)
  {
  int exprType;
  SemanticCondPrint("SemanticStatWhileHeadCk\n");

  exprType = exp.type;

  if ((exprType != INTTYPE) && (exprType != FLOATTYPE))
    {
    SemanticErr(1);
    }
  }

void
SemanticStatIfHeadCk(struct MYTOK exp)
  {
  int exprType;
  SemanticCondPrint("SemanticStatIfHeadCk\n");

  exprType = exp.type;

  if ((exprType != INTTYPE) && (exprType != FLOATTYPE))
    {
    SemanticErr(1);
    }
  }

void
SemanticStatReturnTailCk(struct MYTOK exp)
  {
  int exprType;
  int nodeType;
  Np  np;

  SemanticCondPrint("SemanticStatReturnTailCk\n");

  /*******************/
  /*OK simple type:int, float, dayofweek, date, time
   *OK: string, size, server, que, job.
   *OK: cnode, resmom
   *NOT: range, set
   * ALSO: expr must have a type consistent with
   *       the defined return type of the function.
   *       INTTYPE <-> FLOATTYPE is allowed.
   */

  exprType = exp.type;
  np = ParserCurrFunPtrGet();

  switch (exprType)
    {

    case DAYOFWEEKTYPE:

    case DATETIMETYPE:

    case STRINGTYPE:

    case SIZETYPE:

    case SERVERTYPE:

    case QUETYPE:

    case JOBTYPE:

    case CNODETYPE:
      {
      if (np == NULL)
        SemanticErr(2);

      if (NodeGetType(np) != exprType)
        SemanticErr(11);

      break;
      }

    case INTTYPE:

    case FLOATTYPE:
      {
      if (np == NULL)
        SemanticErr(2);

      nodeType = NodeGetType(np);

      if (nodeType != INTTYPE && nodeType != FLOATTYPE)
        SemanticErr(11);

      break;
      }

    default:
      {
      SemanticErr(1);
      }
    }
  }

/* need specify other attribs e.g level?? */
/* tested*/
void
SemanticVarDefCk(struct MYTOK var)
  {
  SemanticCondPrint("SemanticVarDefCk\n");

  if (SymTabFindNodeByLexemeInProg((char *) var.lexeme) == NULL)
    SemanticErr(6);
  }

void
SemanticStatForHeadCk(struct MYTOK exp6, struct MYTOK exp8)
  {
  SemanticCondPrint("SemanticStatForHeadCk\n");

  /* Only INTTYPE and FLOATTYPE which can assign each other. */

  if ((exp6.type != INTTYPE && exp6.type != FLOATTYPE)  || \
      (exp8.type != INTTYPE && exp8.type != FLOATTYPE))
    {
    SemanticErr(1);
    }
  }

void
SemanticStatForAssignCk(struct MYTOK exp1, struct MYTOK exp2)
  {
  SemanticCondPrint("SemanticStatForAssignCk\n");

  if ((exp1.type != INTTYPE && exp1.type != FLOATTYPE)  || \
      (exp2.type != INTTYPE && exp2.type != FLOATTYPE))
    {
    SemanticErr(1);
    }
  }

void
SemanticStatForPostAssignCk(struct MYTOK exp)
  {
  SemanticCondPrint("SemanticStatForPostAssignCk\n");

  if (exp.type != INTTYPE && exp.type != FLOATTYPE)
    {
    SemanticErr(1);
    }
  }

void
SemanticStatForeachHeadCk(struct MYTOK val1, struct MYTOK val2)
  {
  SemanticCondPrint("SemanticStatForeachHeadCk\n");

  switch (val2.type)
    {

    case SERVERSETTYPE:

      if (val1.type != SERVERTYPE)
        SemanticErr(3);

      break;

    case QUESETTYPE:
      if (val1.type != QUETYPE)
        SemanticErr(3);

      break;

    case JOBSETTYPE:
      if (val1.type != JOBTYPE)
        SemanticErr(3);

      break;

    case CNODESETTYPE:
      if (val1.type != CNODETYPE)
        SemanticErr(3);

      break;

    default:
      SemanticErr(1);
    }
  }

/* this returns the val param's corresponding proto type */
int
SemanticParamVarCk(struct MYTOK val)
  {
  int  valType, protoType;
  int  valFunFlag, protoFunFlag;
  Np   npVal, npProto;

  if ((npVal = SymTabFindNodeByLexemeInProg((char *) val.lexeme)) == NULL)
    SemanticErr(6);

  valType    = NodeGetType(npVal);

  valFunFlag = NodeGetFunFlag(npVal);

  npProto = ParserCurrFunParamPtrGet();

  if (npProto == NULL)
    {
    SemanticErr(9);
    }

  protoType    = NodeGetType(npProto);

  protoFunFlag = NodeGetFunFlag(npProto);

  /* YES, YES_INT fun flags are treated the same */

  if ((protoType != GENERICTYPE && valType != protoType) || \
      (valFunFlag != protoFunFlag && \
       (valFunFlag == NO || protoFunFlag == NO)))
    {
    SemanticErr(9);
    }

  /*      set to next argument */
  ParserCurrFunParamPtrPut(npProto);

  return(protoType);
  }

/* Returns the val's corresponding proto type */
int
SemanticParamConstsCk(struct MYTOK val)
  {
  int  valType, protoType;
  Np   npProto;

  valType    = val.type;

  npProto = ParserCurrFunParamPtrGet();

  if (npProto == NULL)
    SemanticErr(9);

  protoType    = NodeGetType(npProto);

  if (protoType != GENERICTYPE && valType != protoType)
    SemanticErr(9);

  /*      set to next argument */
  ParserCurrFunParamPtrPut(npProto);

  return(protoType);
  }

void
SemanticCaseInVarCk(struct MYTOK var)
  {
  if (var.type != INTRANGETYPE && var.type != FLOATRANGETYPE && \
      var.type != DAYOFWEEKRANGETYPE && var.type != DATETIMERANGETYPE && \
      var.type != SIZERANGETYPE && \
      var.type != SERVERSETTYPE && \
      var.type != QUESETTYPE    && var.type != JOBSETTYPE    && \
      var.type != CNODESETTYPE)
    SemanticErr(1);
  }

void
SemanticCaseTypeCk(struct MYTOK val)
  {

  struct MYTOK switchVar;

  switchVar = ParserCurrSwitchVarGet();

  if (switchVar.type != val.type)
    SemanticErr(13);
  }

void
SemanticCaseInTypeCk(struct MYTOK val)
  {

  struct MYTOK switchVar;

  switchVar = ParserCurrSwitchVarGet();

  switch (switchVar.type)
    {

    case SERVERTYPE:
      {
      if (val.type != SERVERSETTYPE)
        SemanticErr(13);

      break;
      }

    case QUETYPE:
      {
      if (val.type != QUESETTYPE)
        SemanticErr(13);

      break;
      }

    case JOBTYPE:
      {
      if (val.type != JOBSETTYPE)
        SemanticErr(13);

      break;
      }

    case CNODETYPE:
      {
      if (val.type != CNODESETTYPE)
        SemanticErr(13);

      break;
      }

    case INTTYPE:
      {
      if (val.type != INTRANGETYPE)
        SemanticErr(13);

      break;
      }

    case FLOATTYPE:
      {
      if (val.type != FLOATRANGETYPE)
        SemanticErr(13);

      break;
      }

    case DAYOFWEEKTYPE:
      {
      if (val.type != DAYOFWEEKRANGETYPE)
        SemanticErr(13);

      break;
      }

    case DATETIMETYPE:
      {
      if (val.type != DATETIMERANGETYPE)
        SemanticErr(13);

      break;
      }

    case SIZETYPE:
      {
      if (val.type != SIZERANGETYPE)
        SemanticErr(13);

      break;
      }

    default:
      {
      SemanticErr(13);
      }
    }
  }

void
SemanticTimeConstCk(struct MYTOK h, struct MYTOK m, struct MYTOK s)
  {
  int hh, mm, ss;

  hh = strToInt((char *)h.lexeme);
  mm = strToInt((char *)m.lexeme);
  ss = strToInt((char *)s.lexeme);

  if (hh < 0 || hh > 23 || \
      mm < 0 || mm > 59 || \
      ss < 0 || ss > 61)
    SemanticErr(16);
  }

void
SemanticDateConstCk(struct MYTOK m, struct MYTOK d, struct MYTOK y)
  {
  int mon, day, year;

  mon  = strToInt((char *)m.lexeme);
  day  = strToInt((char *)d.lexeme);
  year = strToInt((char *)y.lexeme);

  if (mon  < 1 || mon > 12 || \
      day  < 1 || day > 31 || \
      year < 0)
    SemanticErr(16);
  }

void
SemanticIntConstRangeCk(struct MYTOK lo, struct MYTOK hi)
  {
  int low, high;

  low  = strToInt((char *)lo.lexeme);
  high = strToInt((char *)hi.lexeme);

  if (low > high)
    SemanticErr(17);
  }

void
SemanticFloatConstRangeCk(struct MYTOK lo, struct MYTOK hi)
  {
  float low, high;

  low = strToFloat((char *)lo.lexeme);
  high = strToFloat((char *)hi.lexeme);

  if (low > high)
    SemanticErr(17);
  }

void
SemanticDayofweekConstRangeCk(struct MYTOK lo, struct MYTOK hi)
  {
  Dayofweek low, high;

  low = strToDayofweek((char *)lo.lexeme);
  high = strToDayofweek((char *)hi.lexeme);

  if (low > high)
    SemanticErr(17);
  }

void
SemanticDateTimeConstRangeCk(struct MYTOK lo, struct MYTOK hi)
  {
  DateTime low, high;

  low  = strToDateTime((char *)lo.lexeme);
  high = strToDateTime((char *)hi.lexeme);

  if ((datecmp(low.d, strToDate("(0|0|0)")) != 0 || \
       datecmp(high.d, strToDate("(0|0|0)")) != 0) && \
      datetimecmp(low, high) > 0)
    SemanticErr(17);
  }

void
SemanticSizeConstRangeCk(struct MYTOK lo, struct MYTOK hi)
  {
  Size low, high;

  low  = strToSize((char *)lo.lexeme);
  high = strToSize((char *)hi.lexeme);

  if (sizecmp(low, high) > 0)
    SemanticErr(17);
  }
