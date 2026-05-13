//===--- ASTConsumers.cpp - ASTConsumer implementations -------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// AST Consumer Implementations.
//
//===----------------------------------------------------------------------===//

#include "clang/Frontend/ASTConsumers.h"
#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/PrettyPrinter.h"
#include "clang/AST/RecordLayout.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/DiagnosticIDs.h"
#include "clang/Basic/SourceManager.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/Timer.h"
#include "llvm/Support/raw_ostream.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/AST/ASTDiagnostic.h"
using namespace clang;

//===----------------------------------------------------------------------===//
/// ASTPrinter - Pretty-printer and dumper of ASTs

namespace {
  class ASTPrinter : public ASTConsumer,
                     public RecursiveASTVisitor<ASTPrinter> {
    typedef RecursiveASTVisitor<ASTPrinter> base;

  public:
    enum Kind { DumpFull, Dump, Print, None };
    ASTPrinter(std::unique_ptr<raw_ostream> Out, Kind K,
               ASTDumpOutputFormat Format, StringRef FilterString,
               bool DumpLookups = false, bool DumpDeclTypes = false)
        : Out(Out ? *Out : llvm::outs()), OwnedOut(std::move(Out)),
          OutputKind(K), OutputFormat(Format), FilterString(FilterString),
          DumpLookups(DumpLookups), DumpDeclTypes(DumpDeclTypes) {}

    void HandleTranslationUnit(ASTContext &Context) override {
      TranslationUnitDecl *D = Context.getTranslationUnitDecl();

      if (FilterString.empty())
        return print(D);

      TraverseDecl(D);
    }

    bool shouldWalkTypesOfTypeLocs() const { return false; }

    bool TraverseDecl(Decl *D) {
      if (D && filterMatches(D)) {
        bool ShowColors = Out.has_colors();
        if (ShowColors)
          Out.changeColor(raw_ostream::BLUE);

        if (OutputFormat == ADOF_Default)
          Out << (OutputKind != Print ? "Dumping " : "Printing ") << getName(D)
              << ":\n";

        if (ShowColors)
          Out.resetColor();
        print(D);
        Out << "\n";
        // Don't traverse child nodes to avoid output duplication.
        return true;
      }
      return base::TraverseDecl(D);
    }

  private:
    std::string getName(Decl *D) {
      if (isa<NamedDecl>(D))
        return cast<NamedDecl>(D)->getQualifiedNameAsString();
      return "";
    }
    bool filterMatches(Decl *D) {
      return getName(D).find(FilterString) != std::string::npos;
    }
    void print(Decl *D) {
      if (DumpLookups) {
        if (DeclContext *DC = dyn_cast<DeclContext>(D)) {
          if (DC == DC->getPrimaryContext())
            DC->dumpLookups(Out, OutputKind != None, OutputKind == DumpFull);
          else
            Out << "Lookup map is in primary DeclContext "
                << DC->getPrimaryContext() << "\n";
        } else
          Out << "Not a DeclContext\n";
      } else if (OutputKind == Print) {
        PrintingPolicy Policy(D->getASTContext().getLangOpts());
        D->print(Out, Policy, /*Indentation=*/0, /*PrintInstantiation=*/true);
      } else if (OutputKind != None) {
        D->dump(Out, OutputKind == DumpFull, OutputFormat);
      }

      if (DumpDeclTypes) {
        Decl *InnerD = D;
        if (auto *TD = dyn_cast<TemplateDecl>(D))
          InnerD = TD->getTemplatedDecl();

        // FIXME: Support OutputFormat in type dumping.
        // FIXME: Support combining -ast-dump-decl-types with -ast-dump-lookups.
        if (auto *VD = dyn_cast<ValueDecl>(InnerD))
          VD->getType().dump(Out, VD->getASTContext());
        if (auto *TD = dyn_cast<TypeDecl>(InnerD))
          TD->getTypeForDecl()->dump(Out, TD->getASTContext());
      }
    }

    raw_ostream &Out;
    std::unique_ptr<raw_ostream> OwnedOut;

    /// How to output individual declarations.
    Kind OutputKind;

    /// What format should the output take?
    ASTDumpOutputFormat OutputFormat;

    /// Which declarations or DeclContexts to display.
    std::string FilterString;

    /// Whether the primary output is lookup results or declarations. Individual
    /// results will be output with a format determined by OutputKind. This is
    /// incompatible with OutputKind == Print.
    bool DumpLookups;

    /// Whether to dump the type for each declaration dumped.
    bool DumpDeclTypes;
  };

