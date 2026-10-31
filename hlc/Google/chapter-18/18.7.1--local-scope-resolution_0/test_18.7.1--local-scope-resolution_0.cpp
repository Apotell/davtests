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
// tests/Google/chapter-18/18.7.1--local-scope-resolution_0.sv. Lines 1-14 are
// the license and the suite's :name:/:description:/:tags: comment; the rest
// is:
//
//   16  class a;
//   17      rand int x;
//   18  endclass
//   19
//   20  function int F(a obj, int x);
//   21      F = obj.randomize() with {x < local::x; };
//   22  endfunction
//
// The point of the fixture is the 'local::' qualifier in an inline
// constraint (IEEE 1800-2023 18.7 and 18.7.1). In 'obj.randomize() with
// { ... }', an unqualified name is searched for first in the class of the
// object being randomized (18.7), so the bare 'x' is class a's rand
// property 'x'. 'local::' makes the name bypass that class scope and resolve
// in the scope that contains the randomize() call (18.7.1), so 'local::x' is
// F's formal argument 'x'. The two 'x' references therefore bind to two
// different declarations.
//
// What is checked, and why:
//   class a (8.3)
//     - exactly one class named "a"; user-defined; not virtual (8.21); no
//       'extends' (8.13), no parameters (8.25), no end label
//     - declares no methods; randomize() is built in (18.6.1)
//     - 'rand int x;' (18.4): the only property; a signed int (6.11),
//       getIsRandomized() true, getRandType() vpiRand, no initializer,
//       public (8.18), not const (8.19)
//   function int F(a obj, int x); (13.4)
//     - exactly one compilation-unit function named "F" (3.12.1)
//     - return type 'int': a signed IntTypespec (13.4.1, 6.11)
//     - static lifetime: a subroutine declared outside a class, module,
//       program or interface with no lifetime keyword is static (13.4.2)
//     - formals, in order: 'obj' (input, a handle to class a) and 'x'
//       (input, signed int). A formal with no direction is an input (13.3).
//   F = obj.randomize() with {x < local::x; }; (13.4.1, 18.7)
//     - the function's only statement, so not wrapped in a Begin: a blocking
//       Assignment whose LHS is a RefObj named "F" (13.4.1: the function name
//       is an implicit variable holding the return value)
//     - its RHS is a hierarchical RefObj whose prefix 'obj' binds to the
//       Variable holding F's formal 'obj' (37.15), then a MethodFuncCall
//       "randomize" with no arguments and a 'with' inline constraint (18.7)
//     - the 'with' block holds exactly one constraint expression,
//       'x < local::x': a vpiLtOp
//     - its left operand 'x' binds to class a's property 'x' (18.7: names
//       are resolved first in the randomized object's class)
//     - its right operand 'local::x' binds to the Variable holding F's
//       formal 'x' (18.7.1, 37.15), not to class a's 'x'
//   Diagnostics
//     - no CompUndefinedVariable for 'obj' or 'x'
//     - no fatal, syntax or error diagnostic points into this file
//     - the whole compilation is legal: zero fatal, syntax and error
//       diagnostics
//
// Reduction and elaboration: nothing in this file reduces or elaborates.
// There is no parameter, no instance and no class specialization; every
// binding asserted is name resolution. No check is gated on getElaborated().
//
// KNOWN FAILURE (HLC cannot parse 'local::' in an inline constraint), not a
// defect in this test: HLC reports PaSyntaxError (5508) three times on
// line 21 -- at column 42 (the 'x' after 'local::') and column 44 (the ';'
// that ends the constraint item) -- although 'local::x' is legal in a
// randomize() with block (18.7.1; A.8.3 primary ::= ... [ local:: ]
// hierarchical_identifier). Because the statement does not parse, F's body
// is not built as 'F = obj.randomize() with {...};', and every check of it
// fails: FBodyAssignsToFunctionName, RhsIsObjRandomizeWithInlineConstraint,
// WithBlockHoldsOneItem, WithConstraintIsXLessThanLocalX,
// BareXBindsToClassAProperty, LocalXBindsToFormalOfF,
// NoErrorIsReportedInThisFile and NoFatalSyntaxOrErrorDiagnostics (3 syntax
// errors). They assert the LRM model and are expected to fail until HLC is
// fixed; they are intentionally not skipped or relaxed.
//
// What is NOT checked, and why:
//   - The value randomize() picks for obj.x, the value F returns (1 on
//     success, 18.6.1) and whether the constraint is satisfiable for a
//     given argument only exist while simulation runs. Permanently out of
//     scope. The static half is covered by WithConstraintIsXLessThanLocalX,
//     BareXBindsToClassAProperty and LocalXBindsToFormalOfF.
//   - How HLC wraps the 'with' block (an unnamed Constraint holding the
//     items, or the expression stored directly) is a model convention:
//     MethodFuncCall::getWith() is typed Any. Both shapes are accepted; the
//     single item, its operator and both bindings are asserted.
//   - The RefObj name HLC records for 'local::x' ("x" or "local::x") is a
//     model convention; its binding is what 18.7.1 fixes, so only a name
//     ending in "x" is required.
//   - What the LHS 'F' binds to (the Function or an implicit return
//     variable) is a model convention; 13.4.1 fixes only that assigning the
//     function name sets the return value.
//   - Line 21 has two ';': the one inside the braces ends the constraint
//     item 'x < local::x' (A.1.10), the one after '}' ends the assignment
//     statement. Neither is a design object of its own, so the 'with' block
//     is expected to hold exactly one item -- see WithBlockHoldsOneItem.
//   - Whether HLC binds the call to the built-in randomize() through
//     getTaskFunc() is a tool convention.
//   - Design-wide counts: built-in packages and classes are compiled with
//     the file, so class a and F are found by name.
//   - The .hlc also puts UVM on the include path, but the file includes and
//     imports nothing from it, so UVM plays no part here.
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
#include <hldb/class_defn.h>
#include <hldb/class_typespec.h>
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

