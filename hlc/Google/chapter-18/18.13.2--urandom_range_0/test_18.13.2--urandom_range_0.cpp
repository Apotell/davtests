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
// tests/Google/chapter-18/18.13.2--urandom_range_0.sv (lines 16-22; lines
// 1-14 are the license and the suite's :name:/:description:/:tags: comment):
//
//   class a;
//       function int do_urandom_range(int unsigned maxval, int unsigned minval);
//           int unsigned val;
//           val = $urandom_range(maxval, minval);
//           return val;
//       endfunction
//   endclass
//
// The point of the fixture is a call to the $urandom_range() system function
// (IEEE 1800-2023 18.13.2) from inside a class method: both arguments come
// from the method's formals, and the result is stored in a method-local
// variable that the method then returns. The class is declared outside any
// module or package, and nothing instantiates it.
//
// What is checked, and why:
//   class a (8.3)
//     - exactly one class named "a"; it is a user-defined class
//     - no 'extends' clause (8.13), no parameter port list (8.25), and
//       'endclass' carries no ': a' end label
//     - no properties: 'int unsigned val' sits inside the method, so it is a
//       method-local variable (13.4), not a class property (8.5)
//     - exactly one declared method, the Function do_urandom_range, with no
//       end label on 'endfunction'
//   function int do_urandom_range(...) (8.6, 13.4)
//     - flagged as a class method
//     - public, since no 'local' or 'protected' qualifier is written (8.18),
//       and not virtual (8.20)
//     - automatic: 8.6 makes the lifetime of every class method automatic
//     - return type 'int', an IntTypespec that is signed (6.11)
//   formals (13.4)
//     - exactly 2, in order: 'maxval' then 'minval'
//     - each is an input (no direction is written, so input is the
//       default) of type 'int unsigned', an IntTypespec that is NOT signed
//       (6.11.3)
//   int unsigned val; (6.21, 13.4)
//     - the only local the method declares
//     - IntTypespec, not signed; no initializer; automatic, because it is
//       declared inside an automatic method (6.21)
//   method body
//     - executes exactly 2 statements, in order: an Assignment and a
//       ReturnStmt
//   val = $urandom_range(maxval, minval); (10.4.1, 18.13.2)
//     - blocking Assignment whose LHS is bound to the local 'val'
//     - RHS is a SysFuncCall named "$urandom_range": it is a system
//       function and its value is used, so it is not a system task call
//     - exactly 2 arguments, in 18.13.2's prototype order: maxval, then
//       minval. Each is a RefObj bound by object identity to the method's
//       formal of the same name. Both are written, so the prototype's
//       default of 0 for minval is not used.
//   return val; (12.8, 13.4.1)
//     - ReturnStmt whose expression is a RefObj bound to the local 'val'
//   Diagnostics
//     - every identifier binds: no COMP_UNDEFINED_VARIABLE or
//       COMP_FAILED_TO_BIND for val, maxval or minval
//     - 'return val;' in a non-void function is legal: no
//       COMP_ILLEGAL_RETURN_VALUE for do_urandom_range
//     - the file is legal: zero fatal, syntax and error diagnostics
//
// Reduction and elaboration: nothing in this file reduces or elaborates.
// There is no parameter, no constant expression, no instance and no class
// specialization, and the call's arguments are formals, so no check is
// gated on getElaborated().
//
// KNOWN COMPILER BUG (class-method lifetime), not a defect in this test:
// IEEE 1800-2023 8.6 says "The lifetime of methods declared as part of a
// class type shall be automatic." HLC instead gives do_urandom_range the
// static default that 13.4.2 applies to subroutines declared outside a
// class, so Function::getAutomatic() returns false. The local 'val' takes
// its lifetime from the method (6.21), so Variable::getAutomatic() is false
// as well. getAutomatic() is the lifetime after defaults are applied (HLDB
// model gap #8), so false is a wrong value, not a different meaning of the
// flag. MethodHasAutomaticLifetime and
// ValIsAnUninitializedAutomaticUnsignedInt assert the LRM value and fail
// until HLC is fixed; they are intentionally not skipped or relaxed.
//
// What is NOT checked, and why:
//   - The value $urandom_range returns, that it lies within
//     [minval, maxval], the swap of the arguments when maxval is less than
//     minval, and the random stream the call draws from (18.13.2, 18.14)
//     only exist while simulation runs. Permanently out of scope; the
//     static half -- that the call is $urandom_range, with maxval then
//     minval bound to the method's formals -- is covered by
//     AssignmentRhsIsUrandomRangeSystemFunctionCall and
//     UrandomRangeArgumentsAreMaxvalThenMinval.
//   - Converting val's 'int unsigned' value to the 'int' return type happens
//     when the return executes. Permanently out of scope; the static half --
//     the declared return type and the returned expression -- is covered by
//     MethodReturnsSignedInt and ReturnYieldsVal.
//   - Whether HLC attaches a typespec for the call's 'int unsigned' result
//     (18.13.2) to the SysFuncCall node is a model-population convention.
//     ValIsAnUninitializedAutomaticUnsignedInt asserts the type the source
//     writes for the destination 'val' instead.
//   - That neither formal declares a default value: no test in this suite
//     reads a subroutine formal's default, so there is no grounded accessor
//     to assert it through. The argument count in
//     AssignmentRhsIsUrandomRangeSystemFunctionCall covers the call side.
//   - Whether an implicit constructor (8.7) appears in getMethods(), and
//     whether the implicit variable named after the function (13.4.1)
//     appears among its Variables, are tool conventions. Both are left out
//     of the counts, which cover only what the source declares.
//   - Which scope owns 'val' -- the Function, or the Begin wrapping its
//     body -- is a tool convention. Both are searched, and
//     MethodDeclaresOnlyValAsALocal checks there is exactly one.
//   - Whether the three 'int unsigned' declarations share one IntTypespec
//     node is a tool convention; each is checked for kind and signedness.
//   - The source writes no lifetime on the class itself. The lifetime rule
//     the LRM gives for class members is 8.6's rule for methods, which
//     MethodHasAutomaticLifetime asserts; ClassDefn::getAutomatic() is not
//     asserted.
//   - Design-wide counts of classes and modules depend on the .hlc file
//     list and on any built-in classes the tool preloads, so class 'a' is
//     found by name and checked for uniqueness by name.
//   - The warning count: a design that declares no module can draw a
//     tool-specific notice (for example about the missing top-level
//     module), so only the fatal, syntax and error counts are asserted.
//   - Source line numbers. A start line encodes nothing about
//     SystemVerilog semantics and changes whenever the fixture is
//     reformatted or a comment is added, so no node's location is asserted.
//   - The license and metadata comments are comments, not design objects.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/class_defn.h>
#include <hldb/design.h>
#include <hldb/extends.h>
#include <hldb/function.h>
#include <hldb/int_typespec.h>
#include <hldb/io_decl.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/return_stmt.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/sys_func_call.h>
#include <hldb/task_func.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <string>
#include <string_view>
#include <vector>

