/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   Henri Sivonen <hsivonen@iki.fi>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsContentErrors.h"
#include "nsContentCreatorFunctions.h"
#include "nsIDOMDocumentType.h"
#include "nsIPresShell.h"
#include "nsPresContext.h"
#include "nsEvent.h"
#include "nsGUIEvent.h"
#include "nsEventDispatcher.h"
#include "nsContentUtils.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsIFormControl.h"
#include "nsNodeUtils.h"
#include "nsIStyleSheetLinkingElement.h"

// this really should be autogenerated...
jArray<PRUnichar,PRInt32> nsHtml5TreeBuilder::ISINDEX_PROMPT = jArray<PRUnichar,PRInt32>();

nsHtml5TreeBuilder::nsHtml5TreeBuilder(nsHtml5Parser* aParser)
  : MARKER(new nsHtml5StackNode(0, nsHtml5ElementName::NULL_ELEMENT_NAME, nsnull)),
    fragment(PR_FALSE),
    documentModeHandler(aParser),
    mParser(aParser),
    formPointer(nsnull),
    headPointer(nsnull)
{
}

nsHtml5TreeBuilder::~nsHtml5TreeBuilder()
{
  delete MARKER;
}

nsIContent*
nsHtml5TreeBuilder::createElement(PRInt32 aNamespace, nsIAtom* aName, nsHtml5HtmlAttributes* aAttributes)
{
  // XXX recheck http://mxr.mozilla.org/mozilla-central/source/content/base/src/nsDocument.cpp#6660
  nsIContent* newContent;
  nsCOMPtr<nsINodeInfo> nodeInfo = mParser->GetNodeInfoManager()->GetNodeInfo(aName, nsnull, aNamespace);
  NS_ASSERTION(nodeInfo, "Got null nodeinfo.");
  NS_NewElement(&newContent, nodeInfo->NamespaceID(), nodeInfo, PR_TRUE);
  NS_ASSERTION(newContent, "Element creation created null pointer.");
  PRInt32 len = aAttributes->getLength();
  for (PRInt32 i = 0; i < len; ++i) {
    newContent->SetAttr(aAttributes->getURI(i), aAttributes->getLocalName(i), aAttributes->getPrefix(i), *(aAttributes->getValue(i)), PR_FALSE);
    // XXX what to do with nsresult?
  }
  
  
  if (aNamespace != kNameSpaceID_MathML && (aName == nsHtml5Atoms::style || (aNamespace == kNameSpaceID_XHTML && aName == nsHtml5Atoms::link))) {
    nsCOMPtr<nsIStyleSheetLinkingElement> ssle(do_QueryInterface(newContent));
    if (ssle) {
      ssle->InitStyleLinkElement(PR_FALSE);
      ssle->SetEnableUpdates(PR_FALSE);
#if 0
      if (!aNodeInfo->Equals(nsGkAtoms::link, kNameSpaceID_XHTML)) {
        ssle->SetLineNumber(aLineNumber);
      }
#endif
    }
  } 
  
  return newContent;
}

nsIContent*
nsHtml5TreeBuilder::createElement(PRInt32 aNamespace, nsIAtom* aName, nsHtml5HtmlAttributes* aAttributes, nsIContent* aFormElement)
{
  nsIContent* content = createElement(aNamespace, aName, aAttributes);
  if (aFormElement) {
    nsCOMPtr<nsIFormControl> formControl(do_QueryInterface(content));
    NS_ASSERTION(formControl, "Form-associated element did not implement nsIFormControl.");
    nsCOMPtr<nsIDOMHTMLFormElement> formElement(do_QueryInterface(aFormElement));
    NS_ASSERTION(formElement, "The form element doesn't implement nsIDOMHTMLFormElement.");
    if (formControl) { // avoid crashing on <output>
      formControl->SetForm(formElement);
    }
  }
  return content; 
}

nsIContent*
nsHtml5TreeBuilder::createHtmlElementSetAsRoot(nsHtml5HtmlAttributes* aAttributes)
{
  nsIContent* content = createElement(kNameSpaceID_XHTML, nsHtml5Atoms::html, aAttributes);
  nsIDocument* doc = mParser->GetDocument();
  PRUint32 childCount = doc->GetChildCount();
  doc->AppendChildTo(content, PR_FALSE);
  // XXX nsresult
  nsNodeUtils::ContentInserted(doc, content, childCount);
  return content;
}

