/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once
#include <clang/AST/DeclVisitor.h>
#include <clang/Basic/SourceManager.h>
#include <llvm/ADT/DenseMap.h>

#include <algorithm>
#include <limits>

#include "atdlib/ATDWriter.h"
namespace ASTLib {

using namespace clang;
template <class ATDWriter>
class NamePrinter : public ConstDeclVisitor<NamePrinter<ATDWriter>> {
  typedef typename ATDWriter::ObjectScope ObjectScope;
  typedef typename ATDWriter::ArrayScope ArrayScope;
  typedef typename ATDWriter::TupleScope TupleScope;
  typedef typename ATDWriter::VariantScope VariantScope;

  const SourceManager &SM;
  ATDWriter &OF;

  struct CachedTemplateArgs {
    const TemplateArgument *Data;
    size_t Size;
    std::string Printed;
  };
  llvm::DenseMap<const NamedDecl *, CachedTemplateArgs> TemplateArgsCache;

  PrintingPolicy getPrintingPolicy();
  void printTemplateArgList(llvm::raw_ostream &OS,
                            const NamedDecl &D,
                            const ArrayRef<TemplateArgument> Args);

 public:
  NamePrinter(const SourceManager &SM, ATDWriter &OF) : SM(SM), OF(OF) {}

  // implementation is inspired by NamedDecl::printQualifiedName
  // but with better handling for anonymous structs,unions and namespaces
  void printDeclName(const NamedDecl &D);

  void VisitNamedDecl(const NamedDecl *D);
  void VisitNamespaceDecl(const NamespaceDecl *ND);
  void VisitTagDecl(const TagDecl *TD);
  void VisitFunctionDecl(const FunctionDecl *FD);
};

// 64 bits fnv-1a
const uint64_t FNV64_hash_start = 14695981039346656037ULL;
const uint64_t FNV64_prime = 1099511628211ULL;
uint64_t fnv64Hash(const char *s, int n) {
  uint64_t hash = FNV64_hash_start;
  for (int i = 0; i < n; ++i) {
    hash ^= s[i];
    hash *= FNV64_prime;
  }
  return hash;
}

uint64_t fnv64Hash(llvm::raw_svector_ostream &OS) {
  llvm::StringRef s = OS.str();
  return fnv64Hash(s.data(), s.size());
}

const int templateLengthThreshold = 40;

// Keep only the literal prefix while hashing the exact bytes Clang prints.
class TemplateArgumentSpellingStream : public llvm::raw_ostream {
  SmallString<64> Prefix;
  uint64_t Hash = FNV64_hash_start;
  uint64_t BytesWritten = 0;

  void write_impl(const char *Data, size_t Size) override {
    if (Prefix.size() < templateLengthThreshold) {
      const size_t Keep = std::min(
          Size, static_cast<size_t>(templateLengthThreshold) - Prefix.size());
      Prefix.append(Data, Data + Keep);
    }
    for (size_t I = 0; I < Size; ++I) {
      // Preserve the original char signedness, including non-ASCII bytes.
      Hash ^= Data[I];
      Hash *= FNV64_prime;
    }
    BytesWritten += Size;
  }

  uint64_t current_pos() const override {
    return BytesWritten;
  }

 public:
  TemplateArgumentSpellingStream() : llvm::raw_ostream(true) {}

  uint64_t size() const {
    return BytesWritten;
  }

