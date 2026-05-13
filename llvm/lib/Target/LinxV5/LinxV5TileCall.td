def CommonDag {
  dag val = (ins simm26_lsb0_bnext:$label,
       GPRPlusImm:$RegM, PlusImm17:$immM,
       GPRPlusImm:$RegN, PlusImm17:$immN,
       GPRPlusImm:$RegK, PlusImm17:$immK);
}

def DRDag {
  dag val = (ins DRImm:$DR);
}

class RegListInDag<bit IsRegListIn> {
   dag val = !if(!eq(IsRegListIn, 1), (ins GPRList:$List), (ins));
}

// isReg: none = 2, Reg = 1, Imm = 0
class TileSizeDagImpl<DAGOperand Op, int NumDst> {
  dag val =
    !if(!eq(NumDst, 4), (ins Op:$TileSize0, Op:$TileSize1, Op:$TileSize2, Op:$TileSize3),
    !if(!eq(NumDst, 3), (ins Op:$TileSize0, Op:$TileSize1, Op:$TileSize2),
    !if(!eq(NumDst, 2), (ins Op:$TileSize0, Op:$TileSize1),
    !if(!eq(NumDst, 1), (ins Op:$TileSize0),
    (ins)))));
}
class TileSizeDag<bits<2> IsReg, int NumDst> {
  dag val = TileSizeDagImpl<!if(!eq(IsReg,1), GPRWithBracket, TileSizeWithBracket), NumDst>.val;
}

// StackIsReg: none = 2, Reg = 1, Imm = 0
class StackSizeDag<bits<2> StackIsReg, int NumDst> {
  DAGOperand Op = !if(!gt(NumDst, 0), Stack_DstNoArrow, Stack_DstWithArrow);
  dag val = !if(!eq(StackIsReg, 1), (ins Op:$DstS, GPRWithBracket:$StackSize),
              !if(!eq(StackIsReg, 0), (ins Op:$DstS, TileSizeWithBracket:$StackSize),
                (ins)));
}

class TileDstDag<int NumDst> {
  dag val =
    !if(!eq(NumDst,4), (outs TILE_DstWithArrow:$Dst0, Tile_DstNoArrow:$Dst1, Tile_DstNoArrow:$Dst2, Tile_DstNoArrow:$Dst3),
    !if(!eq(NumDst,3), (outs TILE_DstWithArrow:$Dst0, Tile_DstNoArrow:$Dst1, Tile_DstNoArrow:$Dst2),
    !if(!eq(NumDst,2), (outs TILE_DstWithArrow:$Dst0, Tile_DstNoArrow:$Dst1),
    !if(!eq(NumDst,1), (outs TILE_DstWithArrow:$Dst0),
    (outs)))));
}

class TileSrcDag<int NumSrc> {
  dag val =
    !if(!eq(NumSrc,8), (ins TILE_Src_Reg:$SrcTile0, TILE_Src_Reg:$SrcTile1, TILE_Src_Reg:$SrcTile2, TILE_Src_Reg:$SrcTile3, TILE_Src_Reg:$SrcTile4, TILE_Src_Reg:$SrcTile5, TILE_Src_Reg:$SrcTile6, TILE_Src_Reg:$SrcTile7),
    !if(!eq(NumSrc,7), (ins TILE_Src_Reg:$SrcTile0, TILE_Src_Reg:$SrcTile1, TILE_Src_Reg:$SrcTile2, TILE_Src_Reg:$SrcTile3, TILE_Src_Reg:$SrcTile4, TILE_Src_Reg:$SrcTile5, TILE_Src_Reg:$SrcTile6),
    !if(!eq(NumSrc,6), (ins TILE_Src_Reg:$SrcTile0, TILE_Src_Reg:$SrcTile1, TILE_Src_Reg:$SrcTile2, TILE_Src_Reg:$SrcTile3, TILE_Src_Reg:$SrcTile4, TILE_Src_Reg:$SrcTile5),
    !if(!eq(NumSrc,5), (ins TILE_Src_Reg:$SrcTile0, TILE_Src_Reg:$SrcTile1, TILE_Src_Reg:$SrcTile2, TILE_Src_Reg:$SrcTile3, TILE_Src_Reg:$SrcTile4),
    !if(!eq(NumSrc,4), (ins TILE_Src_Reg:$SrcTile0, TILE_Src_Reg:$SrcTile1, TILE_Src_Reg:$SrcTile2, TILE_Src_Reg:$SrcTile3),
    !if(!eq(NumSrc,3), (ins TILE_Src_Reg:$SrcTile0, TILE_Src_Reg:$SrcTile1, TILE_Src_Reg:$SrcTile2),
    !if(!eq(NumSrc,2), (ins TILE_Src_Reg:$SrcTile0, TILE_Src_Reg:$SrcTile1),
    !if(!eq(NumSrc,1), (ins TILE_Src_Reg:$SrcTile0),
    (ins)))))))));
}

