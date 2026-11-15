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

// Test for tests/Google/chapter-23/chapter-23_testsuit-1/23.2.2--port-data-types.sv
//
// THE SOURCE
//
//   20: typedef struct {
//   21:   bit isfloat;
//   22:   union { int i; shortreal f; } n;
//   23: } tagged_st;
//   24:
//   25: module mh1(input var int in1,
//   26:            input var shortreal in2,
//   27:            output tagged_st out);
//   28: endmodule
//
// WHAT THE LRM (IEEE 1800-2023) SAYS ABOUT IT
//
// "A port can be a declaration of an interface, an event, or a variable or
// net of any allowed data type, including an array, a structure, or a
// union." (23.2.2). This is the mh1 example of 23.2.2 with its "..."
// removed. The three ports are ANSI declarations (23.2.2.2), so each one
// carries its own direction and data type: in1 is an input of type int, in2
// an input of type shortreal, and out an output of the structure type
// tagged_st. "vpiPortIndex can be used to determine the port order. The
// first port has a port index of zero." (37.14 detail 9).
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   CompilesWithoutErrors      no fatal, syntax or error diagnostic
//   ModuleMh1IsDeclared        one module from this file, mh1, lines 25-28
//   ThreePortsInDeclarationOrder   in1, in2, out with vpiPortIndex 0, 1, 2
//   PortIn1IsAnIntInput        vpiDirection vpiInput, typespec int
//   PortIn2IsAShortrealInput   vpiDirection vpiInput, typespec shortreal
//   PortOutIsATaggedStOutput   vpiDirection vpiOutput, typespec tagged_st
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - Port kinds (net or variable). Those are the rules of 23.2.2.3, which
//     a later fixture of this suite covers.
//   - The members of tagged_st and of its union. They belong to the data
//     type clauses (7.2, 7.3), not to the port declaration.

#include <hlc/Common/Session.h>
#include <hlc/Events/EventDefinition.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/port.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/typespec.h>
#include <hldb/vpi_user.h>

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace hlc {
namespace {
constexpr std::string_view kSourceFileName = "23.2.2--port-data-types.sv";
constexpr uint32_t kModuleFirstLine = 25;
constexpr uint32_t kModuleLastLine = 28;

bool endsWith(std::string_view text, std::string_view suffix) {
  return (text.size() >= suffix.size()) && (text.substr(text.size() - suffix.size()) == suffix);
}

template <typename T>
size_t countFromSourceFile(const std::vector<T *> *collection) {
  size_t count = 0;
  if (collection != nullptr) {
    for (const T *object : *collection) {
      if (endsWith(object->getFile(), kSourceFileName)) ++count;
    }
  }
  return count;
}

template <typename T>
const T *findFromSourceFile(const std::vector<T *> *collection) {
  if (collection != nullptr) {
    for (const T *object : *collection) {
      if (endsWith(object->getFile(), kSourceFileName)) return object;
    }
  }
  return nullptr;
}

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
}  // namespace

class PortDataTypesTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "port_data_types.hlc"}); }
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

  static const hldb::Module *findModule() {
    return (m_design == nullptr) ? nullptr : findFromSourceFile(m_design->getAllModules());
  }

  static const hldb::Port *findPort(std::string_view name) {
    const hldb::Module *const module = findModule();
    return (module == nullptr) ? nullptr : hldb::findByName<hldb::Port>(name, module->getPorts());
  }
};

TEST_F(PortDataTypesTest, CompilesWithoutErrors) { EXPECT_EQ(allEvents(), ""); }

TEST_F(PortDataTypesTest, ModuleMh1IsDeclared) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(countFromSourceFile(m_design->getAllModules()), 1u);
  const hldb::Module *const module = findModule();
  ASSERT_NE(module, nullptr);
  EXPECT_EQ(module->getDefName(), std::string_view("mh1"));
  EXPECT_EQ(module->getStartLine(), kModuleFirstLine);
  EXPECT_EQ(module->getEndLine(), kModuleLastLine);
}

TEST_F(PortDataTypesTest, ThreePortsInDeclarationOrder) {
  const hldb::Module *const module = findModule();
  ASSERT_NE(module, nullptr);
  const hldb::PortCollection *const ports = module->getPorts();
  ASSERT_NE(ports, nullptr);
  ASSERT_EQ(ports->size(), 3u);

  const std::vector<std::string_view> names = {"in1", "in2", "out"};
  for (size_t index = 0; index < names.size(); ++index) {
    const hldb::Port *const port = hldb::findByName<hldb::Port>(names[index], ports);
    ASSERT_NE(port, nullptr) << names[index];
    EXPECT_EQ(port->getPortIndex(), static_cast<int32_t>(index)) << names[index];
  }
}

TEST_F(PortDataTypesTest, PortIn1IsAnIntInput) {
  const hldb::Port *const port = findPort("in1");
  ASSERT_NE(port, nullptr);
  EXPECT_EQ(port->getDirection(), vpiInput);
  const hldb::Typespec *const typespec = hldb::getTypespec(port);
  ASSERT_NE(typespec, nullptr);
  EXPECT_EQ(typespec->getAnyType(), hldb::AnyType::IntTypespec);
}

TEST_F(PortDataTypesTest, PortIn2IsAShortrealInput) {
  const hldb::Port *const port = findPort("in2");
  ASSERT_NE(port, nullptr);
  EXPECT_EQ(port->getDirection(), vpiInput);
  const hldb::Typespec *const typespec = hldb::getTypespec(port);
  ASSERT_NE(typespec, nullptr);
  EXPECT_EQ(typespec->getAnyType(), hldb::AnyType::ShortRealTypespec);
}

TEST_F(PortDataTypesTest, PortOutIsATaggedStOutput) {
  const hldb::Port *const port = findPort("out");
  ASSERT_NE(port, nullptr);
  EXPECT_EQ(port->getDirection(), vpiOutput);
  const hldb::Typespec *const typespec = hldb::getTypespec(port);
  ASSERT_NE(typespec, nullptr);
  EXPECT_EQ(typespec->getName(), std::string_view("tagged_st"));
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