namespace hlc {

class UrandomRange0Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "18.13.2--urandom_range_0.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::ClassDefn *getClassA() {
    return hldb::findByName<hldb::ClassDefn>("a", m_design->getAllClasses());
  }

  static const hldb::Function *getDoUrandomRange() {
    const hldb::ClassDefn *const cls = getClassA();
    if (cls == nullptr) return nullptr;
    return hldb::findByName<hldb::Function>("do_urandom_range", cls->getMethods());
  }

  // The methods the source declares. 8.7 provides an implicit 'new' when a
  // class writes none; whether HLC materializes it in getMethods() is a
  // tool convention, so it is left out.
  static std::vector<const hldb::TaskFunc *> getDeclaredMethods() {
    std::vector<const hldb::TaskFunc *> methods;
    const hldb::ClassDefn *const cls = getClassA();
    if (cls == nullptr || cls->getMethods() == nullptr) return methods;
    for (const hldb::TaskFunc *const method : *cls->getMethods()) {
      if (method->getName() != "new") methods.emplace_back(method);
    }
    return methods;
  }

  static const hldb::IODecl *getFormal(std::string_view name) {
    const hldb::Function *const fn = getDoUrandomRange();
    if (fn == nullptr) return nullptr;
    return hldb::findByName<hldb::IODecl>(name, fn->getIODecls());
  }

  static const hldb::Begin *getBody() {
    const hldb::Function *const fn = getDoUrandomRange();
    if (fn == nullptr) return nullptr;
    return fn->getStmt<hldb::Begin>();
  }

  // 'int unsigned val;' may be owned by the Function itself or by the Begin
  // wrapping its body. Which scope owns it is a tool convention, so both are
  // searched.
  static const hldb::Variable *getVal() {
    const hldb::Function *const fn = getDoUrandomRange();
    if (fn == nullptr) return nullptr;
    if (const hldb::Variable *const val = hldb::findByName<hldb::Variable>("val", fn->getVariables())) return val;
    const hldb::Begin *const body = getBody();
    if (body == nullptr) return nullptr;
    return hldb::findByName<hldb::Variable>("val", body->getVariables());
  }

  // Names of the locals declared across both candidate scopes. 13.4.1 also
  // declares an implicit variable named after the function; whether HLC
  // materializes it is a tool convention, so it is left out.
  static std::vector<std::string> getDeclaredLocalNames() {
    std::vector<std::string> names;
    const hldb::Function *const fn = getDoUrandomRange();
    if (fn == nullptr) return names;
    if (fn->getVariables() != nullptr) {
      for (const hldb::Variable *const var : *fn->getVariables()) {
        if (var->getName() != fn->getName()) names.emplace_back(var->getName());
      }
    }
    const hldb::Begin *const body = getBody();
    if (body != nullptr && body->getVariables() != nullptr) {
      for (const hldb::Variable *const var : *body->getVariables()) {
        if (var->getName() != fn->getName()) names.emplace_back(var->getName());
      }
    }
    return names;
  }

  // The function body's statements with bare Variable declarations removed.
  static std::vector<const hldb::Any *> getExecutableStmts() {
    std::vector<const hldb::Any *> stmts;
    const hldb::Begin *const body = getBody();
    if (body == nullptr || body->getStmts() == nullptr) return stmts;
    for (const hldb::Any *const stmt : *body->getStmts()) {
      if (any_cast<hldb::Variable>(stmt) == nullptr) stmts.emplace_back(stmt);
    }
    return stmts;
  }

  static const hldb::Assignment *getAssign() {
    const std::vector<const hldb::Any *> stmts = getExecutableStmts();
    if (stmts.empty()) return nullptr;
    return any_cast<hldb::Assignment>(stmts[0]);
  }

  static const hldb::SysFuncCall *getCall() {
    const hldb::Assignment *const assign = getAssign();
    if (assign == nullptr) return nullptr;
    return assign->getRhs<hldb::SysFuncCall>();
  }

  static const hldb::ReturnStmt *getReturnStmt() {
    const std::vector<const hldb::Any *> stmts = getExecutableStmts();
    if (stmts.size() < 2) return nullptr;
    return any_cast<hldb::ReturnStmt>(stmts[1]);
  }

  // Verifies 'io' is the formal 'name', an input of type 'int unsigned'.
  static void ExpectUnsignedIntInput(const hldb::IODecl *io, std::string_view name) {
    ASSERT_NE(io, nullptr) << "formal '" << name << "' not found";
    EXPECT_EQ(io->getName(), name);
    EXPECT_EQ(io->getDirection(), vpiInput) << "13.4: '" << name << "' writes no direction, so it is an input";
    ASSERT_NE(io->getTypespec(), nullptr);
    const hldb::IntTypespec *const ts = io->getTypespec()->getActual<hldb::IntTypespec>();
    ASSERT_NE(ts, nullptr) << "'" << name << "' is declared 'int unsigned'";
    EXPECT_FALSE(ts->getSigned()) << "6.11.3: the 'unsigned' keyword makes '" << name << "' unsigned";
  }

  // Verifies 'expr' is a RefObj named 'name' bound by object identity to
  // 'target'.
  static void ExpectBoundRef(const hldb::Any *expr, std::string_view name, const hldb::Any *target) {
    const hldb::RefObj *const ref = any_cast<hldb::RefObj>(expr);
    ASSERT_NE(ref, nullptr) << "'" << name << "' should be a RefObj";
    EXPECT_EQ(ref->getName(), name);
    ASSERT_NE(target, nullptr) << "the declaration of '" << name << "' was not found";
    EXPECT_EQ(ref->getActual(), target) << "'" << name << "' must bind to its declaration";
  }
};

