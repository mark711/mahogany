///////////////////////////////////////////////////////////////////////////////
// Project:     M - cross platform e-mail GUI client
// File name:   src/modules/spam/DspamFilter.cpp
// Purpose:     SpamFilter implementation using dspam
// Author:      Vadim Zeitlin
// Modified by:
// Created:     2004-08-04
// CVS-ID:      $Id$
// Copyright:   (c) 2004 Vadim Zeitlin <vadim@wxwindows.org>
// Licence:     M licence
///////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#include "Mpch.h"

#ifndef USE_PCH
   #include "Mcommon.h"

   #include "MApplication.h"

   #include <wx/msgdlg.h>        // for wxMessageBox
#endif //USE_PCH

#include "MFolder.h"
#include "MailFolder.h"
#include "Message.h"
#include "HeaderInfo.h"

#include "SpamFilter.h"
#include "gui/SpamOptionsPage.h"

extern "C"
{
   #include <libdspam.h>

   extern void dspam_set_home(const char *home);
}

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

// currently we always use the same user name...
static const char *M_DSPAM_USER = "mahogany";

// ----------------------------------------------------------------------------
// DspamCtx simple wrapper around DSPAM_CTX
// ----------------------------------------------------------------------------

class DspamCtx
{
public:
   // take ownership of the specified context
   DspamCtx(DSPAM_CTX *ctx) : m_ctx(ctx) { }

   // destroy the context
   ~DspamCtx()
   {
      _ds_destroy_message(m_ctx->message);
      if ( dspam_destroy(m_ctx) != 0 )
      {
         wxLogDebug(_T("dspam_destroy() failed"));
      }
   }

   operator DSPAM_CTX *() const { return m_ctx; }
   DSPAM_CTX *operator->() const { return m_ctx; }

private:
   DSPAM_CTX *m_ctx;

   DECLARE_NO_COPY_CLASS(DspamCtx);
};

// ----------------------------------------------------------------------------
// DspamFilter class
// ----------------------------------------------------------------------------

class DspamFilter : public SpamFilter
{
public:
   DspamFilter();

   // these public methods are used by GUI class (DspamOptionsPage below)

   // show dspam statistics to the user
   void ShowStats(wxWindow *parent);

   // train dsapm
   void Train(wxWindow *parent);

protected:
   virtual void DoReclassify(const Message& msg, bool isSpam);
   virtual void DoTrain(const Message& msg, bool isSpam);
   virtual bool DoCheckIfSpam(const Message& msg,
                              const String& param,
                              String *result);
   virtual const wxChar *GetOptionPageIconName() const { return _T("dspam"); }
   virtual SpamOptionsPage *CreateOptionPage(wxListOrNoteBook *notebook,
                                             Profile *profile,
                                             String *params) const;

private:
   // helper: used as DoProcess() argument to initialize the context or to get
   // result from it
   class ContextHandler
   {
   public:
      virtual void OnInit(DSPAM_CTX *ctx) { }
      virtual void OnDone(DSPAM_CTX *ctx) { }
      virtual ~ContextHandler() { }
   };

   // ContextHandler used by DoReclassify() and DoTrain()
   class ClassifyContextHandler : public ContextHandler
   {
   public:
      enum Operation
      {
         Reclassify,
         Train
      };

      ClassifyContextHandler(Operation operation, bool isSpam)
      {
         m_operation = operation;
         m_isSpam = isSpam;
      }

      virtual void OnInit(DSPAM_CTX *ctx)
      {
         ctx->classification = m_isSpam ? DSR_ISSPAM : DSR_ISINNOCENT;
         ctx->source = m_operation == Reclassify ? DSS_ERROR : DSS_CORPUS;
      }

   private:
      Operation m_operation;
      bool m_isSpam;
   };


   // common part of all DoXXX() functions: processes the message and returns
   // false if we failed, use ContextHandler to customize processing
   bool DoProcess(const Message& msg, ContextHandler& handler);


   DECLARE_SPAM_FILTER("dspam", 100);
};

IMPLEMENT_SPAM_FILTER(DspamFilter,
                      gettext_noop("DSPAM Statistical Spam Filter"),
                      _T("(c) 2004 Vadim Zeitlin <vadim@wxwindows.org>"));

// ----------------------------------------------------------------------------
// DspamOptionsPage
// ----------------------------------------------------------------------------

