/**
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <clang/AST/APValue.h>
#include <clang/AST/ASTContext.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/Expr.h>
#include <clang/AST/Mangle.h>
#include <clang/Basic/SourceManager.h>
#include <llvm/ADT/SmallString.h>
#include <llvm/Support/raw_ostream.h>

namespace ASTLib {

// An identity, not a diagnostic rendering: APValue::printPretty rounds floating
// values through double and can omit array elements. Length-prefixed components
// retain the complete value without ambiguous delimiters or pointer addresses.
class StructuralValuePrinter {
  clang::ASTContext &Context;
  clang::MangleContext &Mangler;
  llvm::raw_ostream &Out;

  void atom(llvm::StringRef S) { Out << S.size() << ':' << S; }

  void integer(const llvm::APInt &I) {
    llvm::SmallString<32> Bits;
    I.toString(Bits, 16, false);
    Out << I.getBitWidth() << ':';
    atom(Bits);
  }

  void type(clang::QualType T) {
    std::string Name;
    llvm::raw_string_ostream OS(Name);
    Mangler.mangleCanonicalTypeName(T.getCanonicalType(), OS);
    atom(Name);
  }

  void location(clang::SourceLocation L) {
    const auto &SM = Context.getSourceManager();
    L = SM.getExpansionLoc(L);
    atom(SM.getFilename(L));
    Out << SM.getFileOffset(L) << ';';
  }

  void decl(const clang::ValueDecl *D) {
    if (!D) {
      atom("");
      return;
    }
    D = llvm::cast<clang::ValueDecl>(D->getCanonicalDecl());
    std::string Name;
    llvm::raw_string_ostream OS(Name);
    if (const auto *F = llvm::dyn_cast<clang::FieldDecl>(D)) {
      Out << 'F';
      type(Context.getRecordType(F->getParent()));
      Out << F->getFieldIndex() << ';';
    } else {
      Out << 'D';
      Mangler.mangleName(D, OS);
      atom(Name);
      if (!D->isExternallyVisible())
        location(D->getLocation());
    }
  }

 public:
  StructuralValuePrinter(clang::ASTContext &Context,
                         clang::MangleContext &Mangler,
                         llvm::raw_ostream &Out)
      : Context(Context), Mangler(Mangler), Out(Out) {}

  void print(clang::QualType T, const clang::APValue &V) {
    type(T);
    value(V);
  }

  void value(const clang::APValue &V) {
    using clang::APValue;
    Out << V.getKind() << '{';
    switch (V.getKind()) {
    case APValue::None:
    case APValue::Indeterminate:
      break;
    case APValue::Int:
      integer(V.getInt());
      break;
    case APValue::Float:
      integer(V.getFloat().bitcastToAPInt());
      break;
    case APValue::FixedPoint:
      integer(V.getFixedPoint().getValue());
      break;
    case APValue::ComplexInt:
      integer(V.getComplexIntReal());
      integer(V.getComplexIntImag());
      break;
    case APValue::ComplexFloat:
      integer(V.getComplexFloatReal().bitcastToAPInt());
      integer(V.getComplexFloatImag().bitcastToAPInt());
      break;
    case APValue::Vector:
      for (unsigned I = 0; I != V.getVectorLength(); ++I)
        value(V.getVectorElt(I));
      break;
    case APValue::Array:
      for (unsigned I = 0; I != V.getArraySize(); ++I)
        value(I < V.getArrayInitializedElts() ? V.getArrayInitializedElt(I)
                                              : V.getArrayFiller());
      break;
    case APValue::Struct:
      Out << V.getStructNumBases() << ';';
      for (unsigned I = 0; I != V.getStructNumBases(); ++I)
        value(V.getStructBase(I));
      for (unsigned I = 0; I != V.getStructNumFields(); ++I)
        value(V.getStructField(I));
      break;
    case APValue::Union:
      if (const auto *F = V.getUnionField()) {
        Out << F->getFieldIndex() + 1 << ';';
        value(V.getUnionValue());
      } else {
        Out << '0';
      }
      break;
    case APValue::LValue: {
      auto Base = V.getLValueBase();
      clang::QualType ElemT;
      if (!Base) {
        Out << '0';
      } else {
        ElemT = Base.getType();
        if (const auto *D = Base.dyn_cast<const clang::ValueDecl *>()) {
          decl(D);
          Out << Base.getCallIndex() << ';' << Base.getVersion() << ';';
        } else if (auto TI = Base.dyn_cast<clang::TypeInfoLValue>()) {
          Out << 'T';
          type(clang::QualType(TI.getType(), 0));
        } else if (auto DA = Base.dyn_cast<clang::DynamicAllocLValue>()) {
          Out << 'A' << DA.getIndex() << ';';
          type(Base.getDynamicAllocType());
        } else {
          Out << 'E';
          location(Base.get<const clang::Expr *>()->getExprLoc());
          Out << Base.getCallIndex() << ';' << Base.getVersion() << ';';
        }
      }
      Out << ';' << V.getLValueOffset().getQuantity() << ';'
          << V.isNullPointer() << V.isLValueOnePastTheEnd()
          << V.hasLValuePath();
      if (V.hasLValuePath()) {
        for (auto Entry : V.getLValuePath()) {
          if (ElemT->isRecordType()) {
            auto BM = Entry.getAsBaseOrMember();
            Out << 'R' << BM.getInt();
            if (const auto *D =
                    llvm::dyn_cast<clang::CXXRecordDecl>(BM.getPointer())) {
              ElemT = Context.getRecordType(D);
              type(ElemT);
            } else {
              const auto *F = llvm::cast<clang::FieldDecl>(BM.getPointer());
              Out << 'F' << F->getFieldIndex() << ';';
              ElemT = F->getType();
            }
          } else {
            Out << 'I' << Entry.getAsArrayIndex() << ';';
            if (ElemT->isAnyComplexType())
              ElemT = ElemT->castAs<clang::ComplexType>()->getElementType();
            else
              ElemT = Context.getAsArrayType(ElemT)->getElementType();
          }
        }
      }
      break;
    }
    case APValue::MemberPointer:
      decl(V.getMemberPointerDecl());
      Out << V.isMemberPointerToDerivedMember();
      for (auto *D : V.getMemberPointerPath())
        type(Context.getRecordType(D));
      break;
    case APValue::AddrLabelDiff:
      location(V.getAddrLabelDiffLHS()->getLabel()->getLocation());
      location(V.getAddrLabelDiffRHS()->getLabel()->getLocation());
      break;
    }
    Out << '}';
  }
};

} // namespace ASTLib
