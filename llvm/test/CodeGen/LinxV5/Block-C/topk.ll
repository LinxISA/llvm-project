; RUN: llc < %s -O2 -enable-all-vector-as-tilereg=true | FileCheck %s --dump-input always -vv --check-prefixes=CHECK

target datalayout = "e-m:e-p:64:64-i8:8:64-i16:16:64-i32:32:64-i64:64-i128:128-n64-S128"
target triple = "linx64v5-unknown-linux-musl"

%class.Tensor = type { [256 x float] }

$_Z25ExpandScalarImpl_RowMajorIN3pto11tile_tensorIfNS0_12MatrixLayoutILi16ELi16ELi16ELi1ELNS0_10LayoutEnumE1EEELi16ELi16ELS3_0ELi512EEEEvNT_9TileDTypeENS6_5DTypeE = comdat any

$_Z20TCopyIn_Vec_RowMajorIN3pto11tile_tensorIfNS0_12MatrixLayoutILi16ELi16ELi16ELi1ELNS0_10LayoutEnumE1EEELi16ELi16ELS3_0ELi512EEENS0_13global_tensorIfS4_EEEvNT_9TileDTypeEPKNT0_5DTypeE = comdat any

$_Z35BitonicSortStepDescend_RowMajor_ImpIN3pto11tile_tensorIfNS0_12MatrixLayoutILi16ELi16ELi16ELi1ELNS0_10LayoutEnumE1EEELi16ELi16ELS3_0ELi512EEEEvNT_9TileDTypeES7_tt = comdat any

$_Z18TCopy_Vec_RowMajorIN3pto11tile_tensorIfNS0_12MatrixLayoutILi16ELi16ELi16ELi1ELNS0_10LayoutEnumE1EEELi16ELi16ELS3_0ELi512EEEEvNT_9TileDTypeES7_ = comdat any

$_Z21TExtract_RowMajor_ImpIN3pto11tile_tensorIfNS0_12MatrixLayoutILi16ELi16ELi16ELi1ELNS0_10LayoutEnumE1EEELi16ELi16ELS3_0ELi512EEES5_EvNT_9TileDTypeENT0_9TileDTypeEtt = comdat any

$_Z21TCopyOut_Vec_RowMajorIN3pto13global_tensorIfNS0_12MatrixLayoutILi16ELi16ELi16ELi1ELNS0_10LayoutEnumE1EEEEENS0_11tile_tensorIfS4_Li16ELi16ELS3_0ELi512EEEEvPNT_5DTypeENT0_9TileDTypeE = comdat any

@.str = private unnamed_addr constant [9 x i8] c"RowMajor\00", align 8
@.str.1 = private unnamed_addr constant [9 x i8] c"ColMajor\00", align 8
@.str.2 = private unnamed_addr constant [9 x i8] c"kNoneBox\00", align 8
@.str.3 = private unnamed_addr constant [18 x i8] c"UnsupportedLayout\00", align 8
@.str.5 = private unnamed_addr constant [7 x i8] c"%3.0f \00", align 8
@str = private unnamed_addr constant [10 x i8] c"input x: \00", align 1
@str.8 = private unnamed_addr constant [13 x i8] c"weight out: \00", align 1
@switch.table._ZN3pto18layout_type_to_strENS_10LayoutEnumE = private unnamed_addr constant [3 x ptr] [ptr @.str.2, ptr @.str, ptr @.str.1], align 8

; Function Attrs: argmemonly mustprogress nocallback nofree nosync nounwind willreturn
declare void @llvm.lifetime.start.p0(i64 immarg, ptr nocapture) #2

; Function Attrs: argmemonly mustprogress nocallback nofree nosync nounwind willreturn
declare void @llvm.lifetime.end.p0(i64 immarg, ptr nocapture) #2

; Function Attrs: nofree nosync nounwind readnone
declare i16 @llvm.blkv.get.index.x() #7

; Function Attrs: nofree nosync nounwind readnone
declare i16 @llvm.blkv.get.index.y() #7

; Function Attrs: nofree nosync nounwind readnone
declare ptr @llvm.blkv.get.tile.ptr.v256f32(<256 x float>) #7

; CHECK-NOT: v.ori	ri1.uh, 0, 	->vt.h
; Function Attrs: mustprogress noinline nounwind
define void @_Z35BitonicSortStepDescend_RowMajor_ImpIN3pto11tile_tensorIfNS0_12MatrixLayoutILi16ELi16ELi16ELi1ELNS0_10LayoutEnumE1EEELi16ELi16ELS3_0ELi512EEEEvNT_9TileDTypeES7_tt(<256 x float> __out__ noundef %dst, <256 x float> noundef %src, i16 noundef zeroext %stage, i16 noundef zeroext %step) #11 {
entry:
  %0 = tail call i16 @llvm.blkv.get.index.x()
  %1 = tail call ptr @llvm.blkv.get.tile.ptr.v256f32(<256 x float> %dst)
  %xor120 = xor i16 %0, %step
  %cmp = icmp ult i16 %0, %xor120
  br i1 %cmp, label %if.then, label %if.end83

