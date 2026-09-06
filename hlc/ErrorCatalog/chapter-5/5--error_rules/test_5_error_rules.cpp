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

// Tests for the IEEE 1800-2023 Clause 5 error scenarios catalogued in the SV
// error catalog (filtered set), rows 5-7.
//
// Scope: as with the Chapter 3 catalog tests (see test_3_error_rules.cpp),
// this file asserts ONLY that the diagnostic each catalog row requires is
// emitted. It makes no assertion about the shape of the compiled model.
//
// Fixture: 5--error_rules.sv (rows 5, 6, 7). All three scenarios parse and
// compile cleanly in one file.
//
// Policy: no GTEST_SKIP() anywhere in this file -- every TEST_F runs
// unconditionally; a row with no diagnostic implemented is marked "EXPECTED
// TO FAIL" above its assertion rather than skipped. All three rows are now
// wired in ModelBuilder.cpp (reportUntypedAssignmentPattern(),
// reportAssignmentPatternShapeMismatch(), reportMissingRecursiveCallParentheses())
// and pass.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

namespace hlc {

class Chapter5ErrorRulesTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "5--error_rules.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// --- row 5: untyped assignment pattern with no context (5.10) --------------

TEST_F(Chapter5ErrorRulesTest, Row5_UntypedAssignmentPatternWithNoContextIsRejected) {
  // catalog row 5 | 5.10 | COMP
  // "An assignment pattern expression ... shall have a type ... If the
  // assignment pattern is not self-determined, it shall get its type from
  // the context." r5_m's '$display('{0, 0.0})' supplies neither a type
  // prefix nor an assignment-like context (a system task argument is never
  // one -- IEEE 1800-2023 10.8 lists assignment-like contexts, and a system
  // task/function's arguments are not among them, unlike a user-defined
  // function/task's typed formals).
  //
  // FIXED 2026-08-28: wired in ModelBuilder::reportUntypedAssignmentPattern()
  // (ModelBuilder.cpp), a post-pass flagging any vpiAssignmentPatternOp
  // Operation whose parent is a SysFuncCall/SysTaskCall. Scoped to system
  // calls only, and does not yet detect an explicit type prefix on the
  // pattern itself (Phase2ModelBuilder doesn't attach one) -- see the
  // function's own comment for the (currently undemonstrated) false-positive
  // risk that leaves open.
  EXPECT_NE(findError(ErrorDefinition::COMP_UNTYPED_ASSIGNMENT_PATTERN, "r5_m"), nullptr)
      << "an untyped assignment pattern needs a type prefix or an assignment-like context "
         "(IEEE 1800-2023 5.10)";
}

// --- row 6: flattened C-style structure literal (5.10) ----------------------

TEST_F(Chapter5ErrorRulesTest, Row6_FlattenedStructureLiteralShapeIsRejected) {
  // catalog row 6 | 5.10 | COMP
  // "Nested assignment patterns are required to reflect the structure of the
  // aggregate type being assigned ... a C-like initial value assignment ...
  // is not allowed." r6_m's '{1, 1.0, 2, 2.0}' flattens two r6_ab structs
  // into one brace level instead of nesting '{...}, {...}'.
  //
  // FIXED 2026-08-28: wired in ModelBuilder::reportAssignmentPatternShapeMismatch()
  // (ModelBuilder.cpp), a post-pass over every Variable declared as an
  // unpacked array of a struct/union type, comparing a flat initializer
  // pattern's top-level operand count against the array's declared size.
  // Scoped to that one canonical shape (array-of-struct with the wrong
  // top-level count); does not attempt a fully general recursive
  // nesting-shape check.
  EXPECT_NE(findError(ErrorDefinition::COMP_ASSIGNMENT_PATTERN_SHAPE_MISMATCH, "r6_abarr"), nullptr)
      << "a structure literal's braces must reflect the structure's shape (IEEE 1800-2023 5.10)";
}

// --- row 7: recursive nonvoid class method call omitting parentheses (5.13) -

TEST_F(Chapter5ErrorRulesTest, Row7_RecursiveClassMethodCallRequiresCallParentheses) {
  // catalog row 7 | 5.13 | COMP
  // 5.13: "empty parentheses ... following the subroutine name are optional"
  // in general. 13.5.5 carves out the actual exception this row names: "It
  // shall be illegal to omit the parentheses in a directly recursive nonvoid
  // class function method call that is not hierarchically qualified." A bare
  // reference to the function's own name is always its implicit return-value
  // variable (13.4.1), never a call. r7_c::r7_f's 'r7_f = n * r7_f' (line 48)
  // omits the call parentheses on its own recursive invocation.
  //
  // (An earlier version of this fixture used 'sequence.triggered' without
  // parentheses, reasoning from 5.13's general wording -- verified against
  // Clause 16 and reverted: every worked example there writes .triggered
  // without parentheses as the normal, legal form, so that was not actually
  // an instance of this rule. See 5--error_rules.sv's row-7 comment.)
  //
  // FIXED 2026-08-28: wired in ModelBuilder::reportMissingRecursiveCallParentheses()
  // (ModelBuilder.cpp), a post-pass over every non-void class method that
  // walks its body (via the generic hldb::Visitor traversal) for a bare
  // RefObj matching the method's own name that is not the direct LHS of a
  // plain assignment -- the one sanctioned way to read/write the implicit
  // return-value variable (13.4.1); any other bare occurrence is presumed to
  // be an attempted recursive call missing its "()".
  EXPECT_NE(findError(ErrorDefinition::COMP_MISSING_CALL_PARENTHESES, "r7_f"), nullptr)
      << "a directly recursive nonvoid class method call requires call parentheses "
         "(IEEE 1800-2023 5.13, 13.5.5)";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
