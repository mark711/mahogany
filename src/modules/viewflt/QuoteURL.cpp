///////////////////////////////////////////////////////////////////////////////
// Project:     M - cross platform e-mail GUI client
// File name:   QuoteURL.cpp: implementation of "default" viewer filter
// Purpose:     default filter handles URL detection and quoting levels
// Author:      Vadim Zeitlin
// Modified by:
// Created:     01.12.02 (extracted from src/classes/MessageView.cpp)
// CVS-ID:      $Id$
// Copyright:   (c) 2002 Vadim Zeitlin <vadim@wxwindows.org>
// Licence:     M license
///////////////////////////////////////////////////////////////////////////////

/*
   Normally we should have 2 different filters instead of only one which does
   both quote level and URL detection but this is difficult because it's not
   clear which one should be applied first: URLs may span several lines so we
   can't handle quoting first, then URLs (at least not if we want to detect
   quoetd wrapped URLs -- which we don't do now). And detecting URLs first and
   then passing the rest through quote filter is quite unnatural -- but maybe
   could still be done?
 */

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#include "Mpch.h"

#ifndef USE_PCH
   #include "Mcommon.h"
#endif //USE_PCH

#include "MTextStyle.h"
#include "ViewFilter.h"

#include "MessageViewer.h"

#include "miscutil.h"         // for GetColourByName()

// ----------------------------------------------------------------------------
// options we use here
// ----------------------------------------------------------------------------

extern const MOption MP_HIGHLIGHT_URLS;
extern const MOption MP_MVIEW_QUOTED_COLOURIZE;
extern const MOption MP_MVIEW_QUOTED_CYCLE_COLOURS;
extern const MOption MP_MVIEW_QUOTED_COLOUR1;
extern const MOption MP_MVIEW_QUOTED_COLOUR2;
extern const MOption MP_MVIEW_QUOTED_COLOUR3;
extern const MOption MP_MVIEW_QUOTED_MAXWHITESPACE;
extern const MOption MP_MVIEW_QUOTED_MAXALPHA;
extern const MOption MP_MVIEW_URLCOLOUR;

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

// the max quoting level for which we have unique colours (after that we
// recycle them or just reuse the last one)
static const size_t QUOTE_LEVEL_MAX = 3;

// invalid quote level
static const size_t LEVEL_INVALID = (size_t)-1;

// ----------------------------------------------------------------------------
// QuoteURLFilter declaration
// ----------------------------------------------------------------------------

class QuoteURLFilter : public ViewFilter
{
public:
   QuoteURLFilter(MessageView *msgView, ViewFilter *next, bool enable)
      : ViewFilter(msgView, next, enable)
   {
      ReadOptions(msgView->GetProfile());
   }

protected:
   // the main work function
   virtual void DoProcess(String& text,
                          MessageViewer *viewer,
                          MTextStyle& style);

   // fill m_options using the values from the given profile
   void ReadOptions(Profile *profile);

   // helpers
   size_t GetQuotedLevel(const wxChar *text) const;
   wxColour GetQuoteColour(size_t qlevel) const;

   struct Options
   {
      // URL colour (used only if highlightURLs)
      wxColour UrlCol;

      // the colours for quoted text (only used if quotedColourize)
      //
      // the first element in this array is the normal foreground colour, i.e.
      // quote level == 0 <=> unquoted
      wxColour QuotedCol[QUOTE_LEVEL_MAX + 1];

      // max number of whitespaces before >
      int quotedMaxWhitespace;

      // max number of A-Z before >
      int quotedMaxAlpha;

      // do quoted text colourizing?
      bool quotedColourize:1;

      // if there is > QUOTE_LEVEL_MAX levels of quoting, recycle colours?
      bool quotedCycleColours:1;

      /// highlight URLs?
      bool highlightURLs:1;
   } m_options;
};

// ============================================================================
// QuoteURLFilter implementation
// ============================================================================

// ----------------------------------------------------------------------------
// the required macro
// ----------------------------------------------------------------------------

// this filter has the default priority -- in fact, the definition of the
// default priority is exactly that this filter has it
IMPLEMENT_VIEWER_FILTER(QuoteURLFilter,
                        ViewFilter::Priority_Default,
                        true,      // enabled by default
                        _("Quotes and URLs"),
                        "(c) 2002 Vadim Zeitlin <vadim@wxwindows.org>");

// ----------------------------------------------------------------------------
// QuoteURLFilter options
// ----------------------------------------------------------------------------

