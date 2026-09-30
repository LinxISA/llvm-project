; RUN: llc -mtriple=linx64v5 -O2 -enable-all-vector-as-tilereg=true \
; RUN:     -linxv5-enable-HL-Inst-Opt=true -linxv5-enable-dim-opt=true \
; RUN:     -linxv5-enable-ldst-bridge=false \
; RUN:     -linxv5-enable-continuous-mem-opt=true \
; RUN:     -linxv5-enable-tile-clock-hand=false \
; RUN:     -linxv5-enable-simt-clock-hand=true -enable-misched=false \
; RUN:     %s -o - | FileCheck %s
; RUN: llc -mtriple=linx64v5 -O2 -enable-all-vector-as-tilereg=true \
; RUN:     -linxv5-enable-HL-Inst-Opt=true -linxv5-enable-dim-opt=true \
; RUN:     -linxv5-enable-ldst-bridge=false \
; RUN:     -linxv5-enable-continuous-mem-opt=true \
; RUN:     -linxv5-enable-tile-clock-hand=false \
; RUN:     -linxv5-enable-simt-clock-hand=true -enable-misched=false \
; RUN:     -filetype=obj %s -o %t

;; Issue #60: an unrolled if-select (`if (j == cid) TMOV(upd[j], cur)` over a
;; Local tile array) leaves a different tile live-out at the same tail-aligned
;; window depth per successor arm. getLiveoutLimits used to abort with
;; "Liveouts sync-up error!". The conflicting register classes (here the T/U
;; tile windows) now keep absolute tile registers instead of being rewritten
;; to window offsets; classes without conflicts (the M window) are still
;; rewritten.

; CHECK: main:
;; Source-level TMOV keeps its FP32 BSTART dtype; the destination stays an
;; absolute tile_ vreg because the T/U window rewrite was skipped.
; CHECK: BSTART.TLSU TMOV, FP32
; CHECK: B.IOT m#1, mask=1111, last, ->tile_{{[tu][0-9]+}}<512B>
; CHECK: BSTART.TLSU TMOV, FP32
; CHECK: B.IOT m#1, mask=1111, last, ->tile_{{[tu][0-9]+}}<512B>
;; The window rotation copy is emitted as a TCOPY macro (DTYPE_NONE carrier).
; CHECK: TCOPY tile_{{[tu][0-9]+}}, ->tile_{{[tu][0-9]+}}<512B>

; ModuleID = 'i60.cpp'
source_filename = "i60.cpp"
target datalayout = "e-m:e-p:64:64-i8:8:64-i16:16:64-i32:32:64-i64:64-i128:128-n64-S128"
target triple = "linx64v5-unknown-linux-musl"

%"struct.pto::Shape" = type { [5 x i32] }
%"struct.pto::Stride" = type { [5 x i32] }

$_ZN3pto12GlobalTensorIfNS_5ShapeILi1ELi1ELi1ELi1ELi1EEENS_6StrideILi1ELi1ELi128ELi128ELi1EEELNS_6LayoutE0EE12defaultShapeE = comdat any

$_ZN3pto12GlobalTensorIfNS_5ShapeILi1ELi1ELi1ELi1ELi1EEENS_6StrideILi1ELi1ELi128ELi128ELi1EEELNS_6LayoutE0EE13defaultStrideE = comdat any

