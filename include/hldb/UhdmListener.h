// -*- c++ -*-

/*

 Copyright 2019 Alain Dargelas

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

/*
 * File:   UhdmListener.h
 * Author: hs
 *
 * Created on March 11, 2022, 00:00 AM
 */

#ifndef UHDM_UHDMLISTENER_H
#define UHDM_UHDMLISTENER_H

#include <uhdm/any.h>
#include <uhdm/containers.h>
#include <uhdm/sv_vpi_user.h>
#include <uhdm/uhdm_types.h>

#include <ostream>
#include <string>

#define UHDMLISTENER_TRACE_CONTEXT                  \
  "[" << ((const Any*)object)->getStartLine() <<    \
  "," << ((const Any*)object)->getStartColumn() <<  \
  ":" << ((const Any*)object)->getEndLine() <<      \
  "," << ((const Any*)object)->getEndColumn() <<    \
  "]"

#define UHDMLISTENER_TRACE_ENTER strm               \
  << std::string(++indent * 2, ' ')                 \
  << __func__ << ": " << UHDMLISTENER_TRACE_CONTEXT \
  << std::endl
#define UHDMLISTENER_TRACE_LEAVE strm               \
  << std::string(2 * indent--, ' ')                 \
  << __func__ << ": " << UHDMLISTENER_TRACE_CONTEXT \
  << std::endl

namespace uhdm {
class Serializer;
class UhdmListener {
protected:
  using any_stack_t = std::vector<const Any *>;

public:
  // Use implicit constructor to initialize all members
  // UhdmListener()
  virtual ~UhdmListener() = default;

public:
  AnySet &getVisited() { return m_visited; }
 const AnySet &getVisited() const { return m_visited; }

  const any_stack_t &getCallstack() const { return m_callstack; }

  bool isOnCallstack(const Any *what) const;
  bool isOnCallstack(const std::set<UhdmType> &types) const;

  void requestAbort() { m_abortRequested = true; }

  bool didVisitAll(const Serializer &serializer) const;

  void listenAny(const Any* object, uint32_t vpiRelation = 0);
  void listenAlias(const Alias* object, uint32_t vpiRelation = 0);
  void listenAlways(const Always* object, uint32_t vpiRelation = 0);
  void listenAnyPattern(const AnyPattern* object, uint32_t vpiRelation = 0);
  void listenArrayExpr(const ArrayExpr* object, uint32_t vpiRelation = 0);
  void listenArrayTypespec(const ArrayTypespec* object, uint32_t vpiRelation = 0);
  void listenAssert(const Assert* object, uint32_t vpiRelation = 0);
  void listenAssignStmt(const AssignStmt* object, uint32_t vpiRelation = 0);
  void listenAssignment(const Assignment* object, uint32_t vpiRelation = 0);
  void listenAssume(const Assume* object, uint32_t vpiRelation = 0);
  void listenAttribute(const Attribute* object, uint32_t vpiRelation = 0);
  void listenBegin(const Begin* object, uint32_t vpiRelation = 0);
  void listenBindDirective(const BindDirective* object, uint32_t vpiRelation = 0);
  void listenBitSelect(const BitSelect* object, uint32_t vpiRelation = 0);
  void listenBitTypespec(const BitTypespec* object, uint32_t vpiRelation = 0);
  void listenBreakStmt(const BreakStmt* object, uint32_t vpiRelation = 0);
  void listenByteTypespec(const ByteTypespec* object, uint32_t vpiRelation = 0);
  void listenCaseItem(const CaseItem* object, uint32_t vpiRelation = 0);
  void listenCaseProperty(const CaseProperty* object, uint32_t vpiRelation = 0);
  void listenCasePropertyItem(const CasePropertyItem* object, uint32_t vpiRelation = 0);
  void listenCaseStmt(const CaseStmt* object, uint32_t vpiRelation = 0);
  void listenChandleTypespec(const ChandleTypespec* object, uint32_t vpiRelation = 0);
  void listenCheckerDecl(const CheckerDecl* object, uint32_t vpiRelation = 0);
  void listenCheckerInst(const CheckerInst* object, uint32_t vpiRelation = 0);
  void listenCheckerInstPort(const CheckerInstPort* object, uint32_t vpiRelation = 0);
  void listenCheckerPort(const CheckerPort* object, uint32_t vpiRelation = 0);
  void listenClassDefn(const ClassDefn* object, uint32_t vpiRelation = 0);
  void listenClassObj(const ClassObj* object, uint32_t vpiRelation = 0);
  void listenClassTypespec(const ClassTypespec* object, uint32_t vpiRelation = 0);
  void listenClause(const Clause* object, uint32_t vpiRelation = 0);
  void listenClockedProperty(const ClockedProperty* object, uint32_t vpiRelation = 0);
  void listenClockedSeq(const ClockedSeq* object, uint32_t vpiRelation = 0);
  void listenClockingBlock(const ClockingBlock* object, uint32_t vpiRelation = 0);
  void listenClockingIODecl(const ClockingIODecl* object, uint32_t vpiRelation = 0);
  void listenComment(const Comment* object, uint32_t vpiRelation = 0);
  void listenConfigDecl(const ConfigDecl* object, uint32_t vpiRelation = 0);
  void listenConfigRule(const ConfigRule* object, uint32_t vpiRelation = 0);
  void listenConstant(const Constant* object, uint32_t vpiRelation = 0);
  void listenConstrForeach(const ConstrForeach* object, uint32_t vpiRelation = 0);
  void listenConstrIf(const ConstrIf* object, uint32_t vpiRelation = 0);
  void listenConstrIfElse(const ConstrIfElse* object, uint32_t vpiRelation = 0);
  void listenConstraint(const Constraint* object, uint32_t vpiRelation = 0);
  void listenConstraintOrdering(const ConstraintOrdering* object, uint32_t vpiRelation = 0);
  void listenContAssign(const ContAssign* object, uint32_t vpiRelation = 0);
  void listenContAssignBit(const ContAssignBit* object, uint32_t vpiRelation = 0);
  void listenContinueStmt(const ContinueStmt* object, uint32_t vpiRelation = 0);
  void listenCover(const Cover* object, uint32_t vpiRelation = 0);
  void listenCoverBin(const CoverBin* object, uint32_t vpiRelation = 0);
  void listenCoverCross(const CoverCross* object, uint32_t vpiRelation = 0);
  void listenCoverGroup(const CoverGroup* object, uint32_t vpiRelation = 0);
  void listenCoverPoint(const CoverPoint* object, uint32_t vpiRelation = 0);
  void listenCoverageOption(const CoverageOption* object, uint32_t vpiRelation = 0);
  void listenDeassign(const Deassign* object, uint32_t vpiRelation = 0);
  void listenDefParam(const DefParam* object, uint32_t vpiRelation = 0);
  void listenDelayControl(const DelayControl* object, uint32_t vpiRelation = 0);
  void listenDelayTerm(const DelayTerm* object, uint32_t vpiRelation = 0);
  void listenDesign(const Design* object, uint32_t vpiRelation = 0);
  void listenDisable(const Disable* object, uint32_t vpiRelation = 0);
  void listenDisableFork(const DisableFork* object, uint32_t vpiRelation = 0);
  void listenDistItem(const DistItem* object, uint32_t vpiRelation = 0);
  void listenDistribution(const Distribution* object, uint32_t vpiRelation = 0);
  void listenDoWhile(const DoWhile* object, uint32_t vpiRelation = 0);
  void listenEnumConst(const EnumConst* object, uint32_t vpiRelation = 0);
  void listenEnumTypespec(const EnumTypespec* object, uint32_t vpiRelation = 0);
  void listenEventControl(const EventControl* object, uint32_t vpiRelation = 0);
  void listenEventStmt(const EventStmt* object, uint32_t vpiRelation = 0);
  void listenEventTypespec(const EventTypespec* object, uint32_t vpiRelation = 0);
  void listenExpectStmt(const ExpectStmt* object, uint32_t vpiRelation = 0);
  void listenExtends(const Extends* object, uint32_t vpiRelation = 0);
  void listenFinalStmt(const FinalStmt* object, uint32_t vpiRelation = 0);
  void listenForStmt(const ForStmt* object, uint32_t vpiRelation = 0);
  void listenForce(const Force* object, uint32_t vpiRelation = 0);
  void listenForeachStmt(const ForeachStmt* object, uint32_t vpiRelation = 0);
  void listenForeverStmt(const ForeverStmt* object, uint32_t vpiRelation = 0);
  void listenForkStmt(const ForkStmt* object, uint32_t vpiRelation = 0);
  void listenFuncCall(const FuncCall* object, uint32_t vpiRelation = 0);
  void listenFunction(const Function* object, uint32_t vpiRelation = 0);
  void listenFunctionDecl(const FunctionDecl* object, uint32_t vpiRelation = 0);
  void listenGate(const Gate* object, uint32_t vpiRelation = 0);
  void listenGateArray(const GateArray* object, uint32_t vpiRelation = 0);
  void listenGenCase(const GenCase* object, uint32_t vpiRelation = 0);
  void listenGenFor(const GenFor* object, uint32_t vpiRelation = 0);
  void listenGenIf(const GenIf* object, uint32_t vpiRelation = 0);
  void listenGenIfElse(const GenIfElse* object, uint32_t vpiRelation = 0);
  void listenGenRegion(const GenRegion* object, uint32_t vpiRelation = 0);
  void listenGenScopeArray(const GenScopeArray* object, uint32_t vpiRelation = 0);
  void listenHierPath(const HierPath* object, uint32_t vpiRelation = 0);
  void listenIODecl(const IODecl* object, uint32_t vpiRelation = 0);
  void listenIdentifier(const Identifier* object, uint32_t vpiRelation = 0);
  void listenIfElse(const IfElse* object, uint32_t vpiRelation = 0);
  void listenIfStmt(const IfStmt* object, uint32_t vpiRelation = 0);
  void listenImmediateAssert(const ImmediateAssert* object, uint32_t vpiRelation = 0);
  void listenImmediateAssume(const ImmediateAssume* object, uint32_t vpiRelation = 0);
  void listenImmediateCover(const ImmediateCover* object, uint32_t vpiRelation = 0);
  void listenImplements(const Implements* object, uint32_t vpiRelation = 0);
  void listenImplication(const Implication* object, uint32_t vpiRelation = 0);
  void listenImportTypespec(const ImportTypespec* object, uint32_t vpiRelation = 0);
  void listenIncludeStmt(const IncludeStmt* object, uint32_t vpiRelation = 0);
  void listenIndexedPartSelect(const IndexedPartSelect* object, uint32_t vpiRelation = 0);
  void listenInitial(const Initial* object, uint32_t vpiRelation = 0);
  void listenIntTypespec(const IntTypespec* object, uint32_t vpiRelation = 0);
  void listenIntegerTypespec(const IntegerTypespec* object, uint32_t vpiRelation = 0);
  void listenInterface(const Interface* object, uint32_t vpiRelation = 0);
  void listenInterfaceArray(const InterfaceArray* object, uint32_t vpiRelation = 0);
  void listenInterfaceTFDecl(const InterfaceTFDecl* object, uint32_t vpiRelation = 0);
  void listenInterfaceTypespec(const InterfaceTypespec* object, uint32_t vpiRelation = 0);
  void listenLetDecl(const LetDecl* object, uint32_t vpiRelation = 0);
  void listenLetExpr(const LetExpr* object, uint32_t vpiRelation = 0);
  void listenLibrary(const Library* object, uint32_t vpiRelation = 0);
  void listenLogicTypespec(const LogicTypespec* object, uint32_t vpiRelation = 0);
  void listenLongIntTypespec(const LongIntTypespec* object, uint32_t vpiRelation = 0);
  void listenMethodFuncCall(const MethodFuncCall* object, uint32_t vpiRelation = 0);
  void listenMethodTaskCall(const MethodTaskCall* object, uint32_t vpiRelation = 0);
  void listenModPath(const ModPath* object, uint32_t vpiRelation = 0);
  void listenModport(const Modport* object, uint32_t vpiRelation = 0);
  void listenModule(const Module* object, uint32_t vpiRelation = 0);
  void listenModuleArray(const ModuleArray* object, uint32_t vpiRelation = 0);
  void listenModuleTypespec(const ModuleTypespec* object, uint32_t vpiRelation = 0);
  void listenMulticlockSequenceExpr(const MulticlockSequenceExpr* object, uint32_t vpiRelation = 0);
  void listenNamedEvent(const NamedEvent* object, uint32_t vpiRelation = 0);
  void listenNamedEventArray(const NamedEventArray* object, uint32_t vpiRelation = 0);
  void listenNet(const Net* object, uint32_t vpiRelation = 0);
  void listenNullStmt(const NullStmt* object, uint32_t vpiRelation = 0);
  void listenOperation(const Operation* object, uint32_t vpiRelation = 0);
  void listenOrderedWait(const OrderedWait* object, uint32_t vpiRelation = 0);
  void listenPackage(const Package* object, uint32_t vpiRelation = 0);
  void listenPackageTypespec(const PackageTypespec* object, uint32_t vpiRelation = 0);
  void listenParamAssign(const ParamAssign* object, uint32_t vpiRelation = 0);
  void listenParameter(const Parameter* object, uint32_t vpiRelation = 0);
  void listenPartSelect(const PartSelect* object, uint32_t vpiRelation = 0);
  void listenPathTerm(const PathTerm* object, uint32_t vpiRelation = 0);
  void listenPort(const Port* object, uint32_t vpiRelation = 0);
  void listenPortBit(const PortBit* object, uint32_t vpiRelation = 0);
  void listenPreprocMacroDefinition(const PreprocMacroDefinition* object, uint32_t vpiRelation = 0);
  void listenPreprocMacroInstance(const PreprocMacroInstance* object, uint32_t vpiRelation = 0);
  void listenPrimTerm(const PrimTerm* object, uint32_t vpiRelation = 0);
  void listenProgram(const Program* object, uint32_t vpiRelation = 0);
  void listenProgramArray(const ProgramArray* object, uint32_t vpiRelation = 0);
  void listenProgramTypespec(const ProgramTypespec* object, uint32_t vpiRelation = 0);
  void listenPropFormalDecl(const PropFormalDecl* object, uint32_t vpiRelation = 0);
  void listenPropertyDecl(const PropertyDecl* object, uint32_t vpiRelation = 0);
  void listenPropertyInst(const PropertyInst* object, uint32_t vpiRelation = 0);
  void listenPropertySpec(const PropertySpec* object, uint32_t vpiRelation = 0);
  void listenPropertyTypespec(const PropertyTypespec* object, uint32_t vpiRelation = 0);
  void listenRange(const Range* object, uint32_t vpiRelation = 0);
  void listenRealTypespec(const RealTypespec* object, uint32_t vpiRelation = 0);
  void listenRefInstance(const RefInstance* object, uint32_t vpiRelation = 0);
  void listenRefObj(const RefObj* object, uint32_t vpiRelation = 0);
  void listenRefTypespec(const RefTypespec* object, uint32_t vpiRelation = 0);
  void listenReg(const Reg* object, uint32_t vpiRelation = 0);
  void listenRegArray(const RegArray* object, uint32_t vpiRelation = 0);
  void listenRelease(const Release* object, uint32_t vpiRelation = 0);
  void listenRepeat(const Repeat* object, uint32_t vpiRelation = 0);
  void listenRepeatControl(const RepeatControl* object, uint32_t vpiRelation = 0);
  void listenRestrict(const Restrict* object, uint32_t vpiRelation = 0);
  void listenReturnStmt(const ReturnStmt* object, uint32_t vpiRelation = 0);
  void listenSeqFormalDecl(const SeqFormalDecl* object, uint32_t vpiRelation = 0);
  void listenSequenceDecl(const SequenceDecl* object, uint32_t vpiRelation = 0);
  void listenSequenceInst(const SequenceInst* object, uint32_t vpiRelation = 0);
  void listenSequenceTypespec(const SequenceTypespec* object, uint32_t vpiRelation = 0);
  void listenShortIntTypespec(const ShortIntTypespec* object, uint32_t vpiRelation = 0);
  void listenShortRealTypespec(const ShortRealTypespec* object, uint32_t vpiRelation = 0);
  void listenSoftDisable(const SoftDisable* object, uint32_t vpiRelation = 0);
  void listenSourceFile(const SourceFile* object, uint32_t vpiRelation = 0);
  void listenSpecParam(const SpecParam* object, uint32_t vpiRelation = 0);
  void listenStringTypespec(const StringTypespec* object, uint32_t vpiRelation = 0);
  void listenStructPattern(const StructPattern* object, uint32_t vpiRelation = 0);
  void listenStructTypespec(const StructTypespec* object, uint32_t vpiRelation = 0);
  void listenSwitchArray(const SwitchArray* object, uint32_t vpiRelation = 0);
  void listenSwitchTran(const SwitchTran* object, uint32_t vpiRelation = 0);
  void listenSysFuncCall(const SysFuncCall* object, uint32_t vpiRelation = 0);
  void listenSysTaskCall(const SysTaskCall* object, uint32_t vpiRelation = 0);
  void listenTableEntry(const TableEntry* object, uint32_t vpiRelation = 0);
  void listenTaggedPattern(const TaggedPattern* object, uint32_t vpiRelation = 0);
  void listenTask(const Task* object, uint32_t vpiRelation = 0);
  void listenTaskCall(const TaskCall* object, uint32_t vpiRelation = 0);
  void listenTaskDecl(const TaskDecl* object, uint32_t vpiRelation = 0);
  void listenTchk(const Tchk* object, uint32_t vpiRelation = 0);
  void listenTchkTerm(const TchkTerm* object, uint32_t vpiRelation = 0);
  void listenThread(const Thread* object, uint32_t vpiRelation = 0);
  void listenTimeTypespec(const TimeTypespec* object, uint32_t vpiRelation = 0);
  void listenTypeParameter(const TypeParameter* object, uint32_t vpiRelation = 0);
  void listenTypedefTypespec(const TypedefTypespec* object, uint32_t vpiRelation = 0);
  void listenTypespecMember(const TypespecMember* object, uint32_t vpiRelation = 0);
  void listenUdp(const Udp* object, uint32_t vpiRelation = 0);
  void listenUdpArray(const UdpArray* object, uint32_t vpiRelation = 0);
  void listenUdpDefn(const UdpDefn* object, uint32_t vpiRelation = 0);
  void listenUdpDefnTypespec(const UdpDefnTypespec* object, uint32_t vpiRelation = 0);
  void listenUnionTypespec(const UnionTypespec* object, uint32_t vpiRelation = 0);
  void listenUniqueness(const Uniqueness* object, uint32_t vpiRelation = 0);
  void listenUnsupportedExpr(const UnsupportedExpr* object, uint32_t vpiRelation = 0);
  void listenUnsupportedStmt(const UnsupportedStmt* object, uint32_t vpiRelation = 0);
  void listenUnsupportedTypespec(const UnsupportedTypespec* object, uint32_t vpiRelation = 0);
  void listenUserSystf(const UserSystf* object, uint32_t vpiRelation = 0);
  void listenVarSelect(const VarSelect* object, uint32_t vpiRelation = 0);
  void listenVariable(const Variable* object, uint32_t vpiRelation = 0);
  void listenVoidTypespec(const VoidTypespec* object, uint32_t vpiRelation = 0);
  void listenWaitFork(const WaitFork* object, uint32_t vpiRelation = 0);
  void listenWaitStmt(const WaitStmt* object, uint32_t vpiRelation = 0);
  void listenWhileStmt(const WhileStmt* object, uint32_t vpiRelation = 0);

