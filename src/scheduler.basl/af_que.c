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

/* System headers */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

/* local headers */
#include "portability.h"
#include "af_que.h"

/* Macros */
/* File Scope Variables */
static char ident[] = "@(#) $RCSfile$ $Revision$";

struct QueSortArgs
  {
  Que **array;
  SetQue  *set;
  union
    {
    int (*ifunc)();
    double(*ffunc)();
    char    *(*sfunc)();
    DateTime(*dfunc)();
    Size(*szfunc)();
    } key;
  enum { INTKEY, FLOATKEY, STRINGKEY, DATETIMEKEY, SIZEKEY } keytype;
  int  order;     /* ASC or DESC */
  };

/* External Variables */
/* External Functions */
/* Structures and Unions */
/* Signal catching functions */
/* External Functions */
/* Functions */

char *QueNameGet(que)
Que *que;
  {
  if (que == NOQUE)
    return(NULLSTR);

  return que->name;
  }

int QueTypeGet(que)
Que *que;
  {
  if (que == NOQUE)
    return(-1); /* unknown type */

  return que->type;
  }

int QueNumJobsGet(que)
Que *que;
  {
  if (que == NOQUE)
    return(0);

  return que->numJobs;
  }

int QuePriorityGet(que)
Que *que;
  {
  if (que == NOQUE)
    return(-1); /* unknown priority */

  return que->priority;
  }

int QueMaxRunJobsGet(que)
Que *que;
  {
  if (que == NOQUE)
    return(0);

  return que->maxRunJobs;
  }

int QueMaxRunJobsPerUserGet(que)
Que *que;
  {
  if (que == NOQUE)
    return(0);

  return que->maxRunJobsPerUser;
  }

int QueMaxRunJobsPerGroupGet(que)
Que *que;
  {
  if (que == NOQUE)
    return(0);

  return que->maxRunJobsPerGroup;
  }

int QueStateGet(que)
Que *que;
  {
  if (que == NOQUE)
    return(SCHED_DISABLED); /* unknown state */

  return que->state;
  }

int QueIntResAvailGet(que, name)
Que   *que;
char    *name;
  {
  if (que == NOQUE)
    return(-1);

  return(IntResValueGet(que->intResAvail, name));
  }

int QueIntResAssignGet(que, name)
Que   *que;
char    *name;
  {
  if (que == NOQUE)
    return(-1);

  return(IntResValueGet(que->intResAssign, name));
  }

Size QueSizeResAvailGet(que, name)
Que   *que;
char    *name;
  {
  if (que == NOQUE)
    return(strToSize("-1b"));

  return(SizeResValueGet(que->sizeResAvail, name));
  }

Size QueSizeResAssignGet(que, name)
Que   *que;
char    *name;
  {
  if (que == NOQUE)
    return(strToSize("-1b"));

  return(SizeResValueGet(que->sizeResAssign, name));
  }

char *QueStringResAvailGet(que, name)
Que  *que;
char    *name;
  {
  if (que == NOQUE)
    return(NULLSTR);

  return(StringResValueGet(que->stringResAvail, name));
  }

char *QueStringResAssignGet(que, name)
Que  *que;
char    *name;
  {
  if (que == NOQUE)
    return(NULLSTR);

  return(StringResValueGet(que->stringResAssign, name));
  }

struct SetJobElement *QueJobsGet(que)
      Que  *que;
  {
  if (que == NOQUE)
    return(EMPTYSETJOB);

  return(que->jobs.head);
  }

/* Put functions */
void QueNamePut(que, queue_name)
Que *que;
char *queue_name;
  {
  assert(que != NOQUE);

  dynamic_strcpy(&que->name, queue_name);
  varstrModPptr(que->name, que);
  }

void QueTypePut(que, type)
Que *que;
int type;
  {
  assert(que != NOQUE);

  que->type = type;
  }

void QueNumJobsPut(que, numJobs)
Que *que;
int numJobs;
  {
  assert(que != NOQUE);

  que->numJobs = numJobs;
  }

void QueMaxRunJobsPut(que, maxRunJobs)
Que *que;
int maxRunJobs;
  {
  assert(que != NOQUE);

  que->maxRunJobs = maxRunJobs;
  }

void QueMaxRunJobsPerUserPut(que, maxRunJobsPerUser)
Que *que;
int maxRunJobsPerUser;
  {
  assert(que != NOQUE);

  que->maxRunJobsPerUser = maxRunJobsPerUser;
  }

void QueMaxRunJobsPerGroupPut(que, maxRunJobsPerGroup)
Que *que;
int maxRunJobsPerGroup;
  {
  assert(que != NOQUE);

  que->maxRunJobsPerGroup = maxRunJobsPerGroup;
  }

void QuePriorityPut(que, priority)
Que *que;
int priority;
  {
  que->priority = priority;
  }

void QueStatePut(que, state)
Que *que;
int state;
  {
  que->state = state;
  }

void QueIntResAvailPut(que, name, value)
Que   *que;
char    *name;
int     value;
  {

  struct   IntRes * m;

  assert(que != NOQUE && name != NULLSTR);

  m = IntResValuePut(que->intResAvail, name, value, que);

  if (m != NULL)
    que->intResAvail = m;
  }

