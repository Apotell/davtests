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
// tests/Google/chapter-18/18.7--in-line-constraints--randomize_1.sv. Lines
// 1-14 are the license and the suite's :name:/:description:/:tags: comment;
// the rest is:
//
//   16  class a1;
//   17      rand int x;
//   18  endclass
//   19
//   20  class a2;
//   21      int x, y;
//   22
//   23      task do_randomize(a1 obj, int x, int z);
//   24          int result;
//   25          result = obj.randomize() with {x > 0; x < y + z;};
//   26      endtask
//   27  endclass
//
// This is the example of IEEE 1800-2023 18.7. The point of the fixture is
// name resolution inside an inline constraint. Three different 'x' are
// visible at line 25: class a1's rand property, class a2's property and the
// task's formal. 18.7: a name in the 'with' block is searched for first in
// the class of the object being randomized (a1), then in the scope that
// contains the call. So:
//   - both 'x' bind to class a1's 'x' -- not to the formal 'x' and not to
//     class a2's 'x', which a1's 'x' hides
//   - 'y' is not declared in a1, so it binds to class a2's 'y' (the class
//     whose method makes the call, 8.5)
//   - 'z' is not declared in a1, so it binds to the task's formal 'z'
//
// What is checked, and why:
//   class a1 (8.3)
//     - exactly one class named "a1"; user-defined; not virtual (8.21); no
//       'extends' (8.13), no parameters (8.25), no end label; declares no
//       methods (randomize() is built in, 18.6.1)
//     - 'rand int x;' (18.4): the only property; a signed int (6.11),
//       getIsRandomized() true, getRandType() vpiRand, no initializer,
//       public (8.18), not const (8.19)
//   class a2 (8.3)
//     - exactly one class named "a2"; user-defined; not virtual; no
//       'extends', no parameters, no end label
//     - 'int x, y;': exactly 2 properties, in order x, y; each a signed int
//       with no initializer, not rand/randc (18.4), public, not const
//     - exactly one method, the Task 'do_randomize'; automatic (8.6),
//       public, no end label
//   task do_randomize(a1 obj, int x, int z); (13.3)
//     - formals, in order: 'obj' (input, a handle to class a1), 'x' (input,
//       signed int) and 'z' (input, signed int); no direction means input
//       (13.3)
//     - 'int result;' declares a signed int variable 'result' local to the
//       task, with no initializer
//   result = obj.randomize() with {x > 0; x < y + z;}; (10.4.1, 18.7)
//     - the task's only statement: a blocking Assignment whose LHS binds to
//       the local 'result'
//     - its RHS is a hierarchical RefObj whose prefix 'obj' binds to the
//       Variable holding the formal 'obj' (37.15), then a MethodFuncCall
//       "randomize" with no arguments and a 'with' inline constraint (18.7)
//     - the 'with' block holds exactly two constraint expressions, in source
//       order: 'x > 0' (vpiGtOp, right operand the literal 0) and
//       'x < y + z' (vpiLtOp whose right operand is vpiAddOp(y, z) -- '+'
//       binds tighter than '<', 11.3.2)
//     - both 'x' bind to class a1's 'x'; 'y' binds to class a2's 'y'; 'z'
//       binds to the Variable holding the formal 'z' (18.7, 37.15)
//   Diagnostics
//     - no CompUndefinedVariable for any variable or formal the file
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
// KNOWN FAILURE (wrong scope for names in a 'with' block), not a defect in
// this test: BothXInWithBindToClassA1Property fails on HLC 577. Both 'x' in
// the 'with' block bind to the task's formal 'x' instead of class a1's 'x'.
// HLC resolves names in the 'with' block only in the scope of the call and
// never looks in the class of the randomized object first, as 18.7
// requires ('y' and 'z', which a1 does not declare, therefore bind
// correctly). It is expected to fail until HLC is fixed and is
// intentionally not skipped or relaxed.
//
// KNOWN FAILURE (errors reported in this file), not a defect in this test:
// NoErrorIsReportedInThisFile and NoFatalSyntaxOrErrorDiagnostics assert
// that this legal file compiles cleanly and are expected to fail. HLC
// reports IllegalPropertyValue (2038) twice at line 25, column 39 --
// the '{' that opens the 'with' block, once per constraint expression --
// although the inline constraint is legal (18.7). With this .hlc: 0 syntax
// errors and 2 errors in total. Neither check is skipped or relaxed.
//
// What is NOT checked, and why:
//   - The value randomize() picks for obj.x and the value stored in
//     'result' only exist while simulation runs; the task is never called.
//     Permanently out of scope. The static half is covered by
//     WithBlockHoldsXGreaterThanZeroThenXLessThanYPlusZ,
//     BothXInWithBindToClassA1Property, YInWithBindsToClassA2Property and
//     ZInWithBindsToFormalZ.
//   - How HLC wraps the 'with' block (an unnamed Constraint holding the
//     items, or the items stored otherwise) is a model convention:
//     MethodFuncCall::getWith() is typed Any. Both an unnamed Constraint
//     and a lone expression are accepted; the items, operators and bindings
//     are asserted.
//   - Whether HLC stores the task body as the bare Assignment or as a Begin
//     holding the declaration of 'result' and the Assignment is a model
//     convention; both are accepted, and in both the Assignment must be the
//     only statement. 'result' is likewise looked up on the task and, if
//     present, on that Begin.
//   - The RefObj name HLC records for each name, beyond the name written in
//     the source.
//   - Whether HLC binds a call to the built-in randomize() through
//     getTaskFunc() is a tool convention.
//   - The file has no module, so there is no top module to check; design-wide
//     counts of classes depend on the built-in libraries, so every
//     declaration is found by name.
//   - The .hlc puts UVM on the include path, but this file neither imports
//     uvm_pkg nor includes a UVM file, so no UVM declaration is checked.
//   - The warning count, source line numbers, and the license/metadata
//     comments.

