//////////////////////////////////////////////////////////////////////////////
// Project:     M - cross platform e-mail GUI client
// File name:   MimePartCC.cpp: implementation of MimePartCC class
// Purpose:     MimePartCC is an implementation of MimePartCC ABC using c-client
// Author:      Vadim Zeitlin
// Modified by:
// Created:     29.07.01
// CVS-ID:      $Id$
// Copyright:   (c) 2001 by Vadim Zeitlin <vadim@wxwindows.org>
// Licence:     M license
///////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#ifdef __GNUG__
   #pragma implementation "MimePartCC.h"
#endif

#include "Mpch.h"

#ifndef USE_PCH
   #include "Mcommon.h"
#endif // USE_PCH

#include <wx/fontmap.h>

#include "MimePartCC.h"

#include "Mcclient.h" // for body_types

// ============================================================================
// MimeType implementation
// ============================================================================

// ----------------------------------------------------------------------------
// initializing
// ----------------------------------------------------------------------------

MimeType::MimeType(Primary primary, const String& subtype)
{
   m_primary = primary;
   m_subtype = subtype.Upper();
}

MimeType& MimeType::Assign(const String& mimetype)
{
   String type = mimetype.BeforeFirst('/').Upper();

   m_primary = INVALID;
   for ( size_t n = 0; body_types[n]; n++ )
   {
      if ( type == body_types[n] )
      {
         m_primary = (MimeType::Primary)n;

         break;
      }
   }

   m_subtype = mimetype.AfterFirst('/').Upper();

   return *this;
}

// ----------------------------------------------------------------------------
// accessors
// ----------------------------------------------------------------------------

String MimeType::GetType() const
{
   ASSERT_MSG( IsOk(), "using uninitialized MimeType" );

   // body_types is defined in c-client/rfc822.c
   return body_types[m_primary];
}

// ----------------------------------------------------------------------------
// tests
// ----------------------------------------------------------------------------

bool MimeType::Matches(const MimeType& wildcard) const
{
   return m_primary == wildcard.m_primary &&
            (wildcard.m_subtype == '*' || m_subtype == wildcard.m_subtype);
}

// ============================================================================
// MimePartCC implementation
// ============================================================================

// ----------------------------------------------------------------------------
// ctors/dtors
// ----------------------------------------------------------------------------

void MimePartCC::Init()
{
   m_body = NULL;

   m_parent =
   m_nested =
   m_next = NULL;

   m_message = NULL;

   m_parameterList =
   m_dispositionParameterList = NULL;
}

MimePartCC::MimePartCC(MessageCC *message)
{
   Init();

   m_message = message;

   // if this message is not multipart, we must have "1", otherwise it is
   // unused anyhow (the trouble is that we don't know if we're multipart or
   // not yet as m_body is not set)
   m_spec = '1';
}

MimePartCC::MimePartCC(MimePartCC *parent, size_t nPart)
{
   Init();

   m_parent = parent;

   // IMAP specs are a bit weird: normally the children of the MULTIPART part
   // have its spec as their spec prefix, but not if the main message is a
   // MULTIPART entity itself - see the example in comments in MimePart.h
   //
   // so check that our parent is not the top level [pseudo] part
   if ( m_parent && m_parent->GetParent() )
   {
      m_spec << m_parent->GetPartSpec() << '.';
   }

   m_spec << wxString::Format("%u", nPart);
}

MimePartCC::~MimePartCC()
{
   delete m_next;
   delete m_nested;

   delete m_parameterList;
   delete m_dispositionParameterList;
}

// ----------------------------------------------------------------------------
// MIME tree access
// ----------------------------------------------------------------------------

MimePart *MimePartCC::GetParent() const
{
   return m_parent;
}

MimePart *MimePartCC::GetNext() const
{
   return m_next;
}

MimePart *MimePartCC::GetNested() const
{
   return m_nested;
}

// ----------------------------------------------------------------------------
// headers access
// ----------------------------------------------------------------------------

MimeType MimePartCC::GetType() const
{
   // cast is ok as we use the same values in MimeType as c-client
   return MimeType((MimeType::Primary)m_body->type, m_body->subtype);
}

String MimePartCC::GetDescription() const
{
   return m_body->description;
}

String MimePartCC::GetFilename() const
{
   // try hard to find an acceptable name for this part
   String filename = GetDispositionParam("filename");

   if ( filename.empty() )
      filename = GetParam("filename");

   if ( filename.empty() )
      filename = GetParam("name");

   return filename;
}

String MimePartCC::GetDisposition() const
{
   return m_body->disposition.type;
}

String MimePartCC::GetPartSpec() const
{
   return m_spec;
}

// ----------------------------------------------------------------------------
// parameters access
// ----------------------------------------------------------------------------

/* static */
String MimePartCC::FindParam(const MimeParameterList& list, const String& name)
{
   String value;

   MimeParameterList::iterator i;
   for ( i = list.begin(); i != list.end(); i++ )
   {
      // parameter names are not case-sensitive, i.e. "charset" == "CHARSET"
      if ( name.CmpNoCase(i->name) == 0 )
      {
         // found
         value = i->value;
         break;
      }
   }

   return value;
}

String MimePartCC::GetParam(const String& name) const
{
   return FindParam(GetParameters(), name);
}

String MimePartCC::GetDispositionParam(const String& name) const
{
   return FindParam(GetDispositionParameters(), name);
}

// ----------------------------------------------------------------------------
// parameter lists handling
// ----------------------------------------------------------------------------

/* static */
void MimePartCC::InitParamList(MimeParameterList *list, PARAMETER *par)
{
   while ( par )
   {
      list->push_back(new MimeParameter(par->attribute, par->value));

      par = par->next;
   }
}

const MimeParameterList& MimePartCC::GetParameters() const
{
   if ( !m_parameterList )
   {
      ((MimePartCC *)this)->m_parameterList = new MimeParameterList;
      InitParamList(m_parameterList, m_body->parameter);
   }

   return *m_parameterList;
}

const MimeParameterList& MimePartCC::GetDispositionParameters() const
{
   if ( !m_dispositionParameterList )
   {
      ((MimePartCC *)this)->m_dispositionParameterList = new MimeParameterList;
      InitParamList(m_dispositionParameterList, m_body->disposition.parameter);
   }

   return *m_dispositionParameterList;
}

// ----------------------------------------------------------------------------
// data access
// ----------------------------------------------------------------------------

MessageCC *MimePartCC::GetMessage() const
{
   // only the top level message has m_message set, so delegate to parent if we
   // have one
   return m_parent ? m_parent->GetMessage() : m_message;
}

// MimePartCC::GetContent() is implemented in MessageCC.cpp to reduce
// compilation dependencies

MimeXferEncoding MimePartCC::GetTransferEncoding() const
{
   // cast is ok as we use the same values for MimeXferEncoding as c-client
   return (MimeXferEncoding)m_body->encoding;
}

size_t MimePartCC::GetSize() const
{
   return m_body->size.bytes;
}

// ----------------------------------------------------------------------------
// text part additional info
// ----------------------------------------------------------------------------

size_t MimePartCC::GetNumberOfLines() const
{
   return m_body->size.lines;
}

wxFontEncoding MimePartCC::GetTextEncoding() const
{
   String charset = GetParam("charset");

   return charset.empty() ? wxFONTENCODING_SYSTEM
                          : wxTheFontMapper->CharsetToEncoding(charset);
}