  virtual void enterAny(const Any* object, uint32_t vpiRelation) {}
  virtual void leaveAny(const Any* object, uint32_t vpiRelation) {}

  virtual void enterAlias(const Alias* object, uint32_t vpiRelation) {}
  virtual void leaveAlias(const Alias* object, uint32_t vpiRelation) {}

  virtual void enterAlways(const Always* object, uint32_t vpiRelation) {}
  virtual void leaveAlways(const Always* object, uint32_t vpiRelation) {}

  virtual void enterAnyPattern(const AnyPattern* object, uint32_t vpiRelation) {}
  virtual void leaveAnyPattern(const AnyPattern* object, uint32_t vpiRelation) {}

  virtual void enterArrayExpr(const ArrayExpr* object, uint32_t vpiRelation) {}
  virtual void leaveArrayExpr(const ArrayExpr* object, uint32_t vpiRelation) {}

  virtual void enterArrayTypespec(const ArrayTypespec* object, uint32_t vpiRelation) {}
  virtual void leaveArrayTypespec(const ArrayTypespec* object, uint32_t vpiRelation) {}

  virtual void enterAssert(const Assert* object, uint32_t vpiRelation) {}
  virtual void leaveAssert(const Assert* object, uint32_t vpiRelation) {}

  virtual void enterAssignStmt(const AssignStmt* object, uint32_t vpiRelation) {}
  virtual void leaveAssignStmt(const AssignStmt* object, uint32_t vpiRelation) {}

  virtual void enterAssignment(const Assignment* object, uint32_t vpiRelation) {}
  virtual void leaveAssignment(const Assignment* object, uint32_t vpiRelation) {}

  virtual void enterAssume(const Assume* object, uint32_t vpiRelation) {}
  virtual void leaveAssume(const Assume* object, uint32_t vpiRelation) {}

  virtual void enterAttribute(const Attribute* object, uint32_t vpiRelation) {}
  virtual void leaveAttribute(const Attribute* object, uint32_t vpiRelation) {}

  virtual void enterBegin(const Begin* object, uint32_t vpiRelation) {}
  virtual void leaveBegin(const Begin* object, uint32_t vpiRelation) {}

  virtual void enterBindDirective(const BindDirective* object, uint32_t vpiRelation) {}
  virtual void leaveBindDirective(const BindDirective* object, uint32_t vpiRelation) {}

  virtual void enterBitSelect(const BitSelect* object, uint32_t vpiRelation) {}
  virtual void leaveBitSelect(const BitSelect* object, uint32_t vpiRelation) {}

  virtual void enterBitTypespec(const BitTypespec* object, uint32_t vpiRelation) {}
  virtual void leaveBitTypespec(const BitTypespec* object, uint32_t vpiRelation) {}

  virtual void enterBreakStmt(const BreakStmt* object, uint32_t vpiRelation) {}
  virtual void leaveBreakStmt(const BreakStmt* object, uint32_t vpiRelation) {}

  virtual void enterByteTypespec(const ByteTypespec* object, uint32_t vpiRelation) {}
  virtual void leaveByteTypespec(const ByteTypespec* object, uint32_t vpiRelation) {}

  virtual void enterCaseItem(const CaseItem* object, uint32_t vpiRelation) {}
  virtual void leaveCaseItem(const CaseItem* object, uint32_t vpiRelation) {}

  virtual void enterCaseProperty(const CaseProperty* object, uint32_t vpiRelation) {}
  virtual void leaveCaseProperty(const CaseProperty* object, uint32_t vpiRelation) {}

  virtual void enterCasePropertyItem(const CasePropertyItem* object, uint32_t vpiRelation) {}
  virtual void leaveCasePropertyItem(const CasePropertyItem* object, uint32_t vpiRelation) {}

  virtual void enterCaseStmt(const CaseStmt* object, uint32_t vpiRelation) {}
  virtual void leaveCaseStmt(const CaseStmt* object, uint32_t vpiRelation) {}

  virtual void enterChandleTypespec(const ChandleTypespec* object, uint32_t vpiRelation) {}
  virtual void leaveChandleTypespec(const ChandleTypespec* object, uint32_t vpiRelation) {}

  virtual void enterCheckerDecl(const CheckerDecl* object, uint32_t vpiRelation) {}
  virtual void leaveCheckerDecl(const CheckerDecl* object, uint32_t vpiRelation) {}

  virtual void enterCheckerInst(const CheckerInst* object, uint32_t vpiRelation) {}
  virtual void leaveCheckerInst(const CheckerInst* object, uint32_t vpiRelation) {}

  virtual void enterCheckerInstPort(const CheckerInstPort* object, uint32_t vpiRelation) {}
  virtual void leaveCheckerInstPort(const CheckerInstPort* object, uint32_t vpiRelation) {}

  virtual void enterCheckerPort(const CheckerPort* object, uint32_t vpiRelation) {}
  virtual void leaveCheckerPort(const CheckerPort* object, uint32_t vpiRelation) {}

  virtual void enterClassDefn(const ClassDefn* object, uint32_t vpiRelation) {}
  virtual void leaveClassDefn(const ClassDefn* object, uint32_t vpiRelation) {}

  virtual void enterClassObj(const ClassObj* object, uint32_t vpiRelation) {}
  virtual void leaveClassObj(const ClassObj* object, uint32_t vpiRelation) {}

  virtual void enterClassTypespec(const ClassTypespec* object, uint32_t vpiRelation) {}
  virtual void leaveClassTypespec(const ClassTypespec* object, uint32_t vpiRelation) {}

  virtual void enterClause(const Clause* object, uint32_t vpiRelation) {}
  virtual void leaveClause(const Clause* object, uint32_t vpiRelation) {}

  virtual void enterClockedProperty(const ClockedProperty* object, uint32_t vpiRelation) {}
  virtual void leaveClockedProperty(const ClockedProperty* object, uint32_t vpiRelation) {}

  virtual void enterClockedSeq(const ClockedSeq* object, uint32_t vpiRelation) {}
  virtual void leaveClockedSeq(const ClockedSeq* object, uint32_t vpiRelation) {}

  virtual void enterClockingBlock(const ClockingBlock* object, uint32_t vpiRelation) {}
  virtual void leaveClockingBlock(const ClockingBlock* object, uint32_t vpiRelation) {}

  virtual void enterClockingIODecl(const ClockingIODecl* object, uint32_t vpiRelation) {}
  virtual void leaveClockingIODecl(const ClockingIODecl* object, uint32_t vpiRelation) {}

  virtual void enterComment(const Comment* object, uint32_t vpiRelation) {}
  virtual void leaveComment(const Comment* object, uint32_t vpiRelation) {}

  virtual void enterConfigDecl(const ConfigDecl* object, uint32_t vpiRelation) {}
  virtual void leaveConfigDecl(const ConfigDecl* object, uint32_t vpiRelation) {}

  virtual void enterConfigRule(const ConfigRule* object, uint32_t vpiRelation) {}
  virtual void leaveConfigRule(const ConfigRule* object, uint32_t vpiRelation) {}

  virtual void enterConstant(const Constant* object, uint32_t vpiRelation) {}
  virtual void leaveConstant(const Constant* object, uint32_t vpiRelation) {}

  virtual void enterConstrForeach(const ConstrForeach* object, uint32_t vpiRelation) {}
  virtual void leaveConstrForeach(const ConstrForeach* object, uint32_t vpiRelation) {}

  virtual void enterConstrIf(const ConstrIf* object, uint32_t vpiRelation) {}
  virtual void leaveConstrIf(const ConstrIf* object, uint32_t vpiRelation) {}

  virtual void enterConstrIfElse(const ConstrIfElse* object, uint32_t vpiRelation) {}
  virtual void leaveConstrIfElse(const ConstrIfElse* object, uint32_t vpiRelation) {}

  virtual void enterConstraint(const Constraint* object, uint32_t vpiRelation) {}
  virtual void leaveConstraint(const Constraint* object, uint32_t vpiRelation) {}

  virtual void enterConstraintOrdering(const ConstraintOrdering* object, uint32_t vpiRelation) {}
  virtual void leaveConstraintOrdering(const ConstraintOrdering* object, uint32_t vpiRelation) {}

  virtual void enterContAssign(const ContAssign* object, uint32_t vpiRelation) {}
  virtual void leaveContAssign(const ContAssign* object, uint32_t vpiRelation) {}

  virtual void enterContAssignBit(const ContAssignBit* object, uint32_t vpiRelation) {}
  virtual void leaveContAssignBit(const ContAssignBit* object, uint32_t vpiRelation) {}

  virtual void enterContinueStmt(const ContinueStmt* object, uint32_t vpiRelation) {}
  virtual void leaveContinueStmt(const ContinueStmt* object, uint32_t vpiRelation) {}

  virtual void enterCover(const Cover* object, uint32_t vpiRelation) {}
  virtual void leaveCover(const Cover* object, uint32_t vpiRelation) {}

  virtual void enterCoverBin(const CoverBin* object, uint32_t vpiRelation) {}
  virtual void leaveCoverBin(const CoverBin* object, uint32_t vpiRelation) {}

  virtual void enterCoverCross(const CoverCross* object, uint32_t vpiRelation) {}
  virtual void leaveCoverCross(const CoverCross* object, uint32_t vpiRelation) {}

  virtual void enterCoverGroup(const CoverGroup* object, uint32_t vpiRelation) {}
  virtual void leaveCoverGroup(const CoverGroup* object, uint32_t vpiRelation) {}

  virtual void enterCoverPoint(const CoverPoint* object, uint32_t vpiRelation) {}
  virtual void leaveCoverPoint(const CoverPoint* object, uint32_t vpiRelation) {}

  virtual void enterCoverageOption(const CoverageOption* object, uint32_t vpiRelation) {}
  virtual void leaveCoverageOption(const CoverageOption* object, uint32_t vpiRelation) {}

  virtual void enterDeassign(const Deassign* object, uint32_t vpiRelation) {}
  virtual void leaveDeassign(const Deassign* object, uint32_t vpiRelation) {}

  virtual void enterDefParam(const DefParam* object, uint32_t vpiRelation) {}
  virtual void leaveDefParam(const DefParam* object, uint32_t vpiRelation) {}

  virtual void enterDelayControl(const DelayControl* object, uint32_t vpiRelation) {}
  virtual void leaveDelayControl(const DelayControl* object, uint32_t vpiRelation) {}

  virtual void enterDelayTerm(const DelayTerm* object, uint32_t vpiRelation) {}
  virtual void leaveDelayTerm(const DelayTerm* object, uint32_t vpiRelation) {}

  virtual void enterDesign(const Design* object, uint32_t vpiRelation) {}
  virtual void leaveDesign(const Design* object, uint32_t vpiRelation) {}

  virtual void enterDisable(const Disable* object, uint32_t vpiRelation) {}
  virtual void leaveDisable(const Disable* object, uint32_t vpiRelation) {}

  virtual void enterDisableFork(const DisableFork* object, uint32_t vpiRelation) {}
  virtual void leaveDisableFork(const DisableFork* object, uint32_t vpiRelation) {}

  virtual void enterDistItem(const DistItem* object, uint32_t vpiRelation) {}
  virtual void leaveDistItem(const DistItem* object, uint32_t vpiRelation) {}

  virtual void enterDistribution(const Distribution* object, uint32_t vpiRelation) {}
  virtual void leaveDistribution(const Distribution* object, uint32_t vpiRelation) {}

  virtual void enterDoWhile(const DoWhile* object, uint32_t vpiRelation) {}
  virtual void leaveDoWhile(const DoWhile* object, uint32_t vpiRelation) {}

  virtual void enterEnumConst(const EnumConst* object, uint32_t vpiRelation) {}
  virtual void leaveEnumConst(const EnumConst* object, uint32_t vpiRelation) {}

  virtual void enterEnumTypespec(const EnumTypespec* object, uint32_t vpiRelation) {}
  virtual void leaveEnumTypespec(const EnumTypespec* object, uint32_t vpiRelation) {}

  virtual void enterEventControl(const EventControl* object, uint32_t vpiRelation) {}
  virtual void leaveEventControl(const EventControl* object, uint32_t vpiRelation) {}

  virtual void enterEventStmt(const EventStmt* object, uint32_t vpiRelation) {}
  virtual void leaveEventStmt(const EventStmt* object, uint32_t vpiRelation) {}

  virtual void enterEventTypespec(const EventTypespec* object, uint32_t vpiRelation) {}
  virtual void leaveEventTypespec(const EventTypespec* object, uint32_t vpiRelation) {}

  virtual void enterExpectStmt(const ExpectStmt* object, uint32_t vpiRelation) {}
  virtual void leaveExpectStmt(const ExpectStmt* object, uint32_t vpiRelation) {}

  virtual void enterExtends(const Extends* object, uint32_t vpiRelation) {}
  virtual void leaveExtends(const Extends* object, uint32_t vpiRelation) {}

  virtual void enterFinalStmt(const FinalStmt* object, uint32_t vpiRelation) {}
  virtual void leaveFinalStmt(const FinalStmt* object, uint32_t vpiRelation) {}

  virtual void enterForStmt(const ForStmt* object, uint32_t vpiRelation) {}
  virtual void leaveForStmt(const ForStmt* object, uint32_t vpiRelation) {}

  virtual void enterForce(const Force* object, uint32_t vpiRelation) {}
  virtual void leaveForce(const Force* object, uint32_t vpiRelation) {}

  virtual void enterForeachStmt(const ForeachStmt* object, uint32_t vpiRelation) {}
  virtual void leaveForeachStmt(const ForeachStmt* object, uint32_t vpiRelation) {}

  virtual void enterForeverStmt(const ForeverStmt* object, uint32_t vpiRelation) {}
  virtual void leaveForeverStmt(const ForeverStmt* object, uint32_t vpiRelation) {}

  virtual void enterForkStmt(const ForkStmt* object, uint32_t vpiRelation) {}
  virtual void leaveForkStmt(const ForkStmt* object, uint32_t vpiRelation) {}

  virtual void enterFuncCall(const FuncCall* object, uint32_t vpiRelation) {}
  virtual void leaveFuncCall(const FuncCall* object, uint32_t vpiRelation) {}

  virtual void enterFunction(const Function* object, uint32_t vpiRelation) {}
  virtual void leaveFunction(const Function* object, uint32_t vpiRelation) {}

  virtual void enterFunctionDecl(const FunctionDecl* object, uint32_t vpiRelation) {}
  virtual void leaveFunctionDecl(const FunctionDecl* object, uint32_t vpiRelation) {}

  virtual void enterGate(const Gate* object, uint32_t vpiRelation) {}
  virtual void leaveGate(const Gate* object, uint32_t vpiRelation) {}

  virtual void enterGateArray(const GateArray* object, uint32_t vpiRelation) {}
  virtual void leaveGateArray(const GateArray* object, uint32_t vpiRelation) {}

  virtual void enterGenCase(const GenCase* object, uint32_t vpiRelation) {}
  virtual void leaveGenCase(const GenCase* object, uint32_t vpiRelation) {}

  virtual void enterGenFor(const GenFor* object, uint32_t vpiRelation) {}
  virtual void leaveGenFor(const GenFor* object, uint32_t vpiRelation) {}

  virtual void enterGenIf(const GenIf* object, uint32_t vpiRelation) {}
  virtual void leaveGenIf(const GenIf* object, uint32_t vpiRelation) {}

  virtual void enterGenIfElse(const GenIfElse* object, uint32_t vpiRelation) {}
  virtual void leaveGenIfElse(const GenIfElse* object, uint32_t vpiRelation) {}

  virtual void enterGenRegion(const GenRegion* object, uint32_t vpiRelation) {}
  virtual void leaveGenRegion(const GenRegion* object, uint32_t vpiRelation) {}

  virtual void enterGenScopeArray(const GenScopeArray* object, uint32_t vpiRelation) {}
  virtual void leaveGenScopeArray(const GenScopeArray* object, uint32_t vpiRelation) {}

  virtual void enterHierPath(const HierPath* object, uint32_t vpiRelation) {}
  virtual void leaveHierPath(const HierPath* object, uint32_t vpiRelation) {}

  virtual void enterIODecl(const IODecl* object, uint32_t vpiRelation) {}
  virtual void leaveIODecl(const IODecl* object, uint32_t vpiRelation) {}

  virtual void enterIdentifier(const Identifier* object, uint32_t vpiRelation) {}
  virtual void leaveIdentifier(const Identifier* object, uint32_t vpiRelation) {}

  virtual void enterIfElse(const IfElse* object, uint32_t vpiRelation) {}
  virtual void leaveIfElse(const IfElse* object, uint32_t vpiRelation) {}

  virtual void enterIfStmt(const IfStmt* object, uint32_t vpiRelation) {}
  virtual void leaveIfStmt(const IfStmt* object, uint32_t vpiRelation) {}

  virtual void enterImmediateAssert(const ImmediateAssert* object, uint32_t vpiRelation) {}
  virtual void leaveImmediateAssert(const ImmediateAssert* object, uint32_t vpiRelation) {}

  virtual void enterImmediateAssume(const ImmediateAssume* object, uint32_t vpiRelation) {}
  virtual void leaveImmediateAssume(const ImmediateAssume* object, uint32_t vpiRelation) {}

  virtual void enterImmediateCover(const ImmediateCover* object, uint32_t vpiRelation) {}
  virtual void leaveImmediateCover(const ImmediateCover* object, uint32_t vpiRelation) {}

  virtual void enterImplements(const Implements* object, uint32_t vpiRelation) {}
  virtual void leaveImplements(const Implements* object, uint32_t vpiRelation) {}

  virtual void enterImplication(const Implication* object, uint32_t vpiRelation) {}
  virtual void leaveImplication(const Implication* object, uint32_t vpiRelation) {}

  virtual void enterImportTypespec(const ImportTypespec* object, uint32_t vpiRelation) {}
  virtual void leaveImportTypespec(const ImportTypespec* object, uint32_t vpiRelation) {}

  virtual void enterIncludeStmt(const IncludeStmt* object, uint32_t vpiRelation) {}
  virtual void leaveIncludeStmt(const IncludeStmt* object, uint32_t vpiRelation) {}

  virtual void enterIndexedPartSelect(const IndexedPartSelect* object, uint32_t vpiRelation) {}
  virtual void leaveIndexedPartSelect(const IndexedPartSelect* object, uint32_t vpiRelation) {}

  virtual void enterInitial(const Initial* object, uint32_t vpiRelation) {}
  virtual void leaveInitial(const Initial* object, uint32_t vpiRelation) {}

  virtual void enterIntTypespec(const IntTypespec* object, uint32_t vpiRelation) {}
  virtual void leaveIntTypespec(const IntTypespec* object, uint32_t vpiRelation) {}

  virtual void enterIntegerTypespec(const IntegerTypespec* object, uint32_t vpiRelation) {}
  virtual void leaveIntegerTypespec(const IntegerTypespec* object, uint32_t vpiRelation) {}

  virtual void enterInterface(const Interface* object, uint32_t vpiRelation) {}
  virtual void leaveInterface(const Interface* object, uint32_t vpiRelation) {}

  virtual void enterInterfaceArray(const InterfaceArray* object, uint32_t vpiRelation) {}
  virtual void leaveInterfaceArray(const InterfaceArray* object, uint32_t vpiRelation) {}

  virtual void enterInterfaceTFDecl(const InterfaceTFDecl* object, uint32_t vpiRelation) {}
  virtual void leaveInterfaceTFDecl(const InterfaceTFDecl* object, uint32_t vpiRelation) {}