class LocalScopeResolution0Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "18.7.1--local-scope-resolution_0.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::ClassDefn *getClassA() {
    return hldb::findByName<hldb::ClassDefn>("a", m_design->getAllClasses());
  }

  static const hldb::Variable *getClassAX() {
    const hldb::ClassDefn *const cls = getClassA();
    if (cls == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::Variable>("x", cls->getVariables());
  }

  static const hldb::Function *getF() {
    if (m_design->getTaskFuncs() == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::Function>("F", m_design->getTaskFuncs());
  }

  // 37.15: a reference to a formal resolves to the Variable holding it,
  // never to the IODecl.
  static const hldb::Variable *getFormalVariable(std::string_view name) {
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

  // The operation 'x < local::x', or nullptr.
  static const hldb::Operation *getWithLessThan() {
    const std::vector<const hldb::Any *> items = getWithItems();
    if (items.size() != 1) {
      return nullptr;
    }
    return any_cast<hldb::Operation>(items[0]);
  }

  static const hldb::Any *getWithOperand(size_t index) {
    const hldb::Operation *const op = getWithLessThan();
    if (op == nullptr || op->getOperands() == nullptr || op->getOperands()->size() <= index) {
      return nullptr;
    }
    return op->getOperands()->at(index);
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
// class a; rand int x; endclass
// ---------------------------------------------------------------------------

TEST_F(LocalScopeResolution0Test, ClassAIsDeclaredOnceAsUserDefinedClass) {
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

TEST_F(LocalScopeResolution0Test, ClassAHasNoBaseClassParametersMethodsOrEndLabel) {
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

TEST_F(LocalScopeResolution0Test, ClassAHasOneRandIntPropertyX) {
  const hldb::ClassDefn *const cls = getClassA();
  ASSERT_NE(cls, nullptr);
  ASSERT_NE(cls->getVariables(), nullptr);
  ASSERT_EQ(cls->getVariables()->size(), 1u) << "'rand int x;' is the only property";
  const hldb::Variable *const x = getClassAX();
  ASSERT_NE(x, nullptr);
  EXPECT_EQ(cls->getVariables()->at(0), x);
  ExpectSignedInt(x->getTypespec(), "class a's 'x'");
  EXPECT_TRUE(x->getIsRandomized()) << "18.4: 'x' is declared with the 'rand' qualifier";
  EXPECT_EQ(x->getRandType(), vpiRand) << "18.4.1: 'rand', not 'randc' (18.4.2)";
  EXPECT_EQ(x->getValue(), nullptr) << "'rand int x;' has no initializer";
  EXPECT_EQ(x->getVisibility(), vpiPublicVis) << "8.18: 'x' has no 'local'/'protected' qualifier";
  EXPECT_FALSE(x->getConstantVariable()) << "8.19: 'x' has no 'const' qualifier";
}

// ---------------------------------------------------------------------------
// function int F(a obj, int x); ... endfunction
// ---------------------------------------------------------------------------

TEST_F(LocalScopeResolution0Test, FIsCompilationUnitFunctionReturningInt) {
  ASSERT_NE(m_design->getTaskFuncs(), nullptr) << "3.12.1: F is declared in the compilation unit";
  size_t count = 0;
  for (const hldb::TaskFunc *const tf : *m_design->getTaskFuncs()) {
    if (tf->getName() == "F") ++count;
  }
  EXPECT_EQ(count, 1u) << "the source declares function 'F' exactly once";
  const hldb::Function *const fn = getF();
  ASSERT_NE(fn, nullptr) << "13.4: 'function int F(...)' is a Function";
  ExpectSignedInt(fn->getReturn(), "F's return type");
}

TEST_F(LocalScopeResolution0Test, FHasStaticLifetime) {
  const hldb::Function *const fn = getF();
  ASSERT_NE(fn, nullptr);
  EXPECT_FALSE(fn->getAutomatic())
      << "13.4.2: a subroutine outside a class with no lifetime keyword has static lifetime";
}

TEST_F(LocalScopeResolution0Test, FHasFormalsObjAndX) {
  const hldb::Function *const fn = getF();
  ASSERT_NE(fn, nullptr);
  ASSERT_NE(fn->getIODecls(), nullptr);
  ASSERT_EQ(fn->getIODecls()->size(), 2u) << "'a obj' and 'int x'";

  const hldb::IODecl *const obj = fn->getIODecls()->at(0);
  ASSERT_NE(obj, nullptr);
  EXPECT_EQ(obj->getName(), "obj");
  EXPECT_EQ(obj->getDirection(), vpiInput) << "13.3: a formal with no direction is an input";
  ExpectClassHandle(obj->getTypespec(), getClassA(), "'obj'");

  const hldb::IODecl *const x = fn->getIODecls()->at(1);
  ASSERT_NE(x, nullptr);
  EXPECT_EQ(x->getName(), "x");
  EXPECT_EQ(x->getDirection(), vpiInput) << "13.3: a formal with no direction is an input";
  ExpectSignedInt(x->getTypespec(), "formal 'x'");
}

// ---------------------------------------------------------------------------
// F = obj.randomize() with {x < local::x; };
// ---------------------------------------------------------------------------

// Expected to fail until HLC is fixed -- see KNOWN FAILURE (HLC cannot parse
// 'local::' in an inline constraint) in the file header.
TEST_F(LocalScopeResolution0Test, FBodyAssignsToFunctionName) {
  const hldb::Assignment *const assign = getFBody();
  ASSERT_NE(assign, nullptr) << "the body is a single statement, so it is not wrapped in a Begin";
  EXPECT_TRUE(assign->getBlocking()) << "10.4.1: '=' in a procedural context is a blocking assignment";
  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr) << "the LHS is a reference";
  EXPECT_EQ(lhs->getName(), "F") << "13.4.1: assigning the function name sets the return value";
}

// Expected to fail until HLC is fixed -- see KNOWN FAILURE (HLC cannot parse
// 'local::' in an inline constraint) in the file header.
TEST_F(LocalScopeResolution0Test, RhsIsObjRandomizeWithInlineConstraint) {
  const hldb::Assignment *const assign = getFBody();
  ASSERT_NE(assign, nullptr);
  const hldb::RefObj *const path = assign->getRhs<hldb::RefObj>();
  ASSERT_NE(path, nullptr) << "'obj.randomize() with {...}' should be a hierarchical RefObj";
  ASSERT_NE(path->getPathElems(), nullptr);
  ASSERT_EQ(path->getPathElems()->size(), 2u);
  const hldb::RefObj *const obj = any_cast<hldb::RefObj>(path->getPathElems()->at(0));
  ASSERT_NE(obj, nullptr);
  EXPECT_EQ(obj->getName(), "obj");
  ASSERT_NE(getFormalVariable("obj"), nullptr);
  EXPECT_EQ(obj->getActual(), getFormalVariable("obj")) << "37.15: 'obj' binds to the Variable holding F's formal";

  const hldb::MethodFuncCall *const call = getRandomizeCall();
  ASSERT_NE(call, nullptr) << "18.6.1: randomize() is a function called through a handle, so a MethodFuncCall";
  EXPECT_EQ(call->getName(), "randomize");
  EXPECT_TRUE(call->getArguments() == nullptr || call->getArguments()->empty())
      << "'randomize()' is written with no arguments";
  EXPECT_NE(call->getWith(), nullptr) << "18.7: the call carries a 'with' inline constraint";
}

// Expected to fail until HLC is fixed -- see KNOWN FAILURE (HLC cannot parse
// 'local::' in an inline constraint) in the file header.
TEST_F(LocalScopeResolution0Test, WithBlockHoldsOneItem) {
  ASSERT_NE(getRandomizeCall(), nullptr);
  EXPECT_EQ(getWithItems().size(), 1u) << "18.7: '{x < local::x; }' holds one constraint expression";
}

// Expected to fail until HLC is fixed -- see KNOWN FAILURE (HLC cannot parse
// 'local::' in an inline constraint) in the file header.
TEST_F(LocalScopeResolution0Test, WithConstraintIsXLessThanLocalX) {
  const hldb::Operation *const op = getWithLessThan();
  ASSERT_NE(op, nullptr) << "'x < local::x' should be an Operation";
  EXPECT_EQ(op->getOpType(), vpiLtOp) << "11.4.4: '<' is a relational operator";
  ASSERT_NE(op->getOperands(), nullptr);
  EXPECT_EQ(op->getOperands()->size(), 2u) << "'<' is a binary operation";
}

// Expected to fail until HLC is fixed -- see KNOWN FAILURE (HLC cannot parse
// 'local::' in an inline constraint) in the file header.
TEST_F(LocalScopeResolution0Test, BareXBindsToClassAProperty) {
  const hldb::RefObj *const ref = any_cast<hldb::RefObj>(getWithOperand(0));
  ASSERT_NE(ref, nullptr) << "the left operand 'x' should be a RefObj";
  EXPECT_EQ(ref->getName(), "x");
  ASSERT_NE(getClassAX(), nullptr);
  EXPECT_EQ(ref->getActual(), getClassAX())
      << "18.7: in 'obj.randomize() with', names resolve first in the randomized object's class";
}

// Expected to fail until HLC is fixed -- see KNOWN FAILURE (HLC cannot parse
// 'local::' in an inline constraint) in the file header.
TEST_F(LocalScopeResolution0Test, LocalXBindsToFormalOfF) {
  const hldb::RefObj *const ref = any_cast<hldb::RefObj>(getWithOperand(1));
  ASSERT_NE(ref, nullptr) << "the right operand 'local::x' should be a RefObj";
  const std::string_view name = ref->getName();
  ASSERT_FALSE(name.empty());
  EXPECT_EQ(name.back(), 'x') << "the reference names 'x' (recorded as \"" << name << "\")";
  ASSERT_NE(getFormalVariable("x"), nullptr);
  EXPECT_EQ(ref->getActual(), getFormalVariable("x"))
      << "18.7.1: 'local::' bypasses class a's scope, so 'local::x' is F's formal 'x' (37.15)";
  EXPECT_NE(ref->getActual(), getClassAX()) << "18.7.1: 'local::x' must not resolve to class a's 'x'";
}

// ---------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------

// The file is legal SystemVerilog, so no fatal, syntax or error diagnostic
// may point into it.
// Expected to fail until HLC is fixed -- see KNOWN FAILURE (HLC cannot parse
// 'local::' in an inline constraint) in the file header.
TEST_F(LocalScopeResolution0Test, NoErrorIsReportedInThisFile) {
  size_t count = 0;
  for (const Event &error : m_session->getEventListener()->getEvents()) {
    const EventDefinition *const info = hldb::findEventDefinition(hlc::getEventTable(), error.getType());
    if (info == nullptr || info->m_severity > EventSeverity::Error) continue;
    for (const EventArg &loc : error.getArguments()) {
      const std::string_view path = loc.m_file;
      if (path.find("18.7.1--local-scope-resolution_0.sv") == std::string_view::npos) continue;
      ++count;
      ADD_FAILURE() << "error " << error.getType() << " reported at " << path << ":" << loc.m_line << ":"
                    << loc.m_column;
    }
  }
  EXPECT_EQ(count, 0u) << "the source is legal SystemVerilog";
}

// Expected to fail until HLC is fixed -- see KNOWN FAILURE (HLC cannot parse
// 'local::' in an inline constraint) in the file header.
TEST_F(LocalScopeResolution0Test, NoFatalSyntaxOrErrorDiagnostics) {
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