void QueIntResAssignPut(que, name, value)
Que   *que;
char    *name;
int     value;
  {

  struct   IntRes *m;

  assert(que != NOQUE && name != NULLSTR);

  m = IntResValuePut(que->intResAssign, name, value, que);

  if (m != NULL)
    que->intResAssign = m;
  }

void QueSizeResAvailPut(que, name, value)
Que   *que;
char    *name;
Size    value;
  {

  struct   SizeRes *m;

  assert(que != NOQUE && name != NULLSTR);

  m = SizeResValuePut(que->sizeResAvail, name, value, que);

  if (m != NULL)
    que->sizeResAvail = m;
  }

void QueSizeResAssignPut(que, name, value)
Que   *que;
char    *name;
Size    value;
  {

  struct   SizeRes *m;

  assert(que != NOQUE && name != NULLSTR);

  m = SizeResValuePut(que->sizeResAssign, name, value, que);

  if (m != NULL)
    que->sizeResAssign = m;
  }

void QueStringResAvailPut(que, name, value)
Que   *que;
char    *name;
char    *value;
  {

  struct   StringRes *m;

  assert(que != NOQUE && name != NULLSTR);

  m = StringResValuePut(que->stringResAvail, name, value, que);

  if (m != NULL)
    que->stringResAvail = m;
  }

void QueStringResAssignPut(que, name, value)
Que   *que;
char    *name;
char    *value;
  {

  struct   StringRes *m;

  assert(que != NOQUE && name != NULLSTR);

  m = StringResValuePut(que->stringResAssign, name, value, que);

  if (m != NULL)
    que->stringResAssign = m;
  }

void QueInit(que)
Que  *que;
  {
  assert(que != NOQUE);

  que->name = NULLSTR;
  QueTypePut(que, QTYPE_U);
  QueNumJobsPut(que, 0);
  QuePriorityPut(que, 0);
  QueMaxRunJobsPut(que, 0);
  QueMaxRunJobsPerUserPut(que, 0);
  QueMaxRunJobsPerGroupPut(que, 0);
  QueStatePut(que, SCHED_DISABLED);
  que->intResAvail = NULL;
  que->intResAssign = NULL;
  que->sizeResAvail = NULL;
  que->sizeResAssign = NULL;
  que->stringResAvail = NULL;
  que->stringResAssign = NULL;
  SetJobInit(&que->jobs);
  que->nextptr = NOQUE;
  }

void QuePrint(que)
Que  *que;
  {
  int    type, state;


  if (que == NOQUE)
    return; /* ignore */

  printf("\tQueue Name = %s\n", QueNameGet(que) ? QueNameGet(que) : "null");

  type = QueTypeGet(que);

  printf("\t\tType = %d ", type);

  switch (type)
    {

    case QTYPE_E:
      printf("(EXECUTION)");
      break;

    case QTYPE_R:
      printf("(ROUTE)");
      break;
    }

  printf("\n");

  printf("\t\tNumber of Jobs = %d\n", QueNumJobsGet(que));
  printf("\t\tPriority = %d\n", QuePriorityGet(que));
  printf("\t\tMax Running Jobs Allowed = %d\n", QueMaxRunJobsGet(que));
  printf("\t\tMax Running Jobs Allowed Per User = %d\n",
         QueMaxRunJobsPerUserGet(que));
  printf("\t\tMax Running Jobs Allowed Per Group = %d\n",
         QueMaxRunJobsPerGroupGet(que));
  state = QueStateGet(que);
  printf("\t\tState = %d ", state);

  switch (state)
    {

    case SCHED_DISABLED:
      printf("(DISABLED)");
      break;

    case SCHED_ENABLED:
      printf("(ENABLED)");
      break;
    }

  printf("\n");

  IntResListPrint(que->intResAvail, "\t\tResources_available");
  SizeResListPrint(que->sizeResAvail, "\t\tResources_available");
  StringResListPrint(que->stringResAvail, "\t\tResources_available");
  IntResListPrint(que->intResAssign, "\t\tResources_assigned");
  SizeResListPrint(que->sizeResAssign, "\t\tResources_assigned");
  StringResListPrint(que->stringResAssign, "\t\tResources_assigned");

  SetJobPrint(QueJobsGet(que));
  }

/* Frees up malloc-ed elements of que. The Job elements must be malloc-ed
   since this function attempts to free that struct too! It will only free up
   the Job elements if the # of links (refcnts) to it is zero. */

void QueFree(que)
Que *que;
  {

  if (que == NOQUE || !inMallocTable(que))
    return;  /* ignore if already free */

  varstrFreeByPptr(que);

  mallocTableFreeByPptr(que);

  SetJobFree(&que->jobs);

  /* keep things consistent */
  que->name = NULLSTR;
  }

void QueJobInsert(que, job)
Que *que;
Job *job;
  {
  SetJobAdd(&que->jobs, job);
  QueNumJobsPut(que, QueNumJobsGet(que) + 1);

  }

void QueJobDelete(que, job)
Que *que;
Job *job;
  {
  SetJobRemove(&que->jobs, job);
  QueNumJobsPut(que, QueNumJobsGet(que) - 1);
  }