void
nsHtml5TreeBuilder::detachFromParent(nsIContent* aElement)
{
  Flush();
  nsIContent* parent = aElement->GetParent();
  if (parent) {
    PRUint32 pos = parent->IndexOf(aElement);
    NS_ASSERTION((pos >= 0), "Element not found as child of its parent");
    parent->RemoveChildAt(pos, PR_FALSE);
    // XXX nsresult
    nsNodeUtils::ContentRemoved(parent, aElement, pos);
  }
}

PRBool
nsHtml5TreeBuilder::hasChildren(nsIContent* aElement)
{
  Flush();
  return !!(aElement->GetChildCount());
}

nsIContent*
nsHtml5TreeBuilder::shallowClone(nsIContent* aElement)
{
  nsINode* clone;
  aElement->Clone(aElement->NodeInfo(), &clone);
  // XXX nsresult
  return static_cast<nsIContent*>(clone);
}

void
nsHtml5TreeBuilder::appendElement(nsIContent* aChild, nsIContent* aParent)
{
  PRUint32 childCount = aParent->GetChildCount();
  aParent->AppendChildTo(aChild, PR_FALSE);
  // XXX nsresult
  mParser->NotifyAppend(aParent, childCount);
}

void
nsHtml5TreeBuilder::appendChildrenToNewParent(nsIContent* aOldParent, nsIContent* aNewParent)
{
  Flush();
  while (aOldParent->GetChildCount()) {
    nsCOMPtr<nsIContent> child = aOldParent->GetChildAt(0);
    aOldParent->RemoveChildAt(0, PR_FALSE);
    nsNodeUtils::ContentRemoved(aOldParent, child, 0);
    PRUint32 childCount = aNewParent->GetChildCount();
    aNewParent->AppendChildTo(child, PR_FALSE);
    mParser->NotifyAppend(aNewParent, childCount);
  }
}

nsIContent*
nsHtml5TreeBuilder::parentElementFor(nsIContent* aElement)
{
  Flush();
  return aElement->GetParent();
}

void
nsHtml5TreeBuilder::insertBefore(nsIContent* aNewChild, nsIContent* aReferenceSibling, nsIContent* aParent)
{
  PRUint32 pos = aParent->IndexOf(aReferenceSibling);
  aParent->InsertChildAt(aNewChild, pos, PR_FALSE);
  // XXX nsresult
  nsNodeUtils::ContentInserted(aParent, aNewChild, pos);
}

void
nsHtml5TreeBuilder::insertCharactersBefore(PRUnichar* aBuffer, PRInt32 aStart, PRInt32 aLength, nsIContent* aReferenceSibling, nsIContent* aParent)
{
  // XXX this should probably coalesce
  nsCOMPtr<nsIContent> text;
  NS_NewTextNode(getter_AddRefs(text), mParser->GetNodeInfoManager());
  // XXX nsresult and comment null check?
  text->SetText(aBuffer + aStart, aLength, PR_FALSE);
  // XXX nsresult
  PRUint32 pos = aParent->IndexOf(aReferenceSibling);
  aParent->InsertChildAt(text, pos, PR_FALSE);
  // XXX nsresult
  nsNodeUtils::ContentInserted(aParent, text, pos);
}

void
nsHtml5TreeBuilder::appendCharacters(nsIContent* aParent, PRUnichar* aBuffer, PRInt32 aStart, PRInt32 aLength)
{
  nsCOMPtr<nsIContent> text;
  NS_NewTextNode(getter_AddRefs(text), mParser->GetNodeInfoManager());
  // XXX nsresult and comment null check?
  text->SetText(aBuffer + aStart, aLength, PR_FALSE);
  // XXX nsresult
  PRUint32 childCount = aParent->GetChildCount();
  aParent->AppendChildTo(text, PR_FALSE);  
  // XXX nsresult
  mParser->NotifyAppend(aParent, childCount);
}

void
nsHtml5TreeBuilder::appendComment(nsIContent* aParent, PRUnichar* aBuffer, PRInt32 aStart, PRInt32 aLength)
{
  nsCOMPtr<nsIContent> comment;
  NS_NewCommentNode(getter_AddRefs(comment), mParser->GetNodeInfoManager());
  // XXX nsresult and comment null check?
  comment->SetText(aBuffer + aStart, aLength, PR_FALSE);
  // XXX nsresult
  PRUint32 childCount = aParent->GetChildCount();
  aParent->AppendChildTo(comment, PR_FALSE);  
  // XXX nsresult
  mParser->NotifyAppend(aParent, childCount);
}

