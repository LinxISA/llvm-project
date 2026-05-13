//== IdenticalExprChecker.cpp - Identical expression checker----------------==//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This defines IdenticalExprChecker, a check that warns about
/// unintended use of identical expressions.
///
/// It checks for use of identical expressions with comparison operators and
/// inside conditional expressions.
///
//===----------------------------------------------------------------------===//

#include "clang/StaticAnalyzer/Checkers/BuiltinCheckerRegistration.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/StaticAnalyzer/Core/BugReporter/BugType.h"
#include "clang/StaticAnalyzer/Core/Checker.h"
#include "clang/StaticAnalyzer/Core/CheckerManager.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/CheckerContext.h"

using namespace clang;
using namespace ento;

/// Returns true when the statements are Type I clones of each other.

namespace {
/// A branch in a switch may consist of several statements; while a branch in
/// an if/else if/else chain is one statement (which may be a CompoundStmt).
using SwitchBranch = llvm::SmallVector<const Stmt *, 2>;
using WBrancherMapT = std::map<const Stmt *, llvm::SmallVector<const Stmt *, 2>>;
using StmtToSvalMapT = std::map<const Stmt *, SVal>;
} // anonymous namespace

//===----------------------------------------------------------------------===//
// FindIdenticalExprVisitor - Identify nodes using identical expressions.
//===----------------------------------------------------------------------===//

namespace {
class FindIdenticalExprVisitor : public IdenticalExprVisitor {
  BugReporter &BR;
  const CheckerBase *Checker;
  AnalysisDeclContext *AC;
  std::vector<StringRef> Messages = {
      StringRef("identical expressions on both sides of bitwise operator"),
      StringRef("identical expressions on both sides of logical operator"),
      StringRef("true and false branches are identical"),
      StringRef("conditions of the inner and outer statements are identical"),
      StringRef("expression is identical to previous condition"),
      StringRef(
          "comparison of identical expressions always evaluates to equal"),
      StringRef("comparison of identical expressions always evaluates to true"),
      StringRef(
          "comparison of identical expressions always evaluates to false"),
      StringRef("identical expressions on both sides of ':' in conditional "
                "expression")};

public:
  explicit FindIdenticalExprVisitor(BugReporter &B,
                                    const CheckerBase *Checker,
                                    AnalysisDeclContext *A)
      : BR(B), Checker(Checker), AC(A) {
    this->Context = &AC->getASTContext();
  }
  const WBrancherMapT &GetWBrMap(void) { return this->WduplicatedBranchMap; }
  bool needIgnore(DiagType DiagID, SourceLocation Loc) { return false; }
  void reportIdentical(DiagType DiagID, const Stmt *S = nullptr,
                       const Expr *Op = nullptr,
                       ArrayRef<SourceRange> Sr = None);
};
} // end anonymous namespace

void FindIdenticalExprVisitor::reportIdentical(DiagType DiagID, const Stmt *S,
                                               const Expr *Op,
                                               ArrayRef<SourceRange> Sr) {
  StringRef Message = Messages[DiagID];
  switch (DiagID) {
  case BitwiseOperator:
  case LogicalOperator: {
    PathDiagnosticLocation ELoc = PathDiagnosticLocation::createOperatorLoc(
        static_cast<const BinaryOperator *>(Op), BR.getSourceManager());
    BR.EmitBasicReport(AC->getDecl(), Checker, "Use of identical expressions",
                       categories::LogicError, Message, ELoc, Sr);
    break;
  }
  case IdenticalBranches: {
    PathDiagnosticLocation ELoc =
        PathDiagnosticLocation::createBegin(S, BR.getSourceManager(), AC);
    BR.EmitBasicReport(AC->getDecl(), Checker, "Identical branches",
                       categories::LogicError, Message, ELoc);
    break;
  }
  case InnerIdenticalConditions: {
    PathDiagnosticLocation ELoc(Op, BR.getSourceManager(), AC);
    BR.EmitBasicReport(AC->getDecl(), Checker, "Identical conditions",
                       categories::LogicError, Message, ELoc);
    break;
  }
  case PrevIdenticalConditions: {
    PathDiagnosticLocation ELoc(Op, BR.getSourceManager(), AC);
    BR.EmitBasicReport(AC->getDecl(), Checker, "Identical conditions",
                       categories::LogicError, Message, ELoc, Sr[0]);
    break;
  }
  case AlwaysEqual:
  case AlwaysTrue:
  case AlwaysFalse: {
    PathDiagnosticLocation ELoc = PathDiagnosticLocation::createOperatorLoc(
        static_cast<const BinaryOperator *>(Op), BR.getSourceManager());
    BR.EmitBasicReport(AC->getDecl(), Checker,
                       "Compare of identical expressions",
                       categories::LogicError, Message, ELoc);
    break;
  }
  case IdenticalExpressions: {
    PathDiagnosticLocation ELoc =
        PathDiagnosticLocation::createConditionalColonLoc(
            static_cast<const ConditionalOperator *>(Op),
            BR.getSourceManager());
    BR.EmitBasicReport(AC->getDecl(), Checker,
                       "Identical expressions in conditional expression",
                       categories::LogicError, Message, ELoc, Sr);
    break;
  }
  default:
    break;
  }
}

