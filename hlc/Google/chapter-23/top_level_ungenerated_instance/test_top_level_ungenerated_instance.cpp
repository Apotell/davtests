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

// Test for tests/Google/chapter-23/23.3.1--top-level-ungenerated-instance.sv
//
// THE SOURCE
//
//   20: module child;
//   21: endmodule
//   22:
//   23: module top;
//   24:   if (0) begin : g
//   25:     child c1 ();
//   26:   end
//   27: endmodule
//
// WHAT THE LRM (IEEE 1800-2023) SAYS ABOUT IT
//
// "Top-level modules are modules that are included in the SystemVerilog
// source text, but do not appear in any module instantiation statement, as
// described in 23.3.2. This applies even if the module instantiation appears
// in a generate block that is not itself instantiated (see 27.3)." (23.3.1).
// The condition of the if-generate on line 24 is 0, so block g is not
// instantiated, but child still appears in the instantiation statement on
// line 25. So top is the only top-level module, and there is no instance of
// child at all. Top-level modules are reached by Design::getTopModules()
// (37.5 detail 1).
//
// THE MODEL
//
// HLC's graph is a definition model: it holds everything the source says,
// so that an instance model can be built from it. An instantiation is an
// unelaborated instance (Module::getRefInstances()) bound through its
// typespec to the definition it names, with its parameter and port
// connections bound to that definition's parameters and ports. Those
// bindings are checked unconditionally. The top-level instances and the
// instance tree exist only in an elaborated model (23.3); those checks are
// gated on m_design->getElaborated(), so they run once HLC elaborates.
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   CompilesWithoutErrors          no fatal, syntax or error event
//   ChildIsInstantiatedInsideG     top's if-generate (condition 0) holds the
//                                  block g, which holds c1 bound to child
//   OnlyTopIsNeverInstantiated     no instantiation names top; child's is in
//                                  g (23.3.1)
//   TopIsTheOnlyTopLevelInstance   elaborated model only: the top-level
//                                  instances are exactly top
//   ChildIsNotATopLevelInstance    elaborated model only: no top-level
//                                  instance is named child
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - Whether g is generated. Conditional generate constructs are 27.5, and
//     only an elaborated model generates them.

#include <hlc/Common/Session.h>
#include <hlc/Events/EventDefinition.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/case_item.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/gen_block.h>
#include <hldb/gen_case.h>
#include <hldb/gen_for.h>
#include <hldb/gen_if.h>
#include <hldb/gen_if_else.h>
#include <hldb/gen_region.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_typespec.h>

