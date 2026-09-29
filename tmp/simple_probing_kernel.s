	.text
	.file	"simple_probing_kernel.cpp"
	.section	.text._Z6lookupILi256ELi1EEvPhPlPiPjjiii,"axG",@progbits,_Z6lookupILi256ELi1EEvPhPlPiPjjiii,comdat
	.weak	_Z6lookupILi256ELi1EEvPhPlPiPjjiii // -- Begin function _Z6lookupILi256ELi1EEvPhPlPiPjjiii
	.p2align	1
	.type	_Z6lookupILi256ELi1EEvPhPlPiPjjiii,@function
_Z6lookupILi256ELi1EEvPhPlPiPjjiii:     // @_Z6lookupILi256ELi1EEvPhPlPiPjjiii
# LLVM-MCA-BEGIN
.LBB0_0:                                // %entry
	v.add	lc0.uh, lc1.uh<<8, 	->vt.w, nosat
	v.cmp.lt	vt#1.reuse.sw, ri5.sw, 	->vt.d
	l.addi	p, 0, 	->t.d
	v.cmp.nei	vt#1.ud, 0, ->p
	l.addi	p, 0, 	->t.d
	l.cmp.eqi	t#1.sd, 0, 	->t.d
	b.ne	t#1, zero, .LBB0_7
	j	.LBB0_1
.LBB0_1:                                // %if.then
	v.icvt.u322u64	vt#2.reuse.uw, 	->vt.d, RNONE, nosat
	v.sdi.u.local	vt#1.reuse.ud, [to1, lc0<<3, 1032]
	v.ld	[ri1.sd, vt#3.reuse.uw<<3], 	->vt.d
	v.sdi.u.local	vt#1.reuse.ud, [to1, lc0<<3, 520]
	v.mov	vt#4.uw, 	->vt.w
	v.icvt.s642s32	vt#2.sd, 	->vt.w, RNONE, nosat
	v.slli	vt#1.sw, 3, 	->vn.w
	v.sw	vn#1.reuse.sw, [ri3.sd, vt#2.uw<<2]
	l.cmp.lti	ri7.sw, 1, 	->t.d
	ori	t#4, 0, 	->t
	ori	t#1, 0, 	->t
	l.ori	zero.sd, 0, 	->t.d
	ori	t#4, 0, 	->t
	b.ne	t#1, zero, .LBB0_7
	j	.LBB0_2
.LBB0_2:                                // %for.body.lr.ph
	l.ori	zero.ud, 0, 	->t.d
	v.mov	zero.uw, 	->vn.w
	ori	t#4, 0, 	->t
.LBB0_3:                                // %for.body
                                        // =>This Inner Loop Header: Depth=1
	v.rem	vn#2.uw, ri4.uw, 	->vt.w
	v.mul	vt#1.reuse.sw, ri6.sw, 	->vt.w
	v.icvt.u322u64	vt#1.reuse.uw, 	->vt.d, RNONE, nosat
	v.add	ri0.sd, vt#1.sd, 	->vt.d, nosat
	v.sdi.u.local	vt#1.reuse.ud, [to1, lc0<<3, 8]
	v.mov	vt#4.uw, 	->vt.w
	v.ld	[ri0.sd, vt#4.uw], 	->vt.d
	v.ldi.u.local	[to1, lc0<<3, 520], 	->vt.d
	v.cmp.eq	vt#2.sd, vt#1.sd, 	->vt.d
	l.addi	p, 0, 	->t.d
	v.cmp.nei	vt#1.ud, 0, ->p
	l.addi	p, 0, 	->u.d
	v.addi	zero.uw, 0, 	->u.w
	l.cmp.eqi	u#2.sd, 0, 	->u.d
	v.mov	vt#4.uw, 	->vt.w
	b.ne	u#1, zero, .LBB0_5
	j	.LBB0_4
.LBB0_4:                                // %if.then14
                                        //   in Loop: Header=BB0_3 Depth=1
	l.ori	ri7.uw, 0, 	->u.w
	v.ldi.u.local	[to1, lc0<<3, 8], 	->vt.d
	v.lwi	[vt#1.sd, 8], 	->vt.w
	v.ldi.u.local	[to1, lc0<<3, 1032], 	->vt.d
	v.sw	vt#2.sw, [ri2.sd, vt#1.sd<<2]
	ori	t#3, 0, 	->t
	ori	t#3, 0, 	->t
	ori	t#3, 0, 	->t
	ori	u#1, 0, 	->u
	l.ori	zero.sd, 0, 	->u.d
	v.mov	vn#1.uw, 	->vn.w
	v.mov	vt#4.uw, 	->vt.w
	v.mov	vt#1.uw, 	->vt.w
.LBB0_5:                                // %if.end
                                        //   in Loop: Header=BB0_3 Depth=1
	v.psel	p, u#2.sw, vn#1.sw, 	->vt.w
	l.addi	t#1.ud, 0, 	->p
	v.addi	vt#2.sw, 1, 	->vn.w
	v.addi	vt#1.sw, 1, 	->vn.w
	v.cmp.ge	vn#1.reuse.sw, ri7.sw, 	->vt.d
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
	b.ne	t#3, zero, .LBB0_3
	j	.LBB0_6
.LBB0_6:                                // %Flow
	l.addi	t#2.ud, 0, 	->p
	ori	t#1, 0, 	->t
	l.ori	zero.sd, 0, 	->t.d
	l.ori	zero.sd, 0, 	->t.d
.LBB0_7:                                // %if.end19
	l.addi	t#3.ud, 0, 	->p
	L.BSTOP
# LLVM-MCA-END
	.section	.stack_size,"a",@progbits
	.globl	_Z6lookupILi256ELi1EEvPhPlPiPjjiii_stack_size
_Z6lookupILi256ELi1EEvPhPlPiPjjiii_stack_size:
	.word	1544
	.section	.text._Z6lookupILi256ELi1EEvPhPlPiPjjiii,"axG",@progbits,_Z6lookupILi256ELi1EEvPhPlPiPjjiii,comdat
.Lfunc_end0:
	.size	_Z6lookupILi256ELi1EEvPhPlPiPjjiii, .Lfunc_end0-_Z6lookupILi256ELi1EEvPhPlPiPjjiii
                                        // -- End function
	.section	".linker-options","e",@llvm_linker_options
	.ident	"clang version 15.0.4 (linx64v5-musl-local f25aa63f7aa291eff97acefb9fa16fe7a9d6580c)"
	.section	".note.GNU-stack","",@progbits