  void print(llvm::raw_ostream &OS) const {
    if (BytesWritten > templateLengthThreshold) {
      OS << "<";
      OS.write_hex(Hash);
      OS << ">";
    } else {
      OS << llvm::StringRef(Prefix.data(), Prefix.size());
    }
  }
};

template <class ATDWriter>
void NamePrinter<ATDWriter>::printTemplateArgList(
    llvm::raw_ostream &OS,
    const NamedDecl &D,
    const ArrayRef<TemplateArgument> Args) {
  auto Cached = TemplateArgsCache.find(&D);
  if (Cached != TemplateArgsCache.end() && Cached->second.Data == Args.data() &&
      Cached->second.Size == Args.size()) {
    OS << Cached->second.Printed;
    return;
  }

  TemplateArgumentSpellingStream Spelling;
  clang::printTemplateArgumentList(Spelling, Args, getPrintingPolicy());
  SmallString<64> Buf;
  llvm::raw_svector_ostream Result(Buf);
  if (Spelling.size() >
      static_cast<uint64_t>(std::numeric_limits<int>::max())) {
    // The original hash helper narrows its length to int. Preserve that
    // behavior for unusually large spellings, too.
    SmallString<64> LegacyBuf;
    llvm::raw_svector_ostream Legacy(LegacyBuf);
    clang::printTemplateArgumentList(Legacy, Args, getPrintingPolicy());
    Result << "<";
    Result.write_hex(fnv64Hash(Legacy));
    Result << ">";
  } else {
    Spelling.print(Result);
  }
  std::string Printed = Result.str().str();
  OS << Printed;
  TemplateArgsCache[&D] = {Args.data(), Args.size(), std::move(Printed)};
}

template <class ATDWriter>
void NamePrinter<ATDWriter>::printDeclName(const NamedDecl &D) {
  const DeclContext *Ctx = D.getDeclContext();
  SmallVector<const NamedDecl *, 8> Contexts;
  Contexts.push_back(&D);

  // Don't dump full qualifier for variables declared
  // inside a function/method/block
  // For structs defined inside functions, dump fully qualified name
  if (Ctx->isFunctionOrMethod() && !isa<TagDecl>(&D)) {
    Ctx = nullptr;
  }

  while (Ctx && isa<NamedDecl>(Ctx)) {
    bool shouldPrintCtx = true;
    // skip inline namespaces when printing qualified name
    if (const NamespaceDecl *ND = dyn_cast<NamespaceDecl>(Ctx)) {
      if (ND->isInline()) {
        shouldPrintCtx = false;
      }
    }
    if (shouldPrintCtx) {
      Contexts.push_back(cast<NamedDecl>(Ctx));
    }
    Ctx = Ctx->getParent();
  }

  ArrayScope aScope(OF, Contexts.size());
  // dump list in reverse
  for (const Decl *Ctx : Contexts) {
    ConstDeclVisitor<NamePrinter<ATDWriter>>::Visit(Ctx);
  }
}

template <class ATDWriter>
void NamePrinter<ATDWriter>::VisitNamedDecl(const NamedDecl *D) {
  OF.emitString(D->getNameAsString());
}

template <class ATDWriter>
void NamePrinter<ATDWriter>::VisitNamespaceDecl(const NamespaceDecl *ND) {
  if (ND->isAnonymousNamespace()) {
    PresumedLoc PLoc = SM.getPresumedLoc(ND->getLocation());
    std::string file = "invalid_loc";
    if (PLoc.isValid()) {
      file = PLoc.getFilename();
    }
    OF.emitString("anonymous_namespace_" + file);
  } else {
    // for non-anonymous namespaces, fallback to normal behavior
    VisitNamedDecl(ND);
  }
}

template <class ATDWriter>
void NamePrinter<ATDWriter>::VisitTagDecl(const TagDecl *D) {
  // heavily inspired by clang's TypePrinter::printTag() function
  SmallString<64> Buf;
  llvm::raw_svector_ostream StrOS(Buf);
  if (const IdentifierInfo *II = D->getIdentifier()) {
    StrOS << II->getName();
  } else if (TypedefNameDecl *Typedef = D->getTypedefNameForAnonDecl()) {
    StrOS << Typedef->getIdentifier()->getName();
  } else {
    if (isa<CXXRecordDecl>(D) && cast<CXXRecordDecl>(D)->isLambda()) {
      StrOS << "lambda";
    } else {
      StrOS << "anonymous_" << D->getKindName();
    }
    PresumedLoc PLoc = SM.getPresumedLoc(D->getLocation());
    if (PLoc.isValid()) {
      StrOS << "_" << PLoc.getFilename() << ':' << PLoc.getLine() << ':'
            << PLoc.getColumn();
    }
  }
  if (const ClassTemplateSpecializationDecl *Spec =
          dyn_cast<ClassTemplateSpecializationDecl>(D)) {
    ArrayRef<TemplateArgument> Args;
    const TemplateArgumentList &TemplateArgs = Spec->getTemplateArgs();
    Args = TemplateArgs.asArray();
    printTemplateArgList(StrOS, *D, Args);
  }
  OF.emitString(StrOS.str().str());
}

template <class ATDWriter>
void NamePrinter<ATDWriter>::VisitFunctionDecl(const FunctionDecl *FD) {
  std::string template_str = "";
  // add instantiated template arguments for readability
  if (const TemplateArgumentList *TemplateArgs =
          FD->getTemplateSpecializationArgs()) {
    SmallString<64> Buf;
    llvm::raw_svector_ostream StrOS(Buf);
    printTemplateArgList(StrOS, *FD, TemplateArgs->asArray());
    template_str = StrOS.str().str();
  }
  OF.emitString(FD->getNameAsString() + template_str);
}

template <class ATDWriter>
PrintingPolicy NamePrinter<ATDWriter>::getPrintingPolicy() {
  // configure what to print
  LangOptions LO;
  PrintingPolicy Policy(LO);
  // print tag types
  Policy.IncludeTagDefinition = false;
  // print fully qualified names - this is needed by PrintTemplateArgumentList
  Policy.SuppressScope = false;
  // don't print inline and anonymous namespaces
  Policy.SuppressUnwrittenScope = true;
  // print locations of anonymous tags
  Policy.AnonymousTagLocations = true;
  // don't add 'struct' inside a name regardless of language
  Policy.SuppressTagKeyword = true;

  return Policy;
}

} // end of namespace ASTLib
