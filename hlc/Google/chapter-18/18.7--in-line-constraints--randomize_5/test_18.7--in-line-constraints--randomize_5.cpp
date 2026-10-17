/*
 Copyright 2020 Apotell

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
*/

// Validates the HLDB model built for
// tests/Google/chapter-18/18.7--in-line-constraints--randomize_5.sv. Lines
// 1-14 are the license and the suite's :name:/:description:/:tags: comment;
// the rest is:
//
//   16  class a;
//   17      rand int x;
//   18      int y = -1;
//   19  endclass
//   20
//   21  function int F(a obj, int y);
//   22      F = obj.randomize() with (x) { x > 0; x < y; };
//   23  endfunction
//
// The point of the fixture is the restricted form of an inline constraint,
// 'with (identifier_list) { ... }' (IEEE 1800-2023 18.7; A.1.10
// inline_constraint_declaration). When the parenthesized list is present,
// only the names it lists are looked up in the class of the randomized
// object; every other unqualified name is resolved in the scope that
// contains the call. Unlike 18.7--in-line-constraints--randomize_3, class a
// here also declares a property 'y', so the list decides which 'y' is meant:
//   - 'x' is listed, so both 'x' bind to class a's rand property 'x'
//   - 'y' is not listed, so it binds where the call is written: F's formal
//     'y' -- not class a's 'y', although a has one. (Without the list, the
//     unrestricted form would make it a's 'y' = -1 and 'x > 0; x < y'
//     unsatisfiable.)
//
// What is checked, and why:
//   class a (8.3)
//     - exactly one class named "a"; user-defined; not virtual (8.21); no
//       'extends' (8.13), no parameters (8.25), no end label; declares no
//       methods (randomize() is built in, 18.6.1)
//     - exactly 2 properties, in order x, y; both signed int (6.11), public
//       (8.18), not const (8.19)
//     - 'rand int x;' (18.4): getIsRandomized() true, getRandType() vpiRand,
//       no initializer
//     - 'int y = -1;': not rand/randc (18.4); initialized with -1 (8.8)
//   function int F(a obj, int y); (13.4)
//     - exactly one compilation-unit function named "F" (3.12.1); return
//       type a signed int (13.4.1); static lifetime (13.4.2: declared
//       outside a class with no lifetime keyword)
//     - formals, in order: 'obj' (input, a handle to class a) and 'y'
//       (input, signed int); no direction means input (13.3)
//   F = obj.randomize() with (x) { x > 0; x < y; }; (13.4.1, 18.7)
//     - the function's only statement, so not wrapped in a Begin: a blocking
//       Assignment whose LHS is a RefObj named "F" (13.4.1)
//     - its RHS is a hierarchical RefObj whose prefix 'obj' binds to the
//       Variable holding F's formal 'obj' (37.15), then a MethodFuncCall
//       "randomize" with no arguments -- '(x)' after 'with' is the
//       identifier list, not an argument of randomize() -- and a 'with'
//       inline constraint (18.7)
//     - the 'with' block holds exactly two constraint expressions, in source
//       order: 'x > 0' (vpiGtOp, right operand the literal 0) and 'x < y'
//       (vpiLtOp)
//     - both 'x' bind to class a's 'x' (18.7: listed in the identifier list)
//     - 'y' binds to the Variable holding F's formal 'y' and not to class a's
//       'y' (18.7: not listed, so resolved in the scope of the call; 37.15)
//   Diagnostics
//     - no COMP_UNDEFINED_VARIABLE for any variable or formal the file
//       references
//     - no fatal, syntax or error diagnostic points into this file
//     - the whole compilation is legal: zero fatal, syntax and error
//       diagnostics
//
// Reduction and elaboration: nothing in this file reduces or elaborates.
// There is no module, no parameter and no class specialization; every
// binding asserted is name resolution. No check is gated on
// getElaborated().
//
// KNOWN FAILURE (names in a 'with' block are not looked up in the
// randomized object's class), not a defect in this test:
// BothXInWithBindToClassAProperty fails on HLC 577. HLC parses
// 'with (x) { ... }' and builds both items with the right shape, but both
// 'x' have no actual, although 'x' is listed in the identifier list and so
// must resolve to class a's 'x' (18.7). HLC resolves names in the 'with'
// block only in the scope of the call; F declares no 'x', so nothing is
// found. 'y' binds correctly to F's formal -- but only because HLC never
// looks in class a at all, not because it honors the identifier list. It
// is expected to fail until HLC is fixed and is intentionally not skipped
// or relaxed.
//
// KNOWN FAILURE (errors reported in this file), not a defect in this test:
// NoErrorIsReportedInThisFile and NoFatalSyntaxOrErrorDiagnostics assert
// that this legal file compiles cleanly and are expected to fail. HLC
// reports HLDB_ILLEGAL_PROPERTY_VALUE (2038) twice at line 22, column 34 --
// the '{' that opens the 'with' block, once per constraint expression --
// although the restricted inline constraint is legal (18.7). With this
// .hlc: 0 syntax errors and 2 errors in total. Neither check is skipped or
// relaxed.
//
// What is NOT checked, and why:
//   - HLDB has no accessor for the identifier list of a restricted inline
//     constraint, so '(x)' itself is not read back. Its effect -- which
//     scope each name in the block resolves in -- is what 18.7 fixes, and
//     that is asserted by BothXInWithBindToClassAProperty and
//     YInWithBindsToFormalOfF.
//   - The value randomize() picks for obj.x and F's return value only exist
//     while simulation runs; F is never called. Permanently out of scope.
//   - How HLC wraps the 'with' block (an unnamed Constraint holding the
//     items, or the items stored otherwise) is a model convention:
//     MethodFuncCall::getWith() is typed Any. Both an unnamed Constraint
//     and a lone expression are accepted; the items, operators and bindings
//     are asserted.
//   - '-1' is the unary minus applied to the literal 1 (11.4.3). Whether
//     HLC keeps the Operation or folds it into a Constant whose source text
//     is "-1" is a model convention; both are accepted.
//   - What the LHS 'F' binds to (the Function or an implicit return
//     variable) is a model convention.
//   - Whether HLC binds a call to the built-in randomize() through
//     getTaskFunc() is a tool convention.
//   - The file has no module, so there is no top module to check; design-wide
//     counts of classes and functions depend on the built-in libraries, so
//     every declaration is found by name.
//   - The .hlc puts UVM on the include path, but this file neither imports
//     uvm_pkg nor includes a UVM file, so no UVM declaration is checked.
//   - The warning count, source line numbers, and the license/metadata
//     comments.

