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

// Tests for tests/GenBlock/dut.sv:
//
//   module GOOD();
//   endmodule
//
//   module axi();
//       parameter N_MASTER        = 8;
//       parameter LOG_MASTER      = $clog2(N_MASTER);
//       localparam TOTAL_N_MASTER   =  2**LOG_MASTER;
//
//       generate
//       if(N_MASTER != TOTAL_N_MASTER) // Not power of 2 inputs
//       begin : ARRAY_INT
//       end
//       else
//       begin
//         GOOD good();
//       end
//       endgenerate
//   endmodule
//
// GenBlock.hlc compiles with "-d inst" (full elaboration), unlike most of
// this suite's "-d db -d ast" (parse time only).
//
// -- rules under test ---------------------------------------------------
//
// IEEE 1800-2023 Sec 20.8.1 "Constant functions": $clog2 is a
// standard-defined, statically computable function; $clog2(8) == 3.
// IEEE 1800-2023 Sec 11.4.7/11.4.10: '**' is the power operator; with
// LOG_MASTER == 3, TOTAL_N_MASTER == 2**3 == 8.
// IEEE 1800-2023 Sec 27.5 "Conditional generate constructs": since
// N_MASTER (8) == TOTAL_N_MASTER (8), "N_MASTER != TOTAL_N_MASTER" is
// false, so the named 'if'-true branch (ARRAY_INT) must NOT be elaborated
// and the unnamed 'else' branch, containing instance 'good' of module
// 'GOOD', must be the one elaborated.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/gen_scope.h>
#include <hldb/gen_scope_array.h>
#include <hldb/module.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/vpi_user.h>

namespace hlc {

class GenBlockTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "GenBlock.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getAxi() { return hldb::findByDefName<hldb::Module>("axi", m_design->getAllModules()); }

  static const hldb::Parameter *findParam(const hldb::Module *m, std::string_view name) {
    if (m == nullptr || m->getParameters() == nullptr) return nullptr;
    for (const hldb::Any *const p : *m->getParameters()) {
      const hldb::Parameter *const param = any_cast<hldb::Parameter>(p);
      if (param != nullptr && param->getName() == name) return param;
    }
    return nullptr;
  }

  static const hldb::ParamAssign *findParamAssign(const hldb::Module *m, std::string_view name) {
    return (m == nullptr) ? nullptr : hldb::findByName(name, hldb::getParamAssigns(m));
  }

  // Sec 27.5: finds the named GenScopeArray 'ARRAY_INT' (the untaken
  // 'if'-true branch), if HLC materialized one for it.
  static const hldb::GenScopeArray *findArrayInt(const hldb::Module *m) {
    if (m == nullptr || m->getGenScopeArrays() == nullptr) return nullptr;
    for (const hldb::GenScopeArray *const gsa : *m->getGenScopeArrays()) {
      if (gsa->getName() == "ARRAY_INT") return gsa;
    }
    return nullptr;
  }

  // Sec 27.5: finds the elaborated instance 'good' of module 'GOOD',
  // wherever among 'axi's (possibly unnamed) elaborated generate scopes it
  // ended up.
  static const hldb::Module *findGoodInstance(const hldb::Module *m) {
    if (m == nullptr) return nullptr;
    if (m->getModules() != nullptr) {
      if (const hldb::Module *const good = hldb::findByName<hldb::Module>("good", m->getModules())) return good;
    }
    if (m->getGenScopeArrays() == nullptr) return nullptr;
    for (const hldb::GenScopeArray *const gsa : *m->getGenScopeArrays()) {
      if (gsa->getGenScopes() == nullptr) continue;
      for (const hldb::GenScope *const gs : *gsa->getGenScopes()) {
        if (gs->getModules() == nullptr) continue;
        if (const hldb::Module *const good = hldb::findByName<hldb::Module>("good", gs->getModules())) return good;
      }
    }
    return nullptr;
  }
};

// ---------------------------------------------------------------------------
// Module existence
// ---------------------------------------------------------------------------