  virtual void enterInterfaceTypespec(const InterfaceTypespec* object, uint32_t vpiRelation) {}
  virtual void leaveInterfaceTypespec(const InterfaceTypespec* object, uint32_t vpiRelation) {}

  virtual void enterLetDecl(const LetDecl* object, uint32_t vpiRelation) {}
  virtual void leaveLetDecl(const LetDecl* object, uint32_t vpiRelation) {}

  virtual void enterLetExpr(const LetExpr* object, uint32_t vpiRelation) {}
  virtual void leaveLetExpr(const LetExpr* object, uint32_t vpiRelation) {}

  virtual void enterLibrary(const Library* object, uint32_t vpiRelation) {}
  virtual void leaveLibrary(const Library* object, uint32_t vpiRelation) {}

  virtual void enterLogicTypespec(const LogicTypespec* object, uint32_t vpiRelation) {}
  virtual void leaveLogicTypespec(const LogicTypespec* object, uint32_t vpiRelation) {}

  virtual void enterLongIntTypespec(const LongIntTypespec* object, uint32_t vpiRelation) {}
  virtual void leaveLongIntTypespec(const LongIntTypespec* object, uint32_t vpiRelation) {}

  virtual void enterMethodFuncCall(const MethodFuncCall* object, uint32_t vpiRelation) {}
  virtual void leaveMethodFuncCall(const MethodFuncCall* object, uint32_t vpiRelation) {}

  virtual void enterMethodTaskCall(const MethodTaskCall* object, uint32_t vpiRelation) {}
  virtual void leaveMethodTaskCall(const MethodTaskCall* object, uint32_t vpiRelation) {}

  virtual void enterModPath(const ModPath* object, uint32_t vpiRelation) {}
  virtual void leaveModPath(const ModPath* object, uint32_t vpiRelation) {}

  virtual void enterModport(const Modport* object, uint32_t vpiRelation) {}
  virtual void leaveModport(const Modport* object, uint32_t vpiRelation) {}

  virtual void enterModule(const Module* object, uint32_t vpiRelation) {}
  virtual void leaveModule(const Module* object, uint32_t vpiRelation) {}

  virtual void enterModuleArray(const ModuleArray* object, uint32_t vpiRelation) {}
  virtual void leaveModuleArray(const ModuleArray* object, uint32_t vpiRelation) {}

  virtual void enterModuleTypespec(const ModuleTypespec* object, uint32_t vpiRelation) {}
  virtual void leaveModuleTypespec(const ModuleTypespec* object, uint32_t vpiRelation) {}

  virtual void enterMulticlockSequenceExpr(const MulticlockSequenceExpr* object, uint32_t vpiRelation) {}
  virtual void leaveMulticlockSequenceExpr(const MulticlockSequenceExpr* object, uint32_t vpiRelation) {}

  virtual void enterNamedEvent(const NamedEvent* object, uint32_t vpiRelation) {}
  virtual void leaveNamedEvent(const NamedEvent* object, uint32_t vpiRelation) {}

  virtual void enterNamedEventArray(const NamedEventArray* object, uint32_t vpiRelation) {}
  virtual void leaveNamedEventArray(const NamedEventArray* object, uint32_t vpiRelation) {}

  virtual void enterNet(const Net* object, uint32_t vpiRelation) {}
  virtual void leaveNet(const Net* object, uint32_t vpiRelation) {}

  virtual void enterNullStmt(const NullStmt* object, uint32_t vpiRelation) {}
  virtual void leaveNullStmt(const NullStmt* object, uint32_t vpiRelation) {}

  virtual void enterOperation(const Operation* object, uint32_t vpiRelation) {}
  virtual void leaveOperation(const Operation* object, uint32_t vpiRelation) {}

  virtual void enterOrderedWait(const OrderedWait* object, uint32_t vpiRelation) {}
  virtual void leaveOrderedWait(const OrderedWait* object, uint32_t vpiRelation) {}

  virtual void enterPackage(const Package* object, uint32_t vpiRelation) {}
  virtual void leavePackage(const Package* object, uint32_t vpiRelation) {}

  virtual void enterPackageTypespec(const PackageTypespec* object, uint32_t vpiRelation) {}
  virtual void leavePackageTypespec(const PackageTypespec* object, uint32_t vpiRelation) {}

  virtual void enterParamAssign(const ParamAssign* object, uint32_t vpiRelation) {}
  virtual void leaveParamAssign(const ParamAssign* object, uint32_t vpiRelation) {}

  virtual void enterParameter(const Parameter* object, uint32_t vpiRelation) {}
  virtual void leaveParameter(const Parameter* object, uint32_t vpiRelation) {}

  virtual void enterPartSelect(const PartSelect* object, uint32_t vpiRelation) {}
  virtual void leavePartSelect(const PartSelect* object, uint32_t vpiRelation) {}

  virtual void enterPathTerm(const PathTerm* object, uint32_t vpiRelation) {}
  virtual void leavePathTerm(const PathTerm* object, uint32_t vpiRelation) {}

  virtual void enterPort(const Port* object, uint32_t vpiRelation) {}
  virtual void leavePort(const Port* object, uint32_t vpiRelation) {}

  virtual void enterPortBit(const PortBit* object, uint32_t vpiRelation) {}
  virtual void leavePortBit(const PortBit* object, uint32_t vpiRelation) {}

  virtual void enterPreprocMacroDefinition(const PreprocMacroDefinition* object, uint32_t vpiRelation) {}
  virtual void leavePreprocMacroDefinition(const PreprocMacroDefinition* object, uint32_t vpiRelation) {}

  virtual void enterPreprocMacroInstance(const PreprocMacroInstance* object, uint32_t vpiRelation) {}
  virtual void leavePreprocMacroInstance(const PreprocMacroInstance* object, uint32_t vpiRelation) {}

  virtual void enterPrimTerm(const PrimTerm* object, uint32_t vpiRelation) {}
  virtual void leavePrimTerm(const PrimTerm* object, uint32_t vpiRelation) {}

  virtual void enterProgram(const Program* object, uint32_t vpiRelation) {}
  virtual void leaveProgram(const Program* object, uint32_t vpiRelation) {}

  virtual void enterProgramArray(const ProgramArray* object, uint32_t vpiRelation) {}
  virtual void leaveProgramArray(const ProgramArray* object, uint32_t vpiRelation) {}

  virtual void enterProgramTypespec(const ProgramTypespec* object, uint32_t vpiRelation) {}
  virtual void leaveProgramTypespec(const ProgramTypespec* object, uint32_t vpiRelation) {}

  virtual void enterPropFormalDecl(const PropFormalDecl* object, uint32_t vpiRelation) {}
  virtual void leavePropFormalDecl(const PropFormalDecl* object, uint32_t vpiRelation) {}

  virtual void enterPropertyDecl(const PropertyDecl* object, uint32_t vpiRelation) {}
  virtual void leavePropertyDecl(const PropertyDecl* object, uint32_t vpiRelation) {}

  virtual void enterPropertyInst(const PropertyInst* object, uint32_t vpiRelation) {}
  virtual void leavePropertyInst(const PropertyInst* object, uint32_t vpiRelation) {}

  virtual void enterPropertySpec(const PropertySpec* object, uint32_t vpiRelation) {}
  virtual void leavePropertySpec(const PropertySpec* object, uint32_t vpiRelation) {}

  virtual void enterPropertyTypespec(const PropertyTypespec* object, uint32_t vpiRelation) {}
  virtual void leavePropertyTypespec(const PropertyTypespec* object, uint32_t vpiRelation) {}

  virtual void enterRange(const Range* object, uint32_t vpiRelation) {}
  virtual void leaveRange(const Range* object, uint32_t vpiRelation) {}

  virtual void enterRealTypespec(const RealTypespec* object, uint32_t vpiRelation) {}
  virtual void leaveRealTypespec(const RealTypespec* object, uint32_t vpiRelation) {}

  virtual void enterRefInstance(const RefInstance* object, uint32_t vpiRelation) {}
  virtual void leaveRefInstance(const RefInstance* object, uint32_t vpiRelation) {}

  virtual void enterRefObj(const RefObj* object, uint32_t vpiRelation) {}
  virtual void leaveRefObj(const RefObj* object, uint32_t vpiRelation) {}

  virtual void enterRefTypespec(const RefTypespec* object, uint32_t vpiRelation) {}
  virtual void leaveRefTypespec(const RefTypespec* object, uint32_t vpiRelation) {}

  virtual void enterReg(const Reg* object, uint32_t vpiRelation) {}
  virtual void leaveReg(const Reg* object, uint32_t vpiRelation) {}

  virtual void enterRegArray(const RegArray* object, uint32_t vpiRelation) {}
  virtual void leaveRegArray(const RegArray* object, uint32_t vpiRelation) {}

  virtual void enterRelease(const Release* object, uint32_t vpiRelation) {}
  virtual void leaveRelease(const Release* object, uint32_t vpiRelation) {}

  virtual void enterRepeat(const Repeat* object, uint32_t vpiRelation) {}
  virtual void leaveRepeat(const Repeat* object, uint32_t vpiRelation) {}

  virtual void enterRepeatControl(const RepeatControl* object, uint32_t vpiRelation) {}
  virtual void leaveRepeatControl(const RepeatControl* object, uint32_t vpiRelation) {}

  virtual void enterRestrict(const Restrict* object, uint32_t vpiRelation) {}
  virtual void leaveRestrict(const Restrict* object, uint32_t vpiRelation) {}

  virtual void enterReturnStmt(const ReturnStmt* object, uint32_t vpiRelation) {}
  virtual void leaveReturnStmt(const ReturnStmt* object, uint32_t vpiRelation) {}

  virtual void enterSeqFormalDecl(const SeqFormalDecl* object, uint32_t vpiRelation) {}
  virtual void leaveSeqFormalDecl(const SeqFormalDecl* object, uint32_t vpiRelation) {}

  virtual void enterSequenceDecl(const SequenceDecl* object, uint32_t vpiRelation) {}
  virtual void leaveSequenceDecl(const SequenceDecl* object, uint32_t vpiRelation) {}

  virtual void enterSequenceInst(const SequenceInst* object, uint32_t vpiRelation) {}
  virtual void leaveSequenceInst(const SequenceInst* object, uint32_t vpiRelation) {}

  virtual void enterSequenceTypespec(const SequenceTypespec* object, uint32_t vpiRelation) {}
  virtual void leaveSequenceTypespec(const SequenceTypespec* object, uint32_t vpiRelation) {}

  virtual void enterShortIntTypespec(const ShortIntTypespec* object, uint32_t vpiRelation) {}
  virtual void leaveShortIntTypespec(const ShortIntTypespec* object, uint32_t vpiRelation) {}

  virtual void enterShortRealTypespec(const ShortRealTypespec* object, uint32_t vpiRelation) {}
  virtual void leaveShortRealTypespec(const ShortRealTypespec* object, uint32_t vpiRelation) {}

  virtual void enterSoftDisable(const SoftDisable* object, uint32_t vpiRelation) {}
  virtual void leaveSoftDisable(const SoftDisable* object, uint32_t vpiRelation) {}

  virtual void enterSourceFile(const SourceFile* object, uint32_t vpiRelation) {}
  virtual void leaveSourceFile(const SourceFile* object, uint32_t vpiRelation) {}

  virtual void enterSpecParam(const SpecParam* object, uint32_t vpiRelation) {}
  virtual void leaveSpecParam(const SpecParam* object, uint32_t vpiRelation) {}

  virtual void enterStringTypespec(const StringTypespec* object, uint32_t vpiRelation) {}
  virtual void leaveStringTypespec(const StringTypespec* object, uint32_t vpiRelation) {}

  virtual void enterStructPattern(const StructPattern* object, uint32_t vpiRelation) {}
  virtual void leaveStructPattern(const StructPattern* object, uint32_t vpiRelation) {}

  virtual void enterStructTypespec(const StructTypespec* object, uint32_t vpiRelation) {}
  virtual void leaveStructTypespec(const StructTypespec* object, uint32_t vpiRelation) {}

  virtual void enterSwitchArray(const SwitchArray* object, uint32_t vpiRelation) {}
  virtual void leaveSwitchArray(const SwitchArray* object, uint32_t vpiRelation) {}

  virtual void enterSwitchTran(const SwitchTran* object, uint32_t vpiRelation) {}
  virtual void leaveSwitchTran(const SwitchTran* object, uint32_t vpiRelation) {}

  virtual void enterSysFuncCall(const SysFuncCall* object, uint32_t vpiRelation) {}
  virtual void leaveSysFuncCall(const SysFuncCall* object, uint32_t vpiRelation) {}

  virtual void enterSysTaskCall(const SysTaskCall* object, uint32_t vpiRelation) {}
  virtual void leaveSysTaskCall(const SysTaskCall* object, uint32_t vpiRelation) {}

  virtual void enterTableEntry(const TableEntry* object, uint32_t vpiRelation) {}
  virtual void leaveTableEntry(const TableEntry* object, uint32_t vpiRelation) {}

  virtual void enterTaggedPattern(const TaggedPattern* object, uint32_t vpiRelation) {}
  virtual void leaveTaggedPattern(const TaggedPattern* object, uint32_t vpiRelation) {}

  virtual void enterTask(const Task* object, uint32_t vpiRelation) {}
  virtual void leaveTask(const Task* object, uint32_t vpiRelation) {}

  virtual void enterTaskCall(const TaskCall* object, uint32_t vpiRelation) {}
  virtual void leaveTaskCall(const TaskCall* object, uint32_t vpiRelation) {}

  virtual void enterTaskDecl(const TaskDecl* object, uint32_t vpiRelation) {}
  virtual void leaveTaskDecl(const TaskDecl* object, uint32_t vpiRelation) {}

  virtual void enterTchk(const Tchk* object, uint32_t vpiRelation) {}
  virtual void leaveTchk(const Tchk* object, uint32_t vpiRelation) {}

  virtual void enterTchkTerm(const TchkTerm* object, uint32_t vpiRelation) {}
  virtual void leaveTchkTerm(const TchkTerm* object, uint32_t vpiRelation) {}

  virtual void enterThread(const Thread* object, uint32_t vpiRelation) {}
  virtual void leaveThread(const Thread* object, uint32_t vpiRelation) {}

  virtual void enterTimeTypespec(const TimeTypespec* object, uint32_t vpiRelation) {}
  virtual void leaveTimeTypespec(const TimeTypespec* object, uint32_t vpiRelation) {}

  virtual void enterTypeParameter(const TypeParameter* object, uint32_t vpiRelation) {}
  virtual void leaveTypeParameter(const TypeParameter* object, uint32_t vpiRelation) {}

  virtual void enterTypedefTypespec(const TypedefTypespec* object, uint32_t vpiRelation) {}
  virtual void leaveTypedefTypespec(const TypedefTypespec* object, uint32_t vpiRelation) {}

  virtual void enterTypespecMember(const TypespecMember* object, uint32_t vpiRelation) {}
  virtual void leaveTypespecMember(const TypespecMember* object, uint32_t vpiRelation) {}

  virtual void enterUdp(const Udp* object, uint32_t vpiRelation) {}
  virtual void leaveUdp(const Udp* object, uint32_t vpiRelation) {}

  virtual void enterUdpArray(const UdpArray* object, uint32_t vpiRelation) {}
  virtual void leaveUdpArray(const UdpArray* object, uint32_t vpiRelation) {}

  virtual void enterUdpDefn(const UdpDefn* object, uint32_t vpiRelation) {}
  virtual void leaveUdpDefn(const UdpDefn* object, uint32_t vpiRelation) {}

  virtual void enterUdpDefnTypespec(const UdpDefnTypespec* object, uint32_t vpiRelation) {}
  virtual void leaveUdpDefnTypespec(const UdpDefnTypespec* object, uint32_t vpiRelation) {}

  virtual void enterUnionTypespec(const UnionTypespec* object, uint32_t vpiRelation) {}
  virtual void leaveUnionTypespec(const UnionTypespec* object, uint32_t vpiRelation) {}

  virtual void enterUniqueness(const Uniqueness* object, uint32_t vpiRelation) {}
  virtual void leaveUniqueness(const Uniqueness* object, uint32_t vpiRelation) {}

  virtual void enterUnsupportedExpr(const UnsupportedExpr* object, uint32_t vpiRelation) {}
  virtual void leaveUnsupportedExpr(const UnsupportedExpr* object, uint32_t vpiRelation) {}

  virtual void enterUnsupportedStmt(const UnsupportedStmt* object, uint32_t vpiRelation) {}
  virtual void leaveUnsupportedStmt(const UnsupportedStmt* object, uint32_t vpiRelation) {}

  virtual void enterUnsupportedTypespec(const UnsupportedTypespec* object, uint32_t vpiRelation) {}
  virtual void leaveUnsupportedTypespec(const UnsupportedTypespec* object, uint32_t vpiRelation) {}

  virtual void enterUserSystf(const UserSystf* object, uint32_t vpiRelation) {}
  virtual void leaveUserSystf(const UserSystf* object, uint32_t vpiRelation) {}

  virtual void enterVarSelect(const VarSelect* object, uint32_t vpiRelation) {}
  virtual void leaveVarSelect(const VarSelect* object, uint32_t vpiRelation) {}

  virtual void enterVariable(const Variable* object, uint32_t vpiRelation) {}
  virtual void leaveVariable(const Variable* object, uint32_t vpiRelation) {}

  virtual void enterVoidTypespec(const VoidTypespec* object, uint32_t vpiRelation) {}
  virtual void leaveVoidTypespec(const VoidTypespec* object, uint32_t vpiRelation) {}

  virtual void enterWaitFork(const WaitFork* object, uint32_t vpiRelation) {}
  virtual void leaveWaitFork(const WaitFork* object, uint32_t vpiRelation) {}

  virtual void enterWaitStmt(const WaitStmt* object, uint32_t vpiRelation) {}
  virtual void leaveWaitStmt(const WaitStmt* object, uint32_t vpiRelation) {}

  virtual void enterWhileStmt(const WhileStmt* object, uint32_t vpiRelation) {}
  virtual void leaveWhileStmt(const WhileStmt* object, uint32_t vpiRelation) {}