#include <hlc/Common/FileSystem.h>
#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/ErrorReporting/Location.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assignment.h>
#include <hldb/class_defn.h>
#include <hldb/class_typespec.h>
#include <hldb/constant.h>
#include <hldb/constraint.h>
#include <hldb/design.h>
#include <hldb/function.h>
#include <hldb/int_typespec.h>
#include <hldb/io_decl.h>
#include <hldb/method_func_call.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/task_func.h>
#include <hldb/typedef.h>
#include <hldb/typedef_typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <string_view>
#include <vector>

namespace hlc {

class InLineConstraintsRandomize5Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "18.7--in-line-constraints--randomize_5.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  // --- class a ------------------------------------------------------------

  static const hldb::ClassDefn *getClassA() {
    return hldb::findByName<hldb::ClassDefn>("a", m_design->getAllClasses());
  }

  static const hldb::Variable *getClassAProperty(std::string_view name) {
    const hldb::ClassDefn *const cls = getClassA();
    if (cls == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::Variable>(name, cls->getVariables());
  }

  static const hldb::Variable *getClassAX() { return getClassAProperty("x"); }

  // --- function F ---------------------------------------------------------

  static const hldb::Function *getF() {
    if (m_design->getTaskFuncs() == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::Function>("F", m_design->getTaskFuncs());
  }

  // 37.15: a reference to a formal resolves to the Variable holding it,
  // never to the IODecl.
  static const hldb::Variable *getFFormalVariable(std::string_view name) {
    const hldb::Function *const fn = getF();
    if (fn == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::Variable>(name, fn->getVariables());
  }

  static const hldb::Assignment *getFBody() {
    const hldb::Function *const fn = getF();
    if (fn == nullptr) {
      return nullptr;
    }
    return fn->getStmt<hldb::Assignment>();
  }

  // The MethodFuncCall 'randomize' at the end of 'obj.randomize() with ...'.
  static const hldb::MethodFuncCall *getRandomizeCall() {
    const hldb::Assignment *const assign = getFBody();
    if (assign == nullptr) {
      return nullptr;
    }
    const hldb::RefObj *const path = assign->getRhs<hldb::RefObj>();
    if (path == nullptr || path->getPathElems() == nullptr || path->getPathElems()->size() != 2) {
      return nullptr;
    }
    return any_cast<hldb::MethodFuncCall>(path->getPathElems()->at(1));
  }

  // The items of the 'with' block. getWith() is typed Any: HLC may store an
  // unnamed Constraint holding the items, or a single expression directly.
  static std::vector<const hldb::Any *> getWithItems() {
    std::vector<const hldb::Any *> items;
    const hldb::MethodFuncCall *const call = getRandomizeCall();
    if (call == nullptr || call->getWith() == nullptr) {
      return items;
    }
    if (const hldb::Constraint *const block = call->getWith<hldb::Constraint>()) {
      if (block->getConstraintItems() == nullptr) {
        return items;
      }
      for (const hldb::Any *const item : *block->getConstraintItems()) {
        items.emplace_back(item);
      }
      return items;
    }
    items.emplace_back(call->getWith());
    return items;
  }

  // Operand 'operand' of 'with' item 'item', or nullptr.
  static const hldb::Any *getWithOperand(size_t item, size_t operand) {
    const std::vector<const hldb::Any *> items = getWithItems();
    if (items.size() <= item) {
      return nullptr;
    }
    const hldb::Operation *const op = any_cast<hldb::Operation>(items[item]);
    if (op == nullptr || op->getOperands() == nullptr || op->getOperands()->size() <= operand) {
      return nullptr;
    }
    return op->getOperands()->at(operand);
  }

  // --- expectations -------------------------------------------------------

  // Verifies 'expr' is a RefObj named 'name' bound by object identity to
  // 'target'.
  static void ExpectBoundRef(const hldb::Any *expr, std::string_view name, const hldb::Any *target) {
    const hldb::RefObj *const ref = any_cast<hldb::RefObj>(expr);
    ASSERT_NE(ref, nullptr) << "'" << name << "' should be a RefObj";
    EXPECT_EQ(ref->getName(), name);
    ASSERT_NE(target, nullptr) << "the declaration of '" << name << "' was not found";
    EXPECT_EQ(ref->getActual(), target) << "'" << name << "' must bind to its declaration";
  }

  // Verifies 'expr' is the integer literal whose source text is 'text'.
  static void ExpectIntLiteral(const hldb::Any *expr, std::string_view text) {
    const hldb::Constant *const literal = any_cast<hldb::Constant>(expr);
    ASSERT_NE(literal, nullptr) << "'" << text << "' should be a Constant";
    EXPECT_EQ(literal->getDecompile(), text);
  }

  // Verifies 'expr' is '-1': either the unary minus applied to the literal 1
  // (11.4.3), or that Operation folded into a Constant "-1".
  static void ExpectMinusOne(const hldb::Any *expr) {
    ASSERT_NE(expr, nullptr) << "8.8: 'int y = -1;' has an initializer";
    if (const hldb::Operation *const op = any_cast<hldb::Operation>(expr)) {
      EXPECT_EQ(op->getOpType(), vpiMinusOp) << "11.4.3: '-1' is the unary minus applied to 1";
      ASSERT_NE(op->getOperands(), nullptr);
      ASSERT_EQ(op->getOperands()->size(), 1u) << "unary minus has one operand";
      ExpectIntLiteral(op->getOperands()->at(0), "1");
      return;
    }
    ExpectIntLiteral(expr, "-1");
  }

  // Verifies 'type' is 'int': an IntTypespec that is signed.
  static void ExpectSignedInt(const hldb::RefTypespec *type, std::string_view what) {
    ASSERT_NE(type, nullptr) << what << " has no typespec";
    const hldb::IntTypespec *const ts = type->getActual<hldb::IntTypespec>();
    ASSERT_NE(ts, nullptr) << what << " is declared 'int'";
    EXPECT_TRUE(ts->getSigned()) << "6.11: 'int' is a signed type, so " << what << " is signed";
  }

  // Verifies 'type' is a class handle type resolving to 'cls'.
  static void ExpectClassHandle(const hldb::RefTypespec *type, const hldb::ClassDefn *cls, std::string_view what) {
    ASSERT_NE(type, nullptr) << what << " has no typespec";
    const hldb::ClassTypespec *ct = nullptr;
    if (const hldb::TypedefTypespec *const tt = type->getActual<hldb::TypedefTypespec>()) {
      if (const hldb::Typedef *const t = tt->getTypedef()) {
        if (const hldb::RefTypespec *const rt = t->getAlias()) {
          ct = rt->getActual<hldb::ClassTypespec>();
        }
      }
    } else {
      ct = type->getActual<hldb::ClassTypespec>();
    }
    ASSERT_NE(ct, nullptr) << what << " is declared with a class type";
    ASSERT_NE(cls, nullptr) << "the class " << what << " refers to was not found";
    EXPECT_EQ(ct->getClassDefn(), cls) << what << " must resolve to its class";
  }
};

// ---------------------------------------------------------------------------
// class a; rand int x; int y = -1; endclass
// ---------------------------------------------------------------------------

TEST_F(InLineConstraintsRandomize5Test, ClassAIsDeclaredOnceAsUserDefinedClass) {
  ASSERT_NE(m_design->getAllClasses(), nullptr);
  size_t count = 0;
  for (const hldb::ClassDefn *const cls : *m_design->getAllClasses()) {
    if (cls->getName() == "a") ++count;
  }
  EXPECT_EQ(count, 1u) << "the source declares class 'a' exactly once";
  const hldb::ClassDefn *const cls = getClassA();
  ASSERT_NE(cls, nullptr);
  EXPECT_EQ(cls->getClassType(), vpiUserDefinedClass) << "8.3: 'class a;' is a user-defined class";
}

TEST_F(InLineConstraintsRandomize5Test, ClassAHasNoBaseClassParametersMethodsOrEndLabel) {
  const hldb::ClassDefn *const cls = getClassA();
  ASSERT_NE(cls, nullptr);
  EXPECT_FALSE(cls->getVirtual()) << "8.21: 'class a' is not declared 'virtual class'";
  EXPECT_EQ(cls->getExtends(), nullptr) << "8.13: 'class a;' has no 'extends' clause";
  EXPECT_TRUE(cls->getParameters() == nullptr || cls->getParameters()->empty())
      << "8.25: 'class a;' has no parameter port list";
  EXPECT_EQ(cls->getEndLabelObj(), nullptr) << "'endclass' is written without ': a'";
  size_t declared = 0;
  if (cls->getMethods() != nullptr) {
    for (const hldb::TaskFunc *const method : *cls->getMethods()) {
      if (method->getName() != "new") ++declared;
    }
  }
  EXPECT_EQ(declared, 0u) << "class a declares no task or function; randomize() is built in (18.6.1)";
}

TEST_F(InLineConstraintsRandomize5Test, ClassAHasPropertiesXAndYInOrder) {
  const hldb::ClassDefn *const cls = getClassA();
  ASSERT_NE(cls, nullptr);
  ASSERT_NE(cls->getVariables(), nullptr);
  ASSERT_EQ(cls->getVariables()->size(), 2u) << "'rand int x;' and 'int y = -1;'";
  const std::vector<std::string_view> expected = {"x", "y"};
  for (size_t i = 0; i < expected.size(); ++i) {
    const hldb::Variable *const prop = cls->getVariables()->at(i);
    ASSERT_NE(prop, nullptr) << "property " << i;
    EXPECT_EQ(prop->getName(), expected[i]) << "property " << i;
    ExpectSignedInt(prop->getTypespec(), expected[i]);
    EXPECT_EQ(prop->getVisibility(), vpiPublicVis) << "8.18: '" << expected[i] << "' has no 'local'/'protected'";
    EXPECT_FALSE(prop->getConstantVariable()) << "8.19: '" << expected[i] << "' has no 'const' qualifier";
  }
}

TEST_F(InLineConstraintsRandomize5Test, ClassAXIsRandWithNoInitializer) {
  const hldb::Variable *const x = getClassAX();
  ASSERT_NE(x, nullptr);
  EXPECT_TRUE(x->getIsRandomized()) << "18.4: 'x' is declared with the 'rand' qualifier";
  EXPECT_EQ(x->getRandType(), vpiRand) << "18.4.1: 'rand', not 'randc' (18.4.2)";
  EXPECT_EQ(x->getValue(), nullptr) << "'rand int x;' has no initializer";
}

TEST_F(InLineConstraintsRandomize5Test, ClassAYIsNotRandAndInitializedToMinusOne) {
  const hldb::Variable *const y = getClassAProperty("y");
  ASSERT_NE(y, nullptr);
  EXPECT_FALSE(y->getIsRandomized()) << "18.4: 'y' has no 'rand'/'randc' qualifier";
  EXPECT_EQ(y->getRandType(), vpiNotRand) << "18.4: 'y' has no 'rand'/'randc' qualifier";
  ExpectMinusOne(y->getValue());
}

// ---------------------------------------------------------------------------
// function int F(a obj, int y); F = obj.randomize() with (x) {...}; endfunction
// ---------------------------------------------------------------------------

TEST_F(InLineConstraintsRandomize5Test, FIsCompilationUnitFunctionReturningInt) {
  ASSERT_NE(m_design->getTaskFuncs(), nullptr) << "3.12.1: F is declared in the compilation unit";
  size_t count = 0;
  for (const hldb::TaskFunc *const tf : *m_design->getTaskFuncs()) {
    if (tf->getName() == "F") ++count;
  }
  EXPECT_EQ(count, 1u) << "the source declares function 'F' exactly once";
  const hldb::Function *const fn = getF();
  ASSERT_NE(fn, nullptr) << "13.4: 'function int F(...)' is a Function";
  ExpectSignedInt(fn->getReturn(), "F's return type");
  EXPECT_FALSE(fn->getAutomatic())
      << "13.4.2: a subroutine outside a class with no lifetime keyword has static lifetime";
}

TEST_F(InLineConstraintsRandomize5Test, FHasFormalsObjAndY) {
  const hldb::Function *const fn = getF();
  ASSERT_NE(fn, nullptr);
  ASSERT_NE(fn->getIODecls(), nullptr);
  ASSERT_EQ(fn->getIODecls()->size(), 2u) << "'a obj' and 'int y'";

  const hldb::IODecl *const obj = fn->getIODecls()->at(0);
  ASSERT_NE(obj, nullptr);
  EXPECT_EQ(obj->getName(), "obj");
  EXPECT_EQ(obj->getDirection(), vpiInput) << "13.3: a formal with no direction is an input";
  ExpectClassHandle(obj->getTypespec(), getClassA(), "'obj'");

  const hldb::IODecl *const y = fn->getIODecls()->at(1);
  ASSERT_NE(y, nullptr);
  EXPECT_EQ(y->getName(), "y");
  EXPECT_EQ(y->getDirection(), vpiInput) << "13.3: a formal with no direction is an input";
  ExpectSignedInt(y->getTypespec(), "formal 'y'");
}

TEST_F(InLineConstraintsRandomize5Test, FBodyAssignsToFunctionName) {
  const hldb::Assignment *const assign = getFBody();
  ASSERT_NE(assign, nullptr) << "the body is a single statement, so it is not wrapped in a Begin";
  EXPECT_TRUE(assign->getBlocking()) << "10.4.1: '=' in a procedural context is a blocking assignment";
  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr) << "the LHS is a reference";
  EXPECT_EQ(lhs->getName(), "F") << "13.4.1: assigning the function name sets the return value";
}

TEST_F(InLineConstraintsRandomize5Test, FRhsIsObjRandomizeWithRestrictedInlineConstraint) {
  const hldb::Assignment *const assign = getFBody();
  ASSERT_NE(assign, nullptr);
  const hldb::RefObj *const path = assign->getRhs<hldb::RefObj>();
  ASSERT_NE(path, nullptr) << "'obj.randomize() with (x) {...}' should be a hierarchical RefObj";
  ASSERT_NE(path->getPathElems(), nullptr);
  ASSERT_EQ(path->getPathElems()->size(), 2u);
  ExpectBoundRef(path->getPathElems()->at(0), "obj", getFFormalVariable("obj"));

  const hldb::MethodFuncCall *const call = getRandomizeCall();
  ASSERT_NE(call, nullptr) << "18.6.1: randomize() is a function called through a handle, so a MethodFuncCall";
  EXPECT_EQ(call->getName(), "randomize");
  EXPECT_TRUE(call->getArguments() == nullptr || call->getArguments()->empty())
      << "'randomize()' is written with no arguments; '(x)' after 'with' is the identifier list (18.7)";
  EXPECT_NE(call->getWith(), nullptr) << "18.7: the call carries a 'with' inline constraint";
}

TEST_F(InLineConstraintsRandomize5Test, WithBlockHoldsXGreaterThanZeroThenXLessThanY) {
  ASSERT_NE(getRandomizeCall(), nullptr);
  const std::vector<const hldb::Any *> items = getWithItems();
  ASSERT_EQ(items.size(), 2u) << "18.7: '{ x > 0; x < y; }' holds two constraint expressions";

  const hldb::Operation *const gt = any_cast<hldb::Operation>(items[0]);
  ASSERT_NE(gt, nullptr) << "'x > 0' should be an Operation";
  EXPECT_EQ(gt->getOpType(), vpiGtOp);
  ASSERT_NE(gt->getOperands(), nullptr);
  EXPECT_EQ(gt->getOperands()->size(), 2u) << "'>' is a binary operator";
  ExpectIntLiteral(getWithOperand(0, 1), "0");

  const hldb::Operation *const lt = any_cast<hldb::Operation>(items[1]);
  ASSERT_NE(lt, nullptr) << "'x < y' should be an Operation";
  EXPECT_EQ(lt->getOpType(), vpiLtOp);
  ASSERT_NE(lt->getOperands(), nullptr);
  EXPECT_EQ(lt->getOperands()->size(), 2u) << "'<' is a binary operator";
}

// Expected to fail until HLC is fixed -- see KNOWN FAILURE (names in a 'with'
// block are not looked up in the randomized object's class) in the file
// header.
TEST_F(InLineConstraintsRandomize5Test, BothXInWithBindToClassAProperty) {
  // 18.7: 'x' is in the identifier list '(x)', so it is looked up in class a,
  // the class of the randomized object.
  ExpectBoundRef(getWithOperand(0, 0), "x", getClassAX());
  ExpectBoundRef(getWithOperand(1, 0), "x", getClassAX());
}

TEST_F(InLineConstraintsRandomize5Test, YInWithBindsToFormalOfF) {
  // 18.7: 'y' is not in the identifier list, so it resolves in the scope of
  // the call -- F's formal 'y' (37.15: the Variable holding it).
  ExpectBoundRef(getWithOperand(1, 1), "y", getFFormalVariable("y"));
  const hldb::RefObj *const y = any_cast<hldb::RefObj>(getWithOperand(1, 1));
  ASSERT_NE(y, nullptr);
  ASSERT_NE(getClassAProperty("y"), nullptr);
  EXPECT_NE(y->getActual(), getClassAProperty("y"))
      << "18.7: 'y' is not in the identifier list '(x)', so class a's 'y' must not be used";
}

// ---------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------

TEST_F(InLineConstraintsRandomize5Test, EveryReferencedVariableAndFormalIsFound) {
  for (std::string_view name : {"obj", "x", "y"}) {
    EXPECT_EQ(findError(ErrorDefinition::COMP_UNDEFINED_VARIABLE, name), nullptr)
        << "'" << name << "' is declared in this file, so it is not undefined";
  }
}

// The file is legal SystemVerilog, so no fatal, syntax or error diagnostic
// may point into it.
// Expected to fail -- see KNOWN FAILURE (errors reported in this file) in the
// file header.
TEST_F(InLineConstraintsRandomize5Test, NoErrorIsReportedInThisFile) {
  const ErrorDefinition::ErrorMap &infos = ErrorDefinition::getErrorInfoMap();
  size_t count = 0;
  for (const Error &error : m_session->getErrorContainer()->getErrors()) {
    const ErrorDefinition::ErrorMap::const_iterator info = infos.find(error.getType());
    if (info == infos.cend() || info->second.m_severity > ErrorDefinition::ERROR) continue;
    for (const Location &loc : error.getLocations()) {
      const std::string_view path = FileSystem::toPath(loc.m_fileId);
      if (path.find("18.7--in-line-constraints--randomize_5.sv") == std::string_view::npos) continue;
      ++count;
      ADD_FAILURE() << "error " << error.getType() << " reported at " << path << ":" << loc.m_line << ":"
                    << loc.m_column;
    }
  }
  EXPECT_EQ(count, 0u) << "the source is legal SystemVerilog";
}

// Expected to fail -- see KNOWN FAILURE (errors reported in this file) in the
// file header.
TEST_F(InLineConstraintsRandomize5Test, NoFatalSyntaxOrErrorDiagnostics) {
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0) << "the source is legal SystemVerilog and should compile without errors";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
