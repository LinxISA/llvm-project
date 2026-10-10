; RUN: llc -mtriple=linx64v5 -O2 -enable-all-vector-as-tilereg=true \
; RUN:     -linxv5-enable-HL-Inst-Opt=true -linxv5-enable-dim-opt=true \
; RUN:     -linxv5-enable-ldst-bridge=false \
; RUN:     -linxv5-enable-continuous-mem-opt=true \
; RUN:     -linxv5-enable-tile-clock-hand=true \
; RUN:     -linxv5-enable-simt-clock-hand=true -enable-misched=false \
; RUN:     %s -o - | FileCheck %s

;; Issue #121: a loop-invariant tile (defined once before the loop, only read
;; inside) that assignLoopInvar placed on an otherwise idle register bank must
;; not be re-anchored with a TLSU TMOV on the back edge. The block performed
;; no allocation for that bank, so the inherited window state is exactly what
;; the sync-group references were encoded against; a physical keep-alive copy
;; is pure overhead (a full 32KB TLSU move per iteration).

;; The invariant stays referenced through its inherited window offset.
; CHECK-LABEL: _Z6kernelPDF16_PKDF16_S_l:
; CHECK: B.IOT {{t#[0-9]+, }}n#1, mask=1111, last, ->t<32KB>
;; No keep-alive TMOV / TCOPY anywhere in the loop.
; CHECK-NOT: TMOV
; CHECK-NOT: TCOPY

; ModuleID = '/tmp/i121/repro.cpp'
source_filename = "/tmp/i121/repro.cpp"
target datalayout = "e-m:e-p:64:64-i8:8:64-i16:16:64-i32:32:64-i64:64-i128:128-n64-S128"
target triple = "linx64v5-unknown-linux-musl"

%"struct.pto::Shape" = type { [5 x i32] }
%"struct.pto::Stride" = type { [5 x i32] }

$_ZN3pto12GlobalTensorIDF16_NS_5ShapeILi1ELi1ELi1ELi1ELi1EEENS_6StrideILi1ELi1ELin1ELin1ELin1EEELNS_6LayoutE0EE12defaultShapeE = comdat any

$_ZN3pto12GlobalTensorIDF16_NS_5ShapeILi1ELi1ELi1ELi1ELi1EEENS_6StrideILi1ELi1ELin1ELin1ELin1EEELNS_6LayoutE0EE13defaultStrideE = comdat any

@.str = private unnamed_addr constant [9 x i8] c"RowMajor\00", align 8
@.str.1 = private unnamed_addr constant [9 x i8] c"ColMajor\00", align 8
@.str.2 = private unnamed_addr constant [9 x i8] c"kNoneBox\00", align 8
@.str.3 = private unnamed_addr constant [18 x i8] c"UnsupportedLayout\00", align 8
@_ZN3pto12GlobalTensorIDF16_NS_5ShapeILi1ELi1ELi1ELi1ELi1EEENS_6StrideILi1ELi1ELin1ELin1ELin1EEELNS_6LayoutE0EE12defaultShapeE = linkonce_odr dso_local global %"struct.pto::Shape" zeroinitializer, comdat, align 8
@_ZGVN3pto12GlobalTensorIDF16_NS_5ShapeILi1ELi1ELi1ELi1ELi1EEENS_6StrideILi1ELi1ELin1ELin1ELin1EEELNS_6LayoutE0EE12defaultShapeE = linkonce_odr dso_local local_unnamed_addr global i64 0, comdat($_ZN3pto12GlobalTensorIDF16_NS_5ShapeILi1ELi1ELi1ELi1ELi1EEENS_6StrideILi1ELi1ELin1ELin1ELin1EEELNS_6LayoutE0EE12defaultShapeE), align 8
@_ZN3pto12GlobalTensorIDF16_NS_5ShapeILi1ELi1ELi1ELi1ELi1EEENS_6StrideILi1ELi1ELin1ELin1ELin1EEELNS_6LayoutE0EE13defaultStrideE = linkonce_odr dso_local global %"struct.pto::Stride" zeroinitializer, comdat, align 8
@_ZGVN3pto12GlobalTensorIDF16_NS_5ShapeILi1ELi1ELi1ELi1ELi1EEENS_6StrideILi1ELi1ELin1ELin1ELin1EEELNS_6LayoutE0EE13defaultStrideE = linkonce_odr dso_local local_unnamed_addr global i64 0, comdat($_ZN3pto12GlobalTensorIDF16_NS_5ShapeILi1ELi1ELi1ELi1ELi1EEENS_6StrideILi1ELi1ELin1ELin1ELin1EEELNS_6LayoutE0EE13defaultStrideE), align 8
@llvm.global_ctors = appending global [2 x { i32, ptr, ptr }] [{ i32, ptr, ptr } { i32 65535, ptr @__cxx_global_var_init, ptr @_ZN3pto12GlobalTensorIDF16_NS_5ShapeILi1ELi1ELi1ELi1ELi1EEENS_6StrideILi1ELi1ELin1ELin1ELin1EEELNS_6LayoutE0EE12defaultShapeE }, { i32, ptr, ptr } { i32 65535, ptr @__cxx_global_var_init.4, ptr @_ZN3pto12GlobalTensorIDF16_NS_5ShapeILi1ELi1ELi1ELi1ELi1EEENS_6StrideILi1ELi1ELin1ELin1ELin1EEELNS_6LayoutE0EE13defaultStrideE }]
@llvm.used = appending global [2 x ptr] [ptr @_ZN3pto12GlobalTensorIDF16_NS_5ShapeILi1ELi1ELi1ELi1ELi1EEENS_6StrideILi1ELi1ELin1ELin1ELin1EEELNS_6LayoutE0EE12defaultShapeE, ptr @_ZN3pto12GlobalTensorIDF16_NS_5ShapeILi1ELi1ELi1ELi1ELi1EEENS_6StrideILi1ELi1ELin1ELin1ELin1EEELNS_6LayoutE0EE13defaultStrideE], section "llvm.metadata"
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

define dso_local void @_Z6kernelPDF16_PKDF16_S_l(ptr noundef %x, ptr noundef %gamma, ptr noundef %out, i64 noundef %rows) local_unnamed_addr #1 {
entry:
  %0 = tail call <4096 x i32> asm sideeffect "BSTART.TLSU TLOAD, ${3:D}\0A.if ${7:c} == 3\0A.if ${8:c} == 21\0AB.DATR ND2M32.normal, Null\0A.elseif ${8:c} == 22\0AB.DATR ND2M16.normal, Null\0A.else\0AB.DATR ND2N8.normal, Null\0A.endif\0A.elseif ${7:c} == 2\0A.if ${8:c} == 21\0AB.DATR ND2M32.normal, Min\0A.elseif ${8:c} == 22\0AB.DATR ND2M16.normal, Min\0A.else\0AB.DATR ND2N8.normal, Min\0A.endif\0A.elseif ${7:c} == 1\0A.if ${8:c} == 21\0AB.DATR ND2M32.normal, Max\0A.elseif ${8:c} == 22\0AB.DATR ND2M16.normal, Max\0A.else\0AB.DATR ND2N8.normal, Max\0A.endif\0A.else\0A.if ${8:c} == 21\0AB.DATR ND2M32.normal, Zero\0A.elseif ${8:c} == 22\0AB.DATR ND2M16.normal, Zero\0A.else\0AB.DATR ND2N8.normal, Zero\0A.endif\0A.endif\0AB.DIM $5, 0, ->lb0\0AB.DIM $6, 0, ->lb1\0AB.IOT mask=1111, last, ->$0<${4:Z}>\0AB.IOR [$1,$2], []\0A", "=@2Tr,r,r,i,i,r,r,i,i,~{memory}"(ptr %gamma, i64 512, i32 4, i32 8, i64 256, i64 32, i32 0, i32 21) #5, !srcloc !6
  %1 = tail call <8192 x i32> asm sideeffect "BSTART.TEPL 27, ${1:D}\0A.if ${8:c} == 0\0AB.DATR ${2:D}, RNONE\0A.elseif ${8:c} == 1\0AB.DATR ${2:D}, RNE\0A.elseif ${8:c} == 2\0AB.DATR ${2:D}, RTZ\0A.elseif ${8:c} == 3\0AB.DATR ${2:D}, RTM\0A.elseif ${8:c} == 4\0AB.DATR ${2:D}, RTP\0A.elseif ${8:c} == 5\0AB.DATR ${2:D}, RNA\0A.elseif ${8:c} == 6\0AB.DATR ${2:D}, RTO\0A.elseif ${8:c} == 7\0AB.DATR ${2:D}, RHB\0A.endif\0AB.DIM $5, 0, ->lb0\0AB.DIM $6, 0, ->lb1\0AB.IOT $3, mask=1111, last, ->$0<${4:Z}>\0A", "=@2Tr,i,i,@2Tr,i,r,r,i,i"(i32 4, i32 1, <4096 x i32> %0, i32 9, i32 256, i32 32, i32 29, i32 0) #5, !srcloc !7
  %cmp98 = icmp sgt i64 %rows, 0
  br i1 %cmp98, label %for.body, label %for.cond.cleanup

for.cond.cleanup:                                 ; preds = %for.body, %entry
  ret void

for.body:                                         ; preds = %entry, %for.body
  %i.099 = phi i64 [ %inc, %for.body ], [ 0, %entry ]
  %mul = shl nsw i64 %i.099, 13
  %add.ptr = getelementptr inbounds half, ptr %x, i64 %mul
  %add.ptr2 = getelementptr inbounds half, ptr %out, i64 %mul
  %2 = tail call <4096 x i32> asm sideeffect "BSTART.TLSU TLOAD, ${3:D}\0A.if ${7:c} == 3\0A.if ${8:c} == 21\0AB.DATR ND2M32.normal, Null\0A.elseif ${8:c} == 22\0AB.DATR ND2M16.normal, Null\0A.else\0AB.DATR ND2N8.normal, Null\0A.endif\0A.elseif ${7:c} == 2\0A.if ${8:c} == 21\0AB.DATR ND2M32.normal, Min\0A.elseif ${8:c} == 22\0AB.DATR ND2M16.normal, Min\0A.else\0AB.DATR ND2N8.normal, Min\0A.endif\0A.elseif ${7:c} == 1\0A.if ${8:c} == 21\0AB.DATR ND2M32.normal, Max\0A.elseif ${8:c} == 22\0AB.DATR ND2M16.normal, Max\0A.else\0AB.DATR ND2N8.normal, Max\0A.endif\0A.else\0A.if ${8:c} == 21\0AB.DATR ND2M32.normal, Zero\0A.elseif ${8:c} == 22\0AB.DATR ND2M16.normal, Zero\0A.else\0AB.DATR ND2N8.normal, Zero\0A.endif\0A.endif\0AB.DIM $5, 0, ->lb0\0AB.DIM $6, 0, ->lb1\0AB.IOT mask=1111, last, ->$0<${4:Z}>\0AB.IOR [$1,$2], []\0A", "=@2Tr,r,r,i,i,r,r,i,i,~{memory}"(ptr %add.ptr, i64 512, i32 4, i32 8, i64 256, i64 32, i32 0, i32 21) #5, !srcloc !6
  %3 = tail call <8192 x i32> asm sideeffect "BSTART.TEPL 27, ${1:D}\0A.if ${8:c} == 0\0AB.DATR ${2:D}, RNONE\0A.elseif ${8:c} == 1\0AB.DATR ${2:D}, RNE\0A.elseif ${8:c} == 2\0AB.DATR ${2:D}, RTZ\0A.elseif ${8:c} == 3\0AB.DATR ${2:D}, RTM\0A.elseif ${8:c} == 4\0AB.DATR ${2:D}, RTP\0A.elseif ${8:c} == 5\0AB.DATR ${2:D}, RNA\0A.elseif ${8:c} == 6\0AB.DATR ${2:D}, RTO\0A.elseif ${8:c} == 7\0AB.DATR ${2:D}, RHB\0A.endif\0AB.DIM $5, 0, ->lb0\0AB.DIM $6, 0, ->lb1\0AB.IOT $3, mask=1111, last, ->$0<${4:Z}>\0A", "=@2Tr,i,i,@2Tr,i,r,r,i,i"(i32 4, i32 1, <4096 x i32> %2, i32 9, i32 256, i32 32, i32 29, i32 0) #5, !srcloc !7
  %4 = tail call <8192 x i32> asm sideeffect "BSTART.TEPL 2, ${1:D}\0A.if ${8:c} == 29\0AB.DATR CUBE_M32, Null\0A.elseif ${8:c} == 31\0AB.DATR CUBE_M16, Null\0A.endif\0AB.DIM $2, 0, ->lb0\0AB.DIM $3, 0, ->lb1\0AB.DIM zero, ${4:c}, ->lb2\0AB.IOT $5, $6, mask=1111, last, ->$0<${7:Z}>\0A", "=@2Tr,i,r,r,i,@2Tr,@2Tr,i,i"(i32 1, i64 256, i64 32, i32 256, <8192 x i32> %3, <8192 x i32> %3, i32 9, i32 29) #5, !srcloc !8
  %5 = tail call <8192 x i32> asm sideeffect "BSTART.TEPL 2, ${1:D}\0A.if ${8:c} == 29\0AB.DATR CUBE_M32, Null\0A.elseif ${8:c} == 31\0AB.DATR CUBE_M16, Null\0A.endif\0AB.DIM $2, 0, ->lb0\0AB.DIM $3, 0, ->lb1\0AB.DIM zero, ${4:c}, ->lb2\0AB.IOT $5, $6, mask=1111, last, ->$0<${7:Z}>\0A", "=@2Tr,i,r,r,i,@2Tr,@2Tr,i,i"(i32 1, i64 256, i64 32, i32 256, <8192 x i32> %4, <8192 x i32> %3, i32 9, i32 29) #5, !srcloc !8
  %6 = tail call <8192 x i32> asm sideeffect "BSTART.TEPL 2, ${1:D}\0A.if ${8:c} == 29\0AB.DATR CUBE_M32, Null\0A.elseif ${8:c} == 31\0AB.DATR CUBE_M16, Null\0A.endif\0AB.DIM $2, 0, ->lb0\0AB.DIM $3, 0, ->lb1\0AB.DIM zero, ${4:c}, ->lb2\0AB.IOT $5, $6, mask=1111, last, ->$0<${7:Z}>\0A", "=@2Tr,i,r,r,i,@2Tr,@2Tr,i,i"(i32 1, i64 256, i64 32, i32 256, <8192 x i32> %5, <8192 x i32> %3, i32 9, i32 29) #5, !srcloc !8
  %7 = tail call <8192 x i32> asm sideeffect "BSTART.TEPL 0, ${1:D}\0A.if ${8:c} == 29\0AB.DATR CUBE_M32, Null\0A.elseif ${8:c} == 31\0AB.DATR CUBE_M16, Null\0A.endif\0AB.DIM $2, 0, ->lb0\0AB.DIM $3, 0, ->lb1\0AB.DIM zero, ${4:c}, ->lb2\0AB.IOT $5, $6, mask=1111, last, ->$0<${7:Z}>\0A", "=@2Tr,i,r,r,i,@2Tr,@2Tr,i,i"(i32 1, i32 256, i32 32, i32 256, <8192 x i32> %4, <8192 x i32> %5, i32 9, i32 29) #5, !srcloc !9
  %8 = tail call <8192 x i32> asm sideeffect "BSTART.TEPL 0, ${1:D}\0A.if ${8:c} == 29\0AB.DATR CUBE_M32, Null\0A.elseif ${8:c} == 31\0AB.DATR CUBE_M16, Null\0A.endif\0AB.DIM $2, 0, ->lb0\0AB.DIM $3, 0, ->lb1\0AB.DIM zero, ${4:c}, ->lb2\0AB.IOT $5, $6, mask=1111, last, ->$0<${7:Z}>\0A", "=@2Tr,i,r,r,i,@2Tr,@2Tr,i,i"(i32 1, i32 256, i32 32, i32 256, <8192 x i32> %5, <8192 x i32> %6, i32 9, i32 29) #5, !srcloc !9
  %9 = tail call <8192 x i32> asm sideeffect "BSTART.TEPL 0, ${1:D}\0A.if ${8:c} == 29\0AB.DATR CUBE_M32, Null\0A.elseif ${8:c} == 31\0AB.DATR CUBE_M16, Null\0A.endif\0AB.DIM $2, 0, ->lb0\0AB.DIM $3, 0, ->lb1\0AB.DIM zero, ${4:c}, ->lb2\0AB.IOT $5, $6, mask=1111, last, ->$0<${7:Z}>\0A", "=@2Tr,i,r,r,i,@2Tr,@2Tr,i,i"(i32 1, i32 256, i32 32, i32 256, <8192 x i32> %6, <8192 x i32> %7, i32 9, i32 29) #5, !srcloc !9
  %10 = tail call <8192 x i32> asm sideeffect "BSTART.TEPL 2, ${1:D}\0A.if ${8:c} == 29\0AB.DATR CUBE_M32, Null\0A.elseif ${8:c} == 31\0AB.DATR CUBE_M16, Null\0A.endif\0AB.DIM $2, 0, ->lb0\0AB.DIM $3, 0, ->lb1\0AB.DIM zero, ${4:c}, ->lb2\0AB.IOT $5, $6, mask=1111, last, ->$0<${7:Z}>\0A", "=@2Tr,i,r,r,i,@2Tr,@2Tr,i,i"(i32 1, i64 256, i64 32, i32 256, <8192 x i32> %3, <8192 x i32> %1, i32 9, i32 29) #5, !srcloc !8
  %11 = tail call <4096 x i32> asm sideeffect "BSTART.TEPL 27, ${1:D}\0A.if ${8:c} == 0\0AB.DATR ${2:D}, RNONE\0A.elseif ${8:c} == 1\0AB.DATR ${2:D}, RNE\0A.elseif ${8:c} == 2\0AB.DATR ${2:D}, RTZ\0A.elseif ${8:c} == 3\0AB.DATR ${2:D}, RTM\0A.elseif ${8:c} == 4\0AB.DATR ${2:D}, RTP\0A.elseif ${8:c} == 5\0AB.DATR ${2:D}, RNA\0A.elseif ${8:c} == 6\0AB.DATR ${2:D}, RTO\0A.elseif ${8:c} == 7\0AB.DATR ${2:D}, RHB\0A.endif\0AB.DIM $5, 0, ->lb0\0AB.DIM $6, 0, ->lb1\0AB.IOT $3, mask=1111, last, ->$0<${4:Z}>\0A", "=@2Tr,i,i,@2Tr,i,r,r,i,i"(i32 1, i32 4, <8192 x i32> %10, i32 8, i32 256, i32 32, i32 29, i32 0) #5, !srcloc !7
  tail call void asm sideeffect "BSTART.TLSU TSTORE, ${3:D}\0AB.DATR M322ND.normal, Null\0AB.DIM $4, 0, ->lb0\0AB.DIM $5, 0, ->lb1\0AB.IOT $1, mask=1111, last\0AB.IOR [$0,$2], []\0A", "r,@2Tr,r,i,r,r,~{memory}"(ptr %add.ptr2, <4096 x i32> %11, i64 512, i32 4, i64 256, i64 32) #5, !srcloc !10
  %inc = add nuw nsw i64 %i.099, 1
  %exitcond.not = icmp eq i64 %inc, %rows
  br i1 %exitcond.not, label %for.cond.cleanup, label %for.body, !llvm.loop !11
}

; Function Attrs: mustprogress nofree nosync nounwind willreturn
define internal void @__cxx_global_var_init() #2 section ".text.startup" comdat($_ZN3pto12GlobalTensorIDF16_NS_5ShapeILi1ELi1ELi1ELi1ELi1EEENS_6StrideILi1ELi1ELin1ELin1ELin1EEELNS_6LayoutE0EE12defaultShapeE) {
entry:
  %0 = load i8, ptr @_ZGVN3pto12GlobalTensorIDF16_NS_5ShapeILi1ELi1ELi1ELi1ELi1EEENS_6StrideILi1ELi1ELin1ELin1ELin1EEELNS_6LayoutE0EE12defaultShapeE, align 8
  %guard.uninitialized = icmp eq i8 %0, 0
  br i1 %guard.uninitialized, label %init.check, label %init.end

init.check:                                       ; preds = %entry
  store i32 1, ptr @_ZN3pto12GlobalTensorIDF16_NS_5ShapeILi1ELi1ELi1ELi1ELi1EEENS_6StrideILi1ELi1ELin1ELin1ELin1EEELNS_6LayoutE0EE12defaultShapeE, align 8, !tbaa !13
  tail call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(16) getelementptr inbounds (%"struct.pto::Shape", ptr @_ZN3pto12GlobalTensorIDF16_NS_5ShapeILi1ELi1ELi1ELi1ELi1EEENS_6StrideILi1ELi1ELin1ELin1ELin1EEELNS_6LayoutE0EE12defaultShapeE, i64 0, i32 0, i64 1), i8 0, i64 16, i1 false), !tbaa !13
  %1 = tail call ptr @llvm.invariant.start.p0(i64 20, ptr nonnull @_ZN3pto12GlobalTensorIDF16_NS_5ShapeILi1ELi1ELi1ELi1ELi1EEENS_6StrideILi1ELi1ELin1ELin1ELin1EEELNS_6LayoutE0EE12defaultShapeE)
  store i8 1, ptr @_ZGVN3pto12GlobalTensorIDF16_NS_5ShapeILi1ELi1ELi1ELi1ELi1EEENS_6StrideILi1ELi1ELin1ELin1ELin1EEELNS_6LayoutE0EE12defaultShapeE, align 8
  br label %init.end