//===----------------------------------------------------------------------===//
// FindIdenticalExprChecker
//===----------------------------------------------------------------------===//

namespace {
class FindIdenticalExprChecker : public Checker<check::ASTCodeBody,
                                                check::PostStmt<DeclStmt>> {
  WBrancherMapT WdupBrMap;
  StmtToSvalMapT StmtToSval;

public:
  void checkASTCodeBody(const Decl *D, AnalysisManager &Mgr,
                        BugReporter &BR) const {
    FindIdenticalExprVisitor Visitor(BR, this, Mgr.getAnalysisDeclContext(D));
    Visitor.TraverseDecl(const_cast<Decl *>(D));
    if (!isa<FunctionDecl>(D) && !isa<ObjCMethodDecl>(D))
      return;

    WBrancherMapT &TmpWdupBrMap = const_cast<FindIdenticalExprChecker *>(this)->GetWdupBrMap();
    TmpWdupBrMap.insert(Visitor.GetWBrMap().begin(), Visitor.GetWBrMap().end());
  }

  void checkPostStmt(const DeclStmt *D, CheckerContext &C) const;

  StmtToSvalMapT &GetStmtToSvalMap(void) {
    return const_cast<FindIdenticalExprChecker *>(this)->StmtToSval;
  }

  WBrancherMapT &GetWdupBrMap(void) {
    return const_cast<FindIdenticalExprChecker *>(this)->WdupBrMap;
  }

private:
  void TravelStmtInPostStmt(const Stmt *InStmt, CheckerContext &C);
  bool CopareStmtInPostStmt(const Stmt *Stmt1, const Stmt *Stmt2, CheckerContext &C);
  SVal GetSValFromStmt(const Stmt *InStmt, CheckerContext &C);
};
} // end anonymous namespace

SVal FindIdenticalExprChecker::GetSValFromStmt(const Stmt *InStmt,
                                               CheckerContext &C) {
  StmtToSvalMapT &InStmtToSval = GetStmtToSvalMap();
  SVal RetSVal = UnknownVal();

  if (InStmtToSval.count(InStmt) != 0) {
    RetSVal = InStmtToSval[InStmt];
    return RetSVal;
  }

  const Expr *ExprVal = dyn_cast<Expr>(InStmt);
  if (!ExprVal)
    return RetSVal;

  // need add all the type that can't get value based on Stmt.
  switch (InStmt->getStmtClass()) {
  case Expr::CStyleCastExprClass:
  case Expr::CXXFunctionalCastExprClass:
  case Expr::CXXStaticCastExprClass:
  case Expr::CXXDynamicCastExprClass:
  case Expr::CXXReinterpretCastExprClass:
  case Expr::CXXConstCastExprClass:
  case Expr::CXXAddrspaceCastExprClass:
  case Expr::ObjCBridgedCastExprClass:
  case Expr::BuiltinBitCastExprClass:
    return RetSVal;
  default:
    break;
  }

  if (ExprVal->IgnoreParens()->isEvaluatable(C.getASTContext())) {
    if (auto SValBuilderVal =
            C.getSValBuilder().getConstantVal(cast<Expr>(ExprVal))) {
      RetSVal = SValBuilderVal.getValue();
      if (!RetSVal.isUnknownOrUndef())
        InStmtToSval[InStmt] = RetSVal;
    }
  }
  return RetSVal;
}

void FindIdenticalExprChecker::TravelStmtInPostStmt(const Stmt *InStmt,
                                                    CheckerContext &C) {
  for (const Stmt *SubStmt : InStmt->children()) {
    if (!SubStmt)
      continue;

    TravelStmtInPostStmt(SubStmt, C);
  }

  switch (InStmt->getStmtClass()) {
  case Stmt::BinaryOperatorClass: {
    const BinaryOperator *Exp = cast<BinaryOperator>(InStmt);
    ProgramStateRef state = C.getState();

    // handle parent node
    Expr *LHS = Exp->getLHS()->IgnoreParens();
    Expr *RHS = Exp->getRHS()->IgnoreParens();
    if (!LHS || !RHS)
      return;

    StmtToSvalMapT &InStmtToSval = GetStmtToSvalMap();
    SVal simplifiedSVal;
    // value is from : map, Sval and Stmt.

    SVal LeftV = GetSValFromStmt(LHS, C);
    SVal RightV = GetSValFromStmt(RHS, C);
    simplifiedSVal = C.getSValBuilder().evalBinOp(
        state, Exp->getOpcode(), LeftV, RightV, Exp->getType());
    if (!simplifiedSVal.isUnknownOrUndef())
      InStmtToSval[InStmt] = simplifiedSVal;

    if (simplifiedSVal.isUnknownOrUndef()) {
      if (LeftV == RightV) {
        // g - g, g-=g case, result = 0
        switch (Exp->getOpcode()) {
        default:
          break;
        case BO_SubAssign:
        case BO_Sub:
          InStmtToSval[InStmt] = C.getSValBuilder().makeZeroVal(Exp->getType());
          break;
        case BO_EQ:
        case BO_LE:
        case BO_GE:
          InStmtToSval[InStmt] =
              C.getSValBuilder().makeTruthVal(true, Exp->getType());
          break;
        case BO_NE:
        case BO_LT:
        case BO_GT:
          InStmtToSval[InStmt] =
              C.getSValBuilder().makeTruthVal(false, Exp->getType());
          break;
        }
      } else {
        // g *= 0， g*0 case, result = 0
        if (Exp->getOpcode() == BO_MulAssign || Exp->getOpcode() == BO_Mul) {
          if (LeftV.isZeroConstant() || RightV.isZeroConstant())
            InStmtToSval[InStmt] =
                C.getSValBuilder().makeIntVal(0, Exp->getType());
        }
      }
    }
    break;
    }
    default:
      break;
  }

  return;
}