@.str = private unnamed_addr constant [9 x i8] c"RowMajor\00", align 8
@.str.1 = private unnamed_addr constant [9 x i8] c"ColMajor\00", align 8
@.str.2 = private unnamed_addr constant [9 x i8] c"kNoneBox\00", align 8
@.str.3 = private unnamed_addr constant [18 x i8] c"UnsupportedLayout\00", align 8
@_ZN3pto12GlobalTensorIfNS_5ShapeILi1ELi1ELi1ELi1ELi1EEENS_6StrideILi1ELi1ELi128ELi128ELi1EEELNS_6LayoutE0EE12defaultShapeE = linkonce_odr dso_local global %"struct.pto::Shape" zeroinitializer, comdat, align 8
@_ZGVN3pto12GlobalTensorIfNS_5ShapeILi1ELi1ELi1ELi1ELi1EEENS_6StrideILi1ELi1ELi128ELi128ELi1EEELNS_6LayoutE0EE12defaultShapeE = linkonce_odr dso_local local_unnamed_addr global i64 0, comdat($_ZN3pto12GlobalTensorIfNS_5ShapeILi1ELi1ELi1ELi1ELi1EEENS_6StrideILi1ELi1ELi128ELi128ELi1EEELNS_6LayoutE0EE12defaultShapeE), align 8
@_ZN3pto12GlobalTensorIfNS_5ShapeILi1ELi1ELi1ELi1ELi1EEENS_6StrideILi1ELi1ELi128ELi128ELi1EEELNS_6LayoutE0EE13defaultStrideE = linkonce_odr dso_local global %"struct.pto::Stride" zeroinitializer, comdat, align 8
@_ZGVN3pto12GlobalTensorIfNS_5ShapeILi1ELi1ELi1ELi1ELi1EEENS_6StrideILi1ELi1ELi128ELi128ELi1EEELNS_6LayoutE0EE13defaultStrideE = linkonce_odr dso_local local_unnamed_addr global i64 0, comdat($_ZN3pto12GlobalTensorIfNS_5ShapeILi1ELi1ELi1ELi1ELi1EEENS_6StrideILi1ELi1ELi128ELi128ELi1EEELNS_6LayoutE0EE13defaultStrideE), align 8
@llvm.global_ctors = appending global [2 x { i32, ptr, ptr }] [{ i32, ptr, ptr } { i32 65535, ptr @__cxx_global_var_init, ptr @_ZN3pto12GlobalTensorIfNS_5ShapeILi1ELi1ELi1ELi1ELi1EEENS_6StrideILi1ELi1ELi128ELi128ELi1EEELNS_6LayoutE0EE12defaultShapeE }, { i32, ptr, ptr } { i32 65535, ptr @__cxx_global_var_init.4, ptr @_ZN3pto12GlobalTensorIfNS_5ShapeILi1ELi1ELi1ELi1ELi1EEENS_6StrideILi1ELi1ELi128ELi128ELi1EEELNS_6LayoutE0EE13defaultStrideE }]
@llvm.used = appending global [2 x ptr] [ptr @_ZN3pto12GlobalTensorIfNS_5ShapeILi1ELi1ELi1ELi1ELi1EEENS_6StrideILi1ELi1ELi128ELi128ELi1EEELNS_6LayoutE0EE12defaultShapeE, ptr @_ZN3pto12GlobalTensorIfNS_5ShapeILi1ELi1ELi1ELi1ELi1EEENS_6StrideILi1ELi1ELi128ELi128ELi1EEELNS_6LayoutE0EE13defaultStrideE], section "llvm.metadata"
@switch.table._ZN3pto18layout_type_to_strENS_10LayoutEnumE = private unnamed_addr constant [3 x ptr] [ptr @.str.2, ptr @.str, ptr @.str.1], align 8

; Function Attrs: mustprogress nofree norecurse nosync nounwind readnone willreturn
define dso_local noundef nonnull ptr @_ZN3pto18layout_type_to_strENS_10LayoutEnumE(i32 noundef signext %type) local_unnamed_addr #0 {
entry:
  %0 = icmp ult i32 %type, 3
  br i1 %0, label %switch.lookup, label %return

switch.lookup:                                    ; preds = %entry
  %1 = sext i32 %type to i64
  %switch.gep = getelementptr inbounds [3 x ptr], ptr @switch.table._ZN3pto18layout_type_to_strENS_10LayoutEnumE, i64 0, i64 %1
  %switch.load = load ptr, ptr %switch.gep, align 8
  br label %return

return:                                           ; preds = %entry, %switch.lookup
  %retval.0 = phi ptr [ %switch.load, %switch.lookup ], [ @.str.3, %entry ]
  ret ptr %retval.0
}

; Function Attrs: norecurse
define dso_local noundef signext i32 @main() local_unnamed_addr #1 {
entry:
  %buf = alloca [128 x float], align 4
  %0 = tail call float asm "", "=r,0"(float 1.000000e+00) #6
  br label %for.body