static Job *intExpr(j, q, func, operator, value, maxj, minj)
Job *j;
Que *q;
int (*func)();
Comp operator;
int value;
Job **maxj;
Job **minj;
  {

  switch (operator)
    {

    case OP_EQ:

      if (func(j) == value)
        return(j);

      break;

    case OP_NEQ:
      if (func(j) != value)
        return(j);

      break;

    case OP_LE:
      if (func(j) <= value)
        {
        return(j);
        }

      break;

    case OP_LT:

      if (func(j) < value)
        return(j);

      break;

    case OP_GE:
      if (func(j) >= value)
        return(j);

      break;

    case OP_GT:
      if (func(j) > value)
        return(j);

      break;

    case OP_MAX:
      if (j == q->jobs.head->job || *maxj == NOJOB || \
          func(j) > func(*maxj))
        {
        *maxj = j;
        }

      break;

    case OP_MIN:

      if (j == q->jobs.head->job || *minj == NOJOB || \
          func(j) < func(*minj))
        {
        *minj = j;
        }

      break;
    }

  return(NOJOB);
  }

static Job *strExpr(j, q, strfunc, operator, valuestr, maxj, minj)
Job  *j;
Que  *q;
char *(*strfunc)();
Comp operator;
char *valuestr;
Job  **maxj;
Job  **minj;
  {

  switch (operator)
    {

    case OP_EQ:

      if (STRCMP(strfunc(j), == , valuestr))
        return(j);

      break;

    case OP_NEQ:
      if (STRCMP(strfunc(j), != , valuestr))
        return(j);

      break;

    case OP_LE:
      if (STRCMP(strfunc(j), <= , valuestr))
        return(j);

      break;

    case OP_LT:
      if (STRCMP(strfunc(j), < , valuestr))
        return(j);

      break;

    case OP_GE:
      if (STRCMP(strfunc(j), >= , valuestr))
        return(j);

      break;

    case OP_GT:
      if (STRCMP(strfunc(j), > , valuestr))
        return(j);

      break;

    case OP_MAX:
      if (j == q->jobs.head->job || STRCMP(strfunc(j), > , strfunc(*maxj)))
        {
        *maxj = j;
        }

      break;

    case OP_MIN:

      if (j == q->jobs.head->job || STRCMP(strfunc(j), < , strfunc(*minj)))
        {
        *minj = j;
        }

      break;
    }

  return(NOJOB);
  }

static Job *dateTimeExpr(j, q, datetfunc, operator, datet, maxj, minj)
Job  *j;
Que  *q;
DateTime(*datetfunc)();
Comp operator;
DateTime datet;
Job  **maxj;
Job  **minj;
  {
  switch (operator)
    {

    case OP_EQ:

      if (DATETIMECMP(datetfunc(j), == , datet))
        return(j);

      break;

    case OP_NEQ:
      if (DATETIMECMP(datetfunc(j), != , datet))
        return(j);

      break;

    case OP_LE:
      if (DATETIMECMP(datetfunc(j), <= , datet))
        return(j);

      break;

    case OP_LT:
      if (DATETIMECMP(datetfunc(j), < , datet))
        return(j);

      break;

    case OP_GE:
      if (DATETIMECMP(datetfunc(j), >= , datet))
        return(j);

      break;

    case OP_GT:
      if (DATETIMECMP(datetfunc(j), > , datet))
        return(j);

      break;

    case OP_MAX:
      if (j == q->jobs.head->job || \
          DATETIMECMP(datetfunc(j), > , datetfunc(*maxj)))
        {
        *maxj = j;
        }

      break;

    case OP_MIN:

      if (j == q->jobs.head->job || \
          DATETIMECMP(datetfunc(j), < , datetfunc(*minj)))
        {
        *minj = j;
        }

      break;
    }

  return(NOJOB);
  }

static Job *sizeExpr(j, q, sizefunc, operator, size, maxj, minj)
Job  *j;
Que  *q;
Size(*sizefunc)();
Comp operator;
Size size;
Job  **maxj;
Job  **minj;
  {
  switch (operator)
    {

    case OP_EQ:

      if (SIZECMP(sizefunc(j), == , size))
        return(j);

      break;

    case OP_NEQ:
      if (SIZECMP(sizefunc(j), != , size))
        return(j);

      break;

    case OP_LE:
      if (SIZECMP(sizefunc(j), <= , size))
        return(j);

      break;

    case OP_LT:
      if (SIZECMP(sizefunc(j), < , size))
        return(j);

      break;

    case OP_GE:
      if (SIZECMP(sizefunc(j), >= , size))
        return(j);

      break;

    case OP_GT:
      if (SIZECMP(sizefunc(j), > , size))
        return(j);

      break;

    case OP_MAX:
      if (j == q->jobs.head->job || \
          SIZECMP(sizefunc(j), > , sizefunc(*maxj)))
        {
        *maxj = j;
        }

      break;

    case OP_MIN:

      if (j == q->jobs.head->job || \
          SIZECMP(sizefunc(j), < , sizefunc(*minj)))
        {
        *minj = j;
        }

      break;
    }

  return(NOJOB);
  }

/* QueJobFindInt: find a job in the que that matches the given    */
/* parameter which can be of the form:       */
/*  func, op, value)       */
/*            where op is one of: GT, GE, LT, LE, EQ, NEQ    */
/*      NOTE: func, and value are of type Int         */
/*     if op is one of: MAX, MIN (operating on ints)  */


