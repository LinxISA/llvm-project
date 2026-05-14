; RUN: timeout 1m llc < %s --march=linx64

define dso_local void @yield() {
entry:
  br label %__here

__here:
  %0 = tail call i64 asm sideeffect "", "=r,~{memory}"()
  %1 = inttoptr i64 %0 to ptr
  %task_state_change = getelementptr inbounds [2 x ptr], ptr %1, i64 0, i32 1
  store i64 ptrtoint (ptr blockaddress(@yield, %__here) to i64), ptr %task_state_change, align 8
  %2 = tail call i64 asm sideeffect "", "=r,~{memory}"()
  %3 = inttoptr i64 %2 to ptr
  store volatile i32 0, ptr %3, align 8
  tail call void asm sideeffect "", "~{memory}"()
  ret void
}