#include <hlc/Common/FileSystem.h>
#include <hlc/Common/Session.h>
#include <hlc/Events/EventDefinition.h>
#include <hlc/Events/EventListener.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/class_defn.h>
#include <hldb/class_typespec.h>
#include <hldb/constant.h>
#include <hldb/constraint.h>
#include <hldb/design.h>
#include <hldb/int_typespec.h>
#include <hldb/io_decl.h>
#include <hldb/method_func_call.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/task.h>
#include <hldb/task_func.h>
#include <hldb/typedef.h>
#include <hldb/typedef_typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <cstdint>
#include <string_view>
#include <vector>

namespace hlc {

class InLineConstraintsRandomize1Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "18.7--in-line-constraints--randomize_1.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  // --- class a1 -----------------------------------------------------------

  static const hldb::ClassDefn *getClassA1() {
    return hldb::findByName<hldb::ClassDefn>("a1", m_design->getAllClasses());
  }

  static const hldb::Variable *getClassA1X() {
    const hldb::ClassDefn *const cls = getClassA1();
    if (cls == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::Variable>("x", cls->getVariables());
  }

  // --- class a2 -----------------------------------------------------------

  static const hldb::ClassDefn *getClassA2() {
    return hldb::findByName<hldb::ClassDefn>("a2", m_design->getAllClasses());
  }

  static const hldb::Variable *getClassA2Property(std::string_view name) {
    const hldb::ClassDefn *const cls = getClassA2();
    if (cls == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::Variable>(name, cls->getVariables());
  }

  // --- task do_randomize --------------------------------------------------

  static const hldb::Task *getDoRandomize() {
    const hldb::ClassDefn *const cls = getClassA2();
    if (cls == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::Task>("do_randomize", cls->getMethods());
  }

  // 37.15: a reference to a formal resolves to the Variable holding it,
  // never to the IODecl.
  static const hldb::Variable *getFormalVariable(std::string_view name) {
    const hldb::Task *const task = getDoRandomize();
    if (task == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::Variable>(name, task->getVariables());
  }

  // The local 'int result;'. HLC may record it on the task or on a Begin
  // wrapping the task body.
  static const hldb::Variable *getResult() {
    const hldb::Task *const task = getDoRandomize();
    if (task == nullptr) {
      return nullptr;
    }
    if (task->getVariables() != nullptr) {
      if (const hldb::Variable *const var = hldb::findByName<hldb::Variable>("result", task->getVariables())) {
        return var;
      }
    }
    const hldb::Begin *const block = task->getStmt<hldb::Begin>();
    if (block == nullptr || block->getVariables() == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::Variable>("result", block->getVariables());
  }

  // The statements of the task body: the bare statement, or the statements
  // of a Begin wrapping it.
  static std::vector<const hldb::Any *> getBodyStmts() {
    std::vector<const hldb::Any *> stmts;
    const hldb::Task *const task = getDoRandomize();
    if (task == nullptr || task->getStmt() == nullptr) {
      return stmts;
    }
    if (const hldb::Begin *const block = task->getStmt<hldb::Begin>()) {
      if (block->getStmts() == nullptr) {
        return stmts;
      }
      for (const hldb::Any *const stmt : *block->getStmts()) {
        stmts.emplace_back(stmt);
      }
      return stmts;
    }
    stmts.emplace_back(task->getStmt());
    return stmts;
  }

  static const hldb::Assignment *getAssignment() {
    const std::vector<const hldb::Any *> stmts = getBodyStmts();
    if (stmts.size() != 1) {
      return nullptr;
    }
    return any_cast<hldb::Assignment>(stmts[0]);
  }

  // The MethodFuncCall 'randomize' at the end of 'obj.randomize() with ...'.
  static const hldb::MethodFuncCall *getRandomizeCall() {
    const hldb::Assignment *const assign = getAssignment();
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

  // Operand 'operand' of the 'y + z' inside 'x < y + z', or nullptr.
  static const hldb::Any *getAddOperand(size_t operand) {
    const hldb::Operation *const add = any_cast<hldb::Operation>(getWithOperand(1, 1));
    if (add == nullptr || add->getOperands() == nullptr || add->getOperands()->size() <= operand) {
      return nullptr;
    }
    return add->getOperands()->at(operand);
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

  // Verifies class 'name' is declared once as a plain user-defined class:
  // not virtual, no base class, no parameters and no end label.
  static void ExpectPlainClass(const hldb::ClassDefn *cls, std::string_view name) {
    ASSERT_NE(m_design->getAllClasses(), nullptr);
    size_t count = 0;
    for (const hldb::ClassDefn *const c : *m_design->getAllClasses()) {
      if (c->getName() == name) ++count;
    }
    EXPECT_EQ(count, 1u) << "the source declares class '" << name << "' exactly once";
    ASSERT_NE(cls, nullptr) << "class '" << name << "' not found";
    EXPECT_EQ(cls->getClassType(), vpiUserDefinedClass) << "8.3: 'class " << name << ";' is a user-defined class";
    EXPECT_FALSE(cls->getVirtual()) << "8.21: '" << name << "' is not declared 'virtual class'";
    EXPECT_EQ(cls->getExtends(), nullptr) << "8.13: 'class " << name << ";' has no 'extends' clause";
    EXPECT_TRUE(cls->getParameters() == nullptr || cls->getParameters()->empty())
        << "8.25: 'class " << name << ";' has no parameter port list";
    EXPECT_EQ(cls->getEndLabelObj(), nullptr) << "'endclass' is written without ': " << name << "'";
  }
};

// ---------------------------------------------------------------------------
// class a1; rand int x; endclass
// ---------------------------------------------------------------------------

TEST_F(InLineConstraintsRandomize1Test, ClassA1IsPlainUserDefinedClass) { ExpectPlainClass(getClassA1(), "a1"); }

TEST_F(InLineConstraintsRandomize1Test, ClassA1DeclaresNoMethods) {
  const hldb::ClassDefn *const cls = getClassA1();
  ASSERT_NE(cls, nullptr);
  size_t declared = 0;
  if (cls->getMethods() != nullptr) {
    for (const hldb::TaskFunc *const method : *cls->getMethods()) {
      if (method->getName() != "new") ++declared;
    }
  }
  EXPECT_EQ(declared, 0u) << "class a1 declares no task or function; randomize() is built in (18.6.1)";
}

TEST_F(InLineConstraintsRandomize1Test, ClassA1HasOneRandIntPropertyX) {
  const hldb::ClassDefn *const cls = getClassA1();
  ASSERT_NE(cls, nullptr);
  ASSERT_NE(cls->getVariables(), nullptr);
  ASSERT_EQ(cls->getVariables()->size(), 1u) << "'rand int x;' is the only property";
  const hldb::Variable *const x = getClassA1X();
  ASSERT_NE(x, nullptr);
  EXPECT_EQ(cls->getVariables()->at(0), x);
  ExpectSignedInt(x->getTypespec(), "class a1's 'x'");
  EXPECT_TRUE(x->getIsRandomized()) << "18.4: 'x' is declared with the 'rand' qualifier";
  EXPECT_EQ(x->getRandType(), vpiRand) << "18.4.1: 'rand', not 'randc' (18.4.2)";
  EXPECT_EQ(x->getValue(), nullptr) << "'rand int x;' has no initializer";
  EXPECT_EQ(x->getVisibility(), vpiPublicVis) << "8.18: 'x' has no 'local'/'protected' qualifier";
  EXPECT_FALSE(x->getConstantVariable()) << "8.19: 'x' has no 'const' qualifier";
}

// ---------------------------------------------------------------------------
// class a2; int x, y; task do_randomize(...); ... endtask endclass
// ---------------------------------------------------------------------------

TEST_F(InLineConstraintsRandomize1Test, ClassA2IsPlainUserDefinedClass) { ExpectPlainClass(getClassA2(), "a2"); }

TEST_F(InLineConstraintsRandomize1Test, ClassA2HasIntPropertiesXAndYInOrder) {
  const hldb::ClassDefn *const cls = getClassA2();
  ASSERT_NE(cls, nullptr);
  ASSERT_NE(cls->getVariables(), nullptr);
  ASSERT_EQ(cls->getVariables()->size(), 2u) << "'int x, y;' declares two properties";
  const std::vector<std::string_view> expected = {"x", "y"};
  for (size_t i = 0; i < expected.size(); ++i) {
    const hldb::Variable *const prop = cls->getVariables()->at(i);
    ASSERT_NE(prop, nullptr) << "property " << i;
    EXPECT_EQ(prop->getName(), expected[i]) << "property " << i;
    ExpectSignedInt(prop->getTypespec(), expected[i]);
    EXPECT_EQ(prop->getValue(), nullptr) << "'" << expected[i] << "' has no initializer";
    EXPECT_FALSE(prop->getIsRandomized()) << "18.4: '" << expected[i] << "' has no 'rand'/'randc' qualifier";
    EXPECT_EQ(prop->getRandType(), vpiNotRand) << "18.4: '" << expected[i] << "' has no 'rand'/'randc' qualifier";
    EXPECT_EQ(prop->getVisibility(), vpiPublicVis) << "8.18: '" << expected[i] << "' has no 'local'/'protected'";
    EXPECT_FALSE(prop->getConstantVariable()) << "8.19: '" << expected[i] << "' has no 'const' qualifier";
  }
}

TEST_F(InLineConstraintsRandomize1Test, ClassA2HasOneAutomaticTaskDoRandomize) {
  const hldb::ClassDefn *const cls = getClassA2();
  ASSERT_NE(cls, nullptr);
  ASSERT_NE(cls->getMethods(), nullptr);
  EXPECT_EQ(cls->getMethods()->size(), 1u) << "the class body declares only 'do_randomize'";
  const hldb::Task *const task = getDoRandomize();
  ASSERT_NE(task, nullptr) << "13.3: 'task do_randomize(...)' is a Task";
  EXPECT_TRUE(task->getMethod());
  EXPECT_TRUE(task->getAutomatic()) << "8.6: the lifetime of a method declared in a class is always automatic";
  EXPECT_EQ(task->getVisibility(), vpiPublicVis) << "8.18: 'do_randomize' has no 'local'/'protected'";
  EXPECT_EQ(task->getEndLabelObj(), nullptr) << "'endtask' is written without ': do_randomize'";
}

// ---------------------------------------------------------------------------
// task do_randomize(a1 obj, int x, int z); int result; ... endtask
// ---------------------------------------------------------------------------

TEST_F(InLineConstraintsRandomize1Test, DoRandomizeHasFormalsObjXZ) {
  const hldb::Task *const task = getDoRandomize();
  ASSERT_NE(task, nullptr);
  ASSERT_NE(task->getIODecls(), nullptr);
  ASSERT_EQ(task->getIODecls()->size(), 3u) << "'a1 obj', 'int x' and 'int z'";

  const hldb::IODecl *const obj = task->getIODecls()->at(0);
  ASSERT_NE(obj, nullptr);
  EXPECT_EQ(obj->getName(), "obj");
  EXPECT_EQ(obj->getDirection(), vpiInput) << "13.3: a formal with no direction is an input";
  ExpectClassHandle(obj->getTypespec(), getClassA1(), "'obj'");

  const hldb::IODecl *const x = task->getIODecls()->at(1);
  ASSERT_NE(x, nullptr);
  EXPECT_EQ(x->getName(), "x");
  EXPECT_EQ(x->getDirection(), vpiInput) << "13.3: a formal with no direction is an input";
  ExpectSignedInt(x->getTypespec(), "formal 'x'");

  const hldb::IODecl *const z = task->getIODecls()->at(2);
  ASSERT_NE(z, nullptr);
  EXPECT_EQ(z->getName(), "z");
  EXPECT_EQ(z->getDirection(), vpiInput) << "13.3: a formal with no direction is an input";
  ExpectSignedInt(z->getTypespec(), "formal 'z'");
}

TEST_F(InLineConstraintsRandomize1Test, DoRandomizeDeclaresLocalIntResult) {
  const hldb::Variable *const result = getResult();
  ASSERT_NE(result, nullptr) << "'int result;' declares a variable local to the task";
  ExpectSignedInt(result->getTypespec(), "'result'");
  EXPECT_EQ(result->getValue(), nullptr) << "'int result;' has no initializer";
  EXPECT_FALSE(result->getIsRandomized()) << "18.4: 'result' is not a rand class property";
}

// ---------------------------------------------------------------------------
// result = obj.randomize() with {x > 0; x < y + z;};
// ---------------------------------------------------------------------------

TEST_F(InLineConstraintsRandomize1Test, BodyIsOneAssignmentToResult) {
  const std::vector<const hldb::Any *> stmts = getBodyStmts();
  ASSERT_EQ(stmts.size(), 1u) << "line 25 is the task's only statement";
  const hldb::Assignment *const assign = getAssignment();
  ASSERT_NE(assign, nullptr) << "'result = ...;' should be an Assignment";
  EXPECT_TRUE(assign->getBlocking()) << "10.4.1: '=' in a procedural context is a blocking assignment";
  ExpectBoundRef(assign->getLhs(), "result", getResult());
}

TEST_F(InLineConstraintsRandomize1Test, RhsIsObjRandomizeWithInlineConstraint) {
  const hldb::Assignment *const assign = getAssignment();
  ASSERT_NE(assign, nullptr);
  const hldb::RefObj *const path = assign->getRhs<hldb::RefObj>();
  ASSERT_NE(path, nullptr) << "'obj.randomize() with {...}' should be a hierarchical RefObj";
  ASSERT_NE(path->getPathElems(), nullptr);
  ASSERT_EQ(path->getPathElems()->size(), 2u);
  ExpectBoundRef(path->getPathElems()->at(0), "obj", getFormalVariable("obj"));

  const hldb::MethodFuncCall *const call = getRandomizeCall();
  ASSERT_NE(call, nullptr) << "18.6.1: randomize() is a function called through a handle, so a MethodFuncCall";
  EXPECT_EQ(call->getName(), "randomize");
  EXPECT_TRUE(call->getArguments() == nullptr || call->getArguments()->empty())
      << "'randomize()' is written with no arguments";
  EXPECT_NE(call->getWith(), nullptr) << "18.7: the call carries a 'with' inline constraint";
}

TEST_F(InLineConstraintsRandomize1Test, WithBlockHoldsXGreaterThanZeroThenXLessThanYPlusZ) {
  ASSERT_NE(getRandomizeCall(), nullptr);
  const std::vector<const hldb::Any *> items = getWithItems();
  ASSERT_EQ(items.size(), 2u) << "18.7: '{x > 0; x < y + z;}' holds two constraint expressions";

  const hldb::Operation *const gt = any_cast<hldb::Operation>(items[0]);
  ASSERT_NE(gt, nullptr) << "'x > 0' should be an Operation";
  EXPECT_EQ(gt->getOpType(), vpiGtOp);
  ASSERT_NE(gt->getOperands(), nullptr);
  EXPECT_EQ(gt->getOperands()->size(), 2u) << "'>' is a binary operator";
  ExpectIntLiteral(getWithOperand(0, 1), "0");

  const hldb::Operation *const lt = any_cast<hldb::Operation>(items[1]);
  ASSERT_NE(lt, nullptr) << "'x < y + z' should be an Operation";
  EXPECT_EQ(lt->getOpType(), vpiLtOp) << "11.3.2: '+' binds tighter than '<', so '<' is the outer operator";
  ASSERT_NE(lt->getOperands(), nullptr);
  EXPECT_EQ(lt->getOperands()->size(), 2u) << "'<' is a binary operator";

  const hldb::Operation *const add = any_cast<hldb::Operation>(getWithOperand(1, 1));
  ASSERT_NE(add, nullptr) << "the right operand 'y + z' should be an Operation";
  EXPECT_EQ(add->getOpType(), vpiAddOp);
  ASSERT_NE(add->getOperands(), nullptr);
  EXPECT_EQ(add->getOperands()->size(), 2u) << "'+' is a binary operator";
}

// Expected to fail until HLC is fixed -- see KNOWN FAILURE (wrong scope for
// names in a 'with' block) in the file header.
TEST_F(InLineConstraintsRandomize1Test, BothXInWithBindToClassA1Property) {
  // 18.7: 'x' is found first in class a1, the class of the randomized object.
  ExpectBoundRef(getWithOperand(0, 0), "x", getClassA1X());
  ExpectBoundRef(getWithOperand(1, 0), "x", getClassA1X());

  const hldb::RefObj *const first = any_cast<hldb::RefObj>(getWithOperand(0, 0));
  const hldb::RefObj *const second = any_cast<hldb::RefObj>(getWithOperand(1, 0));
  ASSERT_NE(first, nullptr);
  ASSERT_NE(second, nullptr);
  EXPECT_NE(first->getActual(), getFormalVariable("x")) << "18.7: a1's 'x' hides the task formal 'x'";
  EXPECT_NE(second->getActual(), getFormalVariable("x")) << "18.7: a1's 'x' hides the task formal 'x'";
  EXPECT_NE(first->getActual(), getClassA2Property("x")) << "18.7: a1's 'x' hides class a2's 'x'";
  EXPECT_NE(second->getActual(), getClassA2Property("x")) << "18.7: a1's 'x' hides class a2's 'x'";
}

TEST_F(InLineConstraintsRandomize1Test, YInWithBindsToClassA2Property) {
  // 18.7: a1 declares no 'y', so it resolves where the call is written:
  // inside a method of a2, where 'y' is a2's property (8.5).
  ExpectBoundRef(getAddOperand(0), "y", getClassA2Property("y"));
}

TEST_F(InLineConstraintsRandomize1Test, ZInWithBindsToFormalZ) {
  // 18.7: a1 declares no 'z', so it resolves where the call is written: the
  // task's formal 'z' (37.15: the Variable holding it).
  ExpectBoundRef(getAddOperand(1), "z", getFormalVariable("z"));
}

// ---------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------

// The file is legal SystemVerilog, so no fatal, syntax or error diagnostic
// may point into it.
// Expected to fail -- see KNOWN FAILURE (errors reported in this file) in the
// file header.
TEST_F(InLineConstraintsRandomize1Test, NoErrorIsReportedInThisFile) {
  size_t count = 0;
  for (const Event &error : m_session->getEventListener()->getEvents()) {
    const EventDefinition *const info = hldb::findEventDefinition(hlc::getEventTable(), error.getType());
    if (info == nullptr || info->m_severity > EventSeverity::Error) continue;
    for (const EventArg &loc : error.getArguments()) {
      const std::string_view path = loc.m_file;
      if (path.find("18.7--in-line-constraints--randomize_1.sv") == std::string_view::npos) continue;
      ++count;
      ADD_FAILURE() << "error " << error.getType() << " reported at " << path << ":" << loc.m_line << ":"
                    << loc.m_column;
    }
  }
  EXPECT_EQ(count, 0u) << "the source is legal SystemVerilog";
}

// Expected to fail -- see KNOWN FAILURE (errors reported in this file) in the
// file header.
TEST_F(InLineConstraintsRandomize1Test, NoFatalSyntaxOrErrorDiagnostics) {
  const EventListener::Stats stats = m_session->getEventListener()->getStats();
  EXPECT_EQ(stats.m_fatals, 0);
  EXPECT_EQ(stats.m_syntaxes, 0);
  EXPECT_EQ(stats.m_errors, 0) << "the source is legal SystemVerilog and should compile without errors";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