Job *QueJobFindInt(Que *que, ...)
  {

  struct  SetJobElement *sje;
  Job     *j1;
  Job     *maxj = NOJOB;
  Job     *minj = NOJOB;
  va_list ap;        /* arg pointer */

  int (*func)();
  Comp    operator;
  int     value;

  void    *generic;

  if (que == NOQUE)
    return(NOJOB);

  va_start(ap, que);

  generic = va_arg(ap, void *);

  operator = va_arg(ap, Comp);

  func = (int (*)()) generic;       /* conv to a func ret INT */

  switch (operator)
    {

    case OP_EQ:

    case OP_NEQ:

    case OP_LE:

    case OP_LT:

    case OP_GE:

    case OP_GT:
      value = va_arg(ap, int);

    case OP_MAX:

    case OP_MIN:

      for (firstJobPtr(&sje, que->jobs.head); sje->job; nextJobPtr(&sje))
        {
        if ((j1 = intExpr(sje->job, que, func, operator, value,
                          &maxj, &minj)) != NOJOB)
          return(j1);
        }

      break;

    default:
      return(NOJOB);
    }

  va_end(ap);

  switch (operator)
    {

    case OP_MAX:
      return(maxj);

    case OP_MIN:
      return(minj);

    default:
      return(NOJOB);
    }
  }

/* QueJobFindStr: find a job in the que that matches the given    */
/* parameter which can be of the form:       */
/*  func, op, value)       */
/*            where op is one of: GT, GE, LT, LE, EQ, NEQ    */
/*      NOTE: func, and value are of type string      */


Job *QueJobFindStr(Que *que, ...)
  {

  struct SetJobElement *sje;
  Job     *j1;
  Job     *maxj = NOJOB;
  Job     *minj = NOJOB;
  va_list ap;        /* arg pointer */

  void    *generic;
  Comp    operator;

  char    *(*strfunc)();
  char    *valuestr;

  if (que == NOQUE)
    return(NOJOB);

  va_start(ap, que);

  generic = va_arg(ap, void *);

  operator = va_arg(ap, Comp);

  strfunc = (char * (*)()) generic; /* conv to a func ret STRING */

  switch (operator)
    {

    case OP_EQ:

    case OP_NEQ:

    case OP_LE:

    case OP_LT:

    case OP_GE:

    case OP_GT:
      valuestr = va_arg(ap, char *);

    case OP_MAX:

    case OP_MIN:

      for (firstJobPtr(&sje, que->jobs.head); sje->job; nextJobPtr(&sje))
        {
        if ((j1 = strExpr(sje->job, que, strfunc, operator, valuestr,
                          &maxj, &minj)) != NOJOB)
          return(j1);
        }

      break;

    default:
      return(NOJOB);
    }

  va_end(ap);

  switch (operator)
    {

    case OP_MAX:
      return(maxj);

    case OP_MIN:
      return(minj);

    default:
      return(NOJOB);
    }
  }

/* QueJobFindDateTime: find a job in the que that matches the given   */
/* parameter which can be of the form:       */
/*  func, op, value)       */
/*            where op is one of: GT, GE, LT, LE, EQ, NEQ    */
/*      NOTE: func, and value are of type Date        */
/*     if op is one of: MAX, MIN (operating on ints)  */


Job *QueJobFindDateTime(Que *que, ...)
  {

  struct SetJobElement *sje;
  Job     *j1;
  Job     *maxj = NOJOB;
  Job     *minj = NOJOB;
  va_list ap;        /* arg pointer */

  void    *generic;
  Comp    operator;

  DateTime(*datetfunc)();
  DateTime datet;

  if (que == NOQUE)
    return(NOJOB);

  va_start(ap, que);

  generic = va_arg(ap, void *);

  operator = va_arg(ap, Comp);

  datetfunc = (DateTime(*)()) generic;   /* conv to a func ret DATETIME*/

  switch (operator)
    {

    case OP_EQ:

    case OP_NEQ:

    case OP_LE:

    case OP_LT:

    case OP_GE:

    case OP_GT:
      datet = va_arg(ap, DateTime);

    case OP_MAX:

    case OP_MIN:

      for (firstJobPtr(&sje, que->jobs.head); sje->job; nextJobPtr(&sje))
        {
        if ((j1 = dateTimeExpr(sje->job, que, datetfunc, operator,
                               datet, &maxj, &minj)) != NOJOB)
          return(j1);
        }

      break;

    default:
      return(NOJOB);
    }

  va_end(ap);

  switch (operator)
    {

    case OP_MAX:
      return(maxj);

    case OP_MIN:
      return(minj);

    default:
      return(NOJOB);
    }
  }

/* QueJobFindSize: find a job in the que that matches the given   */
/* parameter which can be of the form:       */
/*  func, op, value)       */
/*            where op is one of: GT, GE, LT, LE, EQ, NEQ    */
/*      NOTE: func, and value are of type Size        */