for.cond.cleanup:                                 ; preds = %_Z12update_cacheRA6_N3pto4TileILNS_8LocationE0EfLi1ELi128ELNS_7BLayoutE0ELi1ELi1ELNS_7SLayoutE0ELi512ELNS_8PadValueE3ELNS_11CompactModeE0EEERS6_Rl.exit
  %1 = tail call <128 x i32> asm sideeffect "BSTART.TEPL 34, ${1:D}\0A.if ${8:c} == 29\0AB.DATR CUBE_M32, Null\0A.elseif ${8:c} == 31\0AB.DATR CUBE_M16, Null\0A.endif\0AB.DIM zero, ${2:c}, ->lb0\0AB.DIM zero, ${3:c}, ->lb1\0AB.DIM zero, ${4:c}, ->lb2\0AB.IOT $5, mask=1111, last, ->$0<${6:Z}>\0AB.IOR [$7],[]\0A", "=@2Tr,i,i,i,i,@2Tr,i,r,i"(i32 1, i32 1, i32 1, i32 128, <128 x i32> %24, i32 3, float %0, i32 0) #7, !srcloc !6
  call void @llvm.lifetime.start.p0(i64 512, ptr nonnull %buf) #7
  call void asm sideeffect "BSTART.TLSU TSTORE, ${2:D}\0AB.DIM zero, ${3:c}, ->lb0\0AB.DIM zero, ${4:c}, ->lb1\0AB.DIM zero, ${5:c}, ->lb2\0AB.IOT $1, mask=1111, last\0AB.IOR [$0,$6], []\0A", "r,@2Tr,i,i,i,i,r,~{memory}"(ptr nonnull %buf, <128 x i32> %1, i32 1, i32 1, i32 1, i32 128, i64 512) #7, !srcloc !7
  call void @llvm.lifetime.end.p0(i64 512, ptr nonnull %buf) #7
  ret i32 0

for.body:                                         ; preds = %entry, %_Z12update_cacheRA6_N3pto4TileILNS_8LocationE0EfLi1ELi128ELNS_7BLayoutE0ELi1ELi1ELNS_7SLayoutE0ELi512ELNS_8PadValueE3ELNS_11CompactModeE0EEERS6_Rl.exit
  %2 = phi <128 x i32> [ undef, %entry ], [ %22, %_Z12update_cacheRA6_N3pto4TileILNS_8LocationE0EfLi1ELi128ELNS_7BLayoutE0ELi1ELi1ELNS_7SLayoutE0ELi512ELNS_8PadValueE3ELNS_11CompactModeE0EEERS6_Rl.exit ]
  %3 = phi <128 x i32> [ undef, %entry ], [ %23, %_Z12update_cacheRA6_N3pto4TileILNS_8LocationE0EfLi1ELi128ELNS_7BLayoutE0ELi1ELi1ELNS_7SLayoutE0ELi512ELNS_8PadValueE3ELNS_11CompactModeE0EEERS6_Rl.exit ]
  %4 = phi <128 x i32> [ undef, %entry ], [ %24, %_Z12update_cacheRA6_N3pto4TileILNS_8LocationE0EfLi1ELi128ELNS_7BLayoutE0ELi1ELi1ELNS_7SLayoutE0ELi512ELNS_8PadValueE3ELNS_11CompactModeE0EEERS6_Rl.exit ]
  %5 = phi <128 x i32> [ undef, %entry ], [ %25, %_Z12update_cacheRA6_N3pto4TileILNS_8LocationE0EfLi1ELi128ELNS_7BLayoutE0ELi1ELi1ELNS_7SLayoutE0ELi512ELNS_8PadValueE3ELNS_11CompactModeE0EEERS6_Rl.exit ]
  %6 = phi <128 x i32> [ undef, %entry ], [ %26, %_Z12update_cacheRA6_N3pto4TileILNS_8LocationE0EfLi1ELi128ELNS_7BLayoutE0ELi1ELi1ELNS_7SLayoutE0ELi512ELNS_8PadValueE3ELNS_11CompactModeE0EEERS6_Rl.exit ]
  %7 = phi <128 x i32> [ undef, %entry ], [ %27, %_Z12update_cacheRA6_N3pto4TileILNS_8LocationE0EfLi1ELi128ELNS_7BLayoutE0ELi1ELi1ELNS_7SLayoutE0ELi512ELNS_8PadValueE3ELNS_11CompactModeE0EEERS6_Rl.exit ]
  %r.017 = phi i64 [ 0, %entry ], [ %add.i.i, %_Z12update_cacheRA6_N3pto4TileILNS_8LocationE0EfLi1ELi128ELNS_7BLayoutE0ELi1ELi1ELNS_7SLayoutE0ELi512ELNS_8PadValueE3ELNS_11CompactModeE0EEERS6_Rl.exit ]
  %8 = tail call <128 x i32> asm sideeffect "BSTART.TEPL 59, ${1:D}\0A.if ${7:c} == 29\0AB.DATR CUBE_M32, Null\0A.elseif ${7:c} == 31\0AB.DATR CUBE_M16, Null\0A.endif\0AB.DIM zero, ${2:c}, ->lb0\0AB.DIM zero, ${3:c}, ->lb1\0AB.DIM zero, ${4:c}, ->lb2\0AB.IOT mask=1111, last, ->$0<${5:Z}>\0AB.IOR [$6],[]\0A", "=@2Tr,i,i,i,i,i,r,i"(i32 1, i32 1, i32 1, i32 128, i32 3, float %0, i32 0) #7, !srcloc !8
  %add.i.i = add nuw nsw i64 %r.017, 1
  %9 = tail call i64 @llvm.cttz.i64(i64 %add.i.i, i1 true), !range !9
  %cmp1.not.i = icmp eq i64 %9, 0
  br i1 %cmp1.not.i, label %for.inc12.4.thread56.i, label %for.inc.i

