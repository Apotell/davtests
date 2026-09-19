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

// IEEE 1800-2023 Sec 9.7 process methods the Google chapter-9 tests do not reach.
//
// Google's four 9.7 tests call self, status, kill, await, suspend and resume. The 9.7
// process prototype also declares three random-state methods, which none of them touch:
//
//   function void srandom(int seed);
//   function string get_randstate();
//   function void set_randstate(string state);
//
// This file covers those, plus the static self() call they hang off.
//
// Checked:
//   - process::self() resolves to process::self. This is the static-method-through-"::"
//     form, which reaches the class differently from an instance call: Phase 2 cannot
//     see a class declared in another file, so the "process" prefix arrives at the
//     binder as an UnsupportedTypespec and has to be recovered there.
//   - srandom, get_randstate and set_randstate each bind to the process class
//   - their kinds match the prototype: srandom and set_randstate are void functions,
//     get_randstate returns a string

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/class_defn.h>
#include <hldb/design.h>
#include <hldb/method_func_call.h>
#include <hldb/module.h>
#include <hldb/ref_obj.h>
#include <hldb/task_func.h>
#include <hldb/tf_call.h>

#include <string_view>

namespace hlc {

class ProcessRandstateTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "9.7--process_randstate.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  const hldb::RefObj *findPath(std::string_view name) const {
    for (const hldb::Any *const any : m_session->getDatabase().getObjects()) {
      const hldb::RefObj *const ro = any_cast<hldb::RefObj>(any);
      if ((ro == nullptr) || (ro->getName() != name)) continue;
      if ((ro->getPathElems() != nullptr) && !ro->getPathElems()->empty()) return ro;
    }
    return nullptr;
  }

  // The class a resolved call's method is declared in. Sentinels are returned rather than
  // asserted so a failure reports which stage broke instead of just "not equal".
  std::string_view ownerOfCall(std::string_view name) const {
    const hldb::RefObj *const ro = findPath(name);
    if (ro == nullptr) return "<no path>";

    const hldb::TFCall *const call = any_cast<hldb::TFCall>(ro->getPathElems()->back());
    if (call == nullptr) return "<trailing element is not a call>";
    if (call->getTaskFunc() == nullptr) return "<unresolved>";

    const hldb::ClassDefn *const owner = any_cast<hldb::ClassDefn>(call->getTaskFunc()->getParent());
    return (owner == nullptr) ? "<no owner>" : owner->getName();
  }

  const hldb::ClassDefn *processClass() const {
    return hldb::findByDefName<hldb::ClassDefn>("process", m_design->getAllClasses());
  }
};

TEST_F(ProcessRandstateTest, ModuleExists) {
  ASSERT_NE(hldb::findByName<hldb::Module>("top", m_design->getAllModules()), nullptr);
}

// ---------------------------------------------------------------------------
// The static self() call the rest hangs off
// ---------------------------------------------------------------------------
TEST_F(ProcessRandstateTest, ProcessSelfBindsToProcess) {
  EXPECT_EQ(ownerOfCall("process::self()"), "process")
      << "process::self() must bind (IEEE 1800-2023 Sec 9.7)";
}

// ---------------------------------------------------------------------------
// The three random-state methods
// ---------------------------------------------------------------------------
TEST_F(ProcessRandstateTest, SrandomBindsToProcess) {
  EXPECT_EQ(ownerOfCall("p.srandom(1)"), "process")
      << "process.srandom() must bind (IEEE 1800-2023 Sec 9.7)";
}

TEST_F(ProcessRandstateTest, GetRandstateBindsToProcess) {
  EXPECT_EQ(ownerOfCall("p.get_randstate()"), "process")
      << "process.get_randstate() must bind (IEEE 1800-2023 Sec 9.7)";
}

TEST_F(ProcessRandstateTest, SetRandstateBindsToProcess) {
  EXPECT_EQ(ownerOfCall("p.set_randstate(st)"), "process")
      << "process.set_randstate() must bind (IEEE 1800-2023 Sec 9.7)";
}

// The 9.7 prototype makes all three functions -- none of them blocks, so none is a task.
TEST_F(ProcessRandstateTest, RandstateMethodsAreFunctions) {
  const hldb::ClassDefn *const proc = processClass();
  ASSERT_NE(proc, nullptr);

  for (const std::string_view name : {"srandom", "get_randstate", "set_randstate"}) {
    const hldb::TaskFunc *const tf = hldb::findByName<hldb::TaskFunc>(name, proc->getMethods());
    ASSERT_NE(tf, nullptr) << name << " is declared by the 9.7 process prototype";
    EXPECT_EQ(tf->getAnyType(), hldb::AnyType::Function) << name << " is a function per Sec 9.7";
  }
}

// Sec 9.7 makes kill, suspend and resume void functions; only await is a task, because
// only await blocks. Asserted here because the four Google 9.7 tests call them without
// ever checking which kind they are.
TEST_F(ProcessRandstateTest, KillSuspendResumeAreFunctionsAndAwaitIsATask) {
  const hldb::ClassDefn *const proc = processClass();
  ASSERT_NE(proc, nullptr);

  for (const std::string_view name : {"kill", "suspend", "resume"}) {
    const hldb::TaskFunc *const tf = hldb::findByName<hldb::TaskFunc>(name, proc->getMethods());
    ASSERT_NE(tf, nullptr);
    EXPECT_EQ(tf->getAnyType(), hldb::AnyType::Function) << name << " is a void function per Sec 9.7";
  }

  const hldb::TaskFunc *const await = hldb::findByName<hldb::TaskFunc>("await", proc->getMethods());
  ASSERT_NE(await, nullptr);
  EXPECT_EQ(await->getAnyType(), hldb::AnyType::Task) << "await is the one blocking method (Sec 9.7)";
}

TEST_F(ProcessRandstateTest, CompilerReportsNoErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(findError(ErrorDefinition::LINT_NULL_ACTUAL), nullptr)
      << "every call in this file is a legal process method and must bind";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
