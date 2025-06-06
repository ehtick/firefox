/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AccIterator.h"

#include "AccGroupInfo.h"
#include "ARIAMap.h"
#include "DocAccessible-inl.h"
#include "LocalAccessible-inl.h"
#include "nsAccUtils.h"
#include "XULTreeAccessible.h"

#include "mozilla/a11y/DocAccessibleParent.h"
#include "mozilla/dom/DocumentOrShadowRoot.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/HTMLLabelElement.h"

using namespace mozilla;
using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// AccIterator
////////////////////////////////////////////////////////////////////////////////

AccIterator::AccIterator(const LocalAccessible* aAccessible,
                         filters::FilterFuncPtr aFilterFunc)
    : mFilterFunc(aFilterFunc) {
  mState = new IteratorState(aAccessible);
}

AccIterator::~AccIterator() {
  while (mState) {
    IteratorState* tmp = mState;
    mState = tmp->mParentState;
    delete tmp;
  }
}

LocalAccessible* AccIterator::Next() {
  while (mState) {
    LocalAccessible* child = mState->mParent->LocalChildAt(mState->mIndex++);
    if (!child) {
      IteratorState* tmp = mState;
      mState = mState->mParentState;
      delete tmp;

      continue;
    }

    uint32_t result = mFilterFunc(child);
    if (result & filters::eMatch) return child;

    if (!(result & filters::eSkipSubtree)) {
      IteratorState* childState = new IteratorState(child, mState);
      mState = childState;
    }
  }

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// nsAccIterator::IteratorState

AccIterator::IteratorState::IteratorState(const LocalAccessible* aParent,
                                          IteratorState* mParentState)
    : mParent(aParent), mIndex(0), mParentState(mParentState) {}

////////////////////////////////////////////////////////////////////////////////
// RelatedAccIterator
////////////////////////////////////////////////////////////////////////////////

RelatedAccIterator::RelatedAccIterator(DocAccessible* aDocument,
                                       nsIContent* aDependentContent,
                                       nsAtom* aRelAttr)
    : mDocument(aDocument),
      mDependentContent(aDependentContent),
      mRelAttr(aRelAttr),
      mProviders(nullptr),
      mIndex(0),
      mIsWalkingDependentElements(false) {
  nsAutoString id;
  if (aDependentContent->IsElement() &&
      aDependentContent->AsElement()->GetAttr(nsGkAtoms::id, id)) {
    mProviders = mDocument->GetRelProviders(aDependentContent->AsElement(), id);
  }
}

LocalAccessible* RelatedAccIterator::Next() {
  if (!mProviders || mIndex == mProviders->Length()) {
    if (mIsWalkingDependentElements) {
      // We've walked both dependent ids and dependent elements, so there are
      // no more targets.
      return nullptr;
    }
    // We've returned all dependent ids, but there might be dependent elements
    // too. Walk those next.
    mIsWalkingDependentElements = true;
    mIndex = 0;
    if (auto providers =
            mDocument->mDependentElementsMap.Lookup(mDependentContent)) {
      mProviders = &providers.Data();
    } else {
      mProviders = nullptr;
      return nullptr;
    }
  }

  while (mIndex < mProviders->Length()) {
    const auto& provider = (*mProviders)[mIndex++];

    // Return related accessible for the given attribute.
    if (mRelAttr && provider->mRelAttr != mRelAttr) {
      continue;
    }
    // If we're walking elements (not ids), the explicitly set attr-element
    // `mDependentContent` must be a descendant of any of the refering element
    // `mProvider->mContent`'s shadow-including ancestors.
    if (mIsWalkingDependentElements &&
        !nsCoreUtils::IsDescendantOfAnyShadowIncludingAncestor(
            mDependentContent, provider->mContent)) {
      continue;
    }
    LocalAccessible* related = mDocument->GetAccessible(provider->mContent);
    if (related) {
      return related;
    }

    // If the document content is pointed by relation then return the
    // document itself.
    if (provider->mContent == mDocument->GetContent()) {
      return mDocument;
    }
  }

  // We exhausted mProviders without returning anything.
  if (!mIsWalkingDependentElements) {
    // Call this function again to start walking the dependent elements.
    return Next();
  }
  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// HTMLLabelIterator
////////////////////////////////////////////////////////////////////////////////

HTMLLabelIterator::HTMLLabelIterator(DocAccessible* aDocument,
                                     const LocalAccessible* aAccessible,
                                     LabelFilter aFilter)
    : mRelIter(aDocument, aAccessible->GetContent(), nsGkAtoms::_for),
      mAcc(aAccessible),
      mLabelFilter(aFilter) {}

bool HTMLLabelIterator::IsLabel(LocalAccessible* aLabel) {
  dom::HTMLLabelElement* labelEl =
      dom::HTMLLabelElement::FromNode(aLabel->GetContent());
  return labelEl && labelEl->GetControl() == mAcc->GetContent();
}

LocalAccessible* HTMLLabelIterator::Next() {
  // Get either <label for="[id]"> element which explicitly points to given
  // element, or <label> ancestor which implicitly point to it.
  LocalAccessible* label = nullptr;
  while ((label = mRelIter.Next())) {
    if (IsLabel(label)) {
      return label;
    }
  }

  // Ignore ancestor label on not widget accessible.
  if (mLabelFilter == eSkipAncestorLabel || !mAcc->IsWidget()) return nullptr;

  // Go up tree to get a name of ancestor label if there is one (an ancestor
  // <label> implicitly points to us). Don't go up farther than form or
  // document.
  LocalAccessible* walkUp = mAcc->LocalParent();
  while (walkUp && !walkUp->IsDoc()) {
    nsIContent* walkUpEl = walkUp->GetContent();
    if (IsLabel(walkUp) && !walkUpEl->AsElement()->HasAttr(nsGkAtoms::_for)) {
      mLabelFilter = eSkipAncestorLabel;  // prevent infinite loop
      return walkUp;
    }

    if (walkUpEl->IsHTMLElement(nsGkAtoms::form)) break;

    walkUp = walkUp->LocalParent();
  }

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// HTMLOutputIterator
////////////////////////////////////////////////////////////////////////////////

HTMLOutputIterator::HTMLOutputIterator(DocAccessible* aDocument,
                                       nsIContent* aElement)
    : mRelIter(aDocument, aElement, nsGkAtoms::_for) {}

LocalAccessible* HTMLOutputIterator::Next() {
  LocalAccessible* output = nullptr;
  while ((output = mRelIter.Next())) {
    if (output->GetContent()->IsHTMLElement(nsGkAtoms::output)) return output;
  }

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// XULLabelIterator
////////////////////////////////////////////////////////////////////////////////

XULLabelIterator::XULLabelIterator(DocAccessible* aDocument,
                                   nsIContent* aElement)
    : mRelIter(aDocument, aElement, nsGkAtoms::control) {}

LocalAccessible* XULLabelIterator::Next() {
  LocalAccessible* label = nullptr;
  while ((label = mRelIter.Next())) {
    if (label->GetContent()->IsXULElement(nsGkAtoms::label)) return label;
  }

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// XULDescriptionIterator
////////////////////////////////////////////////////////////////////////////////

XULDescriptionIterator::XULDescriptionIterator(DocAccessible* aDocument,
                                               nsIContent* aElement)
    : mRelIter(aDocument, aElement, nsGkAtoms::control) {}

LocalAccessible* XULDescriptionIterator::Next() {
  LocalAccessible* descr = nullptr;
  while ((descr = mRelIter.Next())) {
    if (descr->GetContent()->IsXULElement(nsGkAtoms::description)) return descr;
  }

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// AssociatedElementsIterator
////////////////////////////////////////////////////////////////////////////////

AssociatedElementsIterator::AssociatedElementsIterator(DocAccessible* aDoc,
                                                       nsIContent* aContent,
                                                       nsAtom* aIDRefsAttr)
    : mContent(aContent), mDoc(aDoc), mCurrIdx(0), mElemIdx(0) {
  if (mContent->IsElement()) {
    mContent->AsElement()->GetAttr(aIDRefsAttr, mIDs);
    if (mIDs.IsEmpty() &&
        (aria::AttrCharacteristicsFor(aIDRefsAttr) & ATTR_REFLECT_ELEMENTS)) {
      nsAccUtils::GetARIAElementsAttr(mContent->AsElement(), aIDRefsAttr,
                                      mElements);
    }
  }
}

const nsDependentSubstring AssociatedElementsIterator::NextID() {
  for (; mCurrIdx < mIDs.Length(); mCurrIdx++) {
    if (!NS_IsAsciiWhitespace(mIDs[mCurrIdx])) break;
  }

  if (mCurrIdx >= mIDs.Length()) return nsDependentSubstring();

  nsAString::index_type idStartIdx = mCurrIdx;
  while (++mCurrIdx < mIDs.Length()) {
    if (NS_IsAsciiWhitespace(mIDs[mCurrIdx])) break;
  }

  return Substring(mIDs, idStartIdx, mCurrIdx++ - idStartIdx);
}

dom::Element* AssociatedElementsIterator::NextElem() {
  while (true) {
    const nsDependentSubstring id = NextID();
    if (id.IsEmpty()) break;

    dom::Element* refContent = GetElem(id);
    if (refContent) return refContent;
  }

  while (dom::Element* element = mElements.SafeElementAt(mElemIdx++)) {
    if (nsCoreUtils::IsDescendantOfAnyShadowIncludingAncestor(element,
                                                              mContent)) {
      return element;
    }
  }

  return nullptr;
}

dom::Element* AssociatedElementsIterator::GetElem(nsIContent* aContent,
                                                  const nsAString& aID) {
  // Get elements in DOM tree by ID attribute if this is an explicit content.
  // In case of bound element check its anonymous subtree.
  if (!aContent->IsInNativeAnonymousSubtree()) {
    dom::DocumentOrShadowRoot* docOrShadowRoot =
        aContent->GetUncomposedDocOrConnectedShadowRoot();
    if (docOrShadowRoot) {
      dom::Element* refElm = docOrShadowRoot->GetElementById(aID);
      if (refElm) {
        return refElm;
      }
    }
  }
  return nullptr;
}

dom::Element* AssociatedElementsIterator::GetElem(
    const nsDependentSubstring& aID) {
  return GetElem(mContent, aID);
}

LocalAccessible* AssociatedElementsIterator::Next() {
  dom::Element* nextEl = nullptr;
  while ((nextEl = NextElem())) {
    LocalAccessible* acc = mDoc->GetAccessible(nextEl);
    if (acc) {
      return acc;
    }
  }
  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// SingleAccIterator
////////////////////////////////////////////////////////////////////////////////

Accessible* SingleAccIterator::Next() {
  Accessible* nextAcc = mAcc;
  mAcc = nullptr;
  if (!nextAcc) {
    return nullptr;
  }

  MOZ_ASSERT(!nextAcc->IsLocal() || !nextAcc->AsLocal()->IsDefunct(),
             "Iterator references defunct accessible?");
  return nextAcc;
}

////////////////////////////////////////////////////////////////////////////////
// ItemIterator
////////////////////////////////////////////////////////////////////////////////

Accessible* ItemIterator::Next() {
  if (mContainer) {
    mAnchor = AccGroupInfo::FirstItemOf(mContainer);
    mContainer = nullptr;
    return mAnchor;
  }

  if (mAnchor) {
    mAnchor = AccGroupInfo::NextItemTo(mAnchor);
  }

  return mAnchor;
}

////////////////////////////////////////////////////////////////////////////////
// XULTreeItemIterator
////////////////////////////////////////////////////////////////////////////////

XULTreeItemIterator::XULTreeItemIterator(const XULTreeAccessible* aXULTree,
                                         nsITreeView* aTreeView,
                                         int32_t aRowIdx)
    : mXULTree(aXULTree),
      mTreeView(aTreeView),
      mRowCount(-1),
      mContainerLevel(-1),
      mCurrRowIdx(aRowIdx + 1) {
  mTreeView->GetRowCount(&mRowCount);
  if (aRowIdx != -1) mTreeView->GetLevel(aRowIdx, &mContainerLevel);
}

LocalAccessible* XULTreeItemIterator::Next() {
  while (mCurrRowIdx < mRowCount) {
    int32_t level = 0;
    mTreeView->GetLevel(mCurrRowIdx, &level);

    if (level == mContainerLevel + 1) {
      return mXULTree->GetTreeItemAccessible(mCurrRowIdx++);
    }

    if (level <= mContainerLevel) {  // got level up
      mCurrRowIdx = mRowCount;
      break;
    }

    mCurrRowIdx++;
  }

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// RemoteAccIterator
////////////////////////////////////////////////////////////////////////////////

Accessible* RemoteAccIterator::Next() {
  while (mIndex < mIds.Length()) {
    uint64_t id = mIds[mIndex++];
    Accessible* acc = mDoc->GetAccessible(id);
    if (acc) {
      return acc;
    }
  }
  return nullptr;
}