for.inc.i:                                        ; preds = %for.body
  %10 = tail call <128 x i32> asm sideeffect "BSTART.TEPL 0, ${1:D}\0A.if ${8:c} == 29\0AB.DATR CUBE_M32, Null\0A.elseif ${8:c} == 31\0AB.DATR CUBE_M16, Null\0A.endif\0AB.DIM zero, ${2:c}, ->lb0\0AB.DIM zero, ${3:c}, ->lb1\0AB.DIM zero, ${4:c}, ->lb2\0AB.IOT $5, $6, mask=1111, last, ->$0<${7:Z}>\0A", "=@2Tr,i,i,i,i,@2Tr,@2Tr,i,i"(i32 1, i32 1, i32 1, i32 128, <128 x i32> %8, <128 x i32> %7, i32 3, i32 0) #7, !srcloc !10
  %cmp1.1.not.i = icmp eq i64 %9, 1
  br i1 %cmp1.1.not.i, label %for.inc12.1.thread.i, label %for.inc.1.i

for.inc.1.i:                                      ; preds = %for.inc.i
  %11 = tail call <128 x i32> asm sideeffect "BSTART.TEPL 0, ${1:D}\0A.if ${8:c} == 29\0AB.DATR CUBE_M32, Null\0A.elseif ${8:c} == 31\0AB.DATR CUBE_M16, Null\0A.endif\0AB.DIM zero, ${2:c}, ->lb0\0AB.DIM zero, ${3:c}, ->lb1\0AB.DIM zero, ${4:c}, ->lb2\0AB.IOT $5, $6, mask=1111, last, ->$0<${7:Z}>\0A", "=@2Tr,i,i,i,i,@2Tr,@2Tr,i,i"(i32 1, i32 1, i32 1, i32 128, <128 x i32> %10, <128 x i32> %6, i32 3, i32 0) #7, !srcloc !10
  %cmp1.2.i = icmp ugt i64 %9, 2
  br i1 %cmp1.2.i, label %for.inc.2.i, label %if.then8.2.i

for.inc.2.i:                                      ; preds = %for.inc.1.i
  %12 = tail call <128 x i32> asm sideeffect "BSTART.TEPL 0, ${1:D}\0A.if ${8:c} == 29\0AB.DATR CUBE_M32, Null\0A.elseif ${8:c} == 31\0AB.DATR CUBE_M16, Null\0A.endif\0AB.DIM zero, ${2:c}, ->lb0\0AB.DIM zero, ${3:c}, ->lb1\0AB.DIM zero, ${4:c}, ->lb2\0AB.IOT $5, $6, mask=1111, last, ->$0<${7:Z}>\0A", "=@2Tr,i,i,i,i,@2Tr,@2Tr,i,i"(i32 1, i32 1, i32 1, i32 128, <128 x i32> %11, <128 x i32> %5, i32 3, i32 0) #7, !srcloc !10
  %cmp1.3.not.i = icmp eq i64 %9, 3
  br i1 %cmp1.3.not.i, label %if.then8.3.i, label %for.inc.3.i

for.inc.3.i:                                      ; preds = %for.inc.2.i
  %13 = tail call <128 x i32> asm sideeffect "BSTART.TEPL 0, ${1:D}\0A.if ${8:c} == 29\0AB.DATR CUBE_M32, Null\0A.elseif ${8:c} == 31\0AB.DATR CUBE_M16, Null\0A.endif\0AB.DIM zero, ${2:c}, ->lb0\0AB.DIM zero, ${3:c}, ->lb1\0AB.DIM zero, ${4:c}, ->lb2\0AB.IOT $5, $6, mask=1111, last, ->$0<${7:Z}>\0A", "=@2Tr,i,i,i,i,@2Tr,@2Tr,i,i"(i32 1, i32 1, i32 1, i32 128, <128 x i32> %12, <128 x i32> %4, i32 3, i32 0) #7, !srcloc !10
  %cmp1.4.i = icmp ugt i64 %9, 4
  br i1 %cmp1.4.i, label %for.inc.4.i, label %if.then8.4.i