#include <algorithm>
#include <iterator>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace hlc {
namespace {
// True for an error-class event: fatal, syntax or error severity.
bool isErrorClass(const Event &event) {
  const EventDefinition *const eventDefinition = hldb::findEventDefinition(hlc::getEventTable(), event.getType());
  if (eventDefinition == nullptr) return false;
  const EventSeverity severity = eventDefinition->m_severity;
  return (severity == EventSeverity::Fatal) || (severity == EventSeverity::Syntax) ||
         (severity == EventSeverity::Error);
}

// One event as hldb::renderEvent() prints it, one line per output line, so that a
// failing test shows what was reported and where.
std::string describeEvent(const Event &event) {
  std::ostringstream oss;
  const std::vector<std::string> lines = hldb::renderEvent(event);
  std::copy(lines.cbegin(), lines.cend(), std::ostream_iterator<std::string>(oss, "\n"));
  return oss.str();
}

// The objects in 'collection'; none for a null collection.
template <typename T>
std::vector<const T *> toVector(const std::vector<T *> *collection) {
  std::vector<const T *> result;
  if (collection == nullptr) return result;
  for (const T *object : *collection) result.emplace_back(object);
  return result;
}

// The object named 'name' among 'objects', or nullptr.
template <typename T>
const T *findNamed(const std::vector<const T *> &objects, std::string_view name) {
  for (const T *object : objects) {
    if (object->getName() == name) return object;
  }
  return nullptr;
}

// The names of 'objects', comma-separated, sorted.
template <typename T>
std::string namesOf(const std::vector<const T *> &objects) {
  std::vector<std::string> names;
  for (const T *object : objects) names.emplace_back(object->getName());
  std::sort(names.begin(), names.end());
  std::string result;
  for (const std::string &name : names) {
    if (!result.empty()) result += ", ";
    result += name;
  }
  return result;
}
// The name of the definition an unelaborated instance is bound to (the module its typespec names), or "" when it
// is not bound to a module.
std::string_view boundDefinition(const hldb::RefInstance *instance) {
  if ((instance == nullptr) || (instance->getTypespec() == nullptr)) return "";
  const hldb::ModuleTypespec *const definition = instance->getTypespec()->getActual<hldb::ModuleTypespec>();
  return (definition == nullptr) ? std::string_view() : definition->getDefName();
}

// Adds to 'instances' the unelaborated instances that 'object' declares: a definition's own and those in its
// generate constructs, at any depth.
void collectRefInstances(const hldb::Any *object, std::vector<const hldb::RefInstance *> &instances) {
  if (object == nullptr) return;
  if (const hldb::RefInstance *const instance = any_cast<hldb::RefInstance>(object)) {
    instances.emplace_back(instance);
  } else if (const hldb::Module *const module = any_cast<hldb::Module>(object)) {
    if (module->getRefInstances() != nullptr) {
      for (const hldb::RefInstance *child : *module->getRefInstances()) collectRefInstances(child, instances);
    }
    if (module->getGenStmts() != nullptr) {
      for (const hldb::Any *statement : *module->getGenStmts()) collectRefInstances(statement, instances);
    }
  } else if (const hldb::GenRegion *const region = any_cast<hldb::GenRegion>(object)) {
    if (region->getStmts() != nullptr) {
      for (const hldb::Any *statement : *region->getStmts()) collectRefInstances(statement, instances);
    }
  } else if (const hldb::GenBlock *const block = any_cast<hldb::GenBlock>(object)) {
    if (block->getStmts() != nullptr) {
      for (const hldb::Any *statement : *block->getStmts()) collectRefInstances(statement, instances);
    }
  } else if (const hldb::GenIf *const genIf = any_cast<hldb::GenIf>(object)) {
    collectRefInstances(genIf->getStmt(), instances);
  } else if (const hldb::GenIfElse *const genIfElse = any_cast<hldb::GenIfElse>(object)) {
    collectRefInstances(genIfElse->getStmt(), instances);
    collectRefInstances(genIfElse->getElseStmt(), instances);
  } else if (const hldb::GenFor *const genFor = any_cast<hldb::GenFor>(object)) {
    collectRefInstances(genFor->getStmt(), instances);
  } else if (const hldb::GenCase *const genCase = any_cast<hldb::GenCase>(object)) {
    if (genCase->getCaseItems() != nullptr) {
      for (const hldb::CaseItem *item : *genCase->getCaseItems()) collectRefInstances(item->getStmt(), instances);
    }
  }
}

// The definitions that no module instantiation statement names (23.3.1), comma-separated in the order of
// getAllModules().
std::string neverInstantiated(const hldb::Design *design) {
  if ((design == nullptr) || (design->getAllModules() == nullptr)) return "<no design>";
  std::vector<const hldb::RefInstance *> instances;
  for (const hldb::Module *module : *design->getAllModules()) collectRefInstances(module, instances);
  std::string result;
  for (const hldb::Module *module : *design->getAllModules()) {
    const std::string_view name = module->getDefName();
    const bool named = std::any_of(instances.cbegin(), instances.cend(), [name](const hldb::RefInstance *instance) {
      return boundDefinition(instance) == name;
    });
    if (named) continue;
    if (!result.empty()) result += ", ";
    result += std::string(name);
  }
  return result;
}

}  // namespace

class TopLevelUngeneratedInstanceTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "top_level_ungenerated_instance.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static std::string allEvents() {
    hlc::EventListener *const listener = m_session->getEventListener();
    std::ostringstream oss;
    for (const Event &event : listener->getEvents()) {
      if (isErrorClass(event)) oss << describeEvent(event);
    }
    return oss.str();
  }

  // The definition of the module named 'name', or nullptr.
  static const hldb::Module *findModule(std::string_view name) {
    if (m_design == nullptr) return nullptr;
    return hldb::findByDefName<hldb::Module>(name, m_design->getAllModules());
  }

  // The unelaborated instance 'name' that the definition 'parent' declares, or nullptr.
  static const hldb::RefInstance *findRefInstance(std::string_view parent, std::string_view name) {
    const hldb::Module *const module = findModule(parent);
    return (module == nullptr) ? nullptr : hldb::findByName<hldb::RefInstance>(name, module->getRefInstances());
  }

  // The top-level instances (37.5 detail 1); there are some only in an elaborated model.
  static std::vector<const hldb::Module *> topInstances() {
    std::vector<const hldb::Module *> result;
    if ((m_design == nullptr) || (m_design->getTopModules() == nullptr)) return result;
    for (const hldb::Module *module : *m_design->getTopModules()) result.emplace_back(module);
    return result;
  }
};

TEST_F(TopLevelUngeneratedInstanceTest, CompilesWithoutErrors) { EXPECT_EQ(allEvents(), ""); }

// "if (0) begin : g child c1 (); end": the instantiation of child is in the source even though g is never
// generated, so child appears in a module instantiation statement (23.3.1).
TEST_F(TopLevelUngeneratedInstanceTest, ChildIsInstantiatedInsideG) {
  const hldb::Module *const top = findModule("top");
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getGenStmts(), nullptr);
  ASSERT_EQ(top->getGenStmts()->size(), 1u);
  const hldb::GenIf *const genIf = any_cast<hldb::GenIf>(top->getGenStmts()->front());
  ASSERT_NE(genIf, nullptr);
  const hldb::Constant *const condition = genIf->getCondition<hldb::Constant>();
  ASSERT_NE(condition, nullptr);
  EXPECT_EQ(condition->getDecompile(), std::string_view("0"));
  const hldb::GenBlock *const block = genIf->getStmt<hldb::GenBlock>();
  ASSERT_NE(block, nullptr);
  EXPECT_EQ(block->getName(), std::string_view("g"));
  const hldb::RefInstance *const c1 = hldb::findByName<hldb::RefInstance>("c1", block->getStmts());
  ASSERT_NE(c1, nullptr);
  EXPECT_EQ(boundDefinition(c1), std::string_view("child"));
}

// "Top-level modules are modules that ... do not appear in any module instantiation statement" (23.3.1).
TEST_F(TopLevelUngeneratedInstanceTest, OnlyTopIsNeverInstantiated) { EXPECT_EQ(neverInstantiated(m_design), "top"); }

TEST_F(TopLevelUngeneratedInstanceTest, TopIsTheOnlyTopLevelInstance) {
  ASSERT_NE(m_design, nullptr);
  if (m_design->getElaborated()) {
    EXPECT_EQ(namesOf(topInstances()), "top");
  }
}

TEST_F(TopLevelUngeneratedInstanceTest, ChildIsNotATopLevelInstance) {
  ASSERT_NE(m_design, nullptr);
  if (m_design->getElaborated()) {
    const std::vector<const hldb::Module *> tops = topInstances();
    // Without any top-level instance the check below would pass vacuously.
    ASSERT_FALSE(tops.empty()) << "no top-level instance at all";
    EXPECT_EQ(findNamed(tops, "child"), nullptr) << "top-level instances: " << namesOf(tops);
  }
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