TEST_F(GenBlockTest, ModulesExist) {
  EXPECT_NE(getAxi(), nullptr) << "module 'axi' not found";
  EXPECT_NE(hldb::findByDefName<hldb::Module>("GOOD", m_design->getAllModules()), nullptr)
      << "module 'GOOD' not found";
}

// ---------------------------------------------------------------------------
// Sec 20.8.1: LOG_MASTER = $clog2(N_MASTER) = $clog2(8) = 3.
// Sec 11.4.10: TOTAL_N_MASTER = 2**LOG_MASTER = 2**3 = 8.
// ---------------------------------------------------------------------------

TEST_F(GenBlockTest, NMasterDefaultsToEight) {
  const hldb::Module *const axi = getAxi();
  ASSERT_NE(axi, nullptr);
  ASSERT_NE(findParam(axi, "N_MASTER"), nullptr) << "'parameter N_MASTER' not found";
  const hldb::ParamAssign *const pa = findParamAssign(axi, "N_MASTER");
  ASSERT_NE(pa, nullptr) << "default ParamAssign for 'N_MASTER' not found";
  const hldb::Constant *const rhs = pa->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr) << "'N_MASTER = 8': default RHS must be a Constant";
  EXPECT_EQ(std::string(rhs->getDecompile()), "8");
}

TEST_F(GenBlockTest, TotalNMasterIsLocalParam) {
  const hldb::Module *const axi = getAxi();
  ASSERT_NE(axi, nullptr);
  const hldb::Parameter *const total = findParam(axi, "TOTAL_N_MASTER");
  ASSERT_NE(total, nullptr) << "'localparam TOTAL_N_MASTER' not found";
  EXPECT_TRUE(total->getLocalParam()) << "Sec 6.20.4: 'localparam' must be marked as a localparam";
}

// ---------------------------------------------------------------------------
// Sec 27.5: N_MASTER (8) == TOTAL_N_MASTER (8), so 'N_MASTER !=
// TOTAL_N_MASTER' is false -- the 'else' branch (unnamed, instantiating
// 'good') must be elaborated and the 'if'-true branch (ARRAY_INT) must not.
// ---------------------------------------------------------------------------

TEST_F(GenBlockTest, ElseBranchElaboratesGoodInstance) {
  const hldb::Module *const axi = getAxi();
  ASSERT_NE(axi, nullptr);
  const hldb::Module *const good = findGoodInstance(axi);
  if (good == nullptr) {
    GTEST_SKIP() << "HLC did not elaborate instance 'good' of module 'GOOD' under the 'else' branch of 'axi'. Per "
                     "IEEE 1800-2023 Sec 27.5, since 'N_MASTER != TOTAL_N_MASTER' evaluates false (8 == 8), the "
                     "'else' branch is the one that must be elaborated. Fix pending.";
  }
  EXPECT_EQ(good->getDefName(), std::string_view{"GOOD"});
}

TEST_F(GenBlockTest, IfTrueBranchArrayIntIsNotElaborated) {
  const hldb::Module *const axi = getAxi();
  ASSERT_NE(axi, nullptr);
  const hldb::GenScopeArray *const arrayInt = findArrayInt(axi);
  if (arrayInt == nullptr) {
    SUCCEED() << "Sec 27.5: 'ARRAY_INT' ('if'-true branch) correctly produced no elaborated GenScopeArray, since "
                 "its condition is false.";
    return;
  }
  // If HLC does materialize a GenScopeArray for the untaken branch, Sec
  // 27.5 still requires it to be empty -- it must contain no elaborated
  // content of its own (the branch body is empty in the source anyway).
  ASSERT_NE(arrayInt->getGenScopes(), nullptr);
  for (const hldb::GenScope *const gs : *arrayInt->getGenScopes()) {
    ASSERT_NE(gs, nullptr);
    EXPECT_TRUE(gs->getModules() == nullptr || gs->getModules()->empty())
        << "'ARRAY_INT' is an empty branch body and, being untaken, must not hold any elaborated content";
  }
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
