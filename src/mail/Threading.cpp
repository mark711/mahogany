///////////////////////////////////////////////////////////////////////////////
// Project:     Mahogany - cross platform e-mail GUI client
// File name:   mail/Threading.cpp - sorting related constants and functions
// Purpose:     implements ThreadParams
// Author:      Vadim Zeitlin
// Modified by:
// Created:     04.09.01
// CVS-ID:      $Id$
// Copyright:   (c) 2001 Vadim Zeitlin
// Licence:     M license
///////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

#ifdef __GNUG__
   #pragma implementation "Threading.h"
#endif

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#include "Mpch.h"

#ifndef USE_PCH
   #include "Mcommon.h"

   #include "Profile.h"
   #include "Threading.h"
#endif // USE_PCH

#include "Mdefaults.h"

// ----------------------------------------------------------------------------
// options we use
// ----------------------------------------------------------------------------

extern const MOption MP_MSGS_BREAK_THREAD;
extern const MOption MP_MSGS_GATHER_SUBJECTS;
extern const MOption MP_MSGS_INDENT_IF_DUMMY;
extern const MOption MP_MSGS_REPLACEMENT_STRING;
extern const MOption MP_MSGS_SIMPLIFYING_REGEX;
extern const MOption MP_MSGS_USE_THREADING;

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// ThreadParams initialization
// ----------------------------------------------------------------------------

ThreadParams::ThreadParams()
{
   useThreading =
   gatherSubjects =
   breakThread =
   indentIfDummyNode = false;
}

void ThreadParams::Read(Profile *profile)
{
   useThreading = READ_CONFIG_BOOL(profile, MP_MSGS_USE_THREADING);
   gatherSubjects = READ_CONFIG_BOOL(profile, MP_MSGS_GATHER_SUBJECTS);
   breakThread = READ_CONFIG_BOOL(profile, MP_MSGS_BREAK_THREAD);
   simplifyingRegex = READ_CONFIG_TEXT(profile, MP_MSGS_SIMPLIFYING_REGEX);
   replacementString = READ_CONFIG_TEXT(profile, MP_MSGS_REPLACEMENT_STRING);
   indentIfDummyNode = READ_CONFIG_BOOL(profile, MP_MSGS_INDENT_IF_DUMMY);
}

// ----------------------------------------------------------------------------
// ThreadParams comparison
// ----------------------------------------------------------------------------

bool ThreadParams::operator==(const ThreadParams& other) const
{
  if ( useThreading != other.useThreading )
     return false;

  if ( !useThreading )
     return true;

  return simplifyingRegex == other.simplifyingRegex &&
         replacementString == other.replacementString &&
         gatherSubjects == other.gatherSubjects &&
         breakThread == other.breakThread &&
         indentIfDummyNode == other.indentIfDummyNode;
}
