	.text
	.file	"simple_probing_min.ll"
	.globl	lookup_min                      // -- Begin function lookup_min
	.p2align	1
	.type	lookup_min,@function
lookup_min:                             // @lookup_min
# LLVM-MCA-BEGIN
.LBB0_0:                                // %entry
	v.icvt.u162u32	lc0.uh, 	->vt.w, RNONE, nosat
	v.cmp.lt	vt#1.sw, ri4.sw, 	->vt.d
	l.addi	p, 0, 	->t.d
	v.cmp.nei	vt#1.ud, 0, ->p
	l.addi	p, 0, 	->t.d
	l.cmp.eqi	t#1.sd, 0, 	->t.d
	b.ne	t#1, zero, .LBB0_7
	j	.LBB0_1
.LBB0_1:                                // %if.then
	v.ld	[ri1.sd, lc0<<3, zero.sd], 	->vt.d
	v.icvt.s642s32	vt#1.reuse.sd, 	->vt.w, RNONE, nosat
	v.slli	vt#1.sw, 3, 	->vt.w
	v.rem	vt#1.uw, ri3.uw, 	->vn.w
	l.cmp.lti	ri6.sw, 1, 	->t.d
	ori	t#4, 0, 	->t
	ori	t#1, 0, 	->t
	l.ori	zero.sd, 0, 	->t.d
	ori	t#4, 0, 	->t
	b.ne	t#1, zero, .LBB0_7
	j	.LBB0_2
.LBB0_2:                                // %for.body.preheader
	l.ori	zero.ud, 0, 	->t.d
	v.mov	zero.uw, 	->vn.w
	ori	t#4, 0, 	->t
.LBB0_3:                                // %for.body
                                        // =>This Inner Loop Header: Depth=1
	v.mul	vn#2.reuse.sw, ri5.sw, 	->vt.w
	v.mov	vt#4.ud, 	->vt.d
	v.icvt.u322u64	vt#2.reuse.uw, 	->vt.d, RNONE, nosat
	v.add	ri0.sd, vt#1.sd, 	->vt.d, nosat
	v.ld	[ri0.sd, vt#4.uw], 	->vt.d
	v.mov	vt#4.ud, 	->vt.d
	v.cmp.eq	vt#2.sd, vt#1.reuse.sd, 	->vt.d
	l.addi	p, 0, 	->t.d
	v.cmp.nei	vt#1.ud, 0, ->p
	l.addi	p, 0, 	->u.d
	v.addi	zero.uw, 0, 	->u.w
	l.cmp.eqi	u#2.sd, 0, 	->u.d
	v.mov	vt#4.ud, 	->vt.d
	b.ne	u#1, zero, .LBB0_5
	j	.LBB0_4
.LBB0_4:                                // %found.then
                                        //   in Loop: Header=BB0_3 Depth=1
	l.ori	ri6.uw, 0, 	->u.w
	v.lwi	[vt#1.sd, 8], 	->vt.w
	v.sw	vt#1.sw, [ri2.sd, lc0<<2, zero.sd<<2]
	ori	t#3, 0, 	->t
	ori	t#3, 0, 	->t
	ori	t#3, 0, 	->t
	ori	u#1, 0, 	->u
	l.ori	zero.sd, 0, 	->u.d
	v.mov	vn#2.uw, 	->vn.w
	v.mov	vn#2.uw, 	->vn.w
	v.mov	vt#4.ud, 	->vt.d
	v.mov	vt#1.ud, 	->vt.d
	v.mov	zero.sd, 	->vt.d
	v.mov	zero.sd, 	->vt.d
.LBB0_5:                                // %for.inc
                                        //   in Loop: Header=BB0_3 Depth=1
	v.psel	p, u#2.sw, vn#1.sw, 	->vt.w
	l.addi	t#1.ud, 0, 	->p
	v.mov	vt#4.ud, 	->vt.d
	v.addi	vn#2.sw, 1, 	->vt.w
	v.rem	vt#1.uw, ri3.uw, 	->vn.w
	v.addi	vt#3.sw, 1, 	->vn.w
	v.cmp.ge	vn#1.reuse.sw, ri6.sw, 	->vt.d
	l.addi	p, 0, 	->t.d
	v.cmp.nei	vt#1.ud, 0, ->p
	l.addi	p, 0, 	->u.d
	l.addi	t#1.ud, 0, 	->p
	l.or	u#1.sd, t#4.sd, 	->t.d
	ori	t#4, 0, 	->t
	l.xori	t#2.sd, -1, 	->t.d
	l.addi	p, 0, 	->u.d
	l.and	t#1.sd, u#1.sd, 	->t.d
	l.addi	t#1.ud, 0, 	->p
	ori	t#4, 0, 	->t
	ori	t#4, 0, 	->t
	l.cmp.nei	t#3.sd, 0, 	->t.d
	ori	t#1, 0, 	->t
	ori	t#4, 0, 	->t
	ori	t#4, 0, 	->t
	v.mov	vn#2.uw, 	->vn.w
	v.mov	vn#2.uw, 	->vn.w
	v.mov	vt#3.ud, 	->vt.d
	v.mov	zero.sd, 	->vt.d
	v.mov	zero.sd, 	->vt.d
	b.ne	t#3, zero, .LBB0_3
	j	.LBB0_6
.LBB0_6:                                // %Flow
	l.addi	t#2.ud, 0, 	->p
	ori	t#1, 0, 	->t
	l.ori	zero.sd, 0, 	->t.d
	l.ori	zero.sd, 0, 	->t.d
.LBB0_7:                                // %exit
	l.addi	t#3.ud, 0, 	->p
	L.BSTOP
# LLVM-MCA-END
	.section	.stack_size,"a",@progbits
	.globl	lookup_min_stack_size
lookup_min_stack_size:
	.word	0
	.text
.Lfunc_end0:
	.size	lookup_min, .Lfunc_end0-lookup_min
                                        // -- End function
	.section	".note.GNU-stack","",@progbits