init.end:                                         ; preds = %init.check, %entry
  ret void
}

; Function Attrs: argmemonly mustprogress nocallback nofree nosync nounwind willreturn
declare ptr @llvm.invariant.start.p0(i64 immarg, ptr nocapture) #3

; Function Attrs: mustprogress nofree nosync nounwind willreturn
define internal void @__cxx_global_var_init.4() #2 section ".text.startup" comdat($_ZN3pto12GlobalTensorIDF16_NS_5ShapeILi1ELi1ELi1ELi1ELi1EEENS_6StrideILi1ELi1ELin1ELin1ELin1EEELNS_6LayoutE0EE13defaultStrideE) {
entry:
  %0 = load i8, ptr @_ZGVN3pto12GlobalTensorIDF16_NS_5ShapeILi1ELi1ELi1ELi1ELi1EEENS_6StrideILi1ELi1ELin1ELin1ELin1EEELNS_6LayoutE0EE13defaultStrideE, align 8
  %guard.uninitialized = icmp eq i8 %0, 0
  br i1 %guard.uninitialized, label %init.check, label %init.end

init.check:                                       ; preds = %entry
  store i32 1, ptr @_ZN3pto12GlobalTensorIDF16_NS_5ShapeILi1ELi1ELi1ELi1ELi1EEENS_6StrideILi1ELi1ELin1ELin1ELin1EEELNS_6LayoutE0EE13defaultStrideE, align 8, !tbaa !13
  tail call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(16) getelementptr inbounds (%"struct.pto::Stride", ptr @_ZN3pto12GlobalTensorIDF16_NS_5ShapeILi1ELi1ELi1ELi1ELi1EEENS_6StrideILi1ELi1ELin1ELin1ELin1EEELNS_6LayoutE0EE13defaultStrideE, i64 0, i32 0, i64 1), i8 0, i64 16, i1 false), !tbaa !13
  %1 = tail call ptr @llvm.invariant.start.p0(i64 20, ptr nonnull @_ZN3pto12GlobalTensorIDF16_NS_5ShapeILi1ELi1ELi1ELi1ELi1EEENS_6StrideILi1ELi1ELin1ELin1ELin1EEELNS_6LayoutE0EE13defaultStrideE)
  store i8 1, ptr @_ZGVN3pto12GlobalTensorIDF16_NS_5ShapeILi1ELi1ELi1ELi1ELi1EEENS_6StrideILi1ELi1ELin1ELin1ELin1EEELNS_6LayoutE0EE13defaultStrideE, align 8
  br label %init.end

