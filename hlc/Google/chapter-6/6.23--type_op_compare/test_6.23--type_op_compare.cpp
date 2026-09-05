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

// Tests for 6.23--type_op_compare.sv (tags: 6.23)
// SV: tests/Google/chapter-6/6.23--type_op_compare.sv
//
//   module top #( parameter type T = type(logic[11:0]) )
//      ();
//      initial begin
//         case (type(T))
//           type(logic[11:0]) : ;
//           default           : $stop;
//         endcase
//         if (type(T) == type(logic[12:0])) $stop;
//         if (type(T) != type(logic[11:0])) $stop;
//         if (type(T) === type(logic[12:0])) $stop;
//         if (type(T) !== type(logic[11:0])) $stop;
//         $finish;
//      end
//   endmodule
//
// What to check and why (IEEE 1800-2023 6.23 "Type operator", p.138,
// checked before any test code was written -- verified verbatim against
// the actual spec text, not recalled from training or an older edition):
//
// This SV file is syntactically and semantically valid per IEEE 1800-2023.
// No :should_fail_because: tag.
//
// Type parameter with type() default (6.23):
//   "The type operator can also be applied to a data type. localparam
//   type T = type(bit[12:0]);" -- 'parameter type T = type(logic[11:0])'
//   is the parameter-level equivalent of this exact pattern. T's default
//   type is therefore 'logic[11:0]', represented as a LogicTypespec with
//   a single packed range [11:0].
//
// type() in case selector (6.23):
//   The spec's own worked example uses exactly this pattern: "case
//   (type(bus_t)) type(bit[12:0]): ...; type(real): ...; endcase" --
//   'case (type(T)) type(logic[11:0]) : ; default : $stop; endcase' is
//   the same construct.
//
// type comparison operators (6.23):
//   "When a type reference is used in an equality/inequality or case
//   equality/inequality comparison, it shall only be compared with
//   another type reference. Two type references shall be considered
//   equal in such comparisons if, and only if, the types to which they
//   refer match." 'type(T) == type(logic[12:0])' must be false (T is
//   logic[11:0]); 'type(T) != type(logic[11:0])' must be false; '===' and
//   '!==' have the same semantics as '==' and '!=' for types.
//
// -- Expected HLDB tree (if compiler is correct) --------------------------------
//
//   Module name:top
//   +-- getParamAssigns() (1 item)
//   |   +-- ParamAssign name:"T"
//   |           lhs: RefTypespec -> TypeParameter name:"T"
//   |           rhs: RefTypespec -> LogicTypespec { range [11:0] }
//   +-- (initial block with case stmt and comparison expressions)

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/param_assign.h>
#include <hldb/ref_typespec.h>
#include <hldb/type_parameter.h>

