// Exact PTO 0.58.6 TileOp macro parser and physical bundle planner.

bool LinxV5AsmParser::emitTileMacroLine(SMLoc IDLoc, MCStreamer &Out) {
  std::unique_ptr<ParsedTileMacroLine> Parsed = std::move(PendingTileMacro);
  if (!Parsed || !Parsed->Operation)
    return Error(IDLoc, "missing TileOp macro state");

  struct DimExpr {
    bool HasValue = false;
    MCRegister Reg = LinxV5::R0;
    int64_t Imm = 0;
  };
  struct ConfigValue {
    bool HasValue = false;
    bool Explicit = false;
    DimExpr Dim;
    unsigned Imm = 0;
  };
  struct RangeValue {
    bool Present = false;
    MCRegister Base = LinxV5::R0;
    unsigned Offset = 0;
    unsigned SizeCode = 0;
  };
  struct BoundValue {
    bool Present = false;
    MCRegister Reg = LinxV5::R0;
    unsigned SharedID = 0;
    unsigned SizeCode = 0;
    char TileClass = 0;
    RangeValue Subview;
    RangeValue Assemble;
  };
  struct Plan {
    const TileMacroFormDesc *Form = nullptr;
    std::vector<ConfigValue> Configs;
    std::vector<BoundValue> Members;
    unsigned PEMask = 0xf;
    unsigned HeaderType = DataType::EMPTY_DataType;
  };

  const auto &Tokens = Parsed->Lexemes;
  auto tokenIs = [&](size_t Pos, StringRef Text) {
    return Pos < Tokens.size() &&
           StringRef(Tokens[Pos].Text).equals_insensitive(Text);
  };
  auto parseDataTypeName = [&](StringRef Name) {
    return StringSwitch<unsigned>(Name.upper())
        .Case("FP64", DataType::FP64)
        .Case("FP32", DataType::FP32)
        .Case("TF32", DataType::TF32)
        .Case("HF32", DataType::HF32)
        .Case("FP16", DataType::FP16)
        .Case("BF16", DataType::BF16)
        .Case("HIF8", DataType::HiF8)
        .Case("E4M3", DataType::e4m3)
        .Case("E5M2", DataType::e5m2)
        .Case("E3M2", DataType::e3m2)
        .Case("E2M3", DataType::e2m3)
        .Case("E2M1X2", DataType::e2m1x2)
        .Case("E1M2X2", DataType::e1m2x2)
        .Case("E8M0", DataType::e8m0)
        .Case("HIF4X2", DataType::HiF4x2)
        .Case("S64", DataType::S64)
        .Case("S32", DataType::S32)
        .Case("S16", DataType::S16)
        .Case("S8", DataType::S8)
        .Case("S4X2", DataType::S4x2)
        .Case("U64", DataType::U64)
        .Case("U32", DataType::U32)
        .Case("U16", DataType::U16)
        .Case("U8", DataType::U8)
        .Case("U4X2", DataType::U4x2)
        .Case("DTYPE_NONE", DataType::EMPTY_DataType)
        .Default(32u);
  };
  auto parseMaskName = [&](StringRef Name) {
    return StringSwitch<unsigned>(Name.upper())
        .Case("NOPE", 0x0U)
        .Case("PE0", 0x8U)
        .Case("PE1", 0x4U)
        .Case("PE2", 0x2U)
        .Case("PE3", 0x1U)
        .Case("PE0_1", 0xcU)
        .Case("PE0_1_2", 0xeU)
        .Case("ALLPE", 0xfU)
        .Default(32u);
  };
  auto parsePadName = [&](StringRef Name) {
    return StringSwitch<unsigned>(Name.lower())
        .Case("zero", PadValue::Zero)
        .Case("max", PadValue::Max)
        .Case("min", PadValue::Min)
        .Case("null", PadValue::Null)
        .Default(32u);
  };
  auto parseByteName = [&](StringRef Name) {
    return StringSwitch<unsigned>(Name.lower())
        .Case("byte0", ByteID::BYTE0)
        .Case("byte1", ByteID::BYTE1)
        .Case("byte2", ByteID::BYTE2)
        .Case("byte3", ByteID::BYTE3)
        .Default(32u);
  };
  auto parseCmpName = [&](StringRef Name) {
    return StringSwitch<unsigned>(Name.upper())
        .Case("EQ", CmpMode::EQ)
        .Case("NE", CmpMode::NE)
        .Case("LT", CmpMode::LT)
        .Case("GT", CmpMode::GT)
        .Case("LE", CmpMode::LE)
        .Case("GE", CmpMode::GE)
        .Default(32u);
  };
  auto parseRoundName = [&](StringRef Name) {
    return StringSwitch<unsigned>(Name.upper())
        .Case("RNONE", RMode::RNONE)
        .Case("RNE", RMode::RNE)
        .Case("RTZ", RMode::RTZ)
        .Case("RTM", RMode::RTM)
        .Case("RTP", RMode::RTP)
        .Case("RNA", RMode::RNA)
        .Case("RTO", RMode::RTO)
        .Case("RHB", RMode::RHB)
        .Default(32u);
  };
  auto parseSatName = [&](StringRef Name) {
    return StringSwitch<unsigned>(Name.lower())
        .Case("nosat", Sat::NOSAT)
        .Case("sat", Sat::SAT)
        .Default(32u);
  };
  auto parseCanonName = [&](StringRef Name) {
    return StringSwitch<unsigned>(Name.lower())
        .Case("normal", Canon::NORMAL_CANON)
        .Case(".normal", Canon::NORMAL_CANON)
        .Case("canon", Canon::CANON)
        .Case(".canon", Canon::CANON)
        .Default(32u);
  };
  auto parseDim = [&](size_t &Pos, DimExpr &Dim) {
    if (Pos >= Tokens.size())
      return false;
    if (Tokens[Pos].Kind == AsmToken::Integer) {
      Dim.HasValue = true;
      Dim.Imm = Tokens[Pos++].Integer;
      return true;
    }
    MCRegister Reg;
    if (!matchRegisterNameHelper(Reg, Tokens[Pos].Text))
      return false;
    Dim.HasValue = true;
    Dim.Reg = Reg;
    ++Pos;
    if (Pos + 1 < Tokens.size() &&
        (Tokens[Pos].Text == "+" || Tokens[Pos].Text == "-")) {
      bool Negative = Tokens[Pos++].Text == "-";
      if (Tokens[Pos].Kind != AsmToken::Integer)
        return false;
      Dim.Imm = Tokens[Pos++].Integer * (Negative ? -1 : 1);
    }
    return true;
  };
  auto parseSize = [&](size_t &Pos, unsigned &SizeCode) {
    if (!tokenIs(Pos, "<"))
      return false;
    ++Pos;
    if (Pos >= Tokens.size())
      return false;
    std::string Text = Tokens[Pos++].Text;
    if (Pos < Tokens.size() && !tokenIs(Pos, ">"))
      Text += Tokens[Pos++].Text;
    if (!tokenIs(Pos, ">"))
      return false;
    ++Pos;
    SizeCode = matchTileSizeHelper(Text);
    return SizeCode < 16;
  };
  auto parseSharedID = [&](StringRef Name, unsigned &ID) {
    if (Name.size() < 2 ||
        std::toupper(static_cast<unsigned char>(Name.front())) != 'S')
      return false;
    return !Name.drop_front().getAsInteger(10, ID) && ID < 64;
  };
  const unsigned MemberCount =
      sizeof(TileMacroBindingMembers) / sizeof(TileMacroBindingMembers[0]);

  auto findConfig = [&](Plan &P, StringRef Field) -> ConfigValue * {
    for (unsigned I = 0; I != P.Form->NumConfigs; ++I)
      if (Field == TileMacroConfigs[P.Form->FirstConfig + I].Field)
        return &P.Configs[I];
    return nullptr;
  };
  auto configImm = [&](Plan &P, StringRef Field, unsigned Fallback) {
    if (ConfigValue *Value = findConfig(P, Field))
      if (Value->HasValue)
        return Value->Imm;
    return Fallback;
  };
  auto applyDefault = [&](const TileMacroConfigDesc &Desc, ConfigValue &Value,
                          Plan &P) {
    if (!Desc.Default)
      return Desc.Optional;
    StringRef Default = Desc.Default;
    if (Desc.IsDimension) {
      int64_t Integer = 0;
      if (!Default.getAsInteger(10, Integer)) {
        Value.HasValue = Value.Dim.HasValue = true;
        Value.Dim.Imm = Integer;
        return true;
      }
      if (Default == "Row" || Default == "Col") {
        if (ConfigValue *Other = findConfig(P, Default)) {
          Value = *Other;
          Value.Explicit = false;
        }
      }
      return true;
    }
    Value.HasValue = true;
    int64_t Integer = 0;
    if (!Default.getAsInteger(10, Integer))
      Value.Imm = Integer;
    else if (Default == "AllPE")
      Value.Imm = 0xf;
    else if (Default == "EQ")
      Value.Imm = CmpMode::EQ;
    else if (Default == "Null")
      Value.Imm = PadValue::Null;
    else if (Default == "DTYPE_NONE")
      Value.Imm = DataType::EMPTY_DataType;
    else
      Value.HasValue = false;
    return true;
  };
  auto parseAttribute = [&](const TileMacroConfigDesc &Desc, size_t &Pos,
                            ConfigValue &Value) {
    if (Pos >= Tokens.size() || tokenIs(Pos, ">"))
      return false;
    StringRef Field = Desc.Field;
    StringRef Name = Tokens[Pos].Text;
    unsigned ParsedValue = 32;
    if (Field == "DataType" || Field == "SrcDataType" ||
        Field == "DstDataType" || Field == "ValueDataType" ||
        Field == "AType" || Field == "BType")
      ParsedValue = parseDataTypeName(Name);
    else if (Field == "U8")
      ParsedValue = Name.equals_insensitive("U8") ? DataType::U8 : 32;
    else if (Field == "U32")
      ParsedValue = Name.equals_insensitive("U32") ? DataType::U32 : 32;
    else if (Field == "DTYPE_NONE")
      ParsedValue =
          Name.equals_insensitive("DTYPE_NONE") ? DataType::EMPTY_DataType : 32;
    else if (Field == "PEMask")
      ParsedValue = parseMaskName(Name);
    else if (Field == "PadValue")
      ParsedValue = parsePadName(Name);
    else if (Field == "PadValueOrByteId") {
      ParsedValue = parsePadName(Name);
      if (ParsedValue == 32)
        ParsedValue = parseByteName(Name);
    } else if (Field == "CMode")
      ParsedValue = parseCmpName(Name);
    else if (Field == "RMode")
      ParsedValue = parseRoundName(Name);
    else if (Field == "Sat" || Field == "SatMode")
      ParsedValue = parseSatName(Name);
    else if (Field == "Canonicalize")
      ParsedValue = parseCanonName(Name);
    else if (Field == "Layout" || Field == "CubeLayout" ||
             Field == "WeightLayout") {
      ParsedValue = calculateBArgFormat(Name);
      if (ParsedValue == static_cast<unsigned>(-1))
        ParsedValue = 32;
      if (Field == "WeightLayout" && ParsedValue != 32 &&
          !LinxV5Op::isWeightLayout(ParsedValue))
        ParsedValue = 32;
      if (Field == "Layout" && ParsedValue != 32) {
        bool Load = StringRef(Parsed->Spelling).equals_insensitive("TLOAD");
        bool Store = StringRef(Parsed->Spelling).equals_insensitive("TSTORE");
        if ((Load && LinxV5Op::isCubeLoadConversion(ParsedValue)) ||
            (Store && LinxV5Op::isCubeStoreConversion(ParsedValue)))
          ParsedValue = 32;
      }
      if (Field == "CubeLayout" && ParsedValue != 32) {
        bool Load = StringRef(Parsed->Spelling).startswith_insensitive("TLOAD");
        bool Store =
            StringRef(Parsed->Spelling).startswith_insensitive("TSTORE");
        if ((Load && !LinxV5Op::isCubeLoadConversion(ParsedValue)) ||
            (Store && !LinxV5Op::isCubeStoreConversion(ParsedValue)))
          ParsedValue = 32;
      }
    } else if (Field == "PreMode" || Field == "PostMode" ||
               Field == "PostScale" || Field == "RowMax" ||
               Field == "GroupMax" || Field == "RowMaxInit" ||
               Field == "FlushToZero" || Field == "TransposeA" ||
               Field == "TransposeB" || Field == "CScale") {
      unsigned Index = StringSwitch<unsigned>(Field)
                           .Case("PreMode", 0)
                           .Case("PostMode", 1)
                           .Case("PostScale", 2)
                           .Case("RowMax", 3)
                           .Case("GroupMax", 4)
                           .Case("RowMaxInit", 5)
                           .Case("FlushToZero", 6)
                           .Case("TransposeA", 7)
                           .Case("TransposeB", 8)
                           .Case("CScale", 9)
                           .Default(10);
      if (!Name.equals_insensitive(Field))
        return false;
      if (Index < 3 && tokenIs(Pos + 1, "=") && Pos + 2 < Tokens.size() &&
          Tokens[Pos + 2].Kind == AsmToken::Integer) {
        Value.Imm = Tokens[Pos + 2].Integer;
        Pos += 3;
      } else if (Index >= 3 && !tokenIs(Pos + 1, "=")) {
        Value.Imm = 1;
        ++Pos;
      } else
        return false;
      Value.HasValue = Value.Explicit = true;
      return true;
    }
    if (ParsedValue == 32)
      return false;
    Value.HasValue = Value.Explicit = true;
    Value.Imm = ParsedValue;
    ++Pos;
    return true;
  };
  auto conditionEnabled = [&](StringRef Condition, Plan &P) {
    if (Condition.empty())
      return true;
    if (Condition == "RowMax&&RowMaxInit")
      return configImm(P, "RowMax", 0) && configImm(P, "RowMaxInit", 0);
    if (Condition == "PreMode=vector")
      return configImm(P, "PreMode", 0) > 1;
    if (Condition == "PreMode=scalar")
      return configImm(P, "PreMode", 0) == 1;
    if (Condition == "PostMode=vector")
      return configImm(P, "PostMode", 0) == 2;
    if (Condition == "PostMode=scalar")
      return configImm(P, "PostMode", 0) == 1;
    if (Condition == "RowMax")
      return configImm(P, "RowMax", 0) != 0;
    if (Condition == "GroupMax")
      return configImm(P, "GroupMax", 0) != 0;
    if (Condition == "CScale")
      return configImm(P, "CScale", 0) != 0;
    if (Condition == "DataType=U8")
      return configImm(P, "DataType", 32) == DataType::U8;
    if (Condition == "AType requires MX scale")
      return LinxV5Op::matrixMXTypeNeedsScale(configImm(P, "AType", 32));
    if (Condition == "BType requires MX scale")
      return LinxV5Op::matrixMXTypeNeedsScale(
          configImm(P, "BType", configImm(P, "AType", 32)));
    return false;
  };
  auto bindingForMember =
      [&](const TileMacroFormDesc &Form,
          unsigned MemberIndex) -> const TileMacroBindingDesc * {
    for (unsigned I = 0; I != Form.NumBindings; ++I) {
      const auto &Binding = TileMacroBindings[Form.FirstBinding + I];
      if (MemberIndex >= Binding.FirstMember &&
          MemberIndex < Binding.FirstMember + Binding.NumMembers)
        return &Binding;
    }
    return nullptr;
  };
  auto matchSimpleOperand = [&](const TileMacroBindingDesc &Binding,
                                size_t &Pos, BoundValue &Value) {
    bool Destination =
        Binding.Kind == TileMacroBindingKind::LocalDestination ||
        Binding.Kind == TileMacroBindingKind::SharedDestination ||
        Binding.Kind == TileMacroBindingKind::GPRDestination ||
        Binding.Kind == TileMacroBindingKind::PredicateTileDestination ||
        Binding.Kind == TileMacroBindingKind::PredicateCellDestination;
    if (Destination != tokenIs(Pos, "->"))
      return false;
    if (Destination)
      ++Pos;
    if ((Binding.Kind == TileMacroBindingKind::GPRSource ||
         Binding.Kind == TileMacroBindingKind::GPRDestination)) {
      StringRef Label = getTileMacroGPRLabel(Binding);
      if (!Label.empty() && tokenIs(Pos, Label) && tokenIs(Pos + 1, "="))
        Pos += 2;
    }
    if (Pos >= Tokens.size())
      return false;
    StringRef Name = Tokens[Pos].Text;
    char Prefix = Name.empty()
                      ? 0
                      : std::toupper(static_cast<unsigned char>(Name.front()));
    MCRegister Reg;
    unsigned SharedID = 0;
    bool Matched = false;
    switch (Binding.Kind) {
    case TileMacroBindingKind::LocalSource:
    case TileMacroBindingKind::LocalDestination:
      Matched = (Prefix == 'T' || Prefix == 'U' || Prefix == 'M' ||
                 Prefix == 'N') &&
                matchTileRegisterNameHelper(Reg, Name);
      if (Binding.Kind == TileMacroBindingKind::LocalDestination)
        Matched = Matched &&
                  (Reg == LinxV5::Tile_T || Reg == LinxV5::Tile_U ||
                   Reg == LinxV5::Tile_M || Reg == LinxV5::Tile_N);
      Value.Reg = Reg;
      break;
    case TileMacroBindingKind::PredicateTileSource:
    case TileMacroBindingKind::PredicateTileDestination:
      Matched = Prefix == 'U' && matchTileRegisterNameHelper(Reg, Name);
      if (Binding.Kind == TileMacroBindingKind::PredicateTileDestination)
        Matched = Matched && Reg == LinxV5::Tile_U;
      Value.Reg = Reg;
      break;
    case TileMacroBindingKind::PredicateCellSource:
    case TileMacroBindingKind::PredicateCellDestination:
      Matched = Prefix == 'M' && matchTileRegisterNameHelper(Reg, Name);
      if (Binding.Kind == TileMacroBindingKind::PredicateCellDestination)
        Matched = Matched && Reg == LinxV5::Tile_M;
      Value.Reg = Reg;
      break;
    case TileMacroBindingKind::SharedSource:
    case TileMacroBindingKind::SharedDestination:
      Matched = parseSharedID(Name, SharedID);
      Value.SharedID = SharedID;
      break;
    case TileMacroBindingKind::GPRSource:
    case TileMacroBindingKind::GPRDestination:
      Matched = Prefix != 'T' && Prefix != 'U' && Prefix != 'M' &&
                Prefix != 'S' && matchRegisterNameHelper(Reg, Name);
      Value.Reg = Reg;
      break;
    case TileMacroBindingKind::GPRListSource:
      return false;
    }
    if (!Matched)
      return false;
    Value.TileClass = Prefix;
    ++Pos;
    bool NeedsSize =
        Binding.Kind == TileMacroBindingKind::LocalDestination ||
        Binding.Kind == TileMacroBindingKind::SharedDestination ||
        Binding.Kind == TileMacroBindingKind::PredicateTileDestination ||
        Binding.Kind == TileMacroBindingKind::PredicateCellDestination;
    if (NeedsSize && !parseSize(Pos, Value.SizeCode))
      return false;
    Value.Present = true;
    return true;
  };
  auto parseRangeModifier = [&](const TileMacroBindingDesc &Binding,
                                size_t &Pos, BoundValue &Value) {
    if (!tokenIs(Pos, "["))
      return true;
    if (Binding.NumMembers != 1)
      return false;
    const auto &Member = TileMacroBindingMembers[Binding.FirstMember];
    bool IsDestination =
        Binding.Kind == TileMacroBindingKind::LocalDestination ||
        Binding.Kind == TileMacroBindingKind::SharedDestination ||
        Binding.Kind == TileMacroBindingKind::PredicateTileDestination ||
        Binding.Kind == TileMacroBindingKind::PredicateCellDestination;
    unsigned RequiredFlag = IsDestination ? 2 : 1;
    if (!(Member.ModifierFlags & RequiredFlag))
      return false;
    ++Pos;
    if (!tokenIs(Pos, "base") || !tokenIs(Pos + 1, "="))
      return false;
    Pos += 2;
    if (Pos >= Tokens.size())
      return false;
    MCRegister Base;
    if (!matchRegisterNameHelper(Base, Tokens[Pos].Text) || Base < LinxV5::R0 ||
        Base > LinxV5::R23)
      return false;
    ++Pos;
    if (!tokenIs(Pos, ",") || !tokenIs(Pos + 1, "offset") ||
        !tokenIs(Pos + 2, "=") || Pos + 3 >= Tokens.size() ||
        Tokens[Pos + 3].Kind != AsmToken::Integer ||
        Tokens[Pos + 3].Integer < 0 || Tokens[Pos + 3].Integer > 2047)
      return false;
    unsigned Offset = Tokens[Pos + 3].Integer;
    Pos += 4;
    if (!tokenIs(Pos, "]"))
      return false;
    ++Pos;
    RangeValue &Range = IsDestination ? Value.Assemble : Value.Subview;
    Range.Present = true;
    Range.Base = Base;
    Range.Offset = Offset;
    Range.SizeCode = 1;
    return true;
  };

  auto tryForm = [&](const TileMacroFormDesc &Form, Plan &P) {
    if (!StringRef(Parsed->Spelling).equals_insensitive(Form.Spelling))
      return false;
    P.Form = &Form;
    P.Configs.resize(Form.NumConfigs);
    P.Members.resize(MemberCount);
    size_t Pos = 0;
    if (!tokenIs(Pos, "<"))
      return false;
    ++Pos;
    for (unsigned I = 0; I != Form.NumConfigs; ++I) {
      const auto &Desc = TileMacroConfigs[Form.FirstConfig + I];
      ConfigValue &Value = P.Configs[I];
      if (tokenIs(Pos, ","))
        ++Pos;
      if (Desc.IsDimension) {
        if (tokenIs(Pos, Desc.Field) && tokenIs(Pos + 1, "=")) {
          Pos += 2;
          if ((tokenIs(Pos, "Derived") || tokenIs(Pos, "SourceDescriptor") ||
               tokenIs(Pos, "SourceOrValidCol") || tokenIs(Pos, "SourceOr1") ||
               tokenIs(Pos, "DestinationLayout") ||
               tokenIs(Pos, "CubeLayout")) &&
              Desc.Default && tokenIs(Pos, Desc.Default)) {
            ++Pos;
            Value.Explicit = true;
          } else {
            if (!parseDim(Pos, Value.Dim))
              return false;
            Value.HasValue = Value.Explicit = true;
          }
        } else if (!applyDefault(Desc, Value, P))
          return false;
      } else if (!parseAttribute(Desc, Pos, Value)) {
        if (!applyDefault(Desc, Value, P))
          return false;
      }
      if (Value.HasValue && !Desc.IsDimension) {
        for (unsigned T = 0; T != Desc.NumTargets; ++T) {
          const auto &Target = TileMacroConfigTargets[Desc.FirstTarget + T];
          if (Target.Kind == TileMacroTargetKind::Header)
            P.HeaderType = Value.Imm;
          if (Target.Slot == TileMacroTargetSlot::PEMask)
            P.PEMask = Value.Imm;
        }
      }
      if (Desc.Constraint && StringRef(Desc.Constraint) == "AllPE" &&
          Value.HasValue && Value.Imm != 0xf)
        return false;
      if (Desc.Constraint && Desc.IsDimension) {
        int64_t Required = 0;
        if (!StringRef(Desc.Constraint).getAsInteger(10, Required) &&
            (!Value.HasValue || Value.Dim.Reg != LinxV5::R0 ||
             Value.Dim.Imm != Required))
          return false;
      }
    }
    if (tokenIs(Pos, ","))
      ++Pos;
    if (!tokenIs(Pos, ">"))
      return false;
    ++Pos;

    for (unsigned I = 0; I != Form.NumBindings; ++I) {
      if (tokenIs(Pos, ","))
        ++Pos;
      const auto &Binding = TileMacroBindings[Form.FirstBinding + I];
      bool Enabled = true;
      for (unsigned M = 0; M != Binding.NumMembers; ++M) {
        const auto &Member = TileMacroBindingMembers[Binding.FirstMember + M];
        if (Member.Condition)
          Enabled &= conditionEnabled(Member.Condition, P);
      }
      if (!Enabled)
        continue;
      if (Binding.Kind == TileMacroBindingKind::GPRListSource) {
        if (!tokenIs(Pos, "[")) {
          bool AllOptional = true;
          for (unsigned M = 0; M != Binding.NumMembers; ++M) {
            const auto &Member =
                TileMacroBindingMembers[Binding.FirstMember + M];
            AllOptional &= Member.Optional || Member.Default;
          }
          if (AllOptional)
            continue;
          return false;
        }
        ++Pos;
        StringRef FirstLabel = getTileMacroGPRListLabel(Binding, 0);
        bool Named = !FirstLabel.empty() && tokenIs(Pos, FirstLabel) &&
                     tokenIs(Pos + 1, "=");
        if (Named) {
          SmallVector<bool, 4> Seen(Binding.NumMembers, false);
          bool First = true;
          while (!tokenIs(Pos, "]")) {
            if (!First) {
              if (!tokenIs(Pos, ","))
                return false;
              ++Pos;
            }
            unsigned MemberIndex = Binding.NumMembers;
            for (unsigned M = 0; M != Binding.NumMembers; ++M)
              if (tokenIs(Pos, getTileMacroGPRListLabel(Binding, M))) {
                MemberIndex = M;
                break;
              }
            if (MemberIndex >= Binding.NumMembers || Seen[MemberIndex] ||
                !tokenIs(Pos + 1, "="))
              return false;
            Pos += 2;
            if (Pos >= Tokens.size())
              return false;
            MCRegister Reg;
            StringRef GPRName = Tokens[Pos].Text;
            char Prefix =
                GPRName.empty()
                    ? 0
                    : std::toupper(static_cast<unsigned char>(GPRName.front()));
            if (Prefix == 'T' || Prefix == 'U' || Prefix == 'M' ||
                Prefix == 'S' || !matchRegisterNameHelper(Reg, GPRName))
              return false;
            Seen[MemberIndex] = true;
            BoundValue &Bound = P.Members[Binding.FirstMember + MemberIndex];
            Bound.Present = true;
            Bound.Reg = Reg;
            ++Pos;
            First = false;
          }
          for (unsigned M = 0; M != Binding.NumMembers; ++M) {
            const auto &Member =
                TileMacroBindingMembers[Binding.FirstMember + M];
            if (!P.Members[Binding.FirstMember + M].Present &&
                !(Member.Optional || Member.Default))
              return false;
          }
          ++Pos;
          continue;
        }
        for (unsigned M = 0; M != Binding.NumMembers; ++M) {
          if (tokenIs(Pos, ","))
            ++Pos;
          const auto &Member = TileMacroBindingMembers[Binding.FirstMember + M];
          BoundValue &Bound = P.Members[Binding.FirstMember + M];
          if (tokenIs(Pos, "]")) {
            if (!(Member.Optional || Member.Default))
              return false;
            continue;
          }
          MCRegister Reg;
          StringRef GPRName = Tokens[Pos].Text;
          char Prefix =
              GPRName.empty()
                  ? 0
                  : std::toupper(static_cast<unsigned char>(GPRName.front()));
          if (Prefix == 'T' || Prefix == 'U' || Prefix == 'M' ||
              Prefix == 'S' || !matchRegisterNameHelper(Reg, GPRName))
            return false;
          Bound.Present = true;
          Bound.Reg = Reg;
          ++Pos;
        }
        if (tokenIs(Pos, ","))
          ++Pos;
        if (!tokenIs(Pos, "]"))
          return false;
        ++Pos;
        continue;
      }
      if (Binding.NumMembers == 1) {
        const auto &Member = TileMacroBindingMembers[Binding.FirstMember];
        if (Member.Default && tokenIs(Pos, Binding.Field) &&
            tokenIs(Pos + 1, "=") && tokenIs(Pos + 2, Member.Default)) {
          Pos += 3;
          continue;
        }
      }
      BoundValue Candidate;
      if (matchSimpleOperand(Binding, Pos, Candidate)) {
        if (!parseRangeModifier(Binding, Pos, Candidate))
          return false;
        for (unsigned M = 0; M != Binding.NumMembers; ++M)
          P.Members[Binding.FirstMember + M] = Candidate;
        continue;
      }
      bool Optional = true;
      for (unsigned M = 0; M != Binding.NumMembers; ++M) {
        const auto &Member = TileMacroBindingMembers[Binding.FirstMember + M];
        Optional &= Member.Optional || Member.Default;
      }
      if (!Optional)
        return false;
    }
    if (tokenIs(Pos, ","))
      ++Pos;
    if (Pos != Tokens.size())
      return false;
    if (P.HeaderType == DataType::EMPTY_DataType &&
        Parsed->Operation->HeaderKind != TileMacroHeaderKind::TLSU)
      return false;

    ConfigValue *Row = findConfig(P, "Row");
    ConfigValue *Col = findConfig(P, "Col");
    ConfigValue *ValidRow = findConfig(P, "ValidRow");
    ConfigValue *ValidCol = findConfig(P, "ValidCol");
    const TileMacroConfigDesc *RowDesc = nullptr;
    if (Row) {
      unsigned Index = Row - P.Configs.data();
      RowDesc = &TileMacroConfigs[Form.FirstConfig + Index];
    }
    if (Col && !Col->HasValue) {
      ConfigValue *Layout = findConfig(P, "CubeLayout");
      if (Layout && Layout->HasValue) {
        unsigned L = Layout->Imm;
        unsigned C = (L == ArgFormat::ND2M32 || L == ArgFormat::M322ND)   ? 32
                     : (L == ArgFormat::ND2M16 || L == ArgFormat::M162ND) ? 16
                                                                          : 8;
        Col->HasValue = Col->Dim.HasValue = true;
        Col->Dim.Imm = C;
      } else {
        unsigned Index = Col - P.Configs.data();
        const auto &Desc = TileMacroConfigs[Form.FirstConfig + Index];
        if (Desc.Default && StringRef(Desc.Default) == "DestinationLayout") {
          if (ValidCol && ValidCol->HasValue) {
            Col->HasValue = true;
            Col->Dim = ValidCol->Dim;
          } else if (Row && Row->HasValue && Row->Dim.Reg == LinxV5::R0 &&
                     (Row->Dim.Imm == 16 || Row->Dim.Imm == 32)) {
            Col->HasValue = Col->Dim.HasValue = true;
            Col->Dim.Imm = Row->Dim.Imm == 32 ? 4 : 8;
          }
        }
      }
    }
    unsigned SizeCode = 0;
    for (unsigned I = 0; I != Form.NumBindings; ++I) {
      const auto &Binding = TileMacroBindings[Form.FirstBinding + I];
      bool Destination =
          Binding.Kind == TileMacroBindingKind::LocalDestination ||
          Binding.Kind == TileMacroBindingKind::SharedDestination ||
          Binding.Kind == TileMacroBindingKind::PredicateTileDestination ||
          Binding.Kind == TileMacroBindingKind::PredicateCellDestination;
      if (Destination && P.Members[Binding.FirstMember].Present) {
        SizeCode = P.Members[Binding.FirstMember].SizeCode;
        break;
      }
    }
    unsigned Type = configImm(
        P, "DstDataType",
        configImm(P, "ValueDataType",
                  configImm(P, "DataType",
                            configImm(P, "U8", configImm(P, "U32", 32)))));
    unsigned TypeBits = Type == 32 ? 0 : getTileMacroDataTypeBits(Type);
    if (!TypeBits)
      if (ConfigValue *Layout = findConfig(P, "CubeLayout"))
        if (Layout->HasValue)
          TypeBits = (Layout->Imm == ArgFormat::ND2M32 ||
                      Layout->Imm == ArgFormat::M322ND)
                         ? 32
                     : (Layout->Imm == ArgFormat::ND2M16 ||
                        Layout->Imm == ArgFormat::M162ND)
                         ? 16
                         : 8;
    DimExpr DerivedRow;
    StringRef RowExpression =
        RowDesc && RowDesc->ResolutionExpression
            ? StringRef(RowDesc->ResolutionExpression)
            : StringRef();
    unsigned LayoutCode = configImm(P, "Layout", ArgFormat::NORM);
    bool DirectCubeLayout = LayoutCode == ArgFormat::CUBE_M32 ||
                            LayoutCode == ArgFormat::CUBE_M16;
    bool LayoutDependentRow =
        RowExpression.startswith("Layout=RowMajor?");
    if ((RowExpression == "ValidRow" ||
         (LayoutDependentRow && DirectCubeLayout)) &&
        ValidRow && !ValidRow->HasValue) {
      // The encoded LB1 carrier is absent, so its architectural value is one.
      ValidRow->HasValue = ValidRow->Dim.HasValue = true;
      ValidRow->Dim.Imm = 1;
    }
    if ((RowExpression == "ValidRow" ||
         (LayoutDependentRow && DirectCubeLayout)) &&
        ValidRow && ValidRow->HasValue) {
      DerivedRow = ValidRow->Dim;
    } else if ((!LayoutDependentRow || !DirectCubeLayout) &&
               RowExpression.contains(".TSize/") && SizeCode && Col &&
               Col->HasValue && Col->Dim.Reg == LinxV5::R0 &&
               Col->Dim.Imm > 0 && TypeBits) {
      uint64_t Bits = (uint64_t{1} << (SizeCode + 6)) * 8;
      uint64_t Denominator = uint64_t(Col->Dim.Imm) * TypeBits;
      if (Denominator && Bits % Denominator == 0) {
        DerivedRow.HasValue = true;
        DerivedRow.Imm = Bits / Denominator;
      }
    }
    if (Row) {
      if (!Row->HasValue && DerivedRow.HasValue) {
        Row->HasValue = true;
        Row->Dim = DerivedRow;
      } else if (Row->HasValue && DerivedRow.HasValue &&
                 (Row->Dim.Reg != DerivedRow.Reg ||
                  Row->Dim.Imm != DerivedRow.Imm))
        return false;
    }
    if ((RowExpression == "ValidRow" ||
         (LayoutDependentRow && DirectCubeLayout)) &&
        !DerivedRow.HasValue)
      return false;
    if (ValidRow && !ValidRow->HasValue) {
      unsigned Index = ValidRow - P.Configs.data();
      const auto &Desc = TileMacroConfigs[Form.FirstConfig + Index];
      if (Desc.Default && StringRef(Desc.Default) == "Row" && Row &&
          Row->HasValue) {
        ValidRow->HasValue = true;
        ValidRow->Dim = Row->Dim;
      }
    }
    if (ValidCol && !ValidCol->HasValue) {
      unsigned Index = ValidCol - P.Configs.data();
      const auto &Desc = TileMacroConfigs[Form.FirstConfig + Index];
      if (Desc.Default && StringRef(Desc.Default) == "Col" && Col &&
          Col->HasValue) {
        ValidCol->HasValue = true;
        ValidCol->Dim = Col->Dim;
      }
    }
    return true;
  };

  std::vector<Plan> Matches;
  for (unsigned I = 0; I != Parsed->Operation->NumForms; ++I) {
    const auto &Form = TileMacroForms[Parsed->Operation->FirstForm + I];
    Plan Candidate;
    if (tryForm(Form, Candidate))
      Matches.push_back(std::move(Candidate));
  }
  if (Matches.size() != 1)
    return Error(IDLoc, Matches.empty()
                            ? "TileOp operands/configuration do not match any "
                              "exact PTO 0.58.6 form"
                            : "TileOp line matches multiple PTO 0.58.6 forms");
  Plan &P = Matches.front();
  const auto &Form = *P.Form;
  const auto &Operation = TileMacroOperations[Form.Operation];

  switch (Operation.HeaderKind) {
  case TileMacroHeaderKind::TEPL:
    emitToStreamer(Out, MCInstBuilder(LinxV5::BSTART_TEPL_NoMode)
                            .addImm(P.HeaderType)
                            .addImm(Operation.Selector));
    break;
  case TileMacroHeaderKind::TLSU:
    emitToStreamer(Out, MCInstBuilder(LinxV5::BSTART_TMA)
                            .addImm(P.HeaderType)
                            .addImm(Operation.Selector));
    break;
  case TileMacroHeaderKind::CUBE:
    emitToStreamer(Out, MCInstBuilder(LinxV5::BSTART_CUBE)
                            .addImm(P.HeaderType)
                            .addImm(Operation.Selector));
    break;
  case TileMacroHeaderKind::GMOV:
    emitToStreamer(Out,
                   MCInstBuilder(LinxV5::BSTART_GMOV).addImm(P.HeaderType));
    break;
  }

  unsigned Layout = ArgFormat::NORM, CanonMode = Canon::NORMAL_CANON;
  unsigned DstType = DataType::EMPTY_DataType;
  unsigned Pad =
      Operation.HeaderKind == TileMacroHeaderKind::CUBE ? 0 : PadValue::Null;
  unsigned CompareMode = CmpMode::EQ, RoundMode = RMode::RNONE;
  unsigned Saturate = Sat::NOSAT, Byte = ByteID::BYTE0;
  bool HasDATR = false, HasFPATR = false;
  unsigned FPAttrValues[10] = {};
  DimExpr Dims[3];
  auto fpAttrIndex = [](TileMacroTargetSlot Slot) -> int {
    switch (Slot) {
    case TileMacroTargetSlot::PreQuantMode:
      return 0;
    case TileMacroTargetSlot::ReluMode:
      return 1;
    case TileMacroTargetSlot::GroupNCode:
      return 2;
    case TileMacroTargetSlot::RowMaxEn:
      return 3;
    case TileMacroTargetSlot::GroupMaxEn:
      return 4;
    case TileMacroTargetSlot::RowMaxInit:
      return 5;
    case TileMacroTargetSlot::MaxAbsEn:
      return 6;
    case TileMacroTargetSlot::TransA:
      return 7;
    case TileMacroTargetSlot::TransB:
      return 8;
    case TileMacroTargetSlot::CScaleEn:
      return 9;
    default:
      return -1;
    }
  };

  for (unsigned I = 0; I != Form.NumConfigs; ++I) {
    const auto &Desc = TileMacroConfigs[Form.FirstConfig + I];
    const ConfigValue &Value = P.Configs[I];
    for (unsigned T = 0; T != Desc.NumTargets; ++T) {
      const auto &Target = TileMacroConfigTargets[Desc.FirstTarget + T];
      if (Target.Kind == TileMacroTargetKind::DATR && Value.HasValue) {
        HasDATR |= Value.Explicit || !Desc.Optional;
        switch (Target.Slot) {
        case TileMacroTargetSlot::Layout:
          Layout = Value.Imm;
          break;
        case TileMacroTargetSlot::Canonicalize:
          CanonMode = Value.Imm;
          break;
        case TileMacroTargetSlot::DataType:
          DstType = Value.Imm;
          break;
        case TileMacroTargetSlot::PadValueOrByteId:
          if (StringRef(Desc.Field) == "PadValueOrByteId" &&
              Value.Imm <= ByteID::BYTE3)
            Byte = Value.Imm;
          else
            Pad = Value.Imm;
          break;
        case TileMacroTargetSlot::CMode:
          CompareMode = Value.Imm;
          break;
        case TileMacroTargetSlot::RMode:
          RoundMode = Value.Imm;
          break;
        case TileMacroTargetSlot::Sat:
          Saturate = Value.Imm;
          break;
        default:
          break;
        }
      } else if (Target.Kind == TileMacroTargetKind::FPATR && Value.HasValue) {
        int Index = fpAttrIndex(Target.Slot);
        if (Index >= 0) {
          HasFPATR = true;
          FPAttrValues[Index] = Value.Imm;
        }
      } else if (Target.Kind == TileMacroTargetKind::DIM && Value.HasValue) {
        unsigned Slot = Target.Slot == TileMacroTargetSlot::LB0   ? 0
                        : Target.Slot == TileMacroTargetSlot::LB1 ? 1
                                                                  : 2;
        bool RuntimeDefault = !Value.Explicit && Desc.Default &&
                              (StringRef(Desc.Default).contains("Source") ||
                               StringRef(Desc.Default) == "DestinationLayout" ||
                               StringRef(Desc.Default) == "CubeLayout");
        bool PhysicalOne = Value.Dim.Reg == LinxV5::R0 &&
                           Value.Dim.Imm == 1;
        if (!RuntimeDefault && !PhysicalOne)
          Dims[Slot] = Value.Dim;
      }
    }
  }
  if (ConfigValue *BType = findConfig(P, "BType"))
    if (!BType->HasValue)
      DstType = P.HeaderType;

  if (HasDATR)
    emitToStreamer(Out, MCInstBuilder(LinxV5::BDATR)
                            .addImm(Layout)
                            .addImm(CanonMode)
                            .addImm(DstType)
                            .addImm(Pad)
                            .addImm(CompareMode)
                            .addImm(RoundMode)
                            .addImm(Saturate)
                            .addImm(Byte));
  if (HasFPATR) {
    MCInstBuilder Builder(LinxV5::B_FPATR);
    for (unsigned Value : FPAttrValues)
      Builder.addImm(Value);
    emitToStreamer(Out, Builder);
  }
  for (unsigned I = 0; I != 3; ++I)
    if (Dims[I].HasValue)
      emitToStreamer(Out, MCInstBuilder(LinxV5::B_DIM)
                              .addImm(I)
                              .addReg(Dims[I].Reg)
                              .addImm(Dims[I].Imm));

  SmallVector<unsigned, 16> ActiveCommands, ActiveIOTs;
  for (unsigned I = 0; I != Form.NumCommands; ++I) {
    const auto &Command = TileMacroCommands[Form.FirstCommand + I];
    bool Active = false;
    for (unsigned M = 0; M != Command.NumMembers; ++M)
      Active |= P.Members[TileMacroCommandMembers[Command.FirstMember + M]
                              .BindingMember]
                    .Present;
    if (Active) {
      ActiveCommands.push_back(I);
      if (Command.Kind == TileMacroCommandKind::IOT)
        ActiveIOTs.push_back(I);
    }
  }

  auto emitRangeModifiers = [&](unsigned Member) {
    const BoundValue &Value = P.Members[Member];
    const auto &Desc = TileMacroBindingMembers[Member];
    if (Value.Subview.Present) {
      unsigned SourceSelect = Desc.Slot == TileMacroMemberSlot::Role1 ? 1 : 0;
      emitToStreamer(Out, MCInstBuilder(LinxV5::B_SUBVIEW)
                              .addImm(SourceSelect)
                              .addReg(Value.Subview.Base)
                              .addImm(Value.Subview.Offset)
                              .addImm(Value.Subview.SizeCode));
    }
    if (Value.Assemble.Present)
      emitToStreamer(Out, MCInstBuilder(LinxV5::B_ASSEMBLE)
                              .addImm(1)
                              .addImm(1)
                              .addReg(Value.Assemble.Base)
                              .addImm(Value.Assemble.Offset)
                              .addImm(Value.Assemble.SizeCode));
  };

  for (unsigned I : ActiveCommands) {
    const auto &Command = TileMacroCommands[Form.FirstCommand + I];
    if (Command.Kind == TileMacroCommandKind::IOT) {
      SmallVector<MCRegister, 2> Sources;
      MCRegister Destination = LinxV5::NoRegister;
      unsigned DestinationSize = 0;
      for (unsigned M = 0; M != Command.NumMembers; ++M) {
        unsigned Member =
            TileMacroCommandMembers[Command.FirstMember + M].BindingMember;
        if (!P.Members[Member].Present)
          continue;
        const auto *Binding = bindingForMember(Form, Member);
        bool IsDestination =
            Binding &&
            (Binding->Kind == TileMacroBindingKind::LocalDestination ||
             Binding->Kind == TileMacroBindingKind::PredicateTileDestination ||
             Binding->Kind == TileMacroBindingKind::PredicateCellDestination);
        if (IsDestination) {
          Destination = P.Members[Member].Reg;
          DestinationSize = P.Members[Member].SizeCode;
        } else
          Sources.push_back(P.Members[Member].Reg);
      }
      bool Last = !ActiveIOTs.empty() && I == ActiveIOTs.back();
      if (Destination != LinxV5::NoRegister) {
        if (Sources.empty())
          emitToStreamer(Out, MCInstBuilder(LinxV5::B_IOT_NoSrc_Dst)
                                  .addReg(Destination)
                                  .addImm(P.PEMask)
                                  .addImm(DestinationSize)
                                  .addImm(Last));
        else if (Sources.size() == 1)
          emitToStreamer(Out, MCInstBuilder(LinxV5::B_IOT_OneSrc_Dst)
                                  .addReg(Destination)
                                  .addImm(P.PEMask)
                                  .addImm(DestinationSize)
                                  .addImm(Last)
                                  .addReg(Sources[0]));
        else
          emitToStreamer(Out, MCInstBuilder(LinxV5::B_IOT_TwoSrc_Dst)
                                  .addReg(Destination)
                                  .addImm(P.PEMask)
                                  .addImm(DestinationSize)
                                  .addImm(Last)
                                  .addReg(Sources[0])
                                  .addReg(Sources[1]));
      } else if (Sources.size() == 1)
        emitToStreamer(Out, MCInstBuilder(LinxV5::B_IOT_OneSrc_NoDst)
                                .addImm(P.PEMask)
                                .addImm(Last)
                                .addReg(Sources[0]));
      else if (Sources.size() == 2)
        emitToStreamer(Out, MCInstBuilder(LinxV5::B_IOT_TwoSrc_NoDst)
                                .addImm(P.PEMask)
                                .addImm(Last)
                                .addReg(Sources[0])
                                .addReg(Sources[1]));
      for (unsigned M = 0; M != Command.NumMembers; ++M) {
        unsigned Member =
            TileMacroCommandMembers[Command.FirstMember + M].BindingMember;
        if (P.Members[Member].Present)
          emitRangeModifiers(Member);
      }
    } else if (Command.Kind == TileMacroCommandKind::IOS) {
      for (unsigned M = 0; M != Command.NumMembers; ++M) {
        unsigned Member =
            TileMacroCommandMembers[Command.FirstMember + M].BindingMember;
        if (!P.Members[Member].Present)
          continue;
        const auto *Binding = bindingForMember(Form, Member);
        bool IsDestination =
            Binding && Binding->Kind == TileMacroBindingKind::SharedDestination;
        emitToStreamer(
            Out, MCInstBuilder(LinxV5::B_IOS)
                     .addImm(P.Members[Member].SharedID)
                     .addImm(P.PEMask)
                     .addImm(IsDestination ? P.Members[Member].SizeCode : 0));
        emitRangeModifiers(Member);
      }
    } else {
      MCRegister Dst = LinxV5::R0;
      MCRegister Sources[4] = {LinxV5::R0, LinxV5::R0, LinxV5::R0, LinxV5::R0};
      for (unsigned M = 0; M != Command.NumMembers; ++M) {
        unsigned Member =
            TileMacroCommandMembers[Command.FirstMember + M].BindingMember;
        if (!P.Members[Member].Present)
          continue;
        const auto &MemberDesc = TileMacroBindingMembers[Member];
        if (MemberDesc.Slot == TileMacroMemberSlot::RegDst)
          Dst = P.Members[Member].Reg;
        else {
          unsigned Slot = MemberDesc.Slot == TileMacroMemberSlot::RegSrc0   ? 0
                          : MemberDesc.Slot == TileMacroMemberSlot::RegSrc1 ? 1
                          : MemberDesc.Slot == TileMacroMemberSlot::RegSrc2 ? 2
                                                                            : 3;
          Sources[Slot] = P.Members[Member].Reg;
        }
      }
      emitToStreamer(Out, MCInstBuilder(LinxV5::B_IO)
                              .addReg(Dst)
                              .addReg(Sources[0])
                              .addReg(Sources[1])
                              .addReg(Sources[2]));
      if (Sources[3] != LinxV5::R0)
        emitToStreamer(Out, MCInstBuilder(LinxV5::B_IO)
                                .addReg(LinxV5::R0)
                                .addReg(Sources[3])
                                .addReg(LinxV5::R0)
                                .addReg(LinxV5::R0));
    }
  }
  return false;
}