  class ASTDeclNodeLister : public ASTConsumer,
                     public RecursiveASTVisitor<ASTDeclNodeLister> {
  public:
    ASTDeclNodeLister(raw_ostream *Out = nullptr)
        : Out(Out ? *Out : llvm::outs()) {}

    void HandleTranslationUnit(ASTContext &Context) override {
      TraverseDecl(Context.getTranslationUnitDecl());
    }

    bool shouldWalkTypesOfTypeLocs() const { return false; }

    bool VisitNamedDecl(NamedDecl *D) {
      D->printQualifiedName(Out);
      Out << '\n';
      return true;
    }

  private:
    raw_ostream &Out;
  };
} // end anonymous namespace

std::unique_ptr<ASTConsumer>
clang::CreateASTPrinter(std::unique_ptr<raw_ostream> Out,
                        StringRef FilterString) {
  return std::make_unique<ASTPrinter>(std::move(Out), ASTPrinter::Print,
                                       ADOF_Default, FilterString);
}

std::unique_ptr<ASTConsumer>
clang::CreateASTDumper(std::unique_ptr<raw_ostream> Out, StringRef FilterString,
                       bool DumpDecls, bool Deserialize, bool DumpLookups,
                       bool DumpDeclTypes, ASTDumpOutputFormat Format) {
  assert((DumpDecls || Deserialize || DumpLookups) && "nothing to dump");
  return std::make_unique<ASTPrinter>(
      std::move(Out),
      Deserialize ? ASTPrinter::DumpFull
                  : DumpDecls ? ASTPrinter::Dump : ASTPrinter::None,
      Format, FilterString, DumpLookups, DumpDeclTypes);
}

std::unique_ptr<ASTConsumer> clang::CreateASTDeclNodeLister() {
  return std::make_unique<ASTDeclNodeLister>(nullptr);
}

//===----------------------------------------------------------------------===//
/// ASTViewer - AST Visualization

namespace {
  class ASTViewer : public ASTConsumer {
    ASTContext *Context;
  public:
    void Initialize(ASTContext &Context) override {
      this->Context = &Context;
    }

    bool HandleTopLevelDecl(DeclGroupRef D) override {
      for (DeclGroupRef::iterator I = D.begin(), E = D.end(); I != E; ++I)
        HandleTopLevelSingleDecl(*I);
      return true;
    }

    void HandleTopLevelSingleDecl(Decl *D);
  };
}

void ASTViewer::HandleTopLevelSingleDecl(Decl *D) {
  if (isa<FunctionDecl>(D) || isa<ObjCMethodDecl>(D)) {
    D->print(llvm::errs());

    if (Stmt *Body = D->getBody()) {
      llvm::errs() << '\n';
      Body->viewAST();
      llvm::errs() << '\n';
    }
  }
}

std::unique_ptr<ASTConsumer> clang::CreateASTViewer() {
  return std::make_unique<ASTViewer>();
}