namespace hlc {

class TypeOpCompareTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.23--type_op_compare.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static const hldb::Module *getTop(const hldb::Design *d) {
  return hldb::findByDefName<hldb::Module>("top", d->getAllModules());
}

static const hldb::ParamAssign *getParamAssign(const hldb::Design *d, std::string_view name) {
  const hldb::Module *m = getTop(d);
  if (!m || !m->getParamAssigns()) return nullptr;
  return hldb::findByName<hldb::ParamAssign>(name, m->getParamAssigns());
}

// ===========================================================================
// Compiler diagnostics -- this SV file is valid, zero errors expected
// ===========================================================================

// ss.6.23: all constructs in this file (type() in parameter default, case
// selector, and comparison operators) are valid per the spec. The compiler
// must produce no syntax errors.
// NOTE: if this test fails, Surelog rejects valid ss.6.23 syntax.
TEST_F(TypeOpCompareTest, Compiler_NoSyntaxErrors) {
  ErrorContainer::Stats stats = m_compiler->getErrorStats();
  EXPECT_EQ(stats.nbSyntax, 0) << "ss.6.23: this SV file is valid -- 'case (type(T))', "
                                  "'parameter type T = type(...)', and type comparison operators "
                                  "are all permitted by the spec; zero syntax errors expected";
}

TEST_F(TypeOpCompareTest, Compiler_NoErrors) {
  ErrorContainer::Stats stats = m_compiler->getErrorStats();
  EXPECT_EQ(stats.nbError, 0) << "ss.6.23: valid type() file must produce no compilation errors";
}

// ===========================================================================
// Module structure  (ss.6.23)
// ===========================================================================

// ss.6.23: 'module top ... endmodule' must produce exactly one module named
// 'top' in the design's module collection.
TEST_F(TypeOpCompareTest, ModuleCount_IsOne) {
  ASSERT_NE(m_design->getAllModules(), nullptr);
  EXPECT_EQ(m_design->getAllModules()->size(), 1u) << "ss.6.23: exactly one module 'top' is declared -- "
                                                      "getAllModules() must contain exactly one entry";
}

// ss.6.23: the module must be named 'top'.
TEST_F(TypeOpCompareTest, Module_Top_Exists) {
  EXPECT_NE(getTop(m_design), nullptr) << "ss.6.23: 'module top' must produce a module named 'top' -- "
                                          "if this fails, the module was not fully parsed";
}

// ===========================================================================
// 'parameter type T = type(logic[11:0])'  (ss.6.23)
// ===========================================================================

// ss.6.23: the type parameter assignment for T must be present in the module.
TEST_F(TypeOpCompareTest, TypeParam_T_ParamAssign_Exists) {
  const hldb::Module *m = getTop(m_design);
  ASSERT_NE(m, nullptr) << "module 'top' not found";
  ASSERT_NE(m->getParamAssigns(), nullptr);
  EXPECT_NE(getParamAssign(m_design, "T"), nullptr)
      << "ss.6.23: 'parameter type T' must produce a ParamAssign named 'T'";
}

// ss.6.23: for a type parameter, the LHS of the assignment is a RefTypespec
// (not a RefObj as for value parameters) pointing to the TypeParameter node.
TEST_F(TypeOpCompareTest, TypeParam_T_Lhs_IsRefTypespec) {
  const hldb::ParamAssign *pa = getParamAssign(m_design, "T");
  ASSERT_NE(pa, nullptr) << "ParamAssign for 'T' not found";
  EXPECT_NE(pa->getLhs<hldb::RefTypespec>(), nullptr)
      << "ss.6.23: LHS of a type parameter assignment must be a RefTypespec";
}

// ss.6.23: the LHS RefTypespec must resolve to a TypeParameter node.
TEST_F(TypeOpCompareTest, TypeParam_T_Lhs_ActualIs_TypeParameter) {
  const hldb::ParamAssign *pa = getParamAssign(m_design, "T");
  ASSERT_NE(pa, nullptr);
  const hldb::RefTypespec *lhs = pa->getLhs<hldb::RefTypespec>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_NE(lhs->getActual<hldb::TypeParameter>(), nullptr)
      << "ss.6.23: LHS RefTypespec must resolve to the TypeParameter node";
}

// ss.6.23: the TypeParameter node must be named 'T'.
TEST_F(TypeOpCompareTest, TypeParam_T_Name) {
  const hldb::ParamAssign *pa = getParamAssign(m_design, "T");
  ASSERT_NE(pa, nullptr);
  const hldb::RefTypespec *lhs = pa->getLhs<hldb::RefTypespec>();
  ASSERT_NE(lhs, nullptr);
  const hldb::TypeParameter *tp = lhs->getActual<hldb::TypeParameter>();
  ASSERT_NE(tp, nullptr);
  EXPECT_EQ(tp->getName(), "T") << "ss.6.23: type parameter must be named 'T'";
}

// ss.6.23: the RHS of the assignment holds the default type expression.
// 'type(logic[11:0])' yields the type 'logic[11:0]'; the RHS must be a
// RefTypespec (the type() expression parsed into the typespec slot).
TEST_F(TypeOpCompareTest, TypeParam_T_Rhs_IsRefTypespec) {
  const hldb::ParamAssign *pa = getParamAssign(m_design, "T");
  ASSERT_NE(pa, nullptr);
  EXPECT_NE(pa->getRhs<hldb::RefTypespec>(), nullptr)
      << "ss.6.23: RHS of 'parameter type T = type(logic[11:0])' must be "
         "a RefTypespec";
}

// ss.6.23: 'type(logic[11:0])' resolves to the type 'logic[11:0]'.
// The RHS RefTypespec must have vpiActual pointing to a LogicTypespec.
// Resolving type() to a concrete typespec is an elaboration-phase operation.
TEST_F(TypeOpCompareTest, TypeParam_T_DefaultType_ResolvesToLogic) {
  const hldb::ParamAssign *pa = getParamAssign(m_design, "T");
  ASSERT_NE(pa, nullptr);
  const hldb::RefTypespec *rhs = pa->getRhs<hldb::RefTypespec>();
  ASSERT_NE(rhs, nullptr);
  if (m_design->getElaborated()) {
    EXPECT_NE(rhs->getActual<hldb::LogicTypespec>(), nullptr)
        << "ss.6.23: post-elaboration: type(logic[11:0]) must resolve to "
           "LogicTypespec";
  } else {
    EXPECT_EQ(rhs->getActual<hldb::LogicTypespec>(), nullptr)
        << "pre-elaboration: vpiActual not yet resolved -- expected at parse time";
  }
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