Job *QueJobFindSize(Que *que, ...)
  {

  struct SetJobElement *sje;
  Job     *j1;
  Job     *maxj = NOJOB;
  Job     *minj = NOJOB;
  va_list ap;        /* arg pointer */

  void    *generic;
  Comp    operator;

  Size(*sizefunc)();
  Size    size;

  if (que == NOQUE)
    return(NOJOB);

  va_start(ap, que);

  generic = va_arg(ap, void *);

  operator = va_arg(ap, Comp);

  sizefunc = (Size(*)()) generic;   /* conv to a func ret TIME */

  switch (operator)
    {

    case OP_EQ:

    case OP_NEQ:

    case OP_LE:

    case OP_LT:

    case OP_GE:

    case OP_GT:
      size = va_arg(ap, Size);

    case OP_MAX:

    case OP_MIN:

      for (firstJobPtr(&sje, que->jobs.head); sje->job; nextJobPtr(&sje))
        {
        if ((j1 = sizeExpr(sje->job, que, sizefunc, operator, size,
                           &maxj, &minj)) != NOJOB)
          return(j1);
        }

      break;

    default:
      return(NOJOB);
    }

  va_end(ap);

  switch (operator)
    {

    case OP_MAX:
      return(maxj);

    case OP_MIN:
      return(minj);

    default:
      return(NOJOB);
    }
  }

/* QueFilterInt: Create "newque" of jobs by filtering the 'que' job     */
/* using the specified parameters (func, operator, value)               */
/*                        if op is one of: GT, GE, LT, LE, EQ, NEQ      */
/*                         NOTE: func, and value are of type Int.       */
/* RETURNS: newque */

Que *QueFilterInt(que, func, operator, value)
Que  *que;
int (*func)();
Comp operator;
int  value;
  {

  struct SetJobElement *sje;
  Job    *j1;
  Que    *newque;

  if (que == NOQUE)
    return(NOQUE);

  newque = (Que *)malloc(sizeof(Que));

  mallocTableAdd(newque, NULL, -1); /* -1 for temporary space */

#ifdef DEBUG
  printf("In QueFilterInt, added newque %x to mallocTable\n", newque);

#endif

  assert(newque != NOQUE);

  QueInit(newque);

  QueNamePut(newque, "QueFilterInt");

  switch (operator)
    {

    case OP_EQ:

    case OP_NEQ:

    case OP_LE:

    case OP_LT:

    case OP_GE:

    case OP_GT:

      for (firstJobPtr(&sje, que->jobs.head); sje->job; nextJobPtr(&sje))
        {
        if ((j1 = intExpr(sje->job, que, func, operator, value,
                          (Job **)NULL, (Job **)NULL)) != NOJOB)
          QueJobInsert(newque, j1);
        }

      break;

    default:
      return(NOQUE);
    }

  return(newque);
  }

/* QueFilterStr: Create a new que of jobs by filtering the 'que' job     */
/* using the specified parameters (strfunc, operator, valuestr)          */
/*                        if op is one of: GT, GE, LT, LE, EQ, NEQ       */
/* RETURNS: newque */

Que *QueFilterStr(que, strfunc, operator, valuestr)
Que  *que;
char *(*strfunc)();
Comp operator;
char *valuestr;
  {

  struct SetJobElement *sje;
  Job    *j1;
  Que    *newque;

  if (que == NOQUE)
    return(NOQUE);

  newque = (Que *)malloc(sizeof(Que));

  mallocTableAdd(newque, NULL, -1);

#ifdef DEBUG
  printf("In QueFilterStr, added newque %x to mallocTable\n", newque);

#endif

  assert(newque != NOQUE);

  QueInit(newque);

  QueNamePut(newque, "QueFilterStr");

  switch (operator)
    {

    case OP_EQ:

    case OP_NEQ:

    case OP_LE:

    case OP_LT:

    case OP_GE:

    case OP_GT:

      for (firstJobPtr(&sje, que->jobs.head); sje->job; nextJobPtr(&sje))
        {
        if ((j1 = strExpr(sje->job, que, strfunc, operator, valuestr,
                          (Job **)NULL, (Job **)NULL)) != NOJOB)
          QueJobInsert(newque, j1);
        }

      break;

    default:
      return(NOQUE);
    }

  return(newque);
  }

/* QueFilterDateTime: Create a new que of jobs by filtering the 'que' job*/
/* using the specified parameters (datetfunc, operator, datet)           */
/*                        if op is one of: GT, GE, LT, LE, EQ, NEQ       */
/* RETURNS: newque ptr.                         */

Que *QueFilterDateTime(que, datetfunc, operator, datet)
Que  *que;
DateTime(*datetfunc)();
Comp operator;
DateTime datet;
  {

  struct SetJobElement *sje;
  Job    *j1;
  Que    *newque;

  if (que == NOQUE)
    return(NOQUE);

  newque = (Que *)malloc(sizeof(Que));

  mallocTableAdd(newque, NULL, -1);

#ifdef DEBUG
  printf("In QueFilterDateTime, added newque %x to mallocTable\n", newque);

#endif

  QueInit(newque);

  QueNamePut(newque, "QueFilterDateTime");

  switch (operator)
    {

    case OP_EQ:

    case OP_NEQ:

    case OP_LE:

    case OP_LT:

    case OP_GE:

    case OP_GT:

      for (firstJobPtr(&sje, que->jobs.head); sje->job; nextJobPtr(&sje))
        {
        if ((j1 = dateTimeExpr(sje->job, que, datetfunc, operator,
                               datet, (Job **)NULL, (Job **)NULL)) != NOJOB)
          QueJobInsert(newque, j1);
        }

      break;

    default:
      return(NOQUE);
    }

  return(newque);
  }