void
QuoteURLFilter::ReadOptions(Profile *profile)
{
   // a macro to make setting many colour options less painful
   #define GET_COLOUR_FROM_PROFILE(col, name) \
      GetColourByName(&col, \
                      READ_CONFIG(profile, MP_MVIEW_##name), \
                      GetStringDefault(MP_MVIEW_##name))

   GET_COLOUR_FROM_PROFILE(m_options.QuotedCol[1], QUOTED_COLOUR1);
   GET_COLOUR_FROM_PROFILE(m_options.QuotedCol[2], QUOTED_COLOUR2);
   GET_COLOUR_FROM_PROFILE(m_options.QuotedCol[3], QUOTED_COLOUR3);

   GET_COLOUR_FROM_PROFILE(m_options.UrlCol, URLCOLOUR);

   #undef GET_COLOUR_FROM_PROFILE

   m_options.quotedColourize =
       READ_CONFIG_BOOL(profile, MP_MVIEW_QUOTED_COLOURIZE);
   m_options.quotedCycleColours =
       READ_CONFIG_BOOL(profile, MP_MVIEW_QUOTED_CYCLE_COLOURS);
   m_options.quotedMaxWhitespace =
       READ_CONFIG_BOOL(profile, MP_MVIEW_QUOTED_MAXWHITESPACE);
   m_options.quotedMaxAlpha = READ_CONFIG(profile, MP_MVIEW_QUOTED_MAXALPHA);

   m_options.highlightURLs = READ_CONFIG_BOOL(profile, MP_HIGHLIGHT_URLS);
}

// ----------------------------------------------------------------------------
// QuoteURLFilter helpers for quoting level code
// ----------------------------------------------------------------------------

size_t
QuoteURLFilter::GetQuotedLevel(const wxChar *text) const
{
   size_t qlevel = strutil_countquotinglevels
                   (
                     text,
                     m_options.quotedMaxWhitespace,
                     m_options.quotedMaxAlpha
                   );

   // note that qlevel is counted from 1, really, as 0 means unquoted and that
   // GetQuoteColour() relies on this
   if ( qlevel > QUOTE_LEVEL_MAX )
   {
      if ( m_options.quotedCycleColours )
      {
         // cycle through the colours: use 1st level colour for QUOTE_LEVEL_MAX
         qlevel = (qlevel - 1) % QUOTE_LEVEL_MAX + 1;
      }
      else
      {
         // use the same colour for all levels deeper than max
         qlevel = QUOTE_LEVEL_MAX;
      }
   }

   return qlevel;
}

wxColour
QuoteURLFilter::GetQuoteColour(size_t qlevel) const
{
   CHECK( qlevel < QUOTE_LEVEL_MAX + 1, wxNullColour,
          _T("QuoteURLFilter::GetQuoteColour(): invalid quoting level") );

   return m_options.QuotedCol[qlevel];
}

// ----------------------------------------------------------------------------
// QuoteURLFilter::DoProcess() itself
// ----------------------------------------------------------------------------

void
QuoteURLFilter::DoProcess(String& text,
                          MessageViewer *viewer,
                          MTextStyle& style)
{
   // the default foreground colour
   m_options.QuotedCol[0] = style.GetTextColour();

   String url,
          before;

   size_t levelBeforeURL = LEVEL_INVALID;

   do
   {
      if ( m_options.highlightURLs )
      {
         // extract the first URL into url string and put all preceding
         // text into before, text is updated to contain only the text
         // after the URL
         before = strutil_findurl(text, url);
      }
      else // no URL highlighting
      {
         before = text;

         text.clear();
      }

      if ( m_options.quotedColourize )
      {
         size_t level;

         // if we have just inserted an URL, restore the same level we were
         // using before as otherwise foo in a line like "> URL foo" wouldn't
         // be highlighted correctly
         if ( levelBeforeURL != LEVEL_INVALID )
         {
            level = levelBeforeURL;
            levelBeforeURL = LEVEL_INVALID;
         }
         else // no preceding URL, we're really at the start of line
         {
            level = GetQuotedLevel(before);
         }

         style.SetTextColour(GetQuoteColour(level));

         // lineCur is the start of the current line, lineNext of the next one
         const wxChar *lineCur = before.c_str();
         const wxChar *lineNext = wxStrchr(lineCur, '\n');
         while ( lineNext )
         {
            // skip '\n'
            lineNext++;

            // calculate the quoting level for this line
            size_t levelNew = GetQuotedLevel(lineNext);
            if ( levelNew != level )
            {
               String line(lineCur, lineNext);
               m_next->Process(line, viewer, style);

               level = levelNew;
               style.SetTextColour(GetQuoteColour(level));

               lineCur = lineNext;
            }
            //else: same level as the previous line, just continue

            if ( !*lineNext )
            {
               // nothing left
               break;
            }

            // we can use +1 here because there must be '\r' before the next
            // '\n' anyhow, i.e. the very next char can't be '\n'
            lineNext = wxStrchr(lineNext + 1, '\n');
         }

         if ( lineCur )
         {
            String line(lineCur);
            m_next->Process(line, viewer, style);
         }

         // remember the current quoting level to be able to restore it later
         levelBeforeURL = level;
      }
      else // no quoted text colourizing
      {
         m_next->Process(before, viewer, style);
      }

      if ( !strutil_isempty(url) )
      {
         // we use the URL itself for text here
         viewer->InsertURL(url, url);
      }
   }
   while ( !text.empty() );
}
