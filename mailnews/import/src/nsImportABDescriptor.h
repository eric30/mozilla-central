/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mark Banner <bugzilla@standard8.demon.co.uk>
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

#ifndef nsImportABDescriptor_h___
#define nsImportABDescriptor_h___

#include "nscore.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsIImportABDescriptor.h"
#include "nsIFile.h"

////////////////////////////////////////////////////////////////////////

class nsImportABDescriptor : public nsIImportABDescriptor
{
public:
  NS_DECL_ISUPPORTS

  NS_IMETHOD GetIdentifier(PRUint32 *pIdentifier) {
    *pIdentifier = mId;
    return NS_OK;
  }
  NS_IMETHOD SetIdentifier(PRUint32 ident) {
    mId = ident;
    return NS_OK;
  }

  NS_IMETHOD GetRef(PRUint32 *pRef) {
    *pRef = mRef;
    return NS_OK;
  }
  NS_IMETHOD SetRef(PRUint32 ref) {
    mRef = ref;
    return NS_OK;
  }

  /* attribute unsigned long size; */
  NS_IMETHOD GetSize(PRUint32 *pSize) {
    *pSize = mSize;
    return NS_OK;
  }
  NS_IMETHOD SetSize(PRUint32 theSize) {
    mSize = theSize;
    return NS_OK;
  }

  /* attribute AString displayName; */
  NS_IMETHOD GetPreferredName(nsAString &aName) {
    aName = mDisplayName;
    return NS_OK;
  }
  NS_IMETHOD SetPreferredName(const nsAString &aName) {
    mDisplayName = aName;
    return NS_OK;
  }

  /* readonly attribute nsIFile fileSpec; */
  NS_IMETHOD GetAbFile(nsIFile **aFile) {
    if (!mFile)
      return NS_ERROR_NULL_POINTER;

    return mFile->Clone(aFile);
  }

  NS_IMETHOD SetAbFile(nsIFile *aFile) {
    if (!aFile) {
      mFile = nsnull;
      return NS_OK;
    }

    return aFile->Clone(getter_AddRefs(mFile));
  }

  /* attribute boolean import; */
  NS_IMETHOD GetImport(PRBool *pImport) {
    *pImport = mImport;
    return NS_OK;
  }
  NS_IMETHOD SetImport(PRBool doImport) {
    mImport = doImport;
    return NS_OK;
  }

  nsImportABDescriptor();
  virtual ~nsImportABDescriptor() {}

  static NS_METHOD Create( nsISupports *aOuter, REFNSIID aIID, void **aResult);

private:
  PRUint32 mId; // used by creator of the structure
  PRUint32 mRef; // depth in the heirarchy
  nsString mDisplayName; // name of this mailbox
  nsCOMPtr<nsIFile> mFile; // source file (if applicable)
  PRUint32 mSize; // size
  PRBool mImport; // import it or not?
};


#endif