void
nsHtml5TreeBuilder::appendCommentToDocument(PRUnichar* aBuffer, PRInt32 aStart, PRInt32 aLength)
{
  nsIDocument* doc = mParser->GetDocument();
  nsCOMPtr<nsIContent> comment;
  NS_NewCommentNode(getter_AddRefs(comment), mParser->GetNodeInfoManager());
  // XXX nsresult and comment null check?
  comment->SetText(aBuffer + aStart, aLength, PR_FALSE);
  // XXX nsresult
  PRUint32 childCount = doc->GetChildCount();
  doc->AppendChildTo(comment, PR_FALSE);
  // XXX nsresult
  nsNodeUtils::ContentInserted(doc, comment, childCount);
}

void
nsHtml5TreeBuilder::addAttributesToElement(nsIContent* aElement, nsHtml5HtmlAttributes* aAttributes)
{
  PRInt32 len = aAttributes->getLength();
  for (PRInt32 i = 0; i < len; ++i) {
    nsIAtom* localName = aAttributes->getLocalName(i);
    PRInt32 nsuri = aAttributes->getURI(i);
    if (!aElement->HasAttr(nsuri, localName)) {
      aElement->SetAttr(nsuri, localName, aAttributes->getPrefix(i), *(aAttributes->getValue(i)), PR_TRUE);
      // XXX should not fire mutation event here
    }
  }  
}

void
nsHtml5TreeBuilder::startCoalescing()
{
  mCharBufferFillLength = 0;
  mCharBufferAllocLength = 1024;
  mCharBuffer = new PRUnichar[mCharBufferAllocLength];
}

void
nsHtml5TreeBuilder::endCoalescing()
{
  delete[] mCharBuffer;
}

void
nsHtml5TreeBuilder::start(PRBool fragment)
{
  mHasProcessedBase = PR_FALSE;
  mParser->WillBuildModelImpl();
  mParser->GetDocument()->BeginLoad(); // XXX fragment?
}

void
nsHtml5TreeBuilder::end()
{
}

void
nsHtml5TreeBuilder::appendDoctypeToDocument(nsIAtom* aName, nsString* aPublicId, nsString* aSystemId)
{
  // Adapted from nsXMLContentSink
  
  // Create a new doctype node
  nsCOMPtr<nsIDOMDocumentType> docType;
  nsAutoString voidString;
  voidString.SetIsVoid(PR_TRUE);
  NS_NewDOMDocumentType(getter_AddRefs(docType), mParser->GetNodeInfoManager(), nsnull,
                             aName, nsnull, nsnull, *aPublicId, *aSystemId,
                             voidString);
//  if (NS_FAILED(rv) || !docType) {
//    return rv;
//  }

  nsCOMPtr<nsIContent> content = do_QueryInterface(docType);
  NS_ASSERTION(content, "doctype isn't content?");

  mParser->GetDocument()->AppendChildTo(content, PR_TRUE);
  // XXX rv

  // nsXMLContentSink can flush here, but what's the point?
  // It can also interrupt here, but we can't.
}

void
nsHtml5TreeBuilder::elementPushed(PRInt32 aNamespace, nsIAtom* aName, nsIContent* aElement)
{
  NS_ASSERTION((aNamespace == kNameSpaceID_XHTML || aNamespace == kNameSpaceID_SVG || aNamespace == kNameSpaceID_MathML), "Element isn't HTML, SVG or MathML!");
  NS_ASSERTION(aName, "Element doesn't have local name!");
  NS_ASSERTION(aElement, "No element!");
  // Give autoloading links a chance to fire
  if (aNamespace == kNameSpaceID_XHTML) {
    if (aName == nsHtml5Atoms::body) {
      mParser->StartLayout(PR_FALSE);
    }
  } else {
    nsIDocShell* docShell = mParser->GetDocShell();
    if (docShell) {
      nsresult rv = aElement->MaybeTriggerAutoLink(docShell);
      if (rv == NS_XML_AUTOLINK_REPLACE ||
          rv == NS_XML_AUTOLINK_UNDEFINED) {
        // If we do not terminate the parse, we just keep generating link trigger
        // events. We want to parse only up to the first replace link, and stop.
        mParser->Terminate();
      }
    }
  }
  MaybeFlushAndMaybeSuspend();  
}