/* QueFilterSize: Create a new que of jobs by filtering the 'que' job    */
/* using the specified parameters (sizefunc, op, size)                   */
/*                        if op is one of: GT, GE, LT, LE, EQ, NEQ       */
/* RETURNS: 0 of successful; non-zero otherwise.                         */

Que *QueFilterSize(que, sizefunc, operator, size)
Que  *que;
Size(*sizefunc)();
Comp operator;
Size size;
  {

  struct  SetJobElement *sje;
  Job     *j1;
  Que     *newque;

  if (que == NOQUE)
    return(NOQUE);

  newque = (Que *)malloc(sizeof(Que));

  mallocTableAdd(newque, NULL, -1);

#ifdef DEBUG
  printf("In QueFilterSize, added newque %x to mallocTable\n", newque);

#endif

  QueInit(newque);

  QueNamePut(newque, "QueFilterSize");

  switch (operator)
    {

    case OP_EQ:

    case OP_NEQ:

    case OP_LE:

    case OP_LT:

    case OP_GE:

    case OP_GT:

      for (firstJobPtr(&sje, que->jobs.head); sje->job; nextJobPtr(&sje))
        {
        if ((j1 = sizeExpr(sje->job, que, sizefunc, operator, size,
                           (Job **)NULL, (Job **)NULL)) != NOJOB)
          QueJobInsert(newque, j1);
        }

      break;

    default:
      return(NOQUE);
    }

  return(newque);
  }

/* set of Ques abstraction */
void SetQueInit(sq)
SetQue *sq;
  {
  sq->head = NOQUE;
  sq->tail = NOQUE;
  }

/* adds to the end of the list */
void SetQueAdd(sq, q)
SetQue  *sq;
Que   *q;
  {
  assert(sq != EMPTYSETQUE);

  assert((sq->head == NOQUE && sq->tail == NOQUE) || \
         (sq->head != NOQUE && sq->tail != NOQUE));

  assert(q != NOQUE);

  mallocTableAdd(q, sq, 0);
#ifdef DEBUG
  printf("Added q %x to SetQue %x\n", q, sq);
  QuePrint(q);
#endif
  q->nextptr = NOQUE;

  if (sq->head == NOQUE && sq->tail == NOQUE)
    {
    sq->head = q;
    sq->tail = q;
    }
  else
    {
    sq->tail->nextptr = q;
    sq->tail = q;
    }

  }

void SetQueFree(sq)
SetQue *sq;
  {
  Que     *q, *qtmp;

  if (sq == EMPTYSETQUE)
    return; /* ignore */

  for (q = sq->head; q; q = qtmp)
    {
    qtmp = q->nextptr;
    /* free up list of jobs attached to q */
    QueFree(q);
    }

  /* Free up the Que structures themselves */
  mallocTableFreeByPptr(sq);

  /* Assuming of course that sq is non-malloced! */
  SetQueInit(sq);
  }

Que *SetQueFindQueByName(sq, que_name)
SetQue *sq;
char   *que_name;
  {
  Que    *q;

  if (sq == EMPTYSETQUE)
    return(NOQUE);

  for (q = sq->head; q; q = q->nextptr)
    {
    if (STRCMP(QueNameGet(q) , == , que_name))
      return(q);

    }

  return(NOQUE);
  }

void SetQuePrint(sq)
SetQue *sq;
  {
  Que *q;

  for (q = sq->head; q; q = q->nextptr)
    {
    QuePrint(q);
    printf("\n");
    }
  }

int inSetQue(que, sq)
Que *que;
SetQue *sq;
  {
  Que *q;

  if (sq == EMPTYSETQUE)
    return(0);

  for (q = sq->head; q; q = q->nextptr)
    {
    if (q == que)
      return(1);
    }

  return(0);
  }

static int QuePartition(A, p, r)