namespace {
struct PathInfo {
  SourceLocation Loc;
  SourceManager &SM;
  SmallVector<SourceRange, 4> Ranges;
  unsigned DiagID;
  PathInfo(SourceLocation L, SourceManager &S, unsigned ID,
           ArrayRef<SourceRange> Ranges = None)
      : Loc(std::move(L)), SM(S), DiagID(ID) {
    for (const auto &SR : Ranges) {
      this->Ranges.push_back(SR);
    }
  }
};

struct comp {
  bool operator()(struct PathInfo left, struct PathInfo right) const {
    unsigned LineLeft = left.SM.getSpellingLineNumber(left.Loc);
    unsigned LineRight = right.SM.getSpellingLineNumber(right.Loc);
    unsigned ColLeft = left.SM.getSpellingColumnNumber(left.Loc);
    unsigned ColRight = right.SM.getSpellingColumnNumber(right.Loc);
    if (LineLeft == LineRight && ColLeft == ColRight &&
        left.DiagID == right.DiagID)
      return false;

    if (LineLeft != LineRight)
      return LineLeft < LineRight;

    if (ColLeft != ColRight)
      return ColLeft < ColRight;

    return left.DiagID < right.DiagID;
  }
};

using SwitchBranch = llvm::SmallVector<const Stmt *, 2>;
class ASTDuplicatedChecker : public ASTConsumer, public IdenticalExprVisitor {
public:
  Preprocessor &PP;
  DiagnosticsEngine *Diags;
  SourceManager &SourceMgr;

private:
  std::vector<unsigned> DiagIDs = {
      diag::warn_duplicated_exp1,     diag::warn_duplicated_exp2,
      diag::warn_duplicated_branches, diag::warn_duplicated_cond_2,
      diag::warn_duplicated_cond,     diag::warn_duplicated_exp3,
      diag::warn_duplicated_exp4,     diag::warn_duplicated_exp5,
      diag::warn_duplicated_branches2};

public:
  ASTDuplicatedChecker(CompilerInstance &CI)
      : PP(CI.getPreprocessor()), SourceMgr(CI.getSourceManager()) {}
  ASTDuplicatedChecker(Preprocessor &PP, SourceManager &SourceMgr)
      : PP(PP), SourceMgr(SourceMgr) {}

  bool needIgnore(DiagType DiagID, SourceLocation Loc);
  void reportIdentical(DiagType DiagID, const Stmt *S = nullptr,
                       const Expr *Op = nullptr,
                       ArrayRef<SourceRange> Sr = None);
  void Initialize(ASTContext &Context) override {
    this->Context = &Context;
    Diags = &PP.getDiagnostics();
  }

  void HandleTranslationUnit(ASTContext &Context) override {
    TraverseDecl(Context.getTranslationUnitDecl());
    for (auto it : warns) {
      DiagnosticBuilder DB = Diags->Report(it.Loc, it.DiagID);
      for (const auto &SR : it.Ranges) {
        DB << SR;
      }
    }
    warns.clear();
  }

private:
  std::set<PathInfo, comp> warns;
};
} // namespace

bool ASTDuplicatedChecker::needIgnore(DiagType DiagID, SourceLocation Loc) {
  if (Diags->isIgnored(DiagIDs[DiagID], Loc))
    return true;
  return false;
}

void ASTDuplicatedChecker::reportIdentical(DiagType DiagID, const Stmt *S,
                                           const Expr *Op,
                                           ArrayRef<SourceRange> Sr) {
  switch (DiagID) {
  case BitwiseOperator:
  case LogicalOperator:
  case AlwaysEqual:
  case AlwaysTrue:
  case AlwaysFalse: {
    PathInfo ELoc(static_cast<const BinaryOperator *>(Op)->getExprLoc(),
                  SourceMgr, DiagIDs[DiagID], Sr);
    warns.insert(ELoc);
    break;
  }
  case IdenticalBranches: {
    PathInfo ELoc(S->getBeginLoc(), SourceMgr, DiagIDs[DiagID]);
    warns.insert(ELoc);
    break;
  }
  case InnerIdenticalConditions:
  case PrevIdenticalConditions: {
    PathInfo ELoc(Op->getBeginLoc(), SourceMgr, DiagIDs[DiagID], Sr);
    warns.insert(ELoc);
    break;
  }
  case IdenticalExpressions: {
    PathInfo ELoc(static_cast<const ConditionalOperator *>(Op)->getColonLoc(),
                  SourceMgr, DiagIDs[DiagID], Sr);
    warns.insert(ELoc);
    break;
  }
  default:
    break;
  }
}

std::unique_ptr<ASTConsumer>
clang::CreateASTDuplicatedChecker(Preprocessor &PP, SourceManager &SourceMgr) {
  return std::make_unique<ASTDuplicatedChecker>(PP, SourceMgr);
}