if.then:                                          ; preds = %entry
  %conv4 = zext i16 %xor120 to i32
  %conv = zext i16 %0 to i32
  %2 = tail call ptr @llvm.blkv.get.tile.ptr.v256f32(<256 x float> %src)
  %3 = tail call i16 @llvm.blkv.get.index.y()
  %and121 = and i16 %xor120, %stage
  %cmp7 = icmp eq i16 %and121, 0
  %conv9 = zext i16 %3 to i32
  %mul = shl nuw nsw i32 %conv9, 4
  %add = add nuw nsw i32 %mul, %conv
  %idxprom = zext i32 %add to i64
  %arrayidx = getelementptr inbounds float, ptr %2, i64 %idxprom
  %4 = load float, ptr %arrayidx, align 4, !tbaa !12
  %add14 = add nuw nsw i32 %mul, %conv4
  %idxprom15 = zext i32 %add14 to i64
  %arrayidx16 = getelementptr inbounds float, ptr %2, i64 %idxprom15
  %5 = load float, ptr %arrayidx16, align 4, !tbaa !12
  br i1 %cmp7, label %if.then8, label %if.else

if.then8:                                         ; preds = %if.then
  %cmp17 = fcmp olt float %4, %5
  br i1 %cmp17, label %if.end83.sink.split, label %if.end83

if.else:                                          ; preds = %if.then
  %cmp55 = fcmp ogt float %4, %5
  br i1 %cmp55, label %if.end83.sink.split, label %if.end83

if.end83.sink.split:                              ; preds = %if.else, %if.then8
  %arrayidx42 = getelementptr inbounds float, ptr %1, i64 %idxprom15
  %arrayidx30 = getelementptr inbounds float, ptr %1, i64 %idxprom
  store float %5, ptr %arrayidx30, align 4, !tbaa !12
  %6 = load float, ptr %arrayidx, align 4, !tbaa !12
  store float %6, ptr %arrayidx42, align 4, !tbaa !12
  br label %if.end83

if.end83:                                         ; preds = %if.end83.sink.split, %if.then8, %if.else, %entry
  ret void
}

attributes #0 = { mustprogress nofree norecurse nosync nounwind readnone willreturn "frame-pointer"="none" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-features"="+relax" }
attributes #1 = { argmemonly mustprogress nofree norecurse nosync nounwind "frame-pointer"="none" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-features"="+relax" }
attributes #2 = { argmemonly mustprogress nocallback nofree nosync nounwind willreturn }
attributes #3 = { norecurse nounwind "frame-pointer"="none" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-features"="+relax" }
attributes #4 = { nofree nounwind "frame-pointer"="none" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-features"="+relax" }
attributes #5 = { mustprogress noinline nounwind "__vec__" "frame-pointer"="none" "min-legal-vector-width"="8192" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-features"="+relax" }
attributes #6 = { nounwind }
attributes #7 = { nofree nosync nounwind readnone }
attributes #8 = { mustprogress noinline nounwind "__mtc__" "frame-pointer"="none" "min-legal-vector-width"="8192" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-features"="+relax" }
attributes #9 = { nofree nounwind }
attributes #10 = { argmemonly nocallback nofree nounwind willreturn writeonly }
attributes #11 = { "__vec__" }

!llvm.linker.options = !{}
!llvm.module.flags = !{!0, !1, !2, !3, !4}
!llvm.ident = !{!5}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 1, !"target-abi", !"lp64"}
!2 = !{i32 7, !"PIC Level", i32 2}
!3 = !{i32 7, !"PIE Level", i32 2}
!4 = !{i32 1, !"SmallDataLimit", i32 8}
!5 = !{!"clang version 15.0.4 (Linx Base Software 103.0.0.B0xx 24xxxx V0.36 b5488b959a7c099d84c0ac11442455468e1a8210)"}
!6 = !{!7, !7, i64 0}
!7 = !{!"short", !8, i64 0}
!8 = !{!"omnipotent char", !9, i64 0}
!9 = !{!"Simple C++ TBAA"}
!10 = distinct !{!10, !11}
!11 = !{!"llvm.loop.mustprogress"}
!12 = !{!13, !13, i64 0}
!13 = !{!"float", !8, i64 0}
!14 = distinct !{!14, !11}
!15 = distinct !{!15, !11}
!16 = distinct !{!16, !11}