// ---------------------------------------------------------------------------
// class a; ... endclass
// ---------------------------------------------------------------------------

TEST_F(UrandomRange0Test, ClassAIsDeclaredOnceAsUserDefinedClass) {
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

TEST_F(UrandomRange0Test, ClassAHasNoBaseClassNoParametersAndNoEndLabel) {
  const hldb::ClassDefn *const cls = getClassA();
  ASSERT_NE(cls, nullptr);
  EXPECT_EQ(cls->getName(), "a");
  EXPECT_EQ(cls->getExtends(), nullptr) << "8.13: 'class a;' has no 'extends' clause";
  EXPECT_TRUE(cls->getParameters() == nullptr || cls->getParameters()->empty())
      << "8.25: 'class a;' has no parameter port list";
  EXPECT_EQ(cls->getEndLabelObj(), nullptr) << "'endclass' is written without ': a'";
}

TEST_F(UrandomRange0Test, ValIsMethodLocalNotAClassProperty) {
  const hldb::ClassDefn *const cls = getClassA();
  ASSERT_NE(cls, nullptr);
  const hldb::Variable *const val = getVal();
  ASSERT_NE(val, nullptr) << "'val' should be found in the method's scope";
  EXPECT_EQ(val->getName(), "val");
  EXPECT_TRUE(cls->getVariables() == nullptr || cls->getVariables()->empty())
      << "8.5: nothing is declared directly in the class body; 'val' is local to the method (13.4)";
}

TEST_F(UrandomRange0Test, ClassAHasExactlyOneDeclaredMethodDoUrandomRange) {
  const std::vector<const hldb::TaskFunc *> methods = getDeclaredMethods();
  ASSERT_EQ(methods.size(), 1u) << "the class body declares exactly one method";
  EXPECT_EQ(methods[0]->getName(), "do_urandom_range");
  const hldb::Function *const fn = any_cast<hldb::Function>(methods[0]);
  ASSERT_NE(fn, nullptr) << "13.4: declared with 'function', so it is a Function, not a Task";
  EXPECT_EQ(fn, getDoUrandomRange());
  EXPECT_EQ(fn->getEndLabelObj(), nullptr) << "'endfunction' is written without ': do_urandom_range'";
}

// ---------------------------------------------------------------------------
// function int do_urandom_range(int unsigned maxval, int unsigned minval);
// ---------------------------------------------------------------------------

TEST_F(UrandomRange0Test, MethodIsFlaggedAsAClassMethod) {
  const hldb::Function *const fn = getDoUrandomRange();
  ASSERT_NE(fn, nullptr);
  EXPECT_TRUE(fn->getMethod()) << "8.6: 'do_urandom_range' is declared in the body of class 'a'";
}

TEST_F(UrandomRange0Test, MethodIsPublicAndNotVirtual) {
  const hldb::Function *const fn = getDoUrandomRange();
  ASSERT_NE(fn, nullptr);
  EXPECT_EQ(fn->getVisibility(), vpiPublicVis) << "8.18: a member with no 'local' or 'protected' qualifier is public";
  EXPECT_FALSE(fn->getVirtual()) << "8.20: no 'virtual' qualifier is written";
}

// Expected to fail until HLC is fixed -- see KNOWN COMPILER BUG (class-method
// lifetime) in the file header.
TEST_F(UrandomRange0Test, MethodHasAutomaticLifetime) {
  const hldb::Function *const fn = getDoUrandomRange();
  ASSERT_NE(fn, nullptr);
  EXPECT_TRUE(fn->getAutomatic()) << "8.6: the lifetime of a method declared in a class is always automatic";
}

TEST_F(UrandomRange0Test, MethodReturnsSignedInt) {
  const hldb::Function *const fn = getDoUrandomRange();
  ASSERT_NE(fn, nullptr);
  ASSERT_NE(fn->getReturn(), nullptr) << "13.4.1: 'function int ...' declares a return type";
  const hldb::IntTypespec *const ts = fn->getReturn()->getActual<hldb::IntTypespec>();
  ASSERT_NE(ts, nullptr) << "the declared return type is 'int'";
  EXPECT_TRUE(ts->getSigned()) << "6.11: 'int' is a signed type";
}

TEST_F(UrandomRange0Test, MethodHasTwoFormalsMaxvalThenMinval) {
  const hldb::Function *const fn = getDoUrandomRange();
  ASSERT_NE(fn, nullptr);
  ASSERT_NE(fn->getIODecls(), nullptr);
  ASSERT_EQ(fn->getIODecls()->size(), 2u);
  EXPECT_EQ(fn->getIODecls()->at(0)->getName(), "maxval");
  EXPECT_EQ(fn->getIODecls()->at(1)->getName(), "minval");
}

TEST_F(UrandomRange0Test, MaxvalIsUnsignedIntInput) { ExpectUnsignedIntInput(getFormal("maxval"), "maxval"); }

TEST_F(UrandomRange0Test, MinvalIsUnsignedIntInput) { ExpectUnsignedIntInput(getFormal("minval"), "minval"); }

// ---------------------------------------------------------------------------
// int unsigned val;
// ---------------------------------------------------------------------------

TEST_F(UrandomRange0Test, MethodDeclaresOnlyValAsALocal) {
  ASSERT_NE(getDoUrandomRange(), nullptr);
  EXPECT_EQ(getDeclaredLocalNames(), std::vector<std::string>{"val"})
      << "'int unsigned val;' is the only declaration in the method body";
}

// Expected to fail until HLC is fixed -- see KNOWN COMPILER BUG (class-method
// lifetime) in the file header.
TEST_F(UrandomRange0Test, ValIsAnUninitializedAutomaticUnsignedInt) {
  const hldb::Variable *const val = getVal();
  ASSERT_NE(val, nullptr);
  ASSERT_NE(val->getTypespec(), nullptr);
  const hldb::IntTypespec *const ts = val->getTypespec()->getActual<hldb::IntTypespec>();
  ASSERT_NE(ts, nullptr) << "'val' is declared 'int unsigned'";
  EXPECT_FALSE(ts->getSigned()) << "6.11.3: the 'unsigned' keyword makes 'val' unsigned";
  EXPECT_EQ(val->getValue(), nullptr) << "'int unsigned val;' has no initializer";
  EXPECT_TRUE(val->getAutomatic()) << "6.21: a variable declared in an automatic method is automatic";
}

// ---------------------------------------------------------------------------
// Method body
// ---------------------------------------------------------------------------

TEST_F(UrandomRange0Test, BodyExecutesAssignmentThenReturn) {
  ASSERT_NE(getBody(), nullptr) << "a body with more than one statement should be wrapped in a Begin";
  const std::vector<const hldb::Any *> stmts = getExecutableStmts();
  ASSERT_EQ(stmts.size(), 2u) << "the body executes exactly two statements";
  EXPECT_NE(any_cast<hldb::Assignment>(stmts[0]), nullptr) << "statement 0 is 'val = $urandom_range(...);'";
  EXPECT_NE(any_cast<hldb::ReturnStmt>(stmts[1]), nullptr) << "statement 1 is 'return val;'";
}

// ---------------------------------------------------------------------------
// val = $urandom_range(maxval, minval);
// ---------------------------------------------------------------------------

TEST_F(UrandomRange0Test, AssignmentIsBlockingAndTargetsVal) {
  const hldb::Assignment *const assign = getAssign();
  ASSERT_NE(assign, nullptr);
  EXPECT_TRUE(assign->getBlocking()) << "10.4.1: '=' in a procedural context is a blocking assignment";
  ExpectBoundRef(assign->getLhs(), "val", getVal());
}

TEST_F(UrandomRange0Test, AssignmentRhsIsUrandomRangeSystemFunctionCall) {
  const hldb::Assignment *const assign = getAssign();
  ASSERT_NE(assign, nullptr);
  const hldb::SysFuncCall *const call = assign->getRhs<hldb::SysFuncCall>();
  ASSERT_NE(call, nullptr) << "18.13.2: $urandom_range is a system function and its value is assigned, so the "
                              "RHS is a SysFuncCall";
  EXPECT_EQ(call->getName(), "$urandom_range");
  ASSERT_NE(call->getArguments(), nullptr);
  EXPECT_EQ(call->getArguments()->size(), 2u)
      << "both maxval and minval are written, so the default of 0 for minval (18.13.2) is not used";
}

TEST_F(UrandomRange0Test, UrandomRangeArgumentsAreMaxvalThenMinval) {
  const hldb::SysFuncCall *const call = getCall();
  ASSERT_NE(call, nullptr);
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 2u);
  const hldb::NamedArgument *const arg0 = call->getArguments()->at(0);
  ASSERT_NE(arg0, nullptr);
  ExpectBoundRef(arg0->getHighConn(), "maxval", getFormal("maxval"));
  const hldb::NamedArgument *const arg1 = call->getArguments()->at(1);
  ASSERT_NE(arg1, nullptr);
  ExpectBoundRef(arg1->getHighConn(), "minval", getFormal("minval"));
}

// ---------------------------------------------------------------------------
// return val;
// ---------------------------------------------------------------------------

TEST_F(UrandomRange0Test, ReturnYieldsVal) {
  const hldb::ReturnStmt *const ret = getReturnStmt();
  ASSERT_NE(ret, nullptr);
  ExpectBoundRef(ret->getCondition(), "val", getVal());
}

// ---------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------

TEST_F(UrandomRange0Test, EveryIdentifierBinds) {
  for (std::string_view name : {"val", "maxval", "minval"}) {
    EXPECT_EQ(findError(ErrorDefinition::COMP_UNDEFINED_VARIABLE, name), nullptr)
        << "'" << name << "' is declared in the method, so it is not undefined";
    EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, name), nullptr)
        << "'" << name << "' is declared in the method, so it must bind";
  }
}

TEST_F(UrandomRange0Test, ReturnWithValueInNonVoidFunctionIsNotReported) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_ILLEGAL_RETURN_VALUE, "do_urandom_range"), nullptr)
      << "12.8, 13.4.1: a function declared to return 'int' returns with an expression";
}

TEST_F(UrandomRange0Test, NoFatalSyntaxOrErrorDiagnostics) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0) << "the file is legal SystemVerilog";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