for.inc.4.i:                                      ; preds = %for.inc.3.i
  %14 = tail call <128 x i32> asm sideeffect "BSTART.TEPL 0, ${1:D}\0A.if ${8:c} == 29\0AB.DATR CUBE_M32, Null\0A.elseif ${8:c} == 31\0AB.DATR CUBE_M16, Null\0A.endif\0AB.DIM zero, ${2:c}, ->lb0\0AB.DIM zero, ${3:c}, ->lb1\0AB.DIM zero, ${4:c}, ->lb2\0AB.IOT $5, $6, mask=1111, last, ->$0<${7:Z}>\0A", "=@2Tr,i,i,i,i,@2Tr,@2Tr,i,i"(i32 1, i32 1, i32 1, i32 128, <128 x i32> %13, <128 x i32> %3, i32 3, i32 0) #7, !srcloc !10
  %cmp1.5.not.i = icmp eq i64 %9, 5
  br i1 %cmp1.5.not.i, label %if.then8.5.i, label %for.inc12.4.i

for.inc12.4.thread56.i:                           ; preds = %for.body
  %15 = tail call <128 x i32> asm sideeffect "BSTART.TLSU TMOV, ${2:D}\0AB.DIM zero, ${4:c}, ->lb0\0AB.DIM zero, ${5:c}, ->lb1\0AB.IOT $1, mask=1111, last, ->$0<${3:Z}>\0A", "=&@2Tr,@2Tr,i,i,i,i"(<128 x i32> %8, i32 1, i32 3, i32 1, i32 1) #7, !srcloc !11
  br label %_Z12update_cacheRA6_N3pto4TileILNS_8LocationE0EfLi1ELi128ELNS_7BLayoutE0ELi1ELi1ELNS_7SLayoutE0ELi512ELNS_8PadValueE3ELNS_11CompactModeE0EEERS6_Rl.exit

for.inc12.1.thread.i:                             ; preds = %for.inc.i
  %16 = tail call <128 x i32> asm sideeffect "BSTART.TLSU TMOV, ${2:D}\0AB.DIM zero, ${4:c}, ->lb0\0AB.DIM zero, ${5:c}, ->lb1\0AB.IOT $1, mask=1111, last, ->$0<${3:Z}>\0A", "=&@2Tr,@2Tr,i,i,i,i"(<128 x i32> %10, i32 1, i32 3, i32 1, i32 1) #7, !srcloc !11
  br label %_Z12update_cacheRA6_N3pto4TileILNS_8LocationE0EfLi1ELi128ELNS_7BLayoutE0ELi1ELi1ELNS_7SLayoutE0ELi512ELNS_8PadValueE3ELNS_11CompactModeE0EEERS6_Rl.exit

if.then8.2.i:                                     ; preds = %for.inc.1.i
  %17 = tail call <128 x i32> asm sideeffect "BSTART.TLSU TMOV, ${2:D}\0AB.DIM zero, ${4:c}, ->lb0\0AB.DIM zero, ${5:c}, ->lb1\0AB.IOT $1, mask=1111, last, ->$0<${3:Z}>\0A", "=&@2Tr,@2Tr,i,i,i,i"(<128 x i32> %11, i32 1, i32 3, i32 1, i32 1) #7, !srcloc !11
  br label %_Z12update_cacheRA6_N3pto4TileILNS_8LocationE0EfLi1ELi128ELNS_7BLayoutE0ELi1ELi1ELNS_7SLayoutE0ELi512ELNS_8PadValueE3ELNS_11CompactModeE0EEERS6_Rl.exit

if.then8.3.i:                                     ; preds = %for.inc.2.i
  %18 = tail call <128 x i32> asm sideeffect "BSTART.TLSU TMOV, ${2:D}\0AB.DIM zero, ${4:c}, ->lb0\0AB.DIM zero, ${5:c}, ->lb1\0AB.IOT $1, mask=1111, last, ->$0<${3:Z}>\0A", "=&@2Tr,@2Tr,i,i,i,i"(<128 x i32> %12, i32 1, i32 3, i32 1, i32 1) #7, !srcloc !11
  br label %_Z12update_cacheRA6_N3pto4TileILNS_8LocationE0EfLi1ELi128ELNS_7BLayoutE0ELi1ELi1ELNS_7SLayoutE0ELi512ELNS_8PadValueE3ELNS_11CompactModeE0EEERS6_Rl.exit