class TileSrcAsm<int NumSrc> {
  string val =
    !if(!eq(NumSrc, 8), "$SrcTile0, $SrcTile1, $SrcTile2, $SrcTile3, $SrcTile4, $SrcTile5, $SrcTile6, $SrcTile7",
    !if(!eq(NumSrc, 7), "$SrcTile0, $SrcTile1, $SrcTile2, $SrcTile3, $SrcTile4, $SrcTile5, $SrcTile6",
    !if(!eq(NumSrc, 6), "$SrcTile0, $SrcTile1, $SrcTile2, $SrcTile3, $SrcTile4, $SrcTile5",
    !if(!eq(NumSrc, 5), "$SrcTile0, $SrcTile1, $SrcTile2, $SrcTile3, $SrcTile4",
    !if(!eq(NumSrc, 4), "$SrcTile0, $SrcTile1, $SrcTile2, $SrcTile3",
    !if(!eq(NumSrc, 3), "$SrcTile0, $SrcTile1, $SrcTile2",
    !if(!eq(NumSrc, 2), "$SrcTile0, $SrcTile1",
    !if(!eq(NumSrc, 1), "$SrcTile0",
    ""))))))));
}

class TileDstAsm<int NumDst, int StackIsReg> {
  string str =
    !if(!eq(NumDst, 4), "$Dst0$TileSize0, $Dst1$TileSize1, $Dst2$TileSize2, $Dst3$TileSize3",
    !if(!eq(NumDst, 3), "$Dst0$TileSize0, $Dst1$TileSize1, $Dst2$TileSize2",
    !if(!eq(NumDst, 2), "$Dst0$TileSize0, $Dst1$TileSize1",
    !if(!eq(NumDst, 1), "$Dst0$TileSize0",
    ""))));
  string val = !if(!eq(StackIsReg, 2), str, !strconcat(str, "$DstS$StackSize"));
}

class DepSrcDag<bit HasDepSrc> {
  dag val = !if(!eq(HasDepSrc, 1), (ins RegDepSrc:$DepSrc), (ins));
}

class TileCallEntry<bits<8> dnum, bits<8> unum, bit reglist, bits<2> sizer, bits<2> mcall, bit dsrc, bit ddst, bits<2> stackIsReg> {
  bits<8> dNum = dnum;
  bits<8> uNum = unum;
  bits<1> RegList = reglist;
  bits<2> SizeR = sizer;
  bits<2> MCall = mcall;
  bits<1> DSrc = dsrc;
  bits<1> DDst = ddst;
  bits<2> StackIsReg = stackIsReg;
  Instruction Insn = !cast<Instruction>(NAME);
}

def TileCallTable : GenericTable {
  let FilterClass = "TileCallEntry";
  let Fields = ["dNum", "uNum", "RegList", "SizeR", "MCall", "DSrc", "DDst", "StackIsReg", "Insn"];
}

def lookupTileCallByDesc : SearchIndex {
  let Table = TileCallTable;
  let Key = ["dNum", "uNum", "RegList", "SizeR", "MCall", "DSrc", "DDst", "StackIsReg"];
}

