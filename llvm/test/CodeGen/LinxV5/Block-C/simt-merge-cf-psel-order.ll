; RUN: llc < %s -O2 -enable-all-vector-as-tilereg=true -stop-after=virtregrewriter -o - | FileCheck %s --check-prefix=VRR

target datalayout = "e-m:e-p:64:64-i8:8:64-i16:16:64-i32:32:64-i64:64-i128:128-n64-S128"
target triple = "linx64v5-unknown-linux-musl"

; Reduced from a probing loop that uses "probe_cnt = max_probe" in the found
; branch. The found branch value must be the left source of v.psel because P=1
; selects the left source.

; VRR-LABEL: name: lookup_min
; VRR-LABEL: bb.4.found.then:
; VRR: %[[FOUND_PROBE:[0-9]+]]:simtcgsl = COPY $simt_ri6
; VRR-LABEL: bb.5.for.inc:
; VRR: %[[SAVED:[0-9]+]]:simtcgs = LinxV5PseudoCopyFromP
; VRR-NEXT: LinxV5PseudoCopy2PTerm
; VRR-NEXT: {{%[0-9]+}}:simtcgvl = SIMT_PSEL {{.*}} %[[SAVED]], 4, %[[FOUND_PROBE]], 5, {{%[0-9]+}}, 5, {{.*}} implicit $simt_p

define dso_local void @lookup_min(ptr noundef %slot,
                                  ptr noundef %keys,
                                  ptr nocapture noundef writeonly %values_output,
                                  i32 noundef %capacity,
                                  i32 noundef signext %num,
                                  i32 noundef signext %entry_size,
                                  i32 noundef signext %max_probe) local_unnamed_addr #0 {
entry:
  %tid16 = tail call i16 @llvm.blkv.get.index.x()
  %tid32 = zext i16 %tid16 to i32
  %in_range = icmp slt i32 %tid32, %num
  br i1 %in_range, label %if.then, label %exit

if.then:
  %idx64 = zext i16 %tid16 to i64
  %keyp = getelementptr inbounds i64, ptr %keys, i64 %idx64
  %key = load volatile i64, ptr %keyp, align 8
  %key32 = trunc i64 %key to i32
  %h = shl i32 %key32, 3
  %curr0 = urem i32 %h, %capacity
  %has_probe = icmp sgt i32 %max_probe, 0
  br i1 %has_probe, label %for.body, label %exit

for.body:
  %probe = phi i32 [ 0, %if.then ], [ %probe.inc, %for.inc ]
  %curr_slot = phi i32 [ %curr0, %if.then ], [ %curr.next, %for.inc ]
  %slot.off32 = mul i32 %curr_slot, %entry_size
  %slot.off64 = zext i32 %slot.off32 to i64
  %key.addr = getelementptr inbounds i8, ptr %slot, i64 %slot.off64
  %curr_key = load volatile i64, ptr %key.addr, align 8
  %found = icmp eq i64 %curr_key, %key
  br i1 %found, label %found.then, label %for.inc

found.then:
  %value.addr.i8 = getelementptr inbounds i8, ptr %key.addr, i64 8
  %curr_val = load volatile i32, ptr %value.addr.i8, align 4
  %outp = getelementptr inbounds i32, ptr %values_output, i64 %idx64
  store volatile i32 %curr_val, ptr %outp, align 4
  br label %for.inc

for.inc:
  %probe.base = phi i32 [ %max_probe, %found.then ], [ %probe, %for.body ]
  %curr.add = add i32 %curr_slot, 1
  %curr.next = urem i32 %curr.add, %capacity
  %probe.inc = add i32 %probe.base, 1
  %keep.going = icmp slt i32 %probe.inc, %max_probe
  br i1 %keep.going, label %for.body, label %exit

exit:
  ret void
}

declare i16 @llvm.blkv.get.index.x() #1

attributes #0 = { mustprogress nofree noinline nounwind "__mtc__" "frame-pointer"="none" "min-legal-vector-width"="0" "no-builtins" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-features"="+relax" }
attributes #1 = { nofree nosync nounwind readnone }