if.then8.4.i:                                     ; preds = %for.inc.3.i
  %19 = tail call <128 x i32> asm sideeffect "BSTART.TLSU TMOV, ${2:D}\0AB.DIM zero, ${4:c}, ->lb0\0AB.DIM zero, ${5:c}, ->lb1\0AB.IOT $1, mask=1111, last, ->$0<${3:Z}>\0A", "=&@2Tr,@2Tr,i,i,i,i"(<128 x i32> %13, i32 1, i32 3, i32 1, i32 1) #7, !srcloc !11
  br label %_Z12update_cacheRA6_N3pto4TileILNS_8LocationE0EfLi1ELi128ELNS_7BLayoutE0ELi1ELi1ELNS_7SLayoutE0ELi512ELNS_8PadValueE3ELNS_11CompactModeE0EEERS6_Rl.exit

for.inc12.4.i:                                    ; preds = %for.inc.4.i
  %20 = tail call <128 x i32> asm sideeffect "BSTART.TEPL 0, ${1:D}\0A.if ${8:c} == 29\0AB.DATR CUBE_M32, Null\0A.elseif ${8:c} == 31\0AB.DATR CUBE_M16, Null\0A.endif\0AB.DIM zero, ${2:c}, ->lb0\0AB.DIM zero, ${3:c}, ->lb1\0AB.DIM zero, ${4:c}, ->lb2\0AB.IOT $5, $6, mask=1111, last, ->$0<${7:Z}>\0A", "=@2Tr,i,i,i,i,@2Tr,@2Tr,i,i"(i32 1, i32 1, i32 1, i32 128, <128 x i32> %14, <128 x i32> %2, i32 3, i32 0) #7, !srcloc !10
  br label %_Z12update_cacheRA6_N3pto4TileILNS_8LocationE0EfLi1ELi128ELNS_7BLayoutE0ELi1ELi1ELNS_7SLayoutE0ELi512ELNS_8PadValueE3ELNS_11CompactModeE0EEERS6_Rl.exit

if.then8.5.i:                                     ; preds = %for.inc.4.i
  %21 = tail call <128 x i32> asm sideeffect "BSTART.TLSU TMOV, ${2:D}\0AB.DIM zero, ${4:c}, ->lb0\0AB.DIM zero, ${5:c}, ->lb1\0AB.IOT $1, mask=1111, last, ->$0<${3:Z}>\0A", "=&@2Tr,@2Tr,i,i,i,i"(<128 x i32> %14, i32 1, i32 3, i32 1, i32 1) #7, !srcloc !11
  br label %_Z12update_cacheRA6_N3pto4TileILNS_8LocationE0EfLi1ELi128ELNS_7BLayoutE0ELi1ELi1ELNS_7SLayoutE0ELi512ELNS_8PadValueE3ELNS_11CompactModeE0EEERS6_Rl.exit

_Z12update_cacheRA6_N3pto4TileILNS_8LocationE0EfLi1ELi128ELNS_7BLayoutE0ELi1ELi1ELNS_7SLayoutE0ELi512ELNS_8PadValueE3ELNS_11CompactModeE0EEERS6_Rl.exit: ; preds = %for.inc12.4.thread56.i, %for.inc12.1.thread.i, %if.then8.2.i, %if.then8.3.i, %if.then8.4.i, %for.inc12.4.i, %if.then8.5.i
  %22 = phi <128 x i32> [ %2, %for.inc12.4.thread56.i ], [ %2, %for.inc12.1.thread.i ], [ %2, %if.then8.2.i ], [ %2, %if.then8.3.i ], [ %2, %if.then8.4.i ], [ %2, %for.inc12.4.i ], [ %21, %if.then8.5.i ]
  %23 = phi <128 x i32> [ %3, %for.inc12.4.thread56.i ], [ %3, %for.inc12.1.thread.i ], [ %3, %if.then8.2.i ], [ %3, %if.then8.3.i ], [ %19, %if.then8.4.i ], [ %3, %for.inc12.4.i ], [ %3, %if.then8.5.i ]
  %24 = phi <128 x i32> [ %4, %for.inc12.4.thread56.i ], [ %4, %for.inc12.1.thread.i ], [ %4, %if.then8.2.i ], [ %18, %if.then8.3.i ], [ %4, %if.then8.4.i ], [ %4, %for.inc12.4.i ], [ %4, %if.then8.5.i ]
  %25 = phi <128 x i32> [ %5, %for.inc12.4.thread56.i ], [ %5, %for.inc12.1.thread.i ], [ %17, %if.then8.2.i ], [ %5, %if.then8.3.i ], [ %5, %if.then8.4.i ], [ %5, %for.inc12.4.i ], [ %5, %if.then8.5.i ]
  %26 = phi <128 x i32> [ %6, %for.inc12.4.thread56.i ], [ %16, %for.inc12.1.thread.i ], [ %6, %if.then8.2.i ], [ %6, %if.then8.3.i ], [ %6, %if.then8.4.i ], [ %6, %for.inc12.4.i ], [ %6, %if.then8.5.i ]
  %27 = phi <128 x i32> [ %15, %for.inc12.4.thread56.i ], [ %7, %for.inc12.1.thread.i ], [ %7, %if.then8.2.i ], [ %7, %if.then8.3.i ], [ %7, %if.then8.4.i ], [ %7, %for.inc12.4.i ], [ %7, %if.then8.5.i ]
  %exitcond.not = icmp eq i64 %add.i.i, 8
  br i1 %exitcond.not, label %for.cond.cleanup, label %for.body, !llvm.loop !12
}