multiclass PseudoTileCall<int DstNum, int SrcNum,
                       bit RegListIn, bits<2> IsReg, bits<2> StackIsReg, bit DSrc, bit DDst> {
  defvar maskChunk  = !if(RegListIn, "_RegListIn", "");
  defvar defUsesChunk  = "_" # !cast<string>(DstNum) # "d" # !cast<string>(SrcNum) # "u";
  defvar sizeChunk  = !if(!eq(IsReg, 1), "_SizeR",
                        !if(!eq(IsReg, 0),"_SizeI",
                        ""));
  defvar stackChunk = !if(!eq(StackIsReg, 1), "_StackR",
                        !if(!eq(StackIsReg, 0),"_StackI",
                        ""));
  defvar dsrcChunk  = !if(DSrc,  "_Dsrc",  "_noDsrc");
  defvar ddstChunk  = !if(DDst,  "_Dst",   "_noDst");
  defvar tileSrcStr = TileSrcAsm<SrcNum>.val;
  defvar dotStr = !if(!eq(tileSrcStr, ""), "", ",");

  defvar maskStr = !if(RegListIn, !strconcat(dotStr, " [$List]"), "");
  defvar dotStr1 = !if(!eq(dotStr, ","), ",",
                    !if(!eq(maskStr, ""), "",
                      ","));

  defvar DepSrcStr = !if(DSrc, !strconcat(dotStr1, " ${DepSrc}"),  "");
  defvar dotStr2 = !if(!eq(dotStr1, ","), ",",
                    !if(!eq(DepSrcStr, ""), "",
                      ","));

  defvar tileDstWithTileSizeStr = !if(!and(!eq(DstNum, 0), !eq(StackIsReg, 2)), "", !strconcat(dotStr2, TileDstAsm<DstNum, StackIsReg>.val));

  defvar DepDstStr = !if(DDst,
                      !if(!or(!gt(DstNum, 0), !ne(StackIsReg, 2)), ", d", ", ->d"),
                        "");

  def PseudoVCall_PAR #
      maskChunk #
      defUsesChunk #
      sizeChunk #
      stackChunk #
      dsrcChunk #
      ddstChunk
      : PseudoVCalllImpl<TileDstDag<DstNum>.val,
                         !con(CommonDag.val,
                              TileSizeDag<IsReg, DstNum>.val,
                              TileSrcDag<SrcNum>.val,
                              DepSrcDag<DSrc>.val,
                              DRDag.val,
                              StackSizeDag<StackIsReg, DstNum>.val,
                              RegListInDag<RegListIn>.val)>,
        TileCallEntry<DstNum, SrcNum, RegListIn, IsReg, 0, DSrc, DDst, StackIsReg> {
    let HasDepOutput = !if(DDst, 1, 0);
    let AsmString = "VPAR \t$label," # "\t<M: $RegM$immM, N: $RegN$immN, K: $RegK${immK}, ${DR}>\t" #
                    tileSrcStr # maskStr # DepSrcStr # tileDstWithTileSizeStr # DepDstStr;
  }

  def PseudoMCall_PAR #
      maskChunk #
      defUsesChunk #
      sizeChunk #
      stackChunk #
      dsrcChunk #
      ddstChunk
      : PseudoMCalllImpl<TileDstDag<DstNum>.val,
                         !con(CommonDag.val,
                              TileSizeDag<IsReg, DstNum>.val,
                              TileSrcDag<SrcNum>.val,
                              DepSrcDag<DSrc>.val,
                              DRDag.val,
                              StackSizeDag<StackIsReg, DstNum>.val,
                              RegListInDag<RegListIn>.val)>,
        TileCallEntry<DstNum, SrcNum, RegListIn, IsReg, 1, DSrc, DDst, StackIsReg> {
    let HasDepOutput = !if(DDst, 1, 0);
    let AsmString = "MPAR \t$label," # "\t<M: $RegM$immM, N: $RegN$immN, K: $RegK${immK}, ${DR}>\t" #
                    tileSrcStr # maskStr # DepSrcStr # tileDstWithTileSizeStr # DepDstStr;
  }
}

// 0d
foreach DstNum = [0] in {
foreach SrcNum = [0, 1,2,3,4,5,6,7,8] in {
foreach RegListIn = [0,1] in {
foreach IsReg = [2] in {
foreach StackIsReg = [0, 2] in {
foreach DSrc = [0,1] in {
foreach DDst = [0,1] in {
  defm "" : PseudoTileCall<DstNum, SrcNum, RegListIn, IsReg, StackIsReg, DSrc, DDst>;
}
}}}}}}

// 1d, 2d
foreach DstNum = [1,2,3,4] in {
foreach SrcNum = [0,1,2,3,4,5,6,7,8] in {
foreach RegListIn = [0,1] in {
// we don't have reg TileSize, ZERO Reg will be treated as a special imm
foreach IsReg = [0] in {
foreach StackIsReg = [0, 2] in {
foreach DSrc = [0,1] in {
foreach DDst = [0,1] in {
  defm "" : PseudoTileCall<DstNum, SrcNum, RegListIn, IsReg, StackIsReg, DSrc, DDst>;
}
}}}}}}