class DspamOptionsPage : public SpamOptionsPage
{
public:
   DspamOptionsPage(DspamFilter *filter,
                    String *params,
                    wxNotebook *parent,
                    const wxChar *title,
                    Profile *profile,
                    FieldInfoArray aFields,
                    ConfigValuesArray aDefaults,
                    size_t nFields,
                    int image = -1)
      : SpamOptionsPage(parent, profile, params)
   {
      m_filter = filter;

      Create(parent, title, profile, aFields, aDefaults, nFields, 0, -1, image);
   }

protected:
   void OnButton(wxCommandEvent& event);

private:
   DspamFilter *m_filter;

   DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(DspamOptionsPage, wxOptionsPageDynamic)
   EVT_BUTTON(wxID_ANY, DspamOptionsPage::OnButton)
END_EVENT_TABLE()



// ============================================================================
// DspamFilter implementation
// ============================================================================

// ----------------------------------------------------------------------------
// DspamFilter public API implementation
// ----------------------------------------------------------------------------

DspamFilter::DspamFilter()
{
   dspam_set_home(mApplication->GetLocalDir());

   dspam_init_driver();
}

bool DspamFilter::DoProcess(const Message& msg, ContextHandler& handler)
{
   DspamCtx ctx(dspam_init
                (
                  M_DSPAM_USER,
                  NULL,
                  DSM_PROCESS,
                  DSF_CHAINED | DSF_NOISE
                ));
   if ( !ctx )
   {
      ERRORMESSAGE((_("DSPAM: library initialization failed.")));

      return false;
   }

   String str;
   if ( !msg.WriteToString(str) )
   {
      ERRORMESSAGE((_("Failed to get the message text.")));

      return false;
   }

   handler.OnInit(ctx);

   if ( dspam_process(ctx, str) != 0 )
   {
      ERRORMESSAGE((_("DSPAM: processing message failed.")));

      return false;
   }

   handler.OnDone(ctx);

   return true;
}

void DspamFilter::DoReclassify(const Message& msg, bool isSpam)
{
   ClassifyContextHandler handler(ClassifyContextHandler::Reclassify, isSpam);

   DoProcess(msg, handler);
}

void DspamFilter::DoTrain(const Message& msg, bool isSpam)
{
   ClassifyContextHandler handler(ClassifyContextHandler::Train, isSpam);

   DoProcess(msg, handler);
}

bool
DspamFilter::DoCheckIfSpam(const Message& msg,
                           const String& param,
                           String *result)
{
   class CheckContextHandler : public ContextHandler
   {
   public:
      CheckContextHandler(bool *rc, float *probability)
      {
         m_rc = rc;
         m_probability = probability;
      }

      virtual void OnDone(DSPAM_CTX *ctx)
      {
         *m_probability = ctx->probability;
         *m_rc = ctx->result == DSR_ISSPAM;
      }

   private:
      bool *m_rc;
      float *m_probability;
   };

   ASSERT_MSG( param.empty(), _T("DspamFilter has no parameters") );

   bool rc;
   float probability;
   CheckContextHandler handler(&rc, &probability);
   if ( !DoProcess(msg, handler) || !rc )
      return false;

   if ( result )
   {
      result->Printf(_T("probability = %0.3f"), probability);
   }

   return true;
}

// ----------------------------------------------------------------------------
// DspamFilter other (private) methods
// ----------------------------------------------------------------------------

void DspamFilter::ShowStats(wxWindow *parent)
{
   DspamCtx ctx(dspam_init(M_DSPAM_USER, NULL, DSM_CLASSIFY, 0));

   if ( !ctx )
   {
      ERRORMESSAGE((_("DSPAM: library initialization failed.")));

      return;
   }

   const _ds_spam_totals& t = ctx->totals;
   wxMessageBox
   (
      wxString::Format
      (
        _T("Total Spam:\t%6ld\n")
        _T("Total Innocent:\t%6ld\n")
        _T("Spam Misclassified:\t%6ld\n")
        _T("Innocent Misclassified:\t%6ld\n")
        _T("Spam Corpusfed:\t%6ld\n")
        _T("Innocent Corpusfed:\t%6ld\n")
        _T("Training Left:\t%6ld\n"),
        wxMax(0, t.spam_learned +
                 t.spam_classified -
                 t.spam_misclassified -
                 t.spam_corpusfed),
        wxMax(0, t.innocent_learned +
                 t.innocent_classified -
                 t.innocent_misclassified -
                 t.innocent_corpusfed),
        t.spam_misclassified,
        t.innocent_misclassified,
        t.spam_corpusfed,
        t.innocent_corpusfed,
        wxMax(0, 2500 - (t.innocent_learned + t.innocent_classified))
      ),
      _("DSPAM Statistics"),
      wxOK | wxICON_INFORMATION,
      parent
   );
}

void DspamFilter::Train(wxWindow *parent)
{
   MFolder_obj folder(MDialog_FolderChoose(parent, NULL, MDlg_Folder_Open));
   if ( !folder )
      return;

   const String name = folder->GetFullName();

   bool isSpam;
   switch ( MDialog_YesNoCancel
            (
               wxString::Format
               (
                  _("Does the folder \"%s\" contain spam?"),
                  name.c_str()
               ),
               parent,
               _("Choose DSPAM training mode")
            ) )
   {
      case MDlg_Yes:
         isSpam = true;
         break;

      case MDlg_No:
         isSpam = false;
         break;

      default:
         FAIL_MSG( _T("unexpected MDialog_YesNoCancel() result") );
         // fall through

      case MDlg_Cancel:
         return;
   }

   MailFolder_obj mf(MailFolder::OpenFolder(folder, MailFolder::ReadOnly));
   if ( !mf )
   {
      wxLogError(_("Failed to open folder \"%s\" with training messages."),
                 name.c_str());
      return;
   }

   // FIXME: this is horribly inefficient, we have to use UIDs while we
   //        simply want to get all messages!

   HeaderInfoList_obj hil(mf->GetHeaders());
   if ( !hil )
   {
      wxLogError(_("Failed to get headers of the training messages."));
      return;
   }

   const size_t count = hil->Count();
   for ( size_t n = 0; n < count; n++ )
   {
      HeaderInfo *hi = hil->GetItemByIndex(n);
      if ( hi )
      {
         Message_obj msg(mf->GetMessage(hi->GetUId()));
         if ( msg )
         {
            DoTrain(*msg, isSpam);

            continue;
         }
      }

      wxLogWarning(_("Failed to retrieve message #%lu."));
   }
}

// ----------------------------------------------------------------------------
// DspamFilter user-configurable options
// ----------------------------------------------------------------------------

// TODO: add "Clean"

enum
{
   DspamPageField_Help,
   DspamPageField_Train,
   DspamPageField_ShowStats,
   DspamPageField_Max
};

SpamOptionsPage *
DspamFilter::CreateOptionPage(wxListOrNoteBook *notebook,
                              Profile *profile,
                              String *params) const
{
   static const wxOptionsPage::FieldInfo s_fields[] =
   {
      {
         gettext_noop
         (
            "This page allows you to configure DSPAM,\n"
            "a powerful statistical hybrid anti-spam filter.\n"
            "\n"
            "Before it can be used with maximal efficiency, DSPAM\n"
            "has to be trained, that is fed some known spam and\n"
            "non-spam (ham) messages. The simplest way to do this\n"
            "is to use the buttons below to train using all messages\n"
            "from an existing folder. You can also check the statistics\n"
            "to see how much training is still required.\n"
            "\n"
            "Alternatively, you can start using DSPAM immediately, but\n"
            "then it would probably misclassify some of the messages and\n"
            "you would need to correct it using the \"Message|Spam\" menu\n"
            "entries."
         ),
         wxOptionsPage::Field_Message,
         -1
      },
      {
         gettext_noop("&Train..."),
         wxOptionsPage::Field_SubDlg,
         -1
      },
      {
         gettext_noop("Show &statistics"),
         wxOptionsPage::Field_SubDlg,
         -1
      },
   };

   wxCOMPILE_TIME_ASSERT2( WXSIZEOF(s_fields) == DspamPageField_Max,
                              FieldsNotInSync, DspamFields );


   static const ConfigValueDefault s_values[] =
   {
      ConfigValueNone(),
      ConfigValueNone(),
      ConfigValueNone(),
   };

   wxCOMPILE_TIME_ASSERT2( WXSIZEOF(s_values) == DspamPageField_Max,
                              ValuesNotInSync, DspamValues );


   return new DspamOptionsPage
              (
                  const_cast<DspamFilter *>(this),
                  params,
                  notebook,
                  gettext_noop("DSPAM"),
                  profile,
                  s_fields,
                  s_values,
                  DspamPageField_Max,
                  notebook->GetPageCount()      // image id
              );
}

// ============================================================================
// DspamOptionsPage implementation
// ============================================================================

void DspamOptionsPage::OnButton(wxCommandEvent& event)
{
   wxObject * const obj = event.GetEventObject();
   if ( obj == GetControl(DspamPageField_ShowStats) )
   {
      m_filter->ShowStats(this);
   }
   else if ( obj == GetControl(DspamPageField_Train) )
   {
      m_filter->Train(this);
   }
   else
   {
      event.Skip();
   }
}