init.end:                                         ; preds = %init.check, %entry
  ret void
}

; Function Attrs: argmemonly nocallback nofree nounwind willreturn writeonly
declare void @llvm.memset.p0.i64(ptr nocapture writeonly, i8, i64, i1 immarg) #4

attributes #0 = { mustprogress nofree norecurse nosync nounwind readnone willreturn "frame-pointer"="none" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-features"="+relax" }
attributes #1 = { "frame-pointer"="none" "min-legal-vector-width"="262144" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-features"="+relax" }
attributes #2 = { mustprogress nofree nosync nounwind willreturn "frame-pointer"="none" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-features"="+relax" }
attributes #3 = { argmemonly mustprogress nocallback nofree nosync nounwind willreturn }
attributes #4 = { argmemonly nocallback nofree nounwind willreturn writeonly }
attributes #5 = { nounwind }

!llvm.linker.options = !{}
!llvm.module.flags = !{!0, !1, !2, !3, !4}
!llvm.ident = !{!5}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 1, !"target-abi", !"lp64"}
!2 = !{i32 7, !"PIC Level", i32 2}
!3 = !{i32 7, !"PIE Level", i32 2}
!4 = !{i32 1, !"SmallDataLimit", i32 8}
!5 = !{!"clang version 15.0.4 (git@github.com:LinxISA/llvm-project.git 92bb9c5f361117c1a007e560ef8e3ee0dadde1b2)"}
!6 = !{i64 1481895, i64 2163896300, i64 2163896380, i64 2163896402, i64 2163896460, i64 2163896486, i64 2163896540, i64 2163896547, i64 2163896620, i64 2163896701, i64 2163896781, i64 2163896803, i64 2163896861, i64 2163896887, i64 2163896941, i64 2163896948, i64 2163897021, i64 2163897102, i64 2163897182, i64 2163897204, i64 2163897262, i64 2163897288, i64 2163897342, i64 2163897349, i64 2163897422, i64 2163897503, i64 2163897584, i64 2163897606, i64 2163897664, i64 2163897690, i64 2163897744, i64 2163897751, i64 2163897824, i64 2163897905, i64 1481965, i64 1481999, i64 1482033, i64 1482089}
!7 = !{i64 1321794, i64 2149743235, i64 2149743255, i64 2149743316, i64 2149743340, i64 2149743397, i64 2149743421, i64 2149743478, i64 2149743502, i64 2149743559, i64 2149743583, i64 2149743640, i64 2149743664, i64 2149743721, i64 2149743745, i64 2149743802, i64 2149743826, i64 2149743843, i64 1321854, i64 1321892, i64 1321930}
!8 = !{i64 1864755, i64 2236826439, i64 2236826465, i64 2236826520, i64 2236826550, i64 2236826601, i64 1864814, i64 1864851, i64 1864888, i64 1864919}
!9 = !{i64 1844381, i64 2236798310, i64 2236798336, i64 2236798391, i64 2236798421, i64 2236798472, i64 1844440, i64 1844482, i64 1844524, i64 1844555}
!10 = !{i64 1491752, i64 1491796, i64 1491833, i64 1491867, i64 1491901, i64 1491941}
!11 = distinct !{!11, !12}
!12 = !{!"llvm.loop.mustprogress"}
!13 = !{!14, !14, i64 0}
!14 = !{!"int", !15, i64 0}
!15 = !{!"omnipotent char", !16, i64 0}
!16 = !{!"Simple C++ TBAA"}