struct QueSortArgs      *A;
int                     p;
int                     r;
  {

  Que                   *temptr;
  Que                   *temptr2;
  int                   i, j;
  int                     k;

  assert(A->keytype == INTKEY || A->keytype == STRINGKEY || \
         A->keytype == DATETIMEKEY || A->keytype == SIZEKEY || \
         A->keytype == FLOATKEY);

  i = p - 1;
  j = r + 1;

  while (TRUE)
    {

    switch (A->keytype)
      {

      case INTKEY:

        do
          {
          j--;
          }
        while ((A->order == ASC  && A->key.ifunc(A->array[j]) > \
                A->key.ifunc(A->array[p])) ||  \
               (A->order == DESC && A->key.ifunc(A->array[j]) < \
                A->key.ifunc(A->array[p])));

        do
          {
          i++;
          }
        while ((A->order == ASC  && A->key.ifunc(A->array[i]) < \
                A->key.ifunc(A->array[p])) ||  \
               (A->order == DESC && A->key.ifunc(A->array[i]) > \
                A->key.ifunc(A->array[p])));

        break;

      case FLOATKEY:
        do
          {
          j--;
          }
        while ((A->order == ASC  && A->key.ffunc(A->array[j]) > \
                A->key.ffunc(A->array[p])) ||  \
               (A->order == DESC && A->key.ffunc(A->array[j]) < \
                A->key.ffunc(A->array[p])));

        do
          {
          i++;
          }
        while ((A->order == ASC  && A->key.ffunc(A->array[i]) < \
                A->key.ffunc(A->array[p])) ||  \
               (A->order == DESC && A->key.ffunc(A->array[i]) > \
                A->key.ffunc(A->array[p])));

        break;

      case STRINGKEY:
        do
          {
          j--;
          }
        while ((A->order == ASC && \
                STRCMP(A->key.sfunc(A->array[j]),
                       > , A->key.sfunc(A->array[p]))) || \
               (A->order == DESC && \
                STRCMP(A->key.sfunc(A->array[j]),
                       < , A->key.sfunc(A->array[p]))));

        do
          {
          i++;
          }
        while ((A->order == ASC && \
                STRCMP(A->key.sfunc(A->array[i]),
                       < , A->key.sfunc(A->array[p]))) || \
               (A->order == DESC && \
                STRCMP(A->key.sfunc(A->array[i]),
                       > , A->key.sfunc(A->array[p]))));

        break;

      case DATETIMEKEY:
        do
          {
          j--;
          }
        while ((A->order == ASC && \
                DATETIMECMP(A->key.dfunc(A->array[j]),
                            > , A->key.dfunc(A->array[p]))) || \
               (A->order == DESC && \
                DATETIMECMP(A->key.dfunc(A->array[j]),
                            < , A->key.dfunc(A->array[p]))));

        do
          {
          i++;
          }
        while ((A->order == ASC && \
                DATETIMECMP(A->key.dfunc(A->array[i]),
                            < , A->key.dfunc(A->array[p]))) || \
               (A->order == DESC && \
                DATETIMECMP(A->key.dfunc(A->array[i]),
                            > , A->key.dfunc(A->array[p]))));

        break;

      case SIZEKEY:
        do
          {
          j--;
          }
        while ((A->order == ASC && \
                SIZECMP(A->key.szfunc(A->array[j]),
                        > , A->key.szfunc(A->array[p]))) || \
               (A->order == DESC && \
                SIZECMP(A->key.szfunc(A->array[j]),
                        < , A->key.szfunc(A->array[p]))));

        do
          {
          i++;
          }
        while ((A->order == ASC && \
                SIZECMP(A->key.szfunc(A->array[i]),
                        < , A->key.szfunc(A->array[p]))) || \
               (A->order == DESC && \
                SIZECMP(A->key.szfunc(A->array[i]),
                        > , A->key.szfunc(A->array[p]))));

        break;
      }

    if (i < j)
      {
      /* move the pointers around */
      if (i + 1 == j)
        {
        temptr2 = A->array[i];
        }
      else
        {
        temptr2 = A->array[i]->nextptr;
        }

      A->array[i]->nextptr = A->array[j]->nextptr;

      A->array[j]->nextptr = temptr2;


      if (i - 1 >= 0 && i - 1 < dynamicArraySize(A->array))
        {
        A->array[i-1]->nextptr = A->array[j];
        }

      if (j - 1 != i && j - 1 >= 0 && j - 1 < dynamicArraySize(A->array))
        {
        A->array[j-1]->nextptr = A->array[i];
        }

      if (A->set->head == A->array[i])
        {
        A->set->head = A->array[j];
        }

      if (A->set->tail == A->array[j])
        {
        A->set->tail = A->array[i];
        }

      /* update the entries on the array */
      temptr = A->array[j];

      A->array[j] = A->array[i];

      A->array[i] = temptr;


      }
    else
      {
      return(j);
      }

    } /* while */
  }

static void QueQuickSort(A, p, r)

struct QueSortArgs      *A;
int                     p;
int                     r;
  {
  int     q;

  if (p < r)
    {

    q = QuePartition(A, p, r);
    QueQuickSort(A, p, q);
    QueQuickSort(A, q + 1, r);
    }
  }

int SetQueSortInt(s, key, order)
SetQue            *s;
int (*key)();
int                  order;
  {

  Que    **s_ptrs = NULL;
  int    beforecnt, aftercnt;

  struct QueSortArgs A;
  Que *q;

  if (order != ASC && order != DESC)
    {
    fprintf(stderr,
            "SetQueSortInt: order != ASC and order !=DESC\n");
    return(FAIL);
    }

  beforecnt = 0;

  for (q = s->head; q; q = q->nextptr)
    {
    s_ptrs = (Que **)extendDynamicArray(s_ptrs,
                                        beforecnt + 1, sizeof(Que *));

    aftercnt = dynamicArraySize(s_ptrs);

    if (beforecnt == aftercnt)
      {
      fprintf(stderr,
              "SetQueSortInt: Unable to realloc s_ptrs");

      if (aftercnt > 0)
        {
        freeDynamicArray(s_ptrs);
        }

      return(FAIL);
      }

    s_ptrs[beforecnt] = q;

    beforecnt++;
    }

  A.array = s_ptrs;

  A.set = s;
  A.key.ifunc = key;
  A.keytype = INTKEY;
  A.order   = order;

  QueQuickSort(&A, 0, beforecnt - 1);

  freeDynamicArray(s_ptrs);
  return(SUCCESS);
  }