; Function Attrs: argmemonly mustprogress nocallback nofree nosync nounwind willreturn
declare void @llvm.lifetime.start.p0(i64 immarg, ptr nocapture) #2

; Function Attrs: argmemonly mustprogress nocallback nofree nosync nounwind willreturn
declare void @llvm.lifetime.end.p0(i64 immarg, ptr nocapture) #2

; Function Attrs: mustprogress nofree nosync nounwind willreturn
define internal void @__cxx_global_var_init() #3 section ".text.startup" comdat($_ZN3pto12GlobalTensorIfNS_5ShapeILi1ELi1ELi1ELi1ELi1EEENS_6StrideILi1ELi1ELi128ELi128ELi1EEELNS_6LayoutE0EE12defaultShapeE) {
entry:
  %0 = load i8, ptr @_ZGVN3pto12GlobalTensorIfNS_5ShapeILi1ELi1ELi1ELi1ELi1EEENS_6StrideILi1ELi1ELi128ELi128ELi1EEELNS_6LayoutE0EE12defaultShapeE, align 8
  %guard.uninitialized = icmp eq i8 %0, 0
  br i1 %guard.uninitialized, label %init.check, label %init.end

init.check:                                       ; preds = %entry
  store i32 1, ptr @_ZN3pto12GlobalTensorIfNS_5ShapeILi1ELi1ELi1ELi1ELi1EEENS_6StrideILi1ELi1ELi128ELi128ELi1EEELNS_6LayoutE0EE12defaultShapeE, align 8, !tbaa !14
  tail call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(16) getelementptr inbounds (%"struct.pto::Shape", ptr @_ZN3pto12GlobalTensorIfNS_5ShapeILi1ELi1ELi1ELi1ELi1EEENS_6StrideILi1ELi1ELi128ELi128ELi1EEELNS_6LayoutE0EE12defaultShapeE, i64 0, i32 0, i64 1), i8 0, i64 16, i1 false), !tbaa !14
  %1 = tail call ptr @llvm.invariant.start.p0(i64 20, ptr nonnull @_ZN3pto12GlobalTensorIfNS_5ShapeILi1ELi1ELi1ELi1ELi1EEENS_6StrideILi1ELi1ELi128ELi128ELi1EEELNS_6LayoutE0EE12defaultShapeE)
  store i8 1, ptr @_ZGVN3pto12GlobalTensorIfNS_5ShapeILi1ELi1ELi1ELi1ELi1EEENS_6StrideILi1ELi1ELi128ELi128ELi1EEELNS_6LayoutE0EE12defaultShapeE, align 8
  br label %init.end

init.end:                                         ; preds = %init.check, %entry
  ret void
}

; Function Attrs: argmemonly mustprogress nocallback nofree nosync nounwind willreturn
declare ptr @llvm.invariant.start.p0(i64 immarg, ptr nocapture) #2

; Function Attrs: mustprogress nofree nosync nounwind willreturn
define internal void @__cxx_global_var_init.4() #3 section ".text.startup" comdat($_ZN3pto12GlobalTensorIfNS_5ShapeILi1ELi1ELi1ELi1ELi1EEENS_6StrideILi1ELi1ELi128ELi128ELi1EEELNS_6LayoutE0EE13defaultStrideE) {
entry:
  %0 = load i8, ptr @_ZGVN3pto12GlobalTensorIfNS_5ShapeILi1ELi1ELi1ELi1ELi1EEENS_6StrideILi1ELi1ELi128ELi128ELi1EEELNS_6LayoutE0EE13defaultStrideE, align 8
  %guard.uninitialized = icmp eq i8 %0, 0
  br i1 %guard.uninitialized, label %init.check, label %init.end