  virtual void enterAliasCollection(const Any* object, const AliasCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveAliasCollection(const Any* object, const AliasCollection& objects, uint32_t vpiRelation) {}

  virtual void enterAlwaysCollection(const Any* object, const AlwaysCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveAlwaysCollection(const Any* object, const AlwaysCollection& objects, uint32_t vpiRelation) {}

  virtual void enterAnyCollection(const Any* object, const AnyCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveAnyCollection(const Any* object, const AnyCollection& objects, uint32_t vpiRelation) {}

  virtual void enterAnyPatternCollection(const Any* object, const AnyPatternCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveAnyPatternCollection(const Any* object, const AnyPatternCollection& objects, uint32_t vpiRelation) {}

  virtual void enterArrayExprCollection(const Any* object, const ArrayExprCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveArrayExprCollection(const Any* object, const ArrayExprCollection& objects, uint32_t vpiRelation) {}

  virtual void enterArrayTypespecCollection(const Any* object, const ArrayTypespecCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveArrayTypespecCollection(const Any* object, const ArrayTypespecCollection& objects, uint32_t vpiRelation) {}

  virtual void enterAssertCollection(const Any* object, const AssertCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveAssertCollection(const Any* object, const AssertCollection& objects, uint32_t vpiRelation) {}

  virtual void enterAssignStmtCollection(const Any* object, const AssignStmtCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveAssignStmtCollection(const Any* object, const AssignStmtCollection& objects, uint32_t vpiRelation) {}

  virtual void enterAssignmentCollection(const Any* object, const AssignmentCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveAssignmentCollection(const Any* object, const AssignmentCollection& objects, uint32_t vpiRelation) {}

  virtual void enterAssumeCollection(const Any* object, const AssumeCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveAssumeCollection(const Any* object, const AssumeCollection& objects, uint32_t vpiRelation) {}

  virtual void enterAtomicStmtCollection(const Any* object, const AtomicStmtCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveAtomicStmtCollection(const Any* object, const AtomicStmtCollection& objects, uint32_t vpiRelation) {}

  virtual void enterAttributeCollection(const Any* object, const AttributeCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveAttributeCollection(const Any* object, const AttributeCollection& objects, uint32_t vpiRelation) {}

  virtual void enterBeginCollection(const Any* object, const BeginCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveBeginCollection(const Any* object, const BeginCollection& objects, uint32_t vpiRelation) {}

  virtual void enterBindDirectiveCollection(const Any* object, const BindDirectiveCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveBindDirectiveCollection(const Any* object, const BindDirectiveCollection& objects, uint32_t vpiRelation) {}

  virtual void enterBitSelectCollection(const Any* object, const BitSelectCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveBitSelectCollection(const Any* object, const BitSelectCollection& objects, uint32_t vpiRelation) {}

  virtual void enterBitTypespecCollection(const Any* object, const BitTypespecCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveBitTypespecCollection(const Any* object, const BitTypespecCollection& objects, uint32_t vpiRelation) {}

  virtual void enterBreakStmtCollection(const Any* object, const BreakStmtCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveBreakStmtCollection(const Any* object, const BreakStmtCollection& objects, uint32_t vpiRelation) {}

  virtual void enterByteTypespecCollection(const Any* object, const ByteTypespecCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveByteTypespecCollection(const Any* object, const ByteTypespecCollection& objects, uint32_t vpiRelation) {}

  virtual void enterCaseItemCollection(const Any* object, const CaseItemCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveCaseItemCollection(const Any* object, const CaseItemCollection& objects, uint32_t vpiRelation) {}

  virtual void enterCasePropertyCollection(const Any* object, const CasePropertyCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveCasePropertyCollection(const Any* object, const CasePropertyCollection& objects, uint32_t vpiRelation) {}

  virtual void enterCasePropertyItemCollection(const Any* object, const CasePropertyItemCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveCasePropertyItemCollection(const Any* object, const CasePropertyItemCollection& objects, uint32_t vpiRelation) {}

  virtual void enterCaseStmtCollection(const Any* object, const CaseStmtCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveCaseStmtCollection(const Any* object, const CaseStmtCollection& objects, uint32_t vpiRelation) {}

  virtual void enterChandleTypespecCollection(const Any* object, const ChandleTypespecCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveChandleTypespecCollection(const Any* object, const ChandleTypespecCollection& objects, uint32_t vpiRelation) {}

  virtual void enterCheckerDeclCollection(const Any* object, const CheckerDeclCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveCheckerDeclCollection(const Any* object, const CheckerDeclCollection& objects, uint32_t vpiRelation) {}

  virtual void enterCheckerInstCollection(const Any* object, const CheckerInstCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveCheckerInstCollection(const Any* object, const CheckerInstCollection& objects, uint32_t vpiRelation) {}

  virtual void enterCheckerInstPortCollection(const Any* object, const CheckerInstPortCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveCheckerInstPortCollection(const Any* object, const CheckerInstPortCollection& objects, uint32_t vpiRelation) {}

  virtual void enterCheckerPortCollection(const Any* object, const CheckerPortCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveCheckerPortCollection(const Any* object, const CheckerPortCollection& objects, uint32_t vpiRelation) {}

  virtual void enterClassDefnCollection(const Any* object, const ClassDefnCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveClassDefnCollection(const Any* object, const ClassDefnCollection& objects, uint32_t vpiRelation) {}

  virtual void enterClassObjCollection(const Any* object, const ClassObjCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveClassObjCollection(const Any* object, const ClassObjCollection& objects, uint32_t vpiRelation) {}

  virtual void enterClassTypespecCollection(const Any* object, const ClassTypespecCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveClassTypespecCollection(const Any* object, const ClassTypespecCollection& objects, uint32_t vpiRelation) {}

  virtual void enterClauseCollection(const Any* object, const ClauseCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveClauseCollection(const Any* object, const ClauseCollection& objects, uint32_t vpiRelation) {}

  virtual void enterClockedPropertyCollection(const Any* object, const ClockedPropertyCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveClockedPropertyCollection(const Any* object, const ClockedPropertyCollection& objects, uint32_t vpiRelation) {}

  virtual void enterClockedSeqCollection(const Any* object, const ClockedSeqCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveClockedSeqCollection(const Any* object, const ClockedSeqCollection& objects, uint32_t vpiRelation) {}

  virtual void enterClockingBlockCollection(const Any* object, const ClockingBlockCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveClockingBlockCollection(const Any* object, const ClockingBlockCollection& objects, uint32_t vpiRelation) {}

  virtual void enterClockingIODeclCollection(const Any* object, const ClockingIODeclCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveClockingIODeclCollection(const Any* object, const ClockingIODeclCollection& objects, uint32_t vpiRelation) {}

  virtual void enterCommentCollection(const Any* object, const CommentCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveCommentCollection(const Any* object, const CommentCollection& objects, uint32_t vpiRelation) {}

  virtual void enterConcurrentAssertionsCollection(const Any* object, const ConcurrentAssertionsCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveConcurrentAssertionsCollection(const Any* object, const ConcurrentAssertionsCollection& objects, uint32_t vpiRelation) {}

  virtual void enterConfigDeclCollection(const Any* object, const ConfigDeclCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveConfigDeclCollection(const Any* object, const ConfigDeclCollection& objects, uint32_t vpiRelation) {}

  virtual void enterConfigRuleCollection(const Any* object, const ConfigRuleCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveConfigRuleCollection(const Any* object, const ConfigRuleCollection& objects, uint32_t vpiRelation) {}

  virtual void enterConstantCollection(const Any* object, const ConstantCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveConstantCollection(const Any* object, const ConstantCollection& objects, uint32_t vpiRelation) {}

  virtual void enterConstrForeachCollection(const Any* object, const ConstrForeachCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveConstrForeachCollection(const Any* object, const ConstrForeachCollection& objects, uint32_t vpiRelation) {}

  virtual void enterConstrIfCollection(const Any* object, const ConstrIfCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveConstrIfCollection(const Any* object, const ConstrIfCollection& objects, uint32_t vpiRelation) {}

  virtual void enterConstrIfElseCollection(const Any* object, const ConstrIfElseCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveConstrIfElseCollection(const Any* object, const ConstrIfElseCollection& objects, uint32_t vpiRelation) {}

  virtual void enterConstraintCollection(const Any* object, const ConstraintCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveConstraintCollection(const Any* object, const ConstraintCollection& objects, uint32_t vpiRelation) {}

  virtual void enterConstraintExprCollection(const Any* object, const ConstraintExprCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveConstraintExprCollection(const Any* object, const ConstraintExprCollection& objects, uint32_t vpiRelation) {}

  virtual void enterConstraintOrderingCollection(const Any* object, const ConstraintOrderingCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveConstraintOrderingCollection(const Any* object, const ConstraintOrderingCollection& objects, uint32_t vpiRelation) {}

  virtual void enterContAssignCollection(const Any* object, const ContAssignCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveContAssignCollection(const Any* object, const ContAssignCollection& objects, uint32_t vpiRelation) {}

  virtual void enterContAssignBitCollection(const Any* object, const ContAssignBitCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveContAssignBitCollection(const Any* object, const ContAssignBitCollection& objects, uint32_t vpiRelation) {}

  virtual void enterContinueStmtCollection(const Any* object, const ContinueStmtCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveContinueStmtCollection(const Any* object, const ContinueStmtCollection& objects, uint32_t vpiRelation) {}

  virtual void enterCoverCollection(const Any* object, const CoverCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveCoverCollection(const Any* object, const CoverCollection& objects, uint32_t vpiRelation) {}

  virtual void enterCoverBinCollection(const Any* object, const CoverBinCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveCoverBinCollection(const Any* object, const CoverBinCollection& objects, uint32_t vpiRelation) {}

  virtual void enterCoverCrossCollection(const Any* object, const CoverCrossCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveCoverCrossCollection(const Any* object, const CoverCrossCollection& objects, uint32_t vpiRelation) {}

  virtual void enterCoverGroupCollection(const Any* object, const CoverGroupCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveCoverGroupCollection(const Any* object, const CoverGroupCollection& objects, uint32_t vpiRelation) {}

  virtual void enterCoverPointCollection(const Any* object, const CoverPointCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveCoverPointCollection(const Any* object, const CoverPointCollection& objects, uint32_t vpiRelation) {}

  virtual void enterCoverageOptionCollection(const Any* object, const CoverageOptionCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveCoverageOptionCollection(const Any* object, const CoverageOptionCollection& objects, uint32_t vpiRelation) {}

  virtual void enterDeassignCollection(const Any* object, const DeassignCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveDeassignCollection(const Any* object, const DeassignCollection& objects, uint32_t vpiRelation) {}

  virtual void enterDefParamCollection(const Any* object, const DefParamCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveDefParamCollection(const Any* object, const DefParamCollection& objects, uint32_t vpiRelation) {}

  virtual void enterDelayControlCollection(const Any* object, const DelayControlCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveDelayControlCollection(const Any* object, const DelayControlCollection& objects, uint32_t vpiRelation) {}

  virtual void enterDelayTermCollection(const Any* object, const DelayTermCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveDelayTermCollection(const Any* object, const DelayTermCollection& objects, uint32_t vpiRelation) {}

  virtual void enterDesignCollection(const Any* object, const DesignCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveDesignCollection(const Any* object, const DesignCollection& objects, uint32_t vpiRelation) {}

  virtual void enterDisableCollection(const Any* object, const DisableCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveDisableCollection(const Any* object, const DisableCollection& objects, uint32_t vpiRelation) {}

  virtual void enterDisableForkCollection(const Any* object, const DisableForkCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveDisableForkCollection(const Any* object, const DisableForkCollection& objects, uint32_t vpiRelation) {}

  virtual void enterDisablesCollection(const Any* object, const DisablesCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveDisablesCollection(const Any* object, const DisablesCollection& objects, uint32_t vpiRelation) {}

  virtual void enterDistItemCollection(const Any* object, const DistItemCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveDistItemCollection(const Any* object, const DistItemCollection& objects, uint32_t vpiRelation) {}

  virtual void enterDistributionCollection(const Any* object, const DistributionCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveDistributionCollection(const Any* object, const DistributionCollection& objects, uint32_t vpiRelation) {}

  virtual void enterDoWhileCollection(const Any* object, const DoWhileCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveDoWhileCollection(const Any* object, const DoWhileCollection& objects, uint32_t vpiRelation) {}

  virtual void enterEnumConstCollection(const Any* object, const EnumConstCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveEnumConstCollection(const Any* object, const EnumConstCollection& objects, uint32_t vpiRelation) {}

  virtual void enterEnumTypespecCollection(const Any* object, const EnumTypespecCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveEnumTypespecCollection(const Any* object, const EnumTypespecCollection& objects, uint32_t vpiRelation) {}

  virtual void enterEventControlCollection(const Any* object, const EventControlCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveEventControlCollection(const Any* object, const EventControlCollection& objects, uint32_t vpiRelation) {}

  virtual void enterEventStmtCollection(const Any* object, const EventStmtCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveEventStmtCollection(const Any* object, const EventStmtCollection& objects, uint32_t vpiRelation) {}

  virtual void enterEventTypespecCollection(const Any* object, const EventTypespecCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveEventTypespecCollection(const Any* object, const EventTypespecCollection& objects, uint32_t vpiRelation) {}

  virtual void enterExpectStmtCollection(const Any* object, const ExpectStmtCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveExpectStmtCollection(const Any* object, const ExpectStmtCollection& objects, uint32_t vpiRelation) {}

  virtual void enterExprCollection(const Any* object, const ExprCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveExprCollection(const Any* object, const ExprCollection& objects, uint32_t vpiRelation) {}

  virtual void enterExtendsCollection(const Any* object, const ExtendsCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveExtendsCollection(const Any* object, const ExtendsCollection& objects, uint32_t vpiRelation) {}

  virtual void enterFinalStmtCollection(const Any* object, const FinalStmtCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveFinalStmtCollection(const Any* object, const FinalStmtCollection& objects, uint32_t vpiRelation) {}

  virtual void enterForStmtCollection(const Any* object, const ForStmtCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveForStmtCollection(const Any* object, const ForStmtCollection& objects, uint32_t vpiRelation) {}

  virtual void enterForceCollection(const Any* object, const ForceCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveForceCollection(const Any* object, const ForceCollection& objects, uint32_t vpiRelation) {}

  virtual void enterForeachStmtCollection(const Any* object, const ForeachStmtCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveForeachStmtCollection(const Any* object, const ForeachStmtCollection& objects, uint32_t vpiRelation) {}

  virtual void enterForeverStmtCollection(const Any* object, const ForeverStmtCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveForeverStmtCollection(const Any* object, const ForeverStmtCollection& objects, uint32_t vpiRelation) {}

  virtual void enterForkStmtCollection(const Any* object, const ForkStmtCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveForkStmtCollection(const Any* object, const ForkStmtCollection& objects, uint32_t vpiRelation) {}

  virtual void enterFuncCallCollection(const Any* object, const FuncCallCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveFuncCallCollection(const Any* object, const FuncCallCollection& objects, uint32_t vpiRelation) {}

  virtual void enterFunctionCollection(const Any* object, const FunctionCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveFunctionCollection(const Any* object, const FunctionCollection& objects, uint32_t vpiRelation) {}

  virtual void enterFunctionDeclCollection(const Any* object, const FunctionDeclCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveFunctionDeclCollection(const Any* object, const FunctionDeclCollection& objects, uint32_t vpiRelation) {}

  virtual void enterGateCollection(const Any* object, const GateCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveGateCollection(const Any* object, const GateCollection& objects, uint32_t vpiRelation) {}

  virtual void enterGateArrayCollection(const Any* object, const GateArrayCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveGateArrayCollection(const Any* object, const GateArrayCollection& objects, uint32_t vpiRelation) {}

  virtual void enterGenCaseCollection(const Any* object, const GenCaseCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveGenCaseCollection(const Any* object, const GenCaseCollection& objects, uint32_t vpiRelation) {}

  virtual void enterGenForCollection(const Any* object, const GenForCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveGenForCollection(const Any* object, const GenForCollection& objects, uint32_t vpiRelation) {}

  virtual void enterGenIfCollection(const Any* object, const GenIfCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveGenIfCollection(const Any* object, const GenIfCollection& objects, uint32_t vpiRelation) {}

  virtual void enterGenIfElseCollection(const Any* object, const GenIfElseCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveGenIfElseCollection(const Any* object, const GenIfElseCollection& objects, uint32_t vpiRelation) {}

  virtual void enterGenRegionCollection(const Any* object, const GenRegionCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveGenRegionCollection(const Any* object, const GenRegionCollection& objects, uint32_t vpiRelation) {}

  virtual void enterGenScopeCollection(const Any* object, const GenScopeCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveGenScopeCollection(const Any* object, const GenScopeCollection& objects, uint32_t vpiRelation) {}

  virtual void enterGenScopeArrayCollection(const Any* object, const GenScopeArrayCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveGenScopeArrayCollection(const Any* object, const GenScopeArrayCollection& objects, uint32_t vpiRelation) {}

  virtual void enterGenStmtCollection(const Any* object, const GenStmtCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveGenStmtCollection(const Any* object, const GenStmtCollection& objects, uint32_t vpiRelation) {}

  virtual void enterHierPathCollection(const Any* object, const HierPathCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveHierPathCollection(const Any* object, const HierPathCollection& objects, uint32_t vpiRelation) {}

  virtual void enterIODeclCollection(const Any* object, const IODeclCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveIODeclCollection(const Any* object, const IODeclCollection& objects, uint32_t vpiRelation) {}

  virtual void enterIdentifierCollection(const Any* object, const IdentifierCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveIdentifierCollection(const Any* object, const IdentifierCollection& objects, uint32_t vpiRelation) {}

  virtual void enterIfElseCollection(const Any* object, const IfElseCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveIfElseCollection(const Any* object, const IfElseCollection& objects, uint32_t vpiRelation) {}

  virtual void enterIfStmtCollection(const Any* object, const IfStmtCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveIfStmtCollection(const Any* object, const IfStmtCollection& objects, uint32_t vpiRelation) {}

  virtual void enterImmediateAssertCollection(const Any* object, const ImmediateAssertCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveImmediateAssertCollection(const Any* object, const ImmediateAssertCollection& objects, uint32_t vpiRelation) {}

  virtual void enterImmediateAssumeCollection(const Any* object, const ImmediateAssumeCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveImmediateAssumeCollection(const Any* object, const ImmediateAssumeCollection& objects, uint32_t vpiRelation) {}

  virtual void enterImmediateCoverCollection(const Any* object, const ImmediateCoverCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveImmediateCoverCollection(const Any* object, const ImmediateCoverCollection& objects, uint32_t vpiRelation) {}

  virtual void enterImplementsCollection(const Any* object, const ImplementsCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveImplementsCollection(const Any* object, const ImplementsCollection& objects, uint32_t vpiRelation) {}

  virtual void enterImplicationCollection(const Any* object, const ImplicationCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveImplicationCollection(const Any* object, const ImplicationCollection& objects, uint32_t vpiRelation) {}

  virtual void enterImportTypespecCollection(const Any* object, const ImportTypespecCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveImportTypespecCollection(const Any* object, const ImportTypespecCollection& objects, uint32_t vpiRelation) {}

  virtual void enterIncludeStmtCollection(const Any* object, const IncludeStmtCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveIncludeStmtCollection(const Any* object, const IncludeStmtCollection& objects, uint32_t vpiRelation) {}

  virtual void enterIndexedPartSelectCollection(const Any* object, const IndexedPartSelectCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveIndexedPartSelectCollection(const Any* object, const IndexedPartSelectCollection& objects, uint32_t vpiRelation) {}

  virtual void enterInitialCollection(const Any* object, const InitialCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveInitialCollection(const Any* object, const InitialCollection& objects, uint32_t vpiRelation) {}

  virtual void enterInstanceCollection(const Any* object, const InstanceCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveInstanceCollection(const Any* object, const InstanceCollection& objects, uint32_t vpiRelation) {}

  virtual void enterInstanceArrayCollection(const Any* object, const InstanceArrayCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveInstanceArrayCollection(const Any* object, const InstanceArrayCollection& objects, uint32_t vpiRelation) {}

  virtual void enterIntTypespecCollection(const Any* object, const IntTypespecCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveIntTypespecCollection(const Any* object, const IntTypespecCollection& objects, uint32_t vpiRelation) {}

  virtual void enterIntegerTypespecCollection(const Any* object, const IntegerTypespecCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveIntegerTypespecCollection(const Any* object, const IntegerTypespecCollection& objects, uint32_t vpiRelation) {}

  virtual void enterInterfaceCollection(const Any* object, const InterfaceCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveInterfaceCollection(const Any* object, const InterfaceCollection& objects, uint32_t vpiRelation) {}

  virtual void enterInterfaceArrayCollection(const Any* object, const InterfaceArrayCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveInterfaceArrayCollection(const Any* object, const InterfaceArrayCollection& objects, uint32_t vpiRelation) {}

  virtual void enterInterfaceTFDeclCollection(const Any* object, const InterfaceTFDeclCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveInterfaceTFDeclCollection(const Any* object, const InterfaceTFDeclCollection& objects, uint32_t vpiRelation) {}

  virtual void enterInterfaceTypespecCollection(const Any* object, const InterfaceTypespecCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveInterfaceTypespecCollection(const Any* object, const InterfaceTypespecCollection& objects, uint32_t vpiRelation) {}

  virtual void enterLetDeclCollection(const Any* object, const LetDeclCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveLetDeclCollection(const Any* object, const LetDeclCollection& objects, uint32_t vpiRelation) {}

  virtual void enterLetExprCollection(const Any* object, const LetExprCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveLetExprCollection(const Any* object, const LetExprCollection& objects, uint32_t vpiRelation) {}

  virtual void enterLibraryCollection(const Any* object, const LibraryCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveLibraryCollection(const Any* object, const LibraryCollection& objects, uint32_t vpiRelation) {}

  virtual void enterLogicTypespecCollection(const Any* object, const LogicTypespecCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveLogicTypespecCollection(const Any* object, const LogicTypespecCollection& objects, uint32_t vpiRelation) {}

  virtual void enterLongIntTypespecCollection(const Any* object, const LongIntTypespecCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveLongIntTypespecCollection(const Any* object, const LongIntTypespecCollection& objects, uint32_t vpiRelation) {}

  virtual void enterMethodFuncCallCollection(const Any* object, const MethodFuncCallCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveMethodFuncCallCollection(const Any* object, const MethodFuncCallCollection& objects, uint32_t vpiRelation) {}

  virtual void enterMethodTaskCallCollection(const Any* object, const MethodTaskCallCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveMethodTaskCallCollection(const Any* object, const MethodTaskCallCollection& objects, uint32_t vpiRelation) {}

  virtual void enterModPathCollection(const Any* object, const ModPathCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveModPathCollection(const Any* object, const ModPathCollection& objects, uint32_t vpiRelation) {}

  virtual void enterModportCollection(const Any* object, const ModportCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveModportCollection(const Any* object, const ModportCollection& objects, uint32_t vpiRelation) {}

  virtual void enterModuleCollection(const Any* object, const ModuleCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveModuleCollection(const Any* object, const ModuleCollection& objects, uint32_t vpiRelation) {}

  virtual void enterModuleArrayCollection(const Any* object, const ModuleArrayCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveModuleArrayCollection(const Any* object, const ModuleArrayCollection& objects, uint32_t vpiRelation) {}

  virtual void enterModuleTypespecCollection(const Any* object, const ModuleTypespecCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveModuleTypespecCollection(const Any* object, const ModuleTypespecCollection& objects, uint32_t vpiRelation) {}

  virtual void enterMulticlockSequenceExprCollection(const Any* object, const MulticlockSequenceExprCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveMulticlockSequenceExprCollection(const Any* object, const MulticlockSequenceExprCollection& objects, uint32_t vpiRelation) {}

  virtual void enterNamedEventCollection(const Any* object, const NamedEventCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveNamedEventCollection(const Any* object, const NamedEventCollection& objects, uint32_t vpiRelation) {}

  virtual void enterNamedEventArrayCollection(const Any* object, const NamedEventArrayCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveNamedEventArrayCollection(const Any* object, const NamedEventArrayCollection& objects, uint32_t vpiRelation) {}

  virtual void enterNetCollection(const Any* object, const NetCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveNetCollection(const Any* object, const NetCollection& objects, uint32_t vpiRelation) {}

  virtual void enterNullStmtCollection(const Any* object, const NullStmtCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveNullStmtCollection(const Any* object, const NullStmtCollection& objects, uint32_t vpiRelation) {}

  virtual void enterOperationCollection(const Any* object, const OperationCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveOperationCollection(const Any* object, const OperationCollection& objects, uint32_t vpiRelation) {}

  virtual void enterOrderedWaitCollection(const Any* object, const OrderedWaitCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveOrderedWaitCollection(const Any* object, const OrderedWaitCollection& objects, uint32_t vpiRelation) {}

  virtual void enterPackageCollection(const Any* object, const PackageCollection& objects, uint32_t vpiRelation) {}
  virtual void leavePackageCollection(const Any* object, const PackageCollection& objects, uint32_t vpiRelation) {}

  virtual void enterPackageTypespecCollection(const Any* object, const PackageTypespecCollection& objects, uint32_t vpiRelation) {}
  virtual void leavePackageTypespecCollection(const Any* object, const PackageTypespecCollection& objects, uint32_t vpiRelation) {}

  virtual void enterParamAssignCollection(const Any* object, const ParamAssignCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveParamAssignCollection(const Any* object, const ParamAssignCollection& objects, uint32_t vpiRelation) {}

  virtual void enterParameterCollection(const Any* object, const ParameterCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveParameterCollection(const Any* object, const ParameterCollection& objects, uint32_t vpiRelation) {}

  virtual void enterPartSelectCollection(const Any* object, const PartSelectCollection& objects, uint32_t vpiRelation) {}
  virtual void leavePartSelectCollection(const Any* object, const PartSelectCollection& objects, uint32_t vpiRelation) {}

  virtual void enterPathTermCollection(const Any* object, const PathTermCollection& objects, uint32_t vpiRelation) {}
  virtual void leavePathTermCollection(const Any* object, const PathTermCollection& objects, uint32_t vpiRelation) {}

  virtual void enterPortCollection(const Any* object, const PortCollection& objects, uint32_t vpiRelation) {}
  virtual void leavePortCollection(const Any* object, const PortCollection& objects, uint32_t vpiRelation) {}

  virtual void enterPortBitCollection(const Any* object, const PortBitCollection& objects, uint32_t vpiRelation) {}
  virtual void leavePortBitCollection(const Any* object, const PortBitCollection& objects, uint32_t vpiRelation) {}

  virtual void enterPortsCollection(const Any* object, const PortsCollection& objects, uint32_t vpiRelation) {}
  virtual void leavePortsCollection(const Any* object, const PortsCollection& objects, uint32_t vpiRelation) {}

  virtual void enterPreprocMacroDefinitionCollection(const Any* object, const PreprocMacroDefinitionCollection& objects, uint32_t vpiRelation) {}
  virtual void leavePreprocMacroDefinitionCollection(const Any* object, const PreprocMacroDefinitionCollection& objects, uint32_t vpiRelation) {}

  virtual void enterPreprocMacroInstanceCollection(const Any* object, const PreprocMacroInstanceCollection& objects, uint32_t vpiRelation) {}
  virtual void leavePreprocMacroInstanceCollection(const Any* object, const PreprocMacroInstanceCollection& objects, uint32_t vpiRelation) {}

  virtual void enterPrimTermCollection(const Any* object, const PrimTermCollection& objects, uint32_t vpiRelation) {}
  virtual void leavePrimTermCollection(const Any* object, const PrimTermCollection& objects, uint32_t vpiRelation) {}

  virtual void enterPrimitiveCollection(const Any* object, const PrimitiveCollection& objects, uint32_t vpiRelation) {}
  virtual void leavePrimitiveCollection(const Any* object, const PrimitiveCollection& objects, uint32_t vpiRelation) {}

  virtual void enterPrimitiveArrayCollection(const Any* object, const PrimitiveArrayCollection& objects, uint32_t vpiRelation) {}
  virtual void leavePrimitiveArrayCollection(const Any* object, const PrimitiveArrayCollection& objects, uint32_t vpiRelation) {}

  virtual void enterProcessCollection(const Any* object, const ProcessCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveProcessCollection(const Any* object, const ProcessCollection& objects, uint32_t vpiRelation) {}

  virtual void enterProgramCollection(const Any* object, const ProgramCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveProgramCollection(const Any* object, const ProgramCollection& objects, uint32_t vpiRelation) {}

  virtual void enterProgramArrayCollection(const Any* object, const ProgramArrayCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveProgramArrayCollection(const Any* object, const ProgramArrayCollection& objects, uint32_t vpiRelation) {}

  virtual void enterProgramTypespecCollection(const Any* object, const ProgramTypespecCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveProgramTypespecCollection(const Any* object, const ProgramTypespecCollection& objects, uint32_t vpiRelation) {}

  virtual void enterPropFormalDeclCollection(const Any* object, const PropFormalDeclCollection& objects, uint32_t vpiRelation) {}
  virtual void leavePropFormalDeclCollection(const Any* object, const PropFormalDeclCollection& objects, uint32_t vpiRelation) {}

  virtual void enterPropertyDeclCollection(const Any* object, const PropertyDeclCollection& objects, uint32_t vpiRelation) {}
  virtual void leavePropertyDeclCollection(const Any* object, const PropertyDeclCollection& objects, uint32_t vpiRelation) {}

  virtual void enterPropertyInstCollection(const Any* object, const PropertyInstCollection& objects, uint32_t vpiRelation) {}
  virtual void leavePropertyInstCollection(const Any* object, const PropertyInstCollection& objects, uint32_t vpiRelation) {}

  virtual void enterPropertySpecCollection(const Any* object, const PropertySpecCollection& objects, uint32_t vpiRelation) {}
  virtual void leavePropertySpecCollection(const Any* object, const PropertySpecCollection& objects, uint32_t vpiRelation) {}

  virtual void enterPropertyTypespecCollection(const Any* object, const PropertyTypespecCollection& objects, uint32_t vpiRelation) {}
  virtual void leavePropertyTypespecCollection(const Any* object, const PropertyTypespecCollection& objects, uint32_t vpiRelation) {}

  virtual void enterRangeCollection(const Any* object, const RangeCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveRangeCollection(const Any* object, const RangeCollection& objects, uint32_t vpiRelation) {}

  virtual void enterRealTypespecCollection(const Any* object, const RealTypespecCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveRealTypespecCollection(const Any* object, const RealTypespecCollection& objects, uint32_t vpiRelation) {}

  virtual void enterRefInstanceCollection(const Any* object, const RefInstanceCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveRefInstanceCollection(const Any* object, const RefInstanceCollection& objects, uint32_t vpiRelation) {}

  virtual void enterRefObjCollection(const Any* object, const RefObjCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveRefObjCollection(const Any* object, const RefObjCollection& objects, uint32_t vpiRelation) {}

  virtual void enterRefTypespecCollection(const Any* object, const RefTypespecCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveRefTypespecCollection(const Any* object, const RefTypespecCollection& objects, uint32_t vpiRelation) {}

  virtual void enterRegCollection(const Any* object, const RegCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveRegCollection(const Any* object, const RegCollection& objects, uint32_t vpiRelation) {}

  virtual void enterRegArrayCollection(const Any* object, const RegArrayCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveRegArrayCollection(const Any* object, const RegArrayCollection& objects, uint32_t vpiRelation) {}

  virtual void enterReleaseCollection(const Any* object, const ReleaseCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveReleaseCollection(const Any* object, const ReleaseCollection& objects, uint32_t vpiRelation) {}

  virtual void enterRepeatCollection(const Any* object, const RepeatCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveRepeatCollection(const Any* object, const RepeatCollection& objects, uint32_t vpiRelation) {}

  virtual void enterRepeatControlCollection(const Any* object, const RepeatControlCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveRepeatControlCollection(const Any* object, const RepeatControlCollection& objects, uint32_t vpiRelation) {}

  virtual void enterRestrictCollection(const Any* object, const RestrictCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveRestrictCollection(const Any* object, const RestrictCollection& objects, uint32_t vpiRelation) {}

  virtual void enterReturnStmtCollection(const Any* object, const ReturnStmtCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveReturnStmtCollection(const Any* object, const ReturnStmtCollection& objects, uint32_t vpiRelation) {}

  virtual void enterScopeCollection(const Any* object, const ScopeCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveScopeCollection(const Any* object, const ScopeCollection& objects, uint32_t vpiRelation) {}

  virtual void enterSelectCollection(const Any* object, const SelectCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveSelectCollection(const Any* object, const SelectCollection& objects, uint32_t vpiRelation) {}

  virtual void enterSeqFormalDeclCollection(const Any* object, const SeqFormalDeclCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveSeqFormalDeclCollection(const Any* object, const SeqFormalDeclCollection& objects, uint32_t vpiRelation) {}

  virtual void enterSequenceDeclCollection(const Any* object, const SequenceDeclCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveSequenceDeclCollection(const Any* object, const SequenceDeclCollection& objects, uint32_t vpiRelation) {}

  virtual void enterSequenceInstCollection(const Any* object, const SequenceInstCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveSequenceInstCollection(const Any* object, const SequenceInstCollection& objects, uint32_t vpiRelation) {}

  virtual void enterSequenceTypespecCollection(const Any* object, const SequenceTypespecCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveSequenceTypespecCollection(const Any* object, const SequenceTypespecCollection& objects, uint32_t vpiRelation) {}

  virtual void enterShortIntTypespecCollection(const Any* object, const ShortIntTypespecCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveShortIntTypespecCollection(const Any* object, const ShortIntTypespecCollection& objects, uint32_t vpiRelation) {}

  virtual void enterShortRealTypespecCollection(const Any* object, const ShortRealTypespecCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveShortRealTypespecCollection(const Any* object, const ShortRealTypespecCollection& objects, uint32_t vpiRelation) {}

  virtual void enterSimpleExprCollection(const Any* object, const SimpleExprCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveSimpleExprCollection(const Any* object, const SimpleExprCollection& objects, uint32_t vpiRelation) {}

  virtual void enterSoftDisableCollection(const Any* object, const SoftDisableCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveSoftDisableCollection(const Any* object, const SoftDisableCollection& objects, uint32_t vpiRelation) {}

  virtual void enterSourceFileCollection(const Any* object, const SourceFileCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveSourceFileCollection(const Any* object, const SourceFileCollection& objects, uint32_t vpiRelation) {}

  virtual void enterSpecParamCollection(const Any* object, const SpecParamCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveSpecParamCollection(const Any* object, const SpecParamCollection& objects, uint32_t vpiRelation) {}

  virtual void enterStringTypespecCollection(const Any* object, const StringTypespecCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveStringTypespecCollection(const Any* object, const StringTypespecCollection& objects, uint32_t vpiRelation) {}

  virtual void enterStructPatternCollection(const Any* object, const StructPatternCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveStructPatternCollection(const Any* object, const StructPatternCollection& objects, uint32_t vpiRelation) {}

  virtual void enterStructTypespecCollection(const Any* object, const StructTypespecCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveStructTypespecCollection(const Any* object, const StructTypespecCollection& objects, uint32_t vpiRelation) {}

  virtual void enterSwitchArrayCollection(const Any* object, const SwitchArrayCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveSwitchArrayCollection(const Any* object, const SwitchArrayCollection& objects, uint32_t vpiRelation) {}

  virtual void enterSwitchTranCollection(const Any* object, const SwitchTranCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveSwitchTranCollection(const Any* object, const SwitchTranCollection& objects, uint32_t vpiRelation) {}

  virtual void enterSysFuncCallCollection(const Any* object, const SysFuncCallCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveSysFuncCallCollection(const Any* object, const SysFuncCallCollection& objects, uint32_t vpiRelation) {}

  virtual void enterSysTaskCallCollection(const Any* object, const SysTaskCallCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveSysTaskCallCollection(const Any* object, const SysTaskCallCollection& objects, uint32_t vpiRelation) {}

  virtual void enterTFCallCollection(const Any* object, const TFCallCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveTFCallCollection(const Any* object, const TFCallCollection& objects, uint32_t vpiRelation) {}

  virtual void enterTableEntryCollection(const Any* object, const TableEntryCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveTableEntryCollection(const Any* object, const TableEntryCollection& objects, uint32_t vpiRelation) {}

  virtual void enterTaggedPatternCollection(const Any* object, const TaggedPatternCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveTaggedPatternCollection(const Any* object, const TaggedPatternCollection& objects, uint32_t vpiRelation) {}

  virtual void enterTaskCollection(const Any* object, const TaskCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveTaskCollection(const Any* object, const TaskCollection& objects, uint32_t vpiRelation) {}

  virtual void enterTaskCallCollection(const Any* object, const TaskCallCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveTaskCallCollection(const Any* object, const TaskCallCollection& objects, uint32_t vpiRelation) {}

  virtual void enterTaskDeclCollection(const Any* object, const TaskDeclCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveTaskDeclCollection(const Any* object, const TaskDeclCollection& objects, uint32_t vpiRelation) {}

  virtual void enterTaskFuncCollection(const Any* object, const TaskFuncCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveTaskFuncCollection(const Any* object, const TaskFuncCollection& objects, uint32_t vpiRelation) {}

  virtual void enterTaskFuncDeclCollection(const Any* object, const TaskFuncDeclCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveTaskFuncDeclCollection(const Any* object, const TaskFuncDeclCollection& objects, uint32_t vpiRelation) {}

  virtual void enterTchkCollection(const Any* object, const TchkCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveTchkCollection(const Any* object, const TchkCollection& objects, uint32_t vpiRelation) {}

  virtual void enterTchkTermCollection(const Any* object, const TchkTermCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveTchkTermCollection(const Any* object, const TchkTermCollection& objects, uint32_t vpiRelation) {}

  virtual void enterThreadCollection(const Any* object, const ThreadCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveThreadCollection(const Any* object, const ThreadCollection& objects, uint32_t vpiRelation) {}

  virtual void enterTimeTypespecCollection(const Any* object, const TimeTypespecCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveTimeTypespecCollection(const Any* object, const TimeTypespecCollection& objects, uint32_t vpiRelation) {}

  virtual void enterTypeParameterCollection(const Any* object, const TypeParameterCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveTypeParameterCollection(const Any* object, const TypeParameterCollection& objects, uint32_t vpiRelation) {}

  virtual void enterTypedefTypespecCollection(const Any* object, const TypedefTypespecCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveTypedefTypespecCollection(const Any* object, const TypedefTypespecCollection& objects, uint32_t vpiRelation) {}

  virtual void enterTypespecCollection(const Any* object, const TypespecCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveTypespecCollection(const Any* object, const TypespecCollection& objects, uint32_t vpiRelation) {}

  virtual void enterTypespecMemberCollection(const Any* object, const TypespecMemberCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveTypespecMemberCollection(const Any* object, const TypespecMemberCollection& objects, uint32_t vpiRelation) {}

  virtual void enterUdpCollection(const Any* object, const UdpCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveUdpCollection(const Any* object, const UdpCollection& objects, uint32_t vpiRelation) {}

  virtual void enterUdpArrayCollection(const Any* object, const UdpArrayCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveUdpArrayCollection(const Any* object, const UdpArrayCollection& objects, uint32_t vpiRelation) {}

  virtual void enterUdpDefnCollection(const Any* object, const UdpDefnCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveUdpDefnCollection(const Any* object, const UdpDefnCollection& objects, uint32_t vpiRelation) {}

  virtual void enterUdpDefnTypespecCollection(const Any* object, const UdpDefnTypespecCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveUdpDefnTypespecCollection(const Any* object, const UdpDefnTypespecCollection& objects, uint32_t vpiRelation) {}

  virtual void enterUnionTypespecCollection(const Any* object, const UnionTypespecCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveUnionTypespecCollection(const Any* object, const UnionTypespecCollection& objects, uint32_t vpiRelation) {}

  virtual void enterUniquenessCollection(const Any* object, const UniquenessCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveUniquenessCollection(const Any* object, const UniquenessCollection& objects, uint32_t vpiRelation) {}

  virtual void enterUnsupportedExprCollection(const Any* object, const UnsupportedExprCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveUnsupportedExprCollection(const Any* object, const UnsupportedExprCollection& objects, uint32_t vpiRelation) {}

  virtual void enterUnsupportedStmtCollection(const Any* object, const UnsupportedStmtCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveUnsupportedStmtCollection(const Any* object, const UnsupportedStmtCollection& objects, uint32_t vpiRelation) {}

  virtual void enterUnsupportedTypespecCollection(const Any* object, const UnsupportedTypespecCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveUnsupportedTypespecCollection(const Any* object, const UnsupportedTypespecCollection& objects, uint32_t vpiRelation) {}

  virtual void enterUserSystfCollection(const Any* object, const UserSystfCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveUserSystfCollection(const Any* object, const UserSystfCollection& objects, uint32_t vpiRelation) {}

  virtual void enterVarSelectCollection(const Any* object, const VarSelectCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveVarSelectCollection(const Any* object, const VarSelectCollection& objects, uint32_t vpiRelation) {}

  virtual void enterVariableCollection(const Any* object, const VariableCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveVariableCollection(const Any* object, const VariableCollection& objects, uint32_t vpiRelation) {}

  virtual void enterVoidTypespecCollection(const Any* object, const VoidTypespecCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveVoidTypespecCollection(const Any* object, const VoidTypespecCollection& objects, uint32_t vpiRelation) {}

  virtual void enterWaitForkCollection(const Any* object, const WaitForkCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveWaitForkCollection(const Any* object, const WaitForkCollection& objects, uint32_t vpiRelation) {}

  virtual void enterWaitStmtCollection(const Any* object, const WaitStmtCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveWaitStmtCollection(const Any* object, const WaitStmtCollection& objects, uint32_t vpiRelation) {}

  virtual void enterWaitsCollection(const Any* object, const WaitsCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveWaitsCollection(const Any* object, const WaitsCollection& objects, uint32_t vpiRelation) {}

  virtual void enterWhileStmtCollection(const Any* object, const WhileStmtCollection& objects, uint32_t vpiRelation) {}
  virtual void leaveWhileStmtCollection(const Any* object, const WhileStmtCollection& objects, uint32_t vpiRelation) {}

private:
  void listenAny_(const Any* object);
  void listenAlias_(const Alias* object);
  void listenAlways_(const Always* object);
  void listenAnyPattern_(const AnyPattern* object);
  void listenArrayExpr_(const ArrayExpr* object);
  void listenArrayTypespec_(const ArrayTypespec* object);
  void listenAssert_(const Assert* object);
  void listenAssignStmt_(const AssignStmt* object);
  void listenAssignment_(const Assignment* object);
  void listenAssume_(const Assume* object);
  void listenAtomicStmt_(const AtomicStmt* object);
  void listenAttribute_(const Attribute* object);
  void listenBegin_(const Begin* object);
  void listenBindDirective_(const BindDirective* object);
  void listenBitSelect_(const BitSelect* object);
  void listenBitTypespec_(const BitTypespec* object);
  void listenBreakStmt_(const BreakStmt* object);
  void listenByteTypespec_(const ByteTypespec* object);
  void listenCaseItem_(const CaseItem* object);
  void listenCasePropertyItem_(const CasePropertyItem* object);
  void listenCaseProperty_(const CaseProperty* object);
  void listenCaseStmt_(const CaseStmt* object);
  void listenChandleTypespec_(const ChandleTypespec* object);
  void listenCheckerDecl_(const CheckerDecl* object);
  void listenCheckerInstPort_(const CheckerInstPort* object);
  void listenCheckerInst_(const CheckerInst* object);
  void listenCheckerPort_(const CheckerPort* object);
  void listenClassDefn_(const ClassDefn* object);
  void listenClassObj_(const ClassObj* object);
  void listenClassTypespec_(const ClassTypespec* object);
  void listenClause_(const Clause* object);
  void listenClockedProperty_(const ClockedProperty* object);
  void listenClockedSeq_(const ClockedSeq* object);
  void listenClockingBlock_(const ClockingBlock* object);
  void listenClockingIODecl_(const ClockingIODecl* object);
  void listenComment_(const Comment* object);
  void listenConcurrentAssertions_(const ConcurrentAssertions* object);
  void listenConfigDecl_(const ConfigDecl* object);
  void listenConfigRule_(const ConfigRule* object);
  void listenConstant_(const Constant* object);
  void listenConstrForeach_(const ConstrForeach* object);
  void listenConstrIfElse_(const ConstrIfElse* object);
  void listenConstrIf_(const ConstrIf* object);
  void listenConstraintExpr_(const ConstraintExpr* object);
  void listenConstraintOrdering_(const ConstraintOrdering* object);
  void listenConstraint_(const Constraint* object);
  void listenContAssignBit_(const ContAssignBit* object);
  void listenContAssign_(const ContAssign* object);
  void listenContinueStmt_(const ContinueStmt* object);
  void listenCoverBin_(const CoverBin* object);
  void listenCoverCross_(const CoverCross* object);
  void listenCoverGroup_(const CoverGroup* object);
  void listenCoverPoint_(const CoverPoint* object);
  void listenCover_(const Cover* object);
  void listenCoverageOption_(const CoverageOption* object);
  void listenDeassign_(const Deassign* object);
  void listenDefParam_(const DefParam* object);
  void listenDelayControl_(const DelayControl* object);
  void listenDelayTerm_(const DelayTerm* object);
  void listenDesign_(const Design* object);
  void listenDisableFork_(const DisableFork* object);
  void listenDisable_(const Disable* object);
  void listenDisables_(const Disables* object);
  void listenDistItem_(const DistItem* object);
  void listenDistribution_(const Distribution* object);
  void listenDoWhile_(const DoWhile* object);
  void listenEnumConst_(const EnumConst* object);
  void listenEnumTypespec_(const EnumTypespec* object);
  void listenEventControl_(const EventControl* object);
  void listenEventStmt_(const EventStmt* object);
  void listenEventTypespec_(const EventTypespec* object);
  void listenExpectStmt_(const ExpectStmt* object);
  void listenExpr_(const Expr* object);
  void listenExtends_(const Extends* object);
  void listenFinalStmt_(const FinalStmt* object);
  void listenForStmt_(const ForStmt* object);
  void listenForce_(const Force* object);
  void listenForeachStmt_(const ForeachStmt* object);
  void listenForeverStmt_(const ForeverStmt* object);
  void listenForkStmt_(const ForkStmt* object);
  void listenFuncCall_(const FuncCall* object);
  void listenFunctionDecl_(const FunctionDecl* object);
  void listenFunction_(const Function* object);
  void listenGateArray_(const GateArray* object);
  void listenGate_(const Gate* object);
  void listenGenCase_(const GenCase* object);
  void listenGenFor_(const GenFor* object);
  void listenGenIfElse_(const GenIfElse* object);
  void listenGenIf_(const GenIf* object);
  void listenGenRegion_(const GenRegion* object);
  void listenGenScopeArray_(const GenScopeArray* object);
  void listenGenScope_(const GenScope* object);
  void listenGenStmt_(const GenStmt* object);
  void listenHierPath_(const HierPath* object);
  void listenIODecl_(const IODecl* object);
  void listenIdentifier_(const Identifier* object);
  void listenIfElse_(const IfElse* object);
  void listenIfStmt_(const IfStmt* object);
  void listenImmediateAssert_(const ImmediateAssert* object);
  void listenImmediateAssume_(const ImmediateAssume* object);
  void listenImmediateCover_(const ImmediateCover* object);
  void listenImplements_(const Implements* object);
  void listenImplication_(const Implication* object);
  void listenImportTypespec_(const ImportTypespec* object);
  void listenIncludeStmt_(const IncludeStmt* object);
  void listenIndexedPartSelect_(const IndexedPartSelect* object);
  void listenInitial_(const Initial* object);
  void listenInstanceArray_(const InstanceArray* object);
  void listenInstance_(const Instance* object);
  void listenIntTypespec_(const IntTypespec* object);
  void listenIntegerTypespec_(const IntegerTypespec* object);
  void listenInterfaceArray_(const InterfaceArray* object);
  void listenInterfaceTFDecl_(const InterfaceTFDecl* object);
  void listenInterfaceTypespec_(const InterfaceTypespec* object);
  void listenInterface_(const Interface* object);
  void listenLetDecl_(const LetDecl* object);
  void listenLetExpr_(const LetExpr* object);
  void listenLibrary_(const Library* object);
  void listenLogicTypespec_(const LogicTypespec* object);
  void listenLongIntTypespec_(const LongIntTypespec* object);
  void listenMethodFuncCall_(const MethodFuncCall* object);
  void listenMethodTaskCall_(const MethodTaskCall* object);
  void listenModPath_(const ModPath* object);
  void listenModport_(const Modport* object);
  void listenModuleArray_(const ModuleArray* object);
  void listenModuleTypespec_(const ModuleTypespec* object);
  void listenModule_(const Module* object);
  void listenMulticlockSequenceExpr_(const MulticlockSequenceExpr* object);
  void listenNamedEventArray_(const NamedEventArray* object);
  void listenNamedEvent_(const NamedEvent* object);
  void listenNet_(const Net* object);
  void listenNullStmt_(const NullStmt* object);
  void listenOperation_(const Operation* object);
  void listenOrderedWait_(const OrderedWait* object);
  void listenPackageTypespec_(const PackageTypespec* object);
  void listenPackage_(const Package* object);
  void listenParamAssign_(const ParamAssign* object);
  void listenParameter_(const Parameter* object);
  void listenPartSelect_(const PartSelect* object);
  void listenPathTerm_(const PathTerm* object);
  void listenPortBit_(const PortBit* object);
  void listenPort_(const Port* object);
  void listenPorts_(const Ports* object);
  void listenPreprocMacroDefinition_(const PreprocMacroDefinition* object);
  void listenPreprocMacroInstance_(const PreprocMacroInstance* object);
  void listenPrimTerm_(const PrimTerm* object);
  void listenPrimitiveArray_(const PrimitiveArray* object);
  void listenPrimitive_(const Primitive* object);
  void listenProcess_(const Process* object);
  void listenProgramArray_(const ProgramArray* object);
  void listenProgramTypespec_(const ProgramTypespec* object);
  void listenProgram_(const Program* object);
  void listenPropFormalDecl_(const PropFormalDecl* object);
  void listenPropertyDecl_(const PropertyDecl* object);
  void listenPropertyInst_(const PropertyInst* object);
  void listenPropertySpec_(const PropertySpec* object);
  void listenPropertyTypespec_(const PropertyTypespec* object);
  void listenRange_(const Range* object);
  void listenRealTypespec_(const RealTypespec* object);
  void listenRefInstance_(const RefInstance* object);
  void listenRefObj_(const RefObj* object);
  void listenRefTypespec_(const RefTypespec* object);
  void listenRegArray_(const RegArray* object);
  void listenReg_(const Reg* object);
  void listenRelease_(const Release* object);
  void listenRepeatControl_(const RepeatControl* object);
  void listenRepeat_(const Repeat* object);
  void listenRestrict_(const Restrict* object);
  void listenReturnStmt_(const ReturnStmt* object);
  void listenScope_(const Scope* object);
  void listenSelect_(const Select* object);
  void listenSeqFormalDecl_(const SeqFormalDecl* object);
  void listenSequenceDecl_(const SequenceDecl* object);
  void listenSequenceInst_(const SequenceInst* object);
  void listenSequenceTypespec_(const SequenceTypespec* object);
  void listenShortIntTypespec_(const ShortIntTypespec* object);
  void listenShortRealTypespec_(const ShortRealTypespec* object);
  void listenSimpleExpr_(const SimpleExpr* object);
  void listenSoftDisable_(const SoftDisable* object);
  void listenSourceFile_(const SourceFile* object);
  void listenSpecParam_(const SpecParam* object);
  void listenStringTypespec_(const StringTypespec* object);
  void listenStructPattern_(const StructPattern* object);
  void listenStructTypespec_(const StructTypespec* object);
  void listenSwitchArray_(const SwitchArray* object);
  void listenSwitchTran_(const SwitchTran* object);
  void listenSysFuncCall_(const SysFuncCall* object);
  void listenSysTaskCall_(const SysTaskCall* object);
  void listenTFCall_(const TFCall* object);
  void listenTableEntry_(const TableEntry* object);
  void listenTaggedPattern_(const TaggedPattern* object);
  void listenTaskCall_(const TaskCall* object);
  void listenTaskDecl_(const TaskDecl* object);
  void listenTaskFuncDecl_(const TaskFuncDecl* object);
  void listenTaskFunc_(const TaskFunc* object);
  void listenTask_(const Task* object);
  void listenTchkTerm_(const TchkTerm* object);
  void listenTchk_(const Tchk* object);
  void listenThread_(const Thread* object);
  void listenTimeTypespec_(const TimeTypespec* object);
  void listenTypeParameter_(const TypeParameter* object);
  void listenTypedefTypespec_(const TypedefTypespec* object);
  void listenTypespecMember_(const TypespecMember* object);
  void listenTypespec_(const Typespec* object);
  void listenUdpArray_(const UdpArray* object);
  void listenUdpDefnTypespec_(const UdpDefnTypespec* object);
  void listenUdpDefn_(const UdpDefn* object);
  void listenUdp_(const Udp* object);
  void listenUnionTypespec_(const UnionTypespec* object);
  void listenUniqueness_(const Uniqueness* object);
  void listenUnsupportedExpr_(const UnsupportedExpr* object);
  void listenUnsupportedStmt_(const UnsupportedStmt* object);
  void listenUnsupportedTypespec_(const UnsupportedTypespec* object);
  void listenUserSystf_(const UserSystf* object);
  void listenVarSelect_(const VarSelect* object);
  void listenVariable_(const Variable* object);
  void listenVoidTypespec_(const VoidTypespec* object);
  void listenWaitFork_(const WaitFork* object);
  void listenWaitStmt_(const WaitStmt* object);
  void listenWaits_(const Waits* object);
  void listenWhileStmt_(const WhileStmt* object);

protected:
  AnySet m_visited;
  any_stack_t m_callstack;
  bool m_abortRequested = false;
};

class UhdmListenerTracer : public UhdmListener {
  public:
    UhdmListenerTracer(std::ostream &strm) : strm(strm) {}
    ~UhdmListenerTracer() final = default;

    void enterAttribute(const Attribute* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveAttribute(const Attribute* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterIdentifier(const Identifier* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveIdentifier(const Identifier* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterComment(const Comment* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveComment(const Comment* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterLetDecl(const LetDecl* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveLetDecl(const LetDecl* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterAlways(const Always* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveAlways(const Always* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterFinalStmt(const FinalStmt* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveFinalStmt(const FinalStmt* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterInitial(const Initial* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveInitial(const Initial* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterDelayControl(const DelayControl* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveDelayControl(const DelayControl* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterDelayTerm(const DelayTerm* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveDelayTerm(const DelayTerm* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterEventControl(const EventControl* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveEventControl(const EventControl* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterRepeatControl(const RepeatControl* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveRepeatControl(const RepeatControl* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterBegin(const Begin* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveBegin(const Begin* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterForkStmt(const ForkStmt* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveForkStmt(const ForkStmt* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterForStmt(const ForStmt* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveForStmt(const ForStmt* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterIfStmt(const IfStmt* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveIfStmt(const IfStmt* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterEventStmt(const EventStmt* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveEventStmt(const EventStmt* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterThread(const Thread* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveThread(const Thread* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterForeverStmt(const ForeverStmt* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveForeverStmt(const ForeverStmt* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterWaitStmt(const WaitStmt* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveWaitStmt(const WaitStmt* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterWaitFork(const WaitFork* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveWaitFork(const WaitFork* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterOrderedWait(const OrderedWait* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveOrderedWait(const OrderedWait* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterDisable(const Disable* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveDisable(const Disable* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterDisableFork(const DisableFork* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveDisableFork(const DisableFork* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterContinueStmt(const ContinueStmt* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveContinueStmt(const ContinueStmt* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterBreakStmt(const BreakStmt* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveBreakStmt(const BreakStmt* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterReturnStmt(const ReturnStmt* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveReturnStmt(const ReturnStmt* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterWhileStmt(const WhileStmt* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveWhileStmt(const WhileStmt* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterRepeat(const Repeat* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveRepeat(const Repeat* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterDoWhile(const DoWhile* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveDoWhile(const DoWhile* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterIfElse(const IfElse* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveIfElse(const IfElse* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterCaseStmt(const CaseStmt* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveCaseStmt(const CaseStmt* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterForce(const Force* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveForce(const Force* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterAssignStmt(const AssignStmt* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveAssignStmt(const AssignStmt* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterDeassign(const Deassign* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveDeassign(const Deassign* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterRelease(const Release* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveRelease(const Release* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterNullStmt(const NullStmt* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveNullStmt(const NullStmt* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterExpectStmt(const ExpectStmt* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveExpectStmt(const ExpectStmt* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterForeachStmt(const ForeachStmt* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveForeachStmt(const ForeachStmt* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterGenScopeArray(const GenScopeArray* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveGenScopeArray(const GenScopeArray* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterAssert(const Assert* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveAssert(const Assert* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterCover(const Cover* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveCover(const Cover* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterAssume(const Assume* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveAssume(const Assume* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterRestrict(const Restrict* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveRestrict(const Restrict* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterImmediateAssert(const ImmediateAssert* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveImmediateAssert(const ImmediateAssert* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterImmediateAssume(const ImmediateAssume* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveImmediateAssume(const ImmediateAssume* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterImmediateCover(const ImmediateCover* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveImmediateCover(const ImmediateCover* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterCaseItem(const CaseItem* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveCaseItem(const CaseItem* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterAssignment(const Assignment* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveAssignment(const Assignment* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterAnyPattern(const AnyPattern* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveAnyPattern(const AnyPattern* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterTaggedPattern(const TaggedPattern* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveTaggedPattern(const TaggedPattern* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterStructPattern(const StructPattern* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveStructPattern(const StructPattern* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterUnsupportedExpr(const UnsupportedExpr* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveUnsupportedExpr(const UnsupportedExpr* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterUnsupportedStmt(const UnsupportedStmt* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveUnsupportedStmt(const UnsupportedStmt* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterPreprocMacroDefinition(const PreprocMacroDefinition* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leavePreprocMacroDefinition(const PreprocMacroDefinition* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterPreprocMacroInstance(const PreprocMacroInstance* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leavePreprocMacroInstance(const PreprocMacroInstance* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterSourceFile(const SourceFile* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveSourceFile(const SourceFile* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterSequenceInst(const SequenceInst* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveSequenceInst(const SequenceInst* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterSeqFormalDecl(const SeqFormalDecl* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveSeqFormalDecl(const SeqFormalDecl* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterSequenceDecl(const SequenceDecl* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveSequenceDecl(const SequenceDecl* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterPropFormalDecl(const PropFormalDecl* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leavePropFormalDecl(const PropFormalDecl* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterPropertyInst(const PropertyInst* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leavePropertyInst(const PropertyInst* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterPropertySpec(const PropertySpec* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leavePropertySpec(const PropertySpec* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterPropertyDecl(const PropertyDecl* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leavePropertyDecl(const PropertyDecl* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterClockedProperty(const ClockedProperty* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveClockedProperty(const ClockedProperty* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterCasePropertyItem(const CasePropertyItem* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveCasePropertyItem(const CasePropertyItem* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterCaseProperty(const CaseProperty* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveCaseProperty(const CaseProperty* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterMulticlockSequenceExpr(const MulticlockSequenceExpr* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveMulticlockSequenceExpr(const MulticlockSequenceExpr* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterClockedSeq(const ClockedSeq* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveClockedSeq(const ClockedSeq* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterConstant(const Constant* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveConstant(const Constant* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterLetExpr(const LetExpr* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveLetExpr(const LetExpr* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterOperation(const Operation* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveOperation(const Operation* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterRefObj(const RefObj* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveRefObj(const RefObj* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterRefInstance(const RefInstance* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveRefInstance(const RefInstance* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterRefTypespec(const RefTypespec* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveRefTypespec(const RefTypespec* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterPartSelect(const PartSelect* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leavePartSelect(const PartSelect* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterIndexedPartSelect(const IndexedPartSelect* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveIndexedPartSelect(const IndexedPartSelect* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterVarSelect(const VarSelect* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveVarSelect(const VarSelect* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterBitSelect(const BitSelect* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveBitSelect(const BitSelect* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterVariable(const Variable* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveVariable(const Variable* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterHierPath(const HierPath* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveHierPath(const HierPath* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterArrayExpr(const ArrayExpr* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveArrayExpr(const ArrayExpr* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterRegArray(const RegArray* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveRegArray(const RegArray* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterReg(const Reg* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveReg(const Reg* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterTask(const Task* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveTask(const Task* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterFunction(const Function* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveFunction(const Function* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterTaskDecl(const TaskDecl* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveTaskDecl(const TaskDecl* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterFunctionDecl(const FunctionDecl* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveFunctionDecl(const FunctionDecl* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterModport(const Modport* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveModport(const Modport* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterInterfaceTFDecl(const InterfaceTFDecl* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveInterfaceTFDecl(const InterfaceTFDecl* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterContAssign(const ContAssign* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveContAssign(const ContAssign* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterContAssignBit(const ContAssignBit* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveContAssignBit(const ContAssignBit* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterPort(const Port* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leavePort(const Port* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterPortBit(const PortBit* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leavePortBit(const PortBit* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterCheckerPort(const CheckerPort* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveCheckerPort(const CheckerPort* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterCheckerInstPort(const CheckerInstPort* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveCheckerInstPort(const CheckerInstPort* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterGate(const Gate* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveGate(const Gate* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterSwitchTran(const SwitchTran* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveSwitchTran(const SwitchTran* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterUdp(const Udp* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveUdp(const Udp* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterModPath(const ModPath* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveModPath(const ModPath* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterTchk(const Tchk* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveTchk(const Tchk* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterRange(const Range* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveRange(const Range* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterUdpDefn(const UdpDefn* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveUdpDefn(const UdpDefn* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterTableEntry(const TableEntry* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveTableEntry(const TableEntry* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterIODecl(const IODecl* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveIODecl(const IODecl* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterAlias(const Alias* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveAlias(const Alias* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterClockingBlock(const ClockingBlock* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveClockingBlock(const ClockingBlock* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterClockingIODecl(const ClockingIODecl* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveClockingIODecl(const ClockingIODecl* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterCoverageOption(const CoverageOption* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveCoverageOption(const CoverageOption* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterCoverBin(const CoverBin* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveCoverBin(const CoverBin* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterCoverPoint(const CoverPoint* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveCoverPoint(const CoverPoint* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterCoverCross(const CoverCross* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveCoverCross(const CoverCross* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterCoverGroup(const CoverGroup* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveCoverGroup(const CoverGroup* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterParamAssign(const ParamAssign* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveParamAssign(const ParamAssign* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterInterfaceArray(const InterfaceArray* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveInterfaceArray(const InterfaceArray* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterProgramArray(const ProgramArray* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveProgramArray(const ProgramArray* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterModuleArray(const ModuleArray* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveModuleArray(const ModuleArray* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterGateArray(const GateArray* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveGateArray(const GateArray* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterSwitchArray(const SwitchArray* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveSwitchArray(const SwitchArray* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterUdpArray(const UdpArray* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveUdpArray(const UdpArray* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterPrimTerm(const PrimTerm* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leavePrimTerm(const PrimTerm* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterPathTerm(const PathTerm* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leavePathTerm(const PathTerm* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterTchkTerm(const TchkTerm* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveTchkTerm(const TchkTerm* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterNet(const Net* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveNet(const Net* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterEventTypespec(const EventTypespec* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveEventTypespec(const EventTypespec* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterNamedEvent(const NamedEvent* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveNamedEvent(const NamedEvent* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterNamedEventArray(const NamedEventArray* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveNamedEventArray(const NamedEventArray* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterParameter(const Parameter* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveParameter(const Parameter* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterDefParam(const DefParam* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveDefParam(const DefParam* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterSpecParam(const SpecParam* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveSpecParam(const SpecParam* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterClassTypespec(const ClassTypespec* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveClassTypespec(const ClassTypespec* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterExtends(const Extends* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveExtends(const Extends* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterImplements(const Implements* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveImplements(const Implements* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterClassDefn(const ClassDefn* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveClassDefn(const ClassDefn* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterClassObj(const ClassObj* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveClassObj(const ClassObj* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterInterface(const Interface* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveInterface(const Interface* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterProgram(const Program* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveProgram(const Program* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterPackage(const Package* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leavePackage(const Package* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterModule(const Module* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveModule(const Module* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterCheckerDecl(const CheckerDecl* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveCheckerDecl(const CheckerDecl* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterCheckerInst(const CheckerInst* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveCheckerInst(const CheckerInst* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterBindDirective(const BindDirective* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveBindDirective(const BindDirective* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterShortRealTypespec(const ShortRealTypespec* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveShortRealTypespec(const ShortRealTypespec* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterRealTypespec(const RealTypespec* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveRealTypespec(const RealTypespec* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterByteTypespec(const ByteTypespec* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveByteTypespec(const ByteTypespec* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterShortIntTypespec(const ShortIntTypespec* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveShortIntTypespec(const ShortIntTypespec* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterIntTypespec(const IntTypespec* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveIntTypespec(const IntTypespec* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterLongIntTypespec(const LongIntTypespec* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveLongIntTypespec(const LongIntTypespec* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterIntegerTypespec(const IntegerTypespec* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveIntegerTypespec(const IntegerTypespec* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterTimeTypespec(const TimeTypespec* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveTimeTypespec(const TimeTypespec* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterEnumTypespec(const EnumTypespec* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveEnumTypespec(const EnumTypespec* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterStringTypespec(const StringTypespec* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveStringTypespec(const StringTypespec* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterTypedefTypespec(const TypedefTypespec* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveTypedefTypespec(const TypedefTypespec* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterChandleTypespec(const ChandleTypespec* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveChandleTypespec(const ChandleTypespec* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterModuleTypespec(const ModuleTypespec* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveModuleTypespec(const ModuleTypespec* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterProgramTypespec(const ProgramTypespec* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveProgramTypespec(const ProgramTypespec* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterUdpDefnTypespec(const UdpDefnTypespec* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveUdpDefnTypespec(const UdpDefnTypespec* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterStructTypespec(const StructTypespec* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveStructTypespec(const StructTypespec* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterUnionTypespec(const UnionTypespec* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveUnionTypespec(const UnionTypespec* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterLogicTypespec(const LogicTypespec* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveLogicTypespec(const LogicTypespec* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterArrayTypespec(const ArrayTypespec* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveArrayTypespec(const ArrayTypespec* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterPackageTypespec(const PackageTypespec* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leavePackageTypespec(const PackageTypespec* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterVoidTypespec(const VoidTypespec* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveVoidTypespec(const VoidTypespec* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterUnsupportedTypespec(const UnsupportedTypespec* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveUnsupportedTypespec(const UnsupportedTypespec* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterSequenceTypespec(const SequenceTypespec* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveSequenceTypespec(const SequenceTypespec* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterPropertyTypespec(const PropertyTypespec* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leavePropertyTypespec(const PropertyTypespec* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterInterfaceTypespec(const InterfaceTypespec* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveInterfaceTypespec(const InterfaceTypespec* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterTypeParameter(const TypeParameter* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveTypeParameter(const TypeParameter* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterTypespecMember(const TypespecMember* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveTypespecMember(const TypespecMember* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterEnumConst(const EnumConst* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveEnumConst(const EnumConst* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterBitTypespec(const BitTypespec* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveBitTypespec(const BitTypespec* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterUserSystf(const UserSystf* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveUserSystf(const UserSystf* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterSysFuncCall(const SysFuncCall* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveSysFuncCall(const SysFuncCall* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterSysTaskCall(const SysTaskCall* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveSysTaskCall(const SysTaskCall* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterMethodFuncCall(const MethodFuncCall* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveMethodFuncCall(const MethodFuncCall* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterMethodTaskCall(const MethodTaskCall* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveMethodTaskCall(const MethodTaskCall* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterFuncCall(const FuncCall* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveFuncCall(const FuncCall* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterTaskCall(const TaskCall* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveTaskCall(const TaskCall* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterConstraintOrdering(const ConstraintOrdering* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveConstraintOrdering(const ConstraintOrdering* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterConstraint(const Constraint* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveConstraint(const Constraint* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterImportTypespec(const ImportTypespec* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveImportTypespec(const ImportTypespec* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterDistItem(const DistItem* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveDistItem(const DistItem* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterDistribution(const Distribution* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveDistribution(const Distribution* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterImplication(const Implication* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveImplication(const Implication* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterConstrIf(const ConstrIf* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveConstrIf(const ConstrIf* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterConstrIfElse(const ConstrIfElse* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveConstrIfElse(const ConstrIfElse* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterConstrForeach(const ConstrForeach* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveConstrForeach(const ConstrForeach* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterSoftDisable(const SoftDisable* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveSoftDisable(const SoftDisable* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterUniqueness(const Uniqueness* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveUniqueness(const Uniqueness* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterGenIf(const GenIf* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveGenIf(const GenIf* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterGenIfElse(const GenIfElse* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveGenIfElse(const GenIfElse* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterGenFor(const GenFor* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveGenFor(const GenFor* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterGenCase(const GenCase* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveGenCase(const GenCase* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterGenRegion(const GenRegion* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveGenRegion(const GenRegion* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterClause(const Clause* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveClause(const Clause* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterConfigRule(const ConfigRule* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveConfigRule(const ConfigRule* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterConfigDecl(const ConfigDecl* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveConfigDecl(const ConfigDecl* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterLibrary(const Library* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveLibrary(const Library* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterIncludeStmt(const IncludeStmt* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveIncludeStmt(const IncludeStmt* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterDesign(const Design* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveDesign(const Design* const object, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterAliasCollection(const Any* const object, const AliasCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveAliasCollection(const Any* const object, const AliasCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterAlwaysCollection(const Any* const object, const AlwaysCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveAlwaysCollection(const Any* const object, const AlwaysCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterAnyPatternCollection(const Any* const object, const AnyPatternCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveAnyPatternCollection(const Any* const object, const AnyPatternCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterArrayExprCollection(const Any* const object, const ArrayExprCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveArrayExprCollection(const Any* const object, const ArrayExprCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterArrayTypespecCollection(const Any* const object, const ArrayTypespecCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveArrayTypespecCollection(const Any* const object, const ArrayTypespecCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterAssertCollection(const Any* const object, const AssertCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveAssertCollection(const Any* const object, const AssertCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterAssignStmtCollection(const Any* const object, const AssignStmtCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveAssignStmtCollection(const Any* const object, const AssignStmtCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterAssignmentCollection(const Any* const object, const AssignmentCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveAssignmentCollection(const Any* const object, const AssignmentCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterAssumeCollection(const Any* const object, const AssumeCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveAssumeCollection(const Any* const object, const AssumeCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterAtomicStmtCollection(const Any* const object, const AtomicStmtCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveAtomicStmtCollection(const Any* const object, const AtomicStmtCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterAttributeCollection(const Any* const object, const AttributeCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveAttributeCollection(const Any* const object, const AttributeCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterBeginCollection(const Any* const object, const BeginCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveBeginCollection(const Any* const object, const BeginCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterBindDirectiveCollection(const Any* const object, const BindDirectiveCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveBindDirectiveCollection(const Any* const object, const BindDirectiveCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterBitSelectCollection(const Any* const object, const BitSelectCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveBitSelectCollection(const Any* const object, const BitSelectCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterBitTypespecCollection(const Any* const object, const BitTypespecCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveBitTypespecCollection(const Any* const object, const BitTypespecCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterBreakStmtCollection(const Any* const object, const BreakStmtCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveBreakStmtCollection(const Any* const object, const BreakStmtCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterByteTypespecCollection(const Any* const object, const ByteTypespecCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveByteTypespecCollection(const Any* const object, const ByteTypespecCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterCaseItemCollection(const Any* const object, const CaseItemCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveCaseItemCollection(const Any* const object, const CaseItemCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterCasePropertyCollection(const Any* const object, const CasePropertyCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveCasePropertyCollection(const Any* const object, const CasePropertyCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterCasePropertyItemCollection(const Any* const object, const CasePropertyItemCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveCasePropertyItemCollection(const Any* const object, const CasePropertyItemCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterCaseStmtCollection(const Any* const object, const CaseStmtCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveCaseStmtCollection(const Any* const object, const CaseStmtCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterChandleTypespecCollection(const Any* const object, const ChandleTypespecCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveChandleTypespecCollection(const Any* const object, const ChandleTypespecCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterCheckerDeclCollection(const Any* const object, const CheckerDeclCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveCheckerDeclCollection(const Any* const object, const CheckerDeclCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterCheckerInstCollection(const Any* const object, const CheckerInstCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveCheckerInstCollection(const Any* const object, const CheckerInstCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterCheckerInstPortCollection(const Any* const object, const CheckerInstPortCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveCheckerInstPortCollection(const Any* const object, const CheckerInstPortCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterCheckerPortCollection(const Any* const object, const CheckerPortCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveCheckerPortCollection(const Any* const object, const CheckerPortCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterClassDefnCollection(const Any* const object, const ClassDefnCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveClassDefnCollection(const Any* const object, const ClassDefnCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterClassObjCollection(const Any* const object, const ClassObjCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveClassObjCollection(const Any* const object, const ClassObjCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterClassTypespecCollection(const Any* const object, const ClassTypespecCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveClassTypespecCollection(const Any* const object, const ClassTypespecCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterClauseCollection(const Any* const object, const ClauseCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveClauseCollection(const Any* const object, const ClauseCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterClockedPropertyCollection(const Any* const object, const ClockedPropertyCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveClockedPropertyCollection(const Any* const object, const ClockedPropertyCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterClockedSeqCollection(const Any* const object, const ClockedSeqCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveClockedSeqCollection(const Any* const object, const ClockedSeqCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterClockingBlockCollection(const Any* const object, const ClockingBlockCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveClockingBlockCollection(const Any* const object, const ClockingBlockCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterClockingIODeclCollection(const Any* const object, const ClockingIODeclCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveClockingIODeclCollection(const Any* const object, const ClockingIODeclCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterCommentCollection(const Any* const object, const CommentCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveCommentCollection(const Any* const object, const CommentCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterConcurrentAssertionsCollection(const Any* const object, const ConcurrentAssertionsCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveConcurrentAssertionsCollection(const Any* const object, const ConcurrentAssertionsCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterConfigDeclCollection(const Any* const object, const ConfigDeclCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveConfigDeclCollection(const Any* const object, const ConfigDeclCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterConfigRuleCollection(const Any* const object, const ConfigRuleCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveConfigRuleCollection(const Any* const object, const ConfigRuleCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterConstantCollection(const Any* const object, const ConstantCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveConstantCollection(const Any* const object, const ConstantCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterConstrForeachCollection(const Any* const object, const ConstrForeachCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveConstrForeachCollection(const Any* const object, const ConstrForeachCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterConstrIfCollection(const Any* const object, const ConstrIfCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveConstrIfCollection(const Any* const object, const ConstrIfCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterConstrIfElseCollection(const Any* const object, const ConstrIfElseCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveConstrIfElseCollection(const Any* const object, const ConstrIfElseCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterConstraintCollection(const Any* const object, const ConstraintCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveConstraintCollection(const Any* const object, const ConstraintCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterConstraintExprCollection(const Any* const object, const ConstraintExprCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveConstraintExprCollection(const Any* const object, const ConstraintExprCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterConstraintOrderingCollection(const Any* const object, const ConstraintOrderingCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveConstraintOrderingCollection(const Any* const object, const ConstraintOrderingCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterContAssignCollection(const Any* const object, const ContAssignCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveContAssignCollection(const Any* const object, const ContAssignCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterContAssignBitCollection(const Any* const object, const ContAssignBitCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveContAssignBitCollection(const Any* const object, const ContAssignBitCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterContinueStmtCollection(const Any* const object, const ContinueStmtCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveContinueStmtCollection(const Any* const object, const ContinueStmtCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterCoverCollection(const Any* const object, const CoverCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveCoverCollection(const Any* const object, const CoverCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterCoverBinCollection(const Any* const object, const CoverBinCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveCoverBinCollection(const Any* const object, const CoverBinCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterCoverCrossCollection(const Any* const object, const CoverCrossCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveCoverCrossCollection(const Any* const object, const CoverCrossCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterCoverGroupCollection(const Any* const object, const CoverGroupCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveCoverGroupCollection(const Any* const object, const CoverGroupCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterCoverPointCollection(const Any* const object, const CoverPointCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveCoverPointCollection(const Any* const object, const CoverPointCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterCoverageOptionCollection(const Any* const object, const CoverageOptionCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveCoverageOptionCollection(const Any* const object, const CoverageOptionCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterDeassignCollection(const Any* const object, const DeassignCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveDeassignCollection(const Any* const object, const DeassignCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterDefParamCollection(const Any* const object, const DefParamCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveDefParamCollection(const Any* const object, const DefParamCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterDelayControlCollection(const Any* const object, const DelayControlCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveDelayControlCollection(const Any* const object, const DelayControlCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterDelayTermCollection(const Any* const object, const DelayTermCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveDelayTermCollection(const Any* const object, const DelayTermCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterDesignCollection(const Any* const object, const DesignCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveDesignCollection(const Any* const object, const DesignCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterDisableCollection(const Any* const object, const DisableCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveDisableCollection(const Any* const object, const DisableCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterDisableForkCollection(const Any* const object, const DisableForkCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveDisableForkCollection(const Any* const object, const DisableForkCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterDisablesCollection(const Any* const object, const DisablesCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveDisablesCollection(const Any* const object, const DisablesCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterDistItemCollection(const Any* const object, const DistItemCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveDistItemCollection(const Any* const object, const DistItemCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterDistributionCollection(const Any* const object, const DistributionCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveDistributionCollection(const Any* const object, const DistributionCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterDoWhileCollection(const Any* const object, const DoWhileCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveDoWhileCollection(const Any* const object, const DoWhileCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterEnumConstCollection(const Any* const object, const EnumConstCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveEnumConstCollection(const Any* const object, const EnumConstCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterEnumTypespecCollection(const Any* const object, const EnumTypespecCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveEnumTypespecCollection(const Any* const object, const EnumTypespecCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterEventControlCollection(const Any* const object, const EventControlCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveEventControlCollection(const Any* const object, const EventControlCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterEventStmtCollection(const Any* const object, const EventStmtCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveEventStmtCollection(const Any* const object, const EventStmtCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterEventTypespecCollection(const Any* const object, const EventTypespecCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveEventTypespecCollection(const Any* const object, const EventTypespecCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterExpectStmtCollection(const Any* const object, const ExpectStmtCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveExpectStmtCollection(const Any* const object, const ExpectStmtCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterExprCollection(const Any* const object, const ExprCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveExprCollection(const Any* const object, const ExprCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterExtendsCollection(const Any* const object, const ExtendsCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveExtendsCollection(const Any* const object, const ExtendsCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterFinalStmtCollection(const Any* const object, const FinalStmtCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveFinalStmtCollection(const Any* const object, const FinalStmtCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterForStmtCollection(const Any* const object, const ForStmtCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveForStmtCollection(const Any* const object, const ForStmtCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterForceCollection(const Any* const object, const ForceCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveForceCollection(const Any* const object, const ForceCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterForeachStmtCollection(const Any* const object, const ForeachStmtCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveForeachStmtCollection(const Any* const object, const ForeachStmtCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterForeverStmtCollection(const Any* const object, const ForeverStmtCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveForeverStmtCollection(const Any* const object, const ForeverStmtCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterForkStmtCollection(const Any* const object, const ForkStmtCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveForkStmtCollection(const Any* const object, const ForkStmtCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterFuncCallCollection(const Any* const object, const FuncCallCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveFuncCallCollection(const Any* const object, const FuncCallCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterFunctionCollection(const Any* const object, const FunctionCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveFunctionCollection(const Any* const object, const FunctionCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterFunctionDeclCollection(const Any* const object, const FunctionDeclCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveFunctionDeclCollection(const Any* const object, const FunctionDeclCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterGateCollection(const Any* const object, const GateCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveGateCollection(const Any* const object, const GateCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterGateArrayCollection(const Any* const object, const GateArrayCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveGateArrayCollection(const Any* const object, const GateArrayCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterGenCaseCollection(const Any* const object, const GenCaseCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveGenCaseCollection(const Any* const object, const GenCaseCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterGenForCollection(const Any* const object, const GenForCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveGenForCollection(const Any* const object, const GenForCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterGenIfCollection(const Any* const object, const GenIfCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveGenIfCollection(const Any* const object, const GenIfCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterGenIfElseCollection(const Any* const object, const GenIfElseCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveGenIfElseCollection(const Any* const object, const GenIfElseCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterGenRegionCollection(const Any* const object, const GenRegionCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveGenRegionCollection(const Any* const object, const GenRegionCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterGenScopeCollection(const Any* const object, const GenScopeCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveGenScopeCollection(const Any* const object, const GenScopeCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterGenScopeArrayCollection(const Any* const object, const GenScopeArrayCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveGenScopeArrayCollection(const Any* const object, const GenScopeArrayCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterGenStmtCollection(const Any* const object, const GenStmtCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveGenStmtCollection(const Any* const object, const GenStmtCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterHierPathCollection(const Any* const object, const HierPathCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveHierPathCollection(const Any* const object, const HierPathCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterIODeclCollection(const Any* const object, const IODeclCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveIODeclCollection(const Any* const object, const IODeclCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterIdentifierCollection(const Any* const object, const IdentifierCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveIdentifierCollection(const Any* const object, const IdentifierCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterIfElseCollection(const Any* const object, const IfElseCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveIfElseCollection(const Any* const object, const IfElseCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterIfStmtCollection(const Any* const object, const IfStmtCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveIfStmtCollection(const Any* const object, const IfStmtCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterImmediateAssertCollection(const Any* const object, const ImmediateAssertCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveImmediateAssertCollection(const Any* const object, const ImmediateAssertCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterImmediateAssumeCollection(const Any* const object, const ImmediateAssumeCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveImmediateAssumeCollection(const Any* const object, const ImmediateAssumeCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterImmediateCoverCollection(const Any* const object, const ImmediateCoverCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveImmediateCoverCollection(const Any* const object, const ImmediateCoverCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterImplementsCollection(const Any* const object, const ImplementsCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveImplementsCollection(const Any* const object, const ImplementsCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterImplicationCollection(const Any* const object, const ImplicationCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveImplicationCollection(const Any* const object, const ImplicationCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterImportTypespecCollection(const Any* const object, const ImportTypespecCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveImportTypespecCollection(const Any* const object, const ImportTypespecCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterIncludeStmtCollection(const Any* const object, const IncludeStmtCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveIncludeStmtCollection(const Any* const object, const IncludeStmtCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterIndexedPartSelectCollection(const Any* const object, const IndexedPartSelectCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveIndexedPartSelectCollection(const Any* const object, const IndexedPartSelectCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterInitialCollection(const Any* const object, const InitialCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveInitialCollection(const Any* const object, const InitialCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterInstanceCollection(const Any* const object, const InstanceCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveInstanceCollection(const Any* const object, const InstanceCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterInstanceArrayCollection(const Any* const object, const InstanceArrayCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveInstanceArrayCollection(const Any* const object, const InstanceArrayCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterIntTypespecCollection(const Any* const object, const IntTypespecCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveIntTypespecCollection(const Any* const object, const IntTypespecCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterIntegerTypespecCollection(const Any* const object, const IntegerTypespecCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveIntegerTypespecCollection(const Any* const object, const IntegerTypespecCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterInterfaceCollection(const Any* const object, const InterfaceCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveInterfaceCollection(const Any* const object, const InterfaceCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterInterfaceArrayCollection(const Any* const object, const InterfaceArrayCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveInterfaceArrayCollection(const Any* const object, const InterfaceArrayCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterInterfaceTFDeclCollection(const Any* const object, const InterfaceTFDeclCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveInterfaceTFDeclCollection(const Any* const object, const InterfaceTFDeclCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterInterfaceTypespecCollection(const Any* const object, const InterfaceTypespecCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveInterfaceTypespecCollection(const Any* const object, const InterfaceTypespecCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterLetDeclCollection(const Any* const object, const LetDeclCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveLetDeclCollection(const Any* const object, const LetDeclCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterLetExprCollection(const Any* const object, const LetExprCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveLetExprCollection(const Any* const object, const LetExprCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterLibraryCollection(const Any* const object, const LibraryCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveLibraryCollection(const Any* const object, const LibraryCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterLogicTypespecCollection(const Any* const object, const LogicTypespecCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveLogicTypespecCollection(const Any* const object, const LogicTypespecCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterLongIntTypespecCollection(const Any* const object, const LongIntTypespecCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveLongIntTypespecCollection(const Any* const object, const LongIntTypespecCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterMethodFuncCallCollection(const Any* const object, const MethodFuncCallCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveMethodFuncCallCollection(const Any* const object, const MethodFuncCallCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterMethodTaskCallCollection(const Any* const object, const MethodTaskCallCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveMethodTaskCallCollection(const Any* const object, const MethodTaskCallCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterModPathCollection(const Any* const object, const ModPathCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveModPathCollection(const Any* const object, const ModPathCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterModportCollection(const Any* const object, const ModportCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveModportCollection(const Any* const object, const ModportCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterModuleCollection(const Any* const object, const ModuleCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveModuleCollection(const Any* const object, const ModuleCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterModuleArrayCollection(const Any* const object, const ModuleArrayCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveModuleArrayCollection(const Any* const object, const ModuleArrayCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterModuleTypespecCollection(const Any* const object, const ModuleTypespecCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveModuleTypespecCollection(const Any* const object, const ModuleTypespecCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterMulticlockSequenceExprCollection(const Any* const object, const MulticlockSequenceExprCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveMulticlockSequenceExprCollection(const Any* const object, const MulticlockSequenceExprCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterNamedEventCollection(const Any* const object, const NamedEventCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveNamedEventCollection(const Any* const object, const NamedEventCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterNamedEventArrayCollection(const Any* const object, const NamedEventArrayCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveNamedEventArrayCollection(const Any* const object, const NamedEventArrayCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterNetCollection(const Any* const object, const NetCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveNetCollection(const Any* const object, const NetCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterNullStmtCollection(const Any* const object, const NullStmtCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveNullStmtCollection(const Any* const object, const NullStmtCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterOperationCollection(const Any* const object, const OperationCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveOperationCollection(const Any* const object, const OperationCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterOrderedWaitCollection(const Any* const object, const OrderedWaitCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveOrderedWaitCollection(const Any* const object, const OrderedWaitCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterPackageCollection(const Any* const object, const PackageCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leavePackageCollection(const Any* const object, const PackageCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterPackageTypespecCollection(const Any* const object, const PackageTypespecCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leavePackageTypespecCollection(const Any* const object, const PackageTypespecCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterParamAssignCollection(const Any* const object, const ParamAssignCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveParamAssignCollection(const Any* const object, const ParamAssignCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterParameterCollection(const Any* const object, const ParameterCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveParameterCollection(const Any* const object, const ParameterCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterPartSelectCollection(const Any* const object, const PartSelectCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leavePartSelectCollection(const Any* const object, const PartSelectCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterPathTermCollection(const Any* const object, const PathTermCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leavePathTermCollection(const Any* const object, const PathTermCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterPortCollection(const Any* const object, const PortCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leavePortCollection(const Any* const object, const PortCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterPortBitCollection(const Any* const object, const PortBitCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leavePortBitCollection(const Any* const object, const PortBitCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterPortsCollection(const Any* const object, const PortsCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leavePortsCollection(const Any* const object, const PortsCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterPreprocMacroDefinitionCollection(const Any* const object, const PreprocMacroDefinitionCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leavePreprocMacroDefinitionCollection(const Any* const object, const PreprocMacroDefinitionCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterPreprocMacroInstanceCollection(const Any* const object, const PreprocMacroInstanceCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leavePreprocMacroInstanceCollection(const Any* const object, const PreprocMacroInstanceCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterPrimTermCollection(const Any* const object, const PrimTermCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leavePrimTermCollection(const Any* const object, const PrimTermCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterPrimitiveCollection(const Any* const object, const PrimitiveCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leavePrimitiveCollection(const Any* const object, const PrimitiveCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterPrimitiveArrayCollection(const Any* const object, const PrimitiveArrayCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leavePrimitiveArrayCollection(const Any* const object, const PrimitiveArrayCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterProcessCollection(const Any* const object, const ProcessCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveProcessCollection(const Any* const object, const ProcessCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterProgramCollection(const Any* const object, const ProgramCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveProgramCollection(const Any* const object, const ProgramCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterProgramArrayCollection(const Any* const object, const ProgramArrayCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveProgramArrayCollection(const Any* const object, const ProgramArrayCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterProgramTypespecCollection(const Any* const object, const ProgramTypespecCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveProgramTypespecCollection(const Any* const object, const ProgramTypespecCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterPropFormalDeclCollection(const Any* const object, const PropFormalDeclCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leavePropFormalDeclCollection(const Any* const object, const PropFormalDeclCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterPropertyDeclCollection(const Any* const object, const PropertyDeclCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leavePropertyDeclCollection(const Any* const object, const PropertyDeclCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterPropertyInstCollection(const Any* const object, const PropertyInstCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leavePropertyInstCollection(const Any* const object, const PropertyInstCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterPropertySpecCollection(const Any* const object, const PropertySpecCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leavePropertySpecCollection(const Any* const object, const PropertySpecCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterPropertyTypespecCollection(const Any* const object, const PropertyTypespecCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leavePropertyTypespecCollection(const Any* const object, const PropertyTypespecCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterRangeCollection(const Any* const object, const RangeCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveRangeCollection(const Any* const object, const RangeCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterRealTypespecCollection(const Any* const object, const RealTypespecCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveRealTypespecCollection(const Any* const object, const RealTypespecCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterRefInstanceCollection(const Any* const object, const RefInstanceCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveRefInstanceCollection(const Any* const object, const RefInstanceCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterRefObjCollection(const Any* const object, const RefObjCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveRefObjCollection(const Any* const object, const RefObjCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterRefTypespecCollection(const Any* const object, const RefTypespecCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveRefTypespecCollection(const Any* const object, const RefTypespecCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterRegCollection(const Any* const object, const RegCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveRegCollection(const Any* const object, const RegCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterRegArrayCollection(const Any* const object, const RegArrayCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveRegArrayCollection(const Any* const object, const RegArrayCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterReleaseCollection(const Any* const object, const ReleaseCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveReleaseCollection(const Any* const object, const ReleaseCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterRepeatCollection(const Any* const object, const RepeatCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveRepeatCollection(const Any* const object, const RepeatCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterRepeatControlCollection(const Any* const object, const RepeatControlCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveRepeatControlCollection(const Any* const object, const RepeatControlCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterRestrictCollection(const Any* const object, const RestrictCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveRestrictCollection(const Any* const object, const RestrictCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterReturnStmtCollection(const Any* const object, const ReturnStmtCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveReturnStmtCollection(const Any* const object, const ReturnStmtCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterScopeCollection(const Any* const object, const ScopeCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveScopeCollection(const Any* const object, const ScopeCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterSelectCollection(const Any* const object, const SelectCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveSelectCollection(const Any* const object, const SelectCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterSeqFormalDeclCollection(const Any* const object, const SeqFormalDeclCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveSeqFormalDeclCollection(const Any* const object, const SeqFormalDeclCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterSequenceDeclCollection(const Any* const object, const SequenceDeclCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveSequenceDeclCollection(const Any* const object, const SequenceDeclCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterSequenceInstCollection(const Any* const object, const SequenceInstCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveSequenceInstCollection(const Any* const object, const SequenceInstCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterSequenceTypespecCollection(const Any* const object, const SequenceTypespecCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveSequenceTypespecCollection(const Any* const object, const SequenceTypespecCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterShortIntTypespecCollection(const Any* const object, const ShortIntTypespecCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveShortIntTypespecCollection(const Any* const object, const ShortIntTypespecCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterShortRealTypespecCollection(const Any* const object, const ShortRealTypespecCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveShortRealTypespecCollection(const Any* const object, const ShortRealTypespecCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterSimpleExprCollection(const Any* const object, const SimpleExprCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveSimpleExprCollection(const Any* const object, const SimpleExprCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterSoftDisableCollection(const Any* const object, const SoftDisableCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveSoftDisableCollection(const Any* const object, const SoftDisableCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterSourceFileCollection(const Any* const object, const SourceFileCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveSourceFileCollection(const Any* const object, const SourceFileCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterSpecParamCollection(const Any* const object, const SpecParamCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveSpecParamCollection(const Any* const object, const SpecParamCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterStringTypespecCollection(const Any* const object, const StringTypespecCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveStringTypespecCollection(const Any* const object, const StringTypespecCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterStructPatternCollection(const Any* const object, const StructPatternCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveStructPatternCollection(const Any* const object, const StructPatternCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterStructTypespecCollection(const Any* const object, const StructTypespecCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveStructTypespecCollection(const Any* const object, const StructTypespecCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterSwitchArrayCollection(const Any* const object, const SwitchArrayCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveSwitchArrayCollection(const Any* const object, const SwitchArrayCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterSwitchTranCollection(const Any* const object, const SwitchTranCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveSwitchTranCollection(const Any* const object, const SwitchTranCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterSysFuncCallCollection(const Any* const object, const SysFuncCallCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveSysFuncCallCollection(const Any* const object, const SysFuncCallCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterSysTaskCallCollection(const Any* const object, const SysTaskCallCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveSysTaskCallCollection(const Any* const object, const SysTaskCallCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterTFCallCollection(const Any* const object, const TFCallCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveTFCallCollection(const Any* const object, const TFCallCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterTableEntryCollection(const Any* const object, const TableEntryCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveTableEntryCollection(const Any* const object, const TableEntryCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterTaggedPatternCollection(const Any* const object, const TaggedPatternCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveTaggedPatternCollection(const Any* const object, const TaggedPatternCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterTaskCollection(const Any* const object, const TaskCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveTaskCollection(const Any* const object, const TaskCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterTaskCallCollection(const Any* const object, const TaskCallCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveTaskCallCollection(const Any* const object, const TaskCallCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterTaskDeclCollection(const Any* const object, const TaskDeclCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveTaskDeclCollection(const Any* const object, const TaskDeclCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterTaskFuncCollection(const Any* const object, const TaskFuncCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveTaskFuncCollection(const Any* const object, const TaskFuncCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterTaskFuncDeclCollection(const Any* const object, const TaskFuncDeclCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveTaskFuncDeclCollection(const Any* const object, const TaskFuncDeclCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterTchkCollection(const Any* const object, const TchkCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveTchkCollection(const Any* const object, const TchkCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterTchkTermCollection(const Any* const object, const TchkTermCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveTchkTermCollection(const Any* const object, const TchkTermCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterThreadCollection(const Any* const object, const ThreadCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveThreadCollection(const Any* const object, const ThreadCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterTimeTypespecCollection(const Any* const object, const TimeTypespecCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveTimeTypespecCollection(const Any* const object, const TimeTypespecCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterTypeParameterCollection(const Any* const object, const TypeParameterCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveTypeParameterCollection(const Any* const object, const TypeParameterCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterTypedefTypespecCollection(const Any* const object, const TypedefTypespecCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveTypedefTypespecCollection(const Any* const object, const TypedefTypespecCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterTypespecCollection(const Any* const object, const TypespecCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveTypespecCollection(const Any* const object, const TypespecCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterTypespecMemberCollection(const Any* const object, const TypespecMemberCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveTypespecMemberCollection(const Any* const object, const TypespecMemberCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterUdpCollection(const Any* const object, const UdpCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveUdpCollection(const Any* const object, const UdpCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterUdpArrayCollection(const Any* const object, const UdpArrayCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveUdpArrayCollection(const Any* const object, const UdpArrayCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterUdpDefnCollection(const Any* const object, const UdpDefnCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveUdpDefnCollection(const Any* const object, const UdpDefnCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterUdpDefnTypespecCollection(const Any* const object, const UdpDefnTypespecCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveUdpDefnTypespecCollection(const Any* const object, const UdpDefnTypespecCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterUnionTypespecCollection(const Any* const object, const UnionTypespecCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveUnionTypespecCollection(const Any* const object, const UnionTypespecCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterUniquenessCollection(const Any* const object, const UniquenessCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveUniquenessCollection(const Any* const object, const UniquenessCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterUnsupportedExprCollection(const Any* const object, const UnsupportedExprCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveUnsupportedExprCollection(const Any* const object, const UnsupportedExprCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterUnsupportedStmtCollection(const Any* const object, const UnsupportedStmtCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveUnsupportedStmtCollection(const Any* const object, const UnsupportedStmtCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterUnsupportedTypespecCollection(const Any* const object, const UnsupportedTypespecCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveUnsupportedTypespecCollection(const Any* const object, const UnsupportedTypespecCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterUserSystfCollection(const Any* const object, const UserSystfCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveUserSystfCollection(const Any* const object, const UserSystfCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterVarSelectCollection(const Any* const object, const VarSelectCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveVarSelectCollection(const Any* const object, const VarSelectCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterVariableCollection(const Any* const object, const VariableCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveVariableCollection(const Any* const object, const VariableCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterVoidTypespecCollection(const Any* const object, const VoidTypespecCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveVoidTypespecCollection(const Any* const object, const VoidTypespecCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterWaitForkCollection(const Any* const object, const WaitForkCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveWaitForkCollection(const Any* const object, const WaitForkCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterWaitStmtCollection(const Any* const object, const WaitStmtCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveWaitStmtCollection(const Any* const object, const WaitStmtCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterWaitsCollection(const Any* const object, const WaitsCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveWaitsCollection(const Any* const object, const WaitsCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

    void enterWhileStmtCollection(const Any* const object, const WhileStmtCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_ENTER; }
    void leaveWhileStmtCollection(const Any* const object, const WhileStmtCollection& objects, uint32_t vpiRelation = 0) final { UHDMLISTENER_TRACE_LEAVE; }

  protected:
   std::ostream &strm;
   int32_t indent = -1;
};
}  // namespace uhdm

#endif  // UHDM_UHDMLISTENER_H
