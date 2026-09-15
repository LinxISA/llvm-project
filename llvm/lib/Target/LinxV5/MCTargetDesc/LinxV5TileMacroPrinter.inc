void LinxV5InstPrinter::printTileMacroDisasm(const MCInst *MI,
                                             const MCSubtargetInfo &STI,
                                             raw_ostream &O) {
  if (MI->getNumOperands() < 3) {
    O << "<malformed TileOp macro>";
    return;
  }
  unsigned FormIndex = MI->getOperand(0).getImm();
  if (FormIndex >= TileMacroFormCount) {
    O << "<unknown TileOp macro>";
    return;
  }
  const TileMacroFormDesc &Form = TileMacroForms[FormIndex];
  unsigned HeaderType = MI->getOperand(1).getImm();
  unsigned NumCommands = MI->getOperand(2).getImm();
  unsigned OperandIndex = 3;
  SmallVector<MCInst, 24> Commands;
  for (unsigned I = 0; I != NumCommands; ++I) {
    if (OperandIndex + 2 > MI->getNumOperands()) {
      O << "<malformed TileOp macro>";
      return;
    }
    MCInst Command;
    Command.setOpcode(MI->getOperand(OperandIndex++).getImm());
    unsigned NumOperands = MI->getOperand(OperandIndex++).getImm();
    for (unsigned J = 0; J != NumOperands; ++J) {
      if (OperandIndex + 2 > MI->getNumOperands()) {
        O << "<malformed TileOp macro>";
        return;
      }
      unsigned Kind = MI->getOperand(OperandIndex++).getImm();
      int64_t Value = MI->getOperand(OperandIndex++).getImm();
      Command.addOperand(Kind == 0 ? MCOperand::createImm(Value)
                                   : MCOperand::createReg(Value));
    }
    Commands.push_back(Command);
  }
  if (OperandIndex != MI->getNumOperands()) {
    O << "<malformed TileOp macro>";
    return;
  }

  const MCInst *DATR = nullptr;
  const MCInst *FPATR = nullptr;
  const MCInst *Dims[3] = {nullptr, nullptr, nullptr};
  unsigned Pos = 0;
  if (Pos < Commands.size() && Commands[Pos].getOpcode() == LinxV5::BDATR)
    DATR = &Commands[Pos++];
  if (Pos < Commands.size() && Commands[Pos].getOpcode() == LinxV5::B_FPATR)
    FPATR = &Commands[Pos++];
  while (Pos < Commands.size() &&
         (Commands[Pos].getOpcode() == LinxV5::B_DIM ||
          Commands[Pos].getOpcode() == LinxV5::C_B_DIMI)) {
    unsigned Slot = Commands[Pos].getOperand(0).getImm();
    if (Slot < 3)
      Dims[Slot] = &Commands[Pos];
    ++Pos;
  }
  unsigned BodyEnd = Commands.size();
  if (BodyEnd && (Commands[BodyEnd - 1].getOpcode() == LinxV5::BSTOP ||
                  Commands[BodyEnd - 1].getOpcode() == LinxV5::BSTOP_C))
    --BodyEnd;
  const unsigned OperandStart = Pos;
  SmallVector<bool, 32> Consumed(BodyEnd, false);
  for (unsigned I = 0; I != OperandStart; ++I)
    Consumed[I] = true;
  auto commandMatchesKind = [&](unsigned Position,
                                TileMacroCommandKind Kind) {
    if (Position >= BodyEnd || Consumed[Position])
      return false;
    unsigned Opcode = Commands[Position].getOpcode();
    if (Kind == TileMacroCommandKind::IOS)
      return Opcode == LinxV5::B_IOS;
    if (Kind == TileMacroCommandKind::IOR)
      return Opcode == LinxV5::B_IO;
    switch (Opcode) {
    case LinxV5::B_IOT_NoSrc_Dst:
    case LinxV5::B_IOT_OneSrc_Dst:
    case LinxV5::B_IOT_TwoSrc_Dst:
    case LinxV5::B_IOT_OneSrc_NoDst:
    case LinxV5::B_IOT_TwoSrc_NoDst:
      return true;
    default:
      return false;
    }
  };
  auto findCommand = [&](TileMacroCommandKind Kind) {
    for (unsigned I = OperandStart; I != BodyEnd; ++I)
      if (commandMatchesKind(I, Kind))
        return I;
    return BodyEnd;
  };
  auto markConsumed = [&](unsigned Begin, unsigned End) {
    for (unsigned I = Begin; I != End; ++I)
      Consumed[I] = true;
  };

  struct RangeValue {
    bool Present = false;
    MCRegister Base = LinxV5::R0;
    unsigned Offset = 0;
  };
  struct BoundValue {
    bool Present = false;
    MCRegister Reg = LinxV5::R0;
    unsigned SharedID = 0;
    unsigned SizeCode = 0;
    RangeValue Subview;
    RangeValue Assemble;
  };
  const unsigned MemberCount =
      sizeof(TileMacroBindingMembers) / sizeof(TileMacroBindingMembers[0]);
  std::vector<BoundValue> Members(MemberCount);

  auto bindingForMember =
      [&](unsigned MemberIndex) -> const TileMacroBindingDesc * {
    for (unsigned I = 0; I != Form.NumBindings; ++I) {
      const auto &Binding = TileMacroBindings[Form.FirstBinding + I];
      if (MemberIndex >= Binding.FirstMember &&
          MemberIndex < Binding.FirstMember + Binding.NumMembers)
        return &Binding;
    }
    return nullptr;
  };
  auto isLocalDestination = [&](TileMacroBindingKind Kind) {
    return Kind == TileMacroBindingKind::LocalDestination ||
           Kind == TileMacroBindingKind::PredicateTileDestination ||
           Kind == TileMacroBindingKind::PredicateCellDestination;
  };
  auto conditionEnabled = [&](StringRef Condition) {
    if (Condition.empty())
      return true;
    if (Condition == "DataType=U8")
      return HeaderType == LinxV5Op::DataType::U8;
    if (Condition == "AType requires MX scale")
      return LinxV5Op::matrixMXTypeNeedsScale(HeaderType);
    if (Condition == "BType requires MX scale") {
      unsigned Type = DATR ? DATR->getOperand(2).getImm() : HeaderType;
      return LinxV5Op::matrixMXTypeNeedsScale(Type);
    }
    if (!FPATR)
      return false;
    if (Condition == "RowMax&&RowMaxInit")
      return FPATR->getOperand(3).getImm() && FPATR->getOperand(5).getImm();
    if (Condition == "PreMode=vector")
      return FPATR->getOperand(0).getImm() &&
             FPATR->getOperand(0).getImm() != 1;
    if (Condition == "PreMode=scalar")
      return FPATR->getOperand(0).getImm() == 1;
    if (Condition == "PostMode=vector")
      return FPATR->getOperand(1).getImm() == 2;
    if (Condition == "PostMode=scalar")
      return FPATR->getOperand(1).getImm() == 1;
    if (Condition == "RowMax")
      return FPATR->getOperand(3).getImm() != 0;
    if (Condition == "GroupMax")
      return FPATR->getOperand(4).getImm() != 0;
    if (Condition == "CScale")
      return FPATR->getOperand(9).getImm() != 0;
    return false;
  };

  unsigned CommonMask = 0xf;
  bool HaveMask = false;
  auto recordMask = [&](unsigned Mask) {
    if (!HaveMask) {
      HaveMask = true;
      CommonMask = Mask;
    }
  };
  auto decodeIOTForMacro =
      [&](const MCInst &Inst, SmallVectorImpl<MCRegister> &Sources,
          MCRegister &Destination, unsigned &DestinationSize) {
        switch (Inst.getOpcode()) {
        case LinxV5::B_IOT_NoSrc_Dst:
          Destination = Inst.getOperand(0).getReg();
          recordMask(Inst.getOperand(1).getImm());
          DestinationSize = Inst.getOperand(2).getImm();
          return true;
        case LinxV5::B_IOT_OneSrc_Dst:
          Destination = Inst.getOperand(0).getReg();
          recordMask(Inst.getOperand(1).getImm());
          DestinationSize = Inst.getOperand(2).getImm();
          Sources.push_back(Inst.getOperand(4).getReg());
          return true;
        case LinxV5::B_IOT_TwoSrc_Dst:
          Destination = Inst.getOperand(0).getReg();
          recordMask(Inst.getOperand(1).getImm());
          DestinationSize = Inst.getOperand(2).getImm();
          Sources.push_back(Inst.getOperand(4).getReg());
          Sources.push_back(Inst.getOperand(5).getReg());
          return true;
        case LinxV5::B_IOT_OneSrc_NoDst:
          recordMask(Inst.getOperand(0).getImm());
          Sources.push_back(Inst.getOperand(2).getReg());
          return true;
        case LinxV5::B_IOT_TwoSrc_NoDst:
          recordMask(Inst.getOperand(0).getImm());
          Sources.push_back(Inst.getOperand(2).getReg());
          Sources.push_back(Inst.getOperand(3).getReg());
          return true;
        default:
          return false;
        }
      };
  auto captureRangeModifiers = [&](ArrayRef<unsigned> SourceMembers,
                                   int DestinationMember, unsigned &Position) {
    while (Position < BodyEnd) {
      const MCInst &Modifier = Commands[Position];
      if (Modifier.getOpcode() == LinxV5::B_SUBVIEW) {
        unsigned SourceSelect = Modifier.getOperand(0).getImm();
        if (SourceSelect >= SourceMembers.size())
          break;
        BoundValue &Value = Members[SourceMembers[SourceSelect]];
        Value.Subview.Present = true;
        Value.Subview.Base = Modifier.getOperand(1).getReg();
        Value.Subview.Offset = Modifier.getOperand(2).getImm();
        ++Position;
        continue;
      }
      if (Modifier.getOpcode() == LinxV5::B_ASSEMBLE &&
          DestinationMember >= 0) {
        BoundValue &Value = Members[DestinationMember];
        Value.Assemble.Present = true;
        Value.Assemble.Base = Modifier.getOperand(2).getReg();
        Value.Assemble.Offset = Modifier.getOperand(3).getImm();
        ++Position;
        continue;
      }
      break;
    }
  };
  for (unsigned C = 0; C != Form.NumCommands; ++C) {
    const auto &Expected = TileMacroCommands[Form.FirstCommand + C];
    SmallVector<unsigned, 8> Active;
    for (unsigned M = 0; M != Expected.NumMembers; ++M) {
      unsigned Member =
          TileMacroCommandMembers[Expected.FirstMember + M].BindingMember;
      const auto &Desc = TileMacroBindingMembers[Member];
      if (!Desc.Condition || conditionEnabled(Desc.Condition))
        Active.push_back(Member);
    }
    if (Active.empty())
      continue;
    Pos = findCommand(Expected.Kind);
    if (Pos >= BodyEnd)
      continue;
    if (Expected.Kind == TileMacroCommandKind::IOT) {
      SmallVector<MCRegister, 2> Sources;
      MCRegister Destination = LinxV5::NoRegister;
      unsigned DestinationSize = 0;
      SmallVector<unsigned, 2> SourceMembers;
      int DestinationMember = -1;
      unsigned RequiredSources = 0;
      bool RequiredDestination = false;
      for (unsigned Member : Active) {
        const auto *Binding = bindingForMember(Member);
        if (Binding && isLocalDestination(Binding->Kind)) {
          DestinationMember = Member;
          RequiredDestination |= !TileMacroBindingMembers[Member].Optional;
        } else {
          SourceMembers.push_back(Member);
          RequiredSources += !TileMacroBindingMembers[Member].Optional;
        }
      }
      unsigned SeenSources = 0;
      bool SeenDestination = false;
      do {
        Pos = findCommand(TileMacroCommandKind::IOT);
        if (Pos >= BodyEnd)
          break;
        Sources.clear();
        Destination = LinxV5::NoRegister;
        DestinationSize = 0;
        if (!decodeIOTForMacro(Commands[Pos], Sources, Destination,
                              DestinationSize))
          break;
        unsigned CommandStart = Pos++;
        SmallVector<unsigned, 2> RangeSources;
        for (MCRegister Source : Sources) {
          if (SeenSources >= SourceMembers.size())
            break;
          unsigned Member = SourceMembers[SeenSources++];
          Members[Member].Present = true;
          Members[Member].Reg = Source;
          RangeSources.push_back(Member);
        }
        int RangeDestination = -1;
        if (Destination != LinxV5::NoRegister && DestinationMember >= 0) {
          Members[DestinationMember].Present = true;
          Members[DestinationMember].Reg = Destination;
          Members[DestinationMember].SizeCode = DestinationSize;
          RangeDestination = DestinationMember;
          SeenDestination = true;
        }
        captureRangeModifiers(RangeSources, RangeDestination, Pos);
        markConsumed(CommandStart, Pos);
      } while (SeenSources < RequiredSources ||
               (RequiredDestination && !SeenDestination));
    } else if (Expected.Kind == TileMacroCommandKind::IOS) {
      for (unsigned Member : Active) {
        if (Pos >= BodyEnd || Commands[Pos].getOpcode() != LinxV5::B_IOS)
          break;
        Members[Member].Present = true;
        Members[Member].SharedID = Commands[Pos].getOperand(0).getImm();
        Members[Member].SizeCode = Commands[Pos].getOperand(2).getImm();
        recordMask(Commands[Pos].getOperand(1).getImm());
        unsigned CommandStart = Pos;
        ++Pos;
        SmallVector<unsigned, 1> RangeSources;
        int RangeDestination = -1;
        const auto *Binding = bindingForMember(Member);
        if (Binding && Binding->Kind == TileMacroBindingKind::SharedDestination)
          RangeDestination = Member;
        else
          RangeSources.push_back(Member);
        captureRangeModifiers(RangeSources, RangeDestination, Pos);
        markConsumed(CommandStart, Pos);
      }
    } else {
      if (Pos >= BodyEnd || Commands[Pos].getOpcode() != LinxV5::B_IO)
        continue;
      unsigned CommandStart = Pos;
      const MCInst &First = Commands[Pos++];
      for (unsigned Member : Active) {
        const auto &Desc = TileMacroBindingMembers[Member];
        unsigned Slot = Desc.Slot == TileMacroMemberSlot::RegDst    ? 0
                        : Desc.Slot == TileMacroMemberSlot::RegSrc0 ? 1
                        : Desc.Slot == TileMacroMemberSlot::RegSrc1 ? 2
                        : Desc.Slot == TileMacroMemberSlot::RegSrc2 ? 3
                                                                    : 4;
        MCRegister Reg = LinxV5::R0;
        if (Slot < 4)
          Reg = First.getOperand(Slot).getReg();
        else if (Pos < BodyEnd && Commands[Pos].getOpcode() == LinxV5::B_IO)
          Reg = Commands[Pos].getOperand(1).getReg();
        if (Reg != LinxV5::R0) {
          Members[Member].Present = true;
          Members[Member].Reg = Reg;
        }
      }
      if (llvm::any_of(Active,
                       [&](unsigned Member) {
                         return TileMacroBindingMembers[Member].Slot ==
                                TileMacroMemberSlot::RegSrc3;
                       }) &&
          Pos < BodyEnd && Commands[Pos].getOpcode() == LinxV5::B_IO)
        ++Pos;
      markConsumed(CommandStart, Pos);
    }
  }

  struct DimValue {
    bool Present = false;
    bool Absolute = false;
    int64_t Value = 1;
    MCRegister Reg = LinxV5::R0;
    int64_t Offset = 0;
  };
  auto readDim = [&](unsigned Slot) {
    DimValue Value;
    if (!Dims[Slot]) {
      Value.Absolute = true;
      return Value;
    }
    Value.Present = true;
    if (Dims[Slot]->getOpcode() == LinxV5::C_B_DIMI) {
      Value.Absolute = true;
      Value.Value = Dims[Slot]->getOperand(1).getImm();
    } else {
      Value.Reg = Dims[Slot]->getOperand(1).getReg();
      Value.Offset = Dims[Slot]->getOperand(2).getImm();
      if (Value.Reg == LinxV5::R0) {
        Value.Absolute = true;
        Value.Value = Value.Offset;
      }
    }
    return Value;
  };
  DimValue Dim[3] = {readDim(0), readDim(1), readDim(2)};
  auto printDim = [&](const DimValue &Value) {
    if (Value.Absolute)
      O << Value.Value;
    else if (!Value.Present)
      O << 1;
    else {
      O << getRegisterName(Value.Reg);
      if (Value.Offset)
        O << (Value.Offset > 0 ? "+" : "") << Value.Offset;
    }
  };
  auto configTarget =
      [&](const TileMacroConfigDesc &Config,
          TileMacroTargetKind Kind) -> const TileMacroConfigTargetDesc * {
    for (unsigned T = 0; T != Config.NumTargets; ++T) {
      const auto &Target = TileMacroConfigTargets[Config.FirstTarget + T];
      if (Target.Kind == Kind)
        return &Target;
    }
    return nullptr;
  };
  auto dimForConfig = [&](const TileMacroConfigDesc &Config) -> DimValue {
    if (const auto *Target = configTarget(Config, TileMacroTargetKind::DIM)) {
      unsigned Slot = Target->Slot == TileMacroTargetSlot::LB0   ? 0
                      : Target->Slot == TileMacroTargetSlot::LB1 ? 1
                                                                 : 2;
      return Dim[Slot];
    }
    if (StringRef(Config.Field) == "Col" && DATR) {
      unsigned Layout = DATR->getOperand(0).getImm();
      DimValue Value;
      Value.Present = Value.Absolute = true;
      Value.Value = (Layout == LinxV5Op::ArgFormat::ND2M32 ||
                     Layout == LinxV5Op::ArgFormat::M322ND)
                        ? 32
                    : (Layout == LinxV5Op::ArgFormat::ND2M16 ||
                       Layout == LinxV5Op::ArgFormat::M162ND)
                        ? 16
                        : 8;
      return Value;
    }
    return DimValue();
  };

  unsigned DestinationSize = 0;
  const TileMacroConfigDesc *RowConfig = nullptr;
  for (unsigned I = 0; I != Form.NumBindings; ++I) {
    const auto &Binding = TileMacroBindings[Form.FirstBinding + I];
    bool Destination = isLocalDestination(Binding.Kind) ||
                       Binding.Kind == TileMacroBindingKind::SharedDestination;
    if (Destination && Members[Binding.FirstMember].Present) {
      DestinationSize = Members[Binding.FirstMember].SizeCode;
      break;
    }
  }
  DimValue Col;
  for (unsigned I = 0; I != Form.NumConfigs; ++I) {
    const auto &Config = TileMacroConfigs[Form.FirstConfig + I];
    if (StringRef(Config.Field) == "Row")
      RowConfig = &Config;
    else if (StringRef(Config.Field) == "Col")
      Col = dimForConfig(Config);
  }
  unsigned ElementType = HeaderType;
  for (unsigned I = 0; I != Form.NumConfigs; ++I) {
    const auto &Config = TileMacroConfigs[Form.FirstConfig + I];
    if (StringRef(Config.Field) == "DstDataType" && DATR)
      ElementType = DATR->getOperand(2).getImm();
  }
  unsigned TypeBits = getTileMacroTypeBits(ElementType);
  if (!TypeBits && DATR) {
    unsigned Layout = DATR->getOperand(0).getImm();
    TypeBits = (Layout == LinxV5Op::ArgFormat::ND2M32 ||
                Layout == LinxV5Op::ArgFormat::M322ND)
                   ? 32
               : (Layout == LinxV5Op::ArgFormat::ND2M16 ||
                  Layout == LinxV5Op::ArgFormat::M162ND)
                   ? 16
                   : 8;
  }
  DimValue Row;
  StringRef RowExpression =
      RowConfig && RowConfig->ResolutionExpression
          ? StringRef(RowConfig->ResolutionExpression)
          : StringRef();
  unsigned LayoutCode =
      DATR ? DATR->getOperand(0).getImm() : LinxV5Op::ArgFormat::NORM;
  bool DirectCubeLayout = LayoutCode == LinxV5Op::ArgFormat::CUBE_M32 ||
                          LayoutCode == LinxV5Op::ArgFormat::CUBE_M16;
  bool LayoutDependentRow =
      RowExpression.startswith("Layout=RowMajor?");
  bool RowMajorCapacityImpossible = false;
  if (RowExpression == "ValidRow" ||
      (LayoutDependentRow && DirectCubeLayout)) {
    for (unsigned I = 0; I != Form.NumConfigs; ++I) {
      const auto &Config = TileMacroConfigs[Form.FirstConfig + I];
      if (StringRef(Config.Field) == "ValidRow") {
        Row = dimForConfig(Config);
        break;
      }
    }
  } else if (RowExpression.contains(".TSize/") && DestinationSize &&
             Col.Absolute && Col.Value > 0 && TypeBits) {
    uint64_t Bits = (uint64_t{1} << (DestinationSize + 6)) * 8;
    uint64_t Denominator = uint64_t(Col.Value) * TypeBits;
    if (Denominator && Bits % Denominator == 0) {
      Row.Present = Row.Absolute = true;
      Row.Value = Bits / Denominator;
    } else if (Denominator)
      RowMajorCapacityImpossible = true;
  }
  if (!Row.Present && LayoutDependentRow && RowMajorCapacityImpossible) {
    for (unsigned I = 0; I != Form.NumConfigs; ++I) {
      const auto &Config = TileMacroConfigs[Form.FirstConfig + I];
      if (StringRef(Config.Field) == "ValidRow") {
        Row = dimForConfig(Config);
        break;
      }
    }
  }

  std::string PrintedMnemonic = StringRef(Form.Spelling).str();
  O << PrintedMnemonic;
  O.indent(PrintedMnemonic.size() < TileMacroMnemonicColumn
               ? TileMacroMnemonicColumn - PrintedMnemonic.size()
               : 1);
  O << "<";
  bool FirstConfig = true;
  auto separator = [&]() {
    if (!FirstConfig)
      O << ", ";
    FirstConfig = false;
  };
  for (unsigned I = 0; I != Form.NumConfigs; ++I) {
    const auto &Config = TileMacroConfigs[Form.FirstConfig + I];
    StringRef Field = Config.Field;
    if (Config.IsDimension) {
      DimValue Value = Field == "Row" ? Row : dimForConfig(Config);
      bool Print = !Config.Optional;
      if (Field == "Row")
        Print = Row.Present;
      else if (Config.Default) {
        StringRef Default = Config.Default;
        if (Default == "Row")
          Print = !(Value.Absolute && Row.Absolute && Value.Value == Row.Value);
        else if (Default == "Col")
          Print = !(Value.Absolute && Col.Absolute && Value.Value == Col.Value);
        else {
          int64_t DefaultValue = 0;
          if (!Default.getAsInteger(10, DefaultValue))
            Print = Value.Present &&
                    !(Value.Absolute && Value.Value == DefaultValue);
          else if (const auto *Target =
                       configTarget(Config, TileMacroTargetKind::DIM)) {
            unsigned Slot = Target->Slot == TileMacroTargetSlot::LB0   ? 0
                            : Target->Slot == TileMacroTargetSlot::LB1 ? 1
                                                                       : 2;
            Print = Dim[Slot].Present;
          }
        }
      }
      if (!Print)
        continue;
      separator();
      O << Field << "=";
      printDim(Value);
      continue;
    }
    if (configTarget(Config, TileMacroTargetKind::Header)) {
      separator();
      printBstartDataType(MI, 1, STI, O);
      continue;
    }
    if (const auto *Target = configTarget(Config, TileMacroTargetKind::FPATR)) {
      if (!FPATR)
        continue;
      int Index = Target->Slot == TileMacroTargetSlot::PreQuantMode ? 0
                  : Target->Slot == TileMacroTargetSlot::ReluMode   ? 1
                  : Target->Slot == TileMacroTargetSlot::GroupNCode ? 2
                  : Target->Slot == TileMacroTargetSlot::RowMaxEn   ? 3
                  : Target->Slot == TileMacroTargetSlot::GroupMaxEn ? 4
                  : Target->Slot == TileMacroTargetSlot::RowMaxInit ? 5
                  : Target->Slot == TileMacroTargetSlot::MaxAbsEn   ? 6
                  : Target->Slot == TileMacroTargetSlot::TransA     ? 7
                  : Target->Slot == TileMacroTargetSlot::TransB     ? 8
                  : Target->Slot == TileMacroTargetSlot::CScaleEn   ? 9
                                                                    : -1;
      if (Index < 0)
        continue;
      int64_t Value = FPATR->getOperand(Index).getImm();
      if (!Value)
        continue;
      separator();
      O << Field;
      if (Index < 3 || Value != 1)
        O << "=" << Value;
      continue;
    }
    if (configTarget(Config, TileMacroTargetKind::IOT) ||
        configTarget(Config, TileMacroTargetKind::IOS)) {
      if (CommonMask == 0xf && Config.Default &&
          StringRef(Config.Default) == "AllPE")
        continue;
      separator();
      O << getCanonicalPEMaskName(CommonMask);
      continue;
    }
    const auto *Target = configTarget(Config, TileMacroTargetKind::DATR);
    if (!Target || !DATR)
      continue;
    bool Print = !Config.Optional;
    switch (Target->Slot) {
    case TileMacroTargetSlot::Layout:
      Print |= DATR->getOperand(0).getImm() != 0;
      break;
    case TileMacroTargetSlot::DataType:
      Print |=
          DATR->getOperand(2).getImm() != HeaderType &&
          DATR->getOperand(2).getImm() != LinxV5Op::DataType::EMPTY_DataType;
      break;
    case TileMacroTargetSlot::PadValueOrByteId:
      Print |= DATR->getOperand(3).getImm() != LinxV5Op::PadValue::Null ||
               DATR->getOperand(7).getImm() != 0;
      break;
    case TileMacroTargetSlot::CMode:
      Print |= DATR->getOperand(4).getImm() != LinxV5Op::CmpMode::EQ;
      break;
    case TileMacroTargetSlot::RMode:
      Print |= DATR->getOperand(5).getImm() != LinxV5Op::RMode::RNONE;
      break;
    case TileMacroTargetSlot::Sat:
      Print |= DATR->getOperand(6).getImm() != LinxV5Op::Sat::NOSAT;
      break;
    case TileMacroTargetSlot::Canonicalize:
      Print |= DATR->getOperand(1).getImm() != LinxV5Op::Canon::NORMAL_CANON;
      break;
    default:
      break;
    }
    if (!Print)
      continue;
    separator();
    switch (Target->Slot) {
    case TileMacroTargetSlot::Layout:
      printBArgFormat(DATR, 0, STI, O);
      break;
    case TileMacroTargetSlot::DataType:
      printBstartDataType(DATR, 2, STI, O);
      break;
    case TileMacroTargetSlot::PadValueOrByteId:
      if (Field == "PadValueOrByteId" && DATR->getOperand(7).getImm() != 0)
        printByteID(DATR, 7, STI, O);
      else
        printPadValue(DATR, 3, STI, O);
      break;
    case TileMacroTargetSlot::CMode:
      printCmpMode(DATR, 4, STI, O);
      break;
    case TileMacroTargetSlot::RMode:
      printRMode(DATR, 5, STI, O);
      break;
    case TileMacroTargetSlot::Sat:
      printSat(DATR, 6, STI, O);
      break;
    case TileMacroTargetSlot::Canonicalize:
      printCanon(DATR, 1, STI, O);
      break;
    default:
      break;
    }
  }
  O << ">";

  auto printSize = [&](unsigned SizeCode) {
    MCInst Size;
    Size.addOperand(MCOperand::createImm(SizeCode));
    printTileSizeWithBracket(&Size, 0, STI, O);
  };
  auto printTile = [&](MCRegister Reg, bool Destination, unsigned SizeCode) {
    if (Destination)
      O << "->";
    StringRef Name = getRegisterName(Reg);
    O << char(std::toupper(static_cast<unsigned char>(Name.front())))
      << Name.drop_front();
    if (Destination)
      printSize(SizeCode);
  };
  for (unsigned I = 0; I != Form.NumBindings; ++I) {
    const auto &Binding = TileMacroBindings[Form.FirstBinding + I];
    if (Binding.Kind == TileMacroBindingKind::GPRListSource) {
      bool Any = false;
      for (unsigned M = 0; M != Binding.NumMembers; ++M)
        Any |= Members[Binding.FirstMember + M].Present;
      if (!Any)
        continue;
      O << ", [";
      bool First = true;
      for (unsigned M = 0; M != Binding.NumMembers; ++M) {
        const auto &Value = Members[Binding.FirstMember + M];
        if (!Value.Present)
          continue;
        if (!First)
          O << ", ";
        First = false;
        StringRef Label = getTileMacroGPRListLabel(Binding, M);
        if (!Label.empty())
          O << Label << "=";
        O << getRegisterName(Value.Reg);
      }
      O << "]";
      continue;
    }
    const BoundValue &Value = Members[Binding.FirstMember];
    if (!Value.Present)
      continue;
    O << ", ";
    switch (Binding.Kind) {
    case TileMacroBindingKind::LocalSource:
      printTile(Value.Reg, false, 0);
      break;
    case TileMacroBindingKind::LocalDestination:
      printTile(Value.Reg, true, Value.SizeCode);
      break;
    case TileMacroBindingKind::PredicateTileSource:
      printTile(Value.Reg, false, 0);
      break;
    case TileMacroBindingKind::PredicateTileDestination:
      printTile(Value.Reg, true, Value.SizeCode);
      break;
    case TileMacroBindingKind::PredicateCellSource:
      printTile(Value.Reg, false, 0);
      break;
    case TileMacroBindingKind::PredicateCellDestination:
      printTile(Value.Reg, true, Value.SizeCode);
      break;
    case TileMacroBindingKind::SharedSource:
      O << "S" << Value.SharedID;
      break;
    case TileMacroBindingKind::SharedDestination:
      O << "->S" << Value.SharedID;
      printSize(Value.SizeCode);
      break;
    case TileMacroBindingKind::GPRSource: {
      StringRef Label = getTileMacroGPRLabel(Binding);
      if (!Label.empty())
        O << Label << "=";
      O << getRegisterName(Value.Reg);
      break;
    }
    case TileMacroBindingKind::GPRDestination: {
      O << "->";
      StringRef Label = getTileMacroGPRLabel(Binding);
      if (!Label.empty())
        O << Label << "=";
      O << getRegisterName(Value.Reg);
      break;
    }
    case TileMacroBindingKind::GPRListSource:
      break;
    }
    if (Value.Subview.Present)
      O << "[base=" << getRegisterName(Value.Subview.Base)
        << ", offset=" << Value.Subview.Offset << "]";
    if (Value.Assemble.Present)
      O << "[base=" << getRegisterName(Value.Assemble.Base)
        << ", offset=" << Value.Assemble.Offset << "]";
  }
}