void
nsHtml5TreeBuilder::elementPopped(PRInt32 aNamespace, nsIAtom* aName, nsIContent* aElement)
{
  NS_ASSERTION((aNamespace == kNameSpaceID_XHTML || aNamespace == kNameSpaceID_SVG || aNamespace == kNameSpaceID_MathML), "Element isn't HTML, SVG or MathML!");
  NS_ASSERTION(aName, "Element doesn't have local name!");
  NS_ASSERTION(aElement, "No element!");

  MaybeFlushAndMaybeSuspend();  
  
  if (aNamespace == kNameSpaceID_MathML) {
    return;
  }  
  // we now have only SVG and HTML
  
  if (aName == nsHtml5Atoms::script) {
//    mConstrainSize = PR_TRUE; // XXX what is this?
    requestSuspension();
    mParser->SetScriptElement(aElement);
    return;
  }
  
  if (aName == nsHtml5Atoms::title) {
    Flush();
    aElement->DoneAddingChildren(PR_TRUE);
    return;
  }
  
  if (aName == nsHtml5Atoms::style || (aNamespace == kNameSpaceID_XHTML && aName == nsHtml5Atoms::link)) {
    mParser->UpdateStyleSheet(aElement);
    return;
  }


  if (aNamespace == kNameSpaceID_SVG) {
#ifdef MOZ_SVG
    if (aElement->HasAttr(kNameSpaceID_None, nsHtml5Atoms::onload)) {
      Flush();

      nsEvent event(PR_TRUE, NS_SVG_LOAD);
      event.eventStructType = NS_SVG_EVENT;
      event.flags |= NS_EVENT_FLAG_CANT_BUBBLE;

      // Do we care about forcing presshell creation if it hasn't happened yet?
      // That is, should this code flush or something?  Does it really matter?
      // For that matter, do we really want to try getting the prescontext?  Does
      // this event ever want one?
      nsRefPtr<nsPresContext> ctx;
      nsCOMPtr<nsIPresShell> shell = mParser->GetDocument()->GetPrimaryShell();
      if (shell) {
        ctx = shell->GetPresContext();
      }
      nsEventDispatcher::Dispatch(aElement, ctx, &event);
    }
#endif
    return;
  }  
  // we now have only HTML

  // Some HTML nodes need DoneAddingChildren() called to initialize
  // properly (eg form state restoration).
  if (aName == nsHtml5Atoms::select ||
        aName == nsHtml5Atoms::textarea ||
#ifdef MOZ_MEDIA
        aName == nsHtml5Atoms::video ||
        aName == nsHtml5Atoms::audio ||
#endif
        aName == nsHtml5Atoms::object ||
        aName == nsHtml5Atoms::applet) {
    Flush();
    aElement->DoneAddingChildren(PR_TRUE);
    return;
  }
  
  if (aName == nsHtml5Atoms::base && !mHasProcessedBase) {
    // The first base wins
    mParser->ProcessBASETag(aElement);
    // result?
    mHasProcessedBase = PR_TRUE;
    return;
  }
  
  if (aName == nsHtml5Atoms::meta) {
    /* Call the nsContentSink method. */
//    mParser->ProcessMETATag(aElement);
    // XXX nsresult
    return;
  }
  return;
}

void
nsHtml5TreeBuilder::accumulateCharacters(PRUnichar* aBuf, PRInt32 aStart, PRInt32 aLength)
{
  PRInt32 newFillLen = mCharBufferFillLength + aLength;
  if (newFillLen > mCharBufferAllocLength) {
    PRInt32 newAllocLength = newFillLen + (newFillLen >> 1);
    PRUnichar* newBuf = new PRUnichar[newAllocLength];
    memcpy(newBuf, mCharBuffer, sizeof(PRUnichar) * mCharBufferFillLength);
    delete[] mCharBuffer;
    mCharBuffer = newBuf;
    mCharBufferAllocLength = newAllocLength;
  }
  memcpy(mCharBuffer + mCharBufferFillLength, aBuf + aStart, sizeof(PRUnichar) * aLength);
  mCharBufferFillLength = newFillLen;
}

void
nsHtml5TreeBuilder::flushCharacters()
{
  if (mCharBufferFillLength > 0) {
    appendCharacters(currentNode(), mCharBuffer, 0,
            mCharBufferFillLength);
    mCharBufferFillLength = 0;
  }
}

void
nsHtml5TreeBuilder::MaybeFlushAndMaybeSuspend()
{
  if (mParser->DidProcessATokenImpl() == NS_ERROR_HTMLPARSER_INTERRUPTED) {
    mParser->Suspend();
    requestSuspension();
  }
  if (mParser->IsTimeToNotify()) {
    Flush();
  }
}

void
nsHtml5TreeBuilder::Flush()
{

}
