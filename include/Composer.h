///////////////////////////////////////////////////////////////////////////////
// Project:     M - cross platform e-mail GUI client
// File name:   Composer.h - abstract composer interface
// Purpose:     Composer class declaration
// Author:      Vadim Zeitlin
// Modified by:
// Created:     20.07.01
// CVS-ID:      $Id$
// Copyright:   (c) 2001 Vadim Zeitlin
// Licence:     M license
///////////////////////////////////////////////////////////////////////////////

#ifndef _COMPOSER_H_
#define _COMPOSER_H_

#include "MailFolder.h"    // for MailFolder::Params

class Profile;
class Message;
class wxComposeView;

// ----------------------------------------------------------------------------
// Composer provides GUI-independent interface for the compose frame
// ----------------------------------------------------------------------------

class Composer
{
public:
   /// recipient address types
   enum RecipientType
   {
      Recipient_To,
      Recipient_Cc,
      Recipient_Bcc,
      Recipient_Newsgroup,
      Recipient_None,
      Recipient_Max
   };

   /** @name Create a new composer window */
   //@{
   /** Constructor for posting news.
       @param profile parent profile
       @param hide if true, do not show frame
       @return pointer to the new compose view
    */
   static Composer *CreateNewArticle(const MailFolder::Params& params,
                                     Profile *profile = NULL,
                                     bool hide = false);

   /// short cut
   static Composer *CreateNewArticle(Profile *profile = NULL,
                                     bool hide = false)
      { return CreateNewArticle(MailFolder::Params(), profile, hide); }

   /** Constructor for sending mail.
       @param profile parent profile
       @param hide if true, do not show frame
       @return pointer to the new compose view
    */
   static Composer *CreateNewMessage(const MailFolder::Params& params,
                                     Profile *profile = NULL,
                                     bool hide = false);

   /// short cut
   static Composer *CreateNewMessage(Profile *profile = NULL,
                                     bool hide = false)
      { return CreateNewMessage(MailFolder::Params(), profile, hide); }

   /** Constructor for sending a reply to a message.

       @param profile parent profile
       @param original message that we replied to
       @param hide if true, do not show frame
       @return pointer to the new compose view
    */
   static Composer *CreateReplyMessage(const MailFolder::Params& params,
                                       Profile *profile,
                                       Message * original = NULL,
                                       bool hide = false);

   /** Constructor for forwarding a message.

       @param templ is the template to use
       @param profile parent profile
       @param hide if true, do not show frame
       @return pointer to the new compose view
    */
   static Composer *CreateFwdMessage(const MailFolder::Params& params,
                                     Profile *profile,
                                     Message * original = NULL,
                                     bool hide = false);
   //@}

   /** @name Accessing composer addresses */
   //@{
   /// get (all) addresses of this type as a single string
   virtual String GetRecipients(RecipientType type) const = 0;
   //@}

   /** @name Set the composer headers */
   //@{
   /// Set the default value for the "From" header (if we have it)
   virtual void SetDefaultFrom() = 0;

   /// sets Subject field
   virtual void SetSubject(const String &subj) = 0;

   /// adds recepients from addr (Recepient_Max means to reuse the last)
   virtual void AddRecipients(const String& addr,
                              RecipientType rcptType = Recipient_Max) = 0;

   /// adds a "To" recipient
   void AddTo(const String& addr) { AddRecipients(addr, Recipient_To); }

   /// adds a "Cc" recipient
   void AddCc(const String& addr) { AddRecipients(addr, Recipient_Cc); }

   /// adds a "Bcc" recipient
   void AddBcc(const String& addr) { AddRecipients(addr, Recipient_Bcc); }

   /** Sets the address fields, To:, CC: and BCC:.
       @param To primary address to send mail to
       @param CC carbon copy addresses
       @param BCC blind carbon copy addresses
   */
   void SetAddresses(const String& to,
                     const String& cc = "",
                     const String& bcc = "")
   {
      AddRecipients(to, Recipient_To);
      AddRecipients(cc, Recipient_Cc);
      AddRecipients(bcc, Recipient_Bcc);
   }

   /** Adds an extra header line.
       @param entry name of header entry
       @param value value of header entry
   */
   virtual void AddHeaderEntry(const String& entry, const String& value) = 0;
   //@}

   /** @name Add/insert stuff into composer */
   //@{
   /** Initializes the composer text: for example, if this is a reply, inserts
       the quoted contents of the message being replied to (except that, in
       fact, it may do whatever the user configured it to do using templates).
       The msg parameter may be NULL only for the new messages, messages
       created with CreateReply/FwdMessage require it to be !NULL.

       @param msg the message we're replying to or forwarding
    */
   virtual void InitText(Message *msg = NULL) = 0;

   /** insert a file into buffer
       @param filename file to insert (ask the user if NULL)
       @param mimetype mimetype to use (auto detect if NULL)
       @param num_mimetype numeric mimetype
    */
   virtual void InsertFile(const char *filename = NULL,
                           const char *mimetype = NULL) = 0;

   /** Insert MIME content data
       @param data pointer to data (we take ownership of it)
       @param len length of data
       @param mimetype mimetype to use
       @param filename optional filename to add to list of parameters
    */
   virtual void InsertData(void *data,
                           size_t length,
                           const char *mimetype = NULL,
                           const char *filename = NULL) = 0;

   /// inserts a text
   virtual void InsertText(const String& txt) = 0;

   /// move the cursor to the given position
   virtual void MoveCursorTo(int x, int y) = 0;

   /// reset the "dirty" flag
   virtual void ResetDirty() = 0;
   //@}

   /** @name Implementation only */
   //@{
   /// get the real private composer class
   virtual wxComposeView *GetComposeView() = 0;

   /// get the parent frame for the composer window
   virtual wxFrame *GetFrame() = 0;
   //@}
};

#endif // _COMPOSER_H_