bool FindIdenticalExprChecker::CopareStmtInPostStmt(const Stmt *Stmt1,
                                                    const Stmt *Stmt2,
                                                    CheckerContext &C) {
  if (!Stmt1 || !Stmt2)
    return !Stmt1 && !Stmt2;

  // Handle map firstly. Compare stmts if these are in map. If one is in map and
  // another is not, getValue first. If either is in map, call isIdenticalStmt.
  StmtToSvalMapT &InStmtToSval = GetStmtToSvalMap();

  if ((InStmtToSval.count(Stmt1) != 0) || (InStmtToSval.count(Stmt2) != 0)) {
    SVal Stmt1V = GetSValFromStmt(Stmt1, C);
    SVal Stmt2V = GetSValFromStmt(Stmt2, C);
    if (Stmt1V != Stmt2V)
      return false;

    return true;
  }

  // If Stmt1 & Stmt2 are of different class then they are not
  // identical statements.
  if (Stmt1->getStmtClass() != Stmt2->getStmtClass())
    return false;

  // if Stmt1 and Stmt2 neither are in map.
  // If all children of two Stmt are identical, return true.
  Stmt::const_child_iterator I1 = Stmt1->child_begin();
  Stmt::const_child_iterator I2 = Stmt2->child_begin();
  while (I1 != Stmt1->child_end() && I2 != Stmt2->child_end()) {
    if (!*I1 || !*I2 || !CopareStmtInPostStmt(*I1, *I2, C))
      return false;

    // stop subStmt
    if ((InStmtToSval.count(*I1) != 0) || (InStmtToSval.count(*I2) != 0))
      return true;

    ++I1;
    ++I2;
  }
  // If there are different number of children in the statements, return false.
  if (I1 != Stmt1->child_end())
    return false;
  if (I2 != Stmt2->child_end())
    return false;

  if (isIdenticalStmt(C.getASTContext(), Stmt1, Stmt2, false, true))
    return true;

  return false;
}

void FindIdenticalExprChecker::checkPostStmt(const DeclStmt *D, CheckerContext &C) const {
  StmtToSvalMapT &TmpStmtToSval =
    const_cast<FindIdenticalExprChecker *>(this)->GetStmtToSvalMap();
  TmpStmtToSval.clear();

  WBrancherMapT &TmpWdupBrMap = const_cast<FindIdenticalExprChecker *>(this)->GetWdupBrMap();
  if (!TmpWdupBrMap.size())
    return;

  for (auto &it : TmpWdupBrMap) {
    const Stmt *Stmt1 = it.first;
    if (!Stmt1)
      continue;

    const_cast<FindIdenticalExprChecker *>(this)->TravelStmtInPostStmt(Stmt1, C);

    for (unsigned int i = 0; i < it.second.size(); i++) {
      const Stmt *Stmt2 = it.second.back();
      if (!Stmt2)
        continue;

      const_cast<FindIdenticalExprChecker *>(this)->TravelStmtInPostStmt(Stmt2, C);
      if(const_cast<FindIdenticalExprChecker *>(this)->CopareStmtInPostStmt(Stmt1, Stmt2, C)) {
        const Decl * DeclV = C.getCurrentAnalysisDeclContext()->getDecl();
        PathDiagnosticLocation ELoc =
          PathDiagnosticLocation::createBegin(Stmt1, C.getBugReporter().getSourceManager(),
            C.getAnalysisManager().getAnalysisDeclContext(DeclV));
        C.getBugReporter().EmitBasicReport(DeclV, this,
                                           "Identical branches",
                                           categories::LogicError,
                                           "true and false branches are identical", ELoc);
      }
    }
  }

  for (auto &it : TmpWdupBrMap)
    it.second.clear();

  TmpWdupBrMap.clear();
  TmpStmtToSval.clear();
}

void ento::registerIdenticalExprChecker(CheckerManager &Mgr) {
  Mgr.registerChecker<FindIdenticalExprChecker>();
}

bool ento::shouldRegisterIdenticalExprChecker(const CheckerManager &mgr) {
  return true;
}