int SetQueSortStr(s, key, order)
SetQue            *s;
char                 *(*key)();
int                  order;
  {

  Que    **s_ptrs = NULL;
  int    beforecnt, aftercnt;

  struct QueSortArgs A;
  Que *q;

  if (order != ASC && order != DESC)
    {
    fprintf(stderr,
            "SetQueSortStr: order != ASC and order !=DESC\n");
    return(FAIL);
    }

  beforecnt = 0;

  for (q = s->head; q; q = q->nextptr)
    {
    s_ptrs = (Que **)extendDynamicArray(s_ptrs,
                                        beforecnt + 1, sizeof(Que *));

    aftercnt = dynamicArraySize(s_ptrs);

    if (beforecnt == aftercnt)
      {
      fprintf(stderr,
              "SetQueSortStr: Unable to realloc s_ptrs");

      if (aftercnt > 0)
        {
        freeDynamicArray(s_ptrs);
        }

      return(FAIL);
      }

    s_ptrs[beforecnt] = q;

    beforecnt++;
    }

  A.array = s_ptrs;

  A.set = s;
  A.key.sfunc = key;
  A.keytype = STRINGKEY;
  A.order   = order;

  QueQuickSort(&A, 0, beforecnt - 1);

  freeDynamicArray(s_ptrs);
  return(SUCCESS);
  }

int SetQueSortDateTime(s, key, order)
SetQue            *s;
DateTime(*key)();
int                  order;
  {

  Que    **s_ptrs = NULL;
  int    beforecnt, aftercnt;

  struct QueSortArgs A;
  Que *q;

  if (order != ASC && order != DESC)
    {
    fprintf(stderr,
            "SetQueSortDateTime: order != ASC and order !=DESC\n");
    return(FAIL);
    }

  beforecnt = 0;

  for (q = s->head; q; q = q->nextptr)
    {
    s_ptrs = (Que **)extendDynamicArray(s_ptrs,
                                        beforecnt + 1, sizeof(Que *));

    aftercnt = dynamicArraySize(s_ptrs);

    if (beforecnt == aftercnt)
      {
      fprintf(stderr,
              "SetQueSortDateTime: Unable to realloc s_ptrs");

      if (aftercnt > 0)
        {
        freeDynamicArray(s_ptrs);
        }

      return(FAIL);
      }

    s_ptrs[beforecnt] = q;

    beforecnt++;
    }

  A.array = s_ptrs;

  A.set = s;
  A.key.dfunc = key;
  A.keytype = DATETIMEKEY;
  A.order   = order;

  QueQuickSort(&A, 0, beforecnt - 1);

  freeDynamicArray(s_ptrs);
  return(SUCCESS);
  }

int SetQueSortSize(s, key, order)
SetQue            *s;
Size(*key)();
int                  order;
  {

  Que    **s_ptrs = NULL;
  int    beforecnt, aftercnt;

  struct QueSortArgs A;
  Que *q;

  if (order != ASC && order != DESC)
    {
    fprintf(stderr,
            "SetQueSortSize: order != ASC and order !=DESC\n");
    return(FAIL);
    }

  beforecnt = 0;

  for (q = s->head; q; q = q->nextptr)
    {
    s_ptrs = (Que **)extendDynamicArray(s_ptrs,
                                        beforecnt + 1, sizeof(Que *));

    aftercnt = dynamicArraySize(s_ptrs);

    if (beforecnt == aftercnt)
      {
      fprintf(stderr,
              "SetQueSortSize: Unable to realloc s_ptrs");

      if (aftercnt > 0)
        {
        freeDynamicArray(s_ptrs);
        }

      return(FAIL);
      }

    s_ptrs[beforecnt] = q;

    beforecnt++;
    }

  A.array = s_ptrs;

  A.set = s;
  A.key.szfunc = key;
  A.keytype = SIZEKEY;
  A.order   = order;

  QueQuickSort(&A, 0, beforecnt - 1);

  freeDynamicArray(s_ptrs);
  return(SUCCESS);
  }

int SetQueSortFloat(s, key, order)
SetQue            *s;
double(*key)();
int                  order;
  {

  Que    **s_ptrs = NULL;
  int    beforecnt, aftercnt;

  struct QueSortArgs A;
  Que *q;

  if (order != ASC && order != DESC)
    {
    fprintf(stderr,
            "SetQueSortSize: order != ASC and order !=DESC\n");
    return(FAIL);
    }

  beforecnt = 0;

  for (q = s->head; q; q = q->nextptr)
    {
    s_ptrs = (Que **)extendDynamicArray(s_ptrs,
                                        beforecnt + 1, sizeof(Que *));

    aftercnt = dynamicArraySize(s_ptrs);

    if (beforecnt == aftercnt)
      {
      fprintf(stderr,
              "SetQueSortSize: Unable to realloc s_ptrs");

      if (aftercnt > 0)
        {
        freeDynamicArray(s_ptrs);
        }

      return(FAIL);
      }

    s_ptrs[beforecnt] = q;

    beforecnt++;
    }

  A.array = s_ptrs;

  A.set = s;
  A.key.ffunc = key;
  A.keytype = FLOATKEY;
  A.order   = order;

  QueQuickSort(&A, 0, beforecnt - 1);

  freeDynamicArray(s_ptrs);
  return(SUCCESS);
  }