init.check:                                       ; preds = %entry
  store i32 1, ptr @_ZN3pto12GlobalTensorIfNS_5ShapeILi1ELi1ELi1ELi1ELi1EEENS_6StrideILi1ELi1ELi128ELi128ELi1EEELNS_6LayoutE0EE13defaultStrideE, align 8, !tbaa !14
  tail call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(16) getelementptr inbounds (%"struct.pto::Stride", ptr @_ZN3pto12GlobalTensorIfNS_5ShapeILi1ELi1ELi1ELi1ELi1EEENS_6StrideILi1ELi1ELi128ELi128ELi1EEELNS_6LayoutE0EE13defaultStrideE, i64 0, i32 0, i64 1), i8 0, i64 16, i1 false), !tbaa !14
  %1 = tail call ptr @llvm.invariant.start.p0(i64 20, ptr nonnull @_ZN3pto12GlobalTensorIfNS_5ShapeILi1ELi1ELi1ELi1ELi1EEENS_6StrideILi1ELi1ELi128ELi128ELi1EEELNS_6LayoutE0EE13defaultStrideE)
  store i8 1, ptr @_ZGVN3pto12GlobalTensorIfNS_5ShapeILi1ELi1ELi1ELi1ELi1EEENS_6StrideILi1ELi1ELi128ELi128ELi1EEELNS_6LayoutE0EE13defaultStrideE, align 8
  br label %init.end

init.end:                                         ; preds = %init.check, %entry
  ret void
}

; Function Attrs: mustprogress nocallback nofree nosync nounwind readnone speculatable willreturn
declare i64 @llvm.cttz.i64(i64, i1 immarg) #4

; Function Attrs: argmemonly nocallback nofree nounwind willreturn writeonly
declare void @llvm.memset.p0.i64(ptr nocapture writeonly, i8, i64, i1 immarg) #5

attributes #0 = { mustprogress nofree norecurse nosync nounwind readnone willreturn "frame-pointer"="none" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-features"="+relax" }
attributes #1 = { norecurse "frame-pointer"="none" "min-legal-vector-width"="4096" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-features"="+relax" }
attributes #2 = { argmemonly mustprogress nocallback nofree nosync nounwind willreturn }
attributes #3 = { mustprogress nofree nosync nounwind willreturn "frame-pointer"="none" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-features"="+relax" }
attributes #4 = { mustprogress nocallback nofree nosync nounwind readnone speculatable willreturn }
attributes #5 = { argmemonly nocallback nofree nounwind willreturn writeonly }
attributes #6 = { nounwind readnone }
attributes #7 = { nounwind }

!llvm.linker.options = !{}
!llvm.module.flags = !{!0, !1, !2, !3, !4}
!llvm.ident = !{!5}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 1, !"target-abi", !"lp64"}
!2 = !{i32 7, !"PIC Level", i32 2}
!3 = !{i32 7, !"PIE Level", i32 2}
!4 = !{i32 1, !"SmallDataLimit", i32 8}
!5 = !{!"clang version 15.0.4 (git@github.com:LinxISA/llvm-project.git 61d4f98bb63f9f4fdd9e948f0667eee7171acd99)"}
!6 = !{i64 1952135, i64 2236851301, i64 2236851327, i64 2236851382, i64 2236851412, i64 2236851463, i64 1952195, i64 1952226, i64 1952257, i64 1952288, i64 1952333}
!7 = !{i64 1475714, i64 1475755, i64 1475791, i64 1475827, i64 1475862, i64 1475899}
!8 = !{i64 2021909, i64 2236873282, i64 2236873308, i64 2236873363, i64 2236873393, i64 2236873444, i64 2021969, i64 2022000, i64 2022031, i64 2022062, i64 2022103}
!9 = !{i64 0, i64 65}
!10 = !{i64 1842399, i64 2236797801, i64 2236797827, i64 2236797882, i64 2236797912, i64 2236797963, i64 1842458, i64 1842489, i64 1842520, i64 1842551}
!11 = !{i64 1329553, i64 1329593, i64 1329633, i64 1329673}
!12 = distinct !{!12, !13}
!13 = !{!"llvm.loop.mustprogress"}
!14 = !{!15, !15, i64 0}
!15 = !{!"int", !16, i64 0}
!16 = !{!"omnipotent char", !17, i64 0}
!17 = !{!"Simple C++ TBAA"}
