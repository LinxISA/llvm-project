	.file	"autovec_nested_mask_branch.ll"
	.text
	.globl	vector_nested_if                #  -- Begin function vector_nested_if
	.p2align	1
	.type	vector_nested_if,@function
vector_nested_if:                       #  @vector_nested_if
#  %bb.0:                               #  %entry
FENTRY	[ra ~ ra], sp!, 8
#  %bb.1:                               #  %entry
C.BSTART.STD
addi	zero, 64,	->a2
c.movi	1,	->a3
lui	263168,	->a4
lui	262144,	->a5
lui	260096,	->a6
lui	266752,	->a7
#  %bb.3:                               #  %entry
C.BSTART.STD
BSTART.MSEQ	VS8
B.TEXT	.__linx_vblock_body.0
B.IOR	[a1,a7,a0],[]
B.IOR	[a6,a0,a5],[]
B.IOR	[a0,a4,a3],[]
B.IOR	[a2],[]
C.B.DIMI	32, 	->lb0
C.B.DIMI	2, 	->lb1
C.B.DIMI	1, 	->lb2
#  %bb.2:                               #  %entry
FRET.STK	[ra ~ ra], sp!, 8
#  %bb.4:
.__linx_vblock_body.0:                  #  EH_LABEL
v.add	lc0, lc1<<5,	->vt#1
v.sub	vt#1, lc0,	->vt#2
v.lw.brg	[ri0, lc0<<2, vt#2<<2],	->vt#3
v.flt	zero, vt#3,	->p
b.nz	.__linx_vbody_vblock_body.0.L1
j	.__linx_vbody_vblock_body.0.L4
.__linx_vbody_vblock_body.0.L1:         #  EH_LABEL
v.flt	ri1, vt#3,	->p
b.nz	.__linx_vbody_vblock_body.0.L2
j	.__linx_vbody_vblock_body.0.L3
.__linx_vbody_vblock_body.0.L2:         #  EH_LABEL
v.sw.brg	ri3, [ri2, lc0<<2, vt#2<<2]
j	.__linx_vbody_vblock_body.0.L5
.__linx_vbody_vblock_body.0.L3:         #  EH_LABEL
v.sw.brg	ri5, [ri4, lc0<<2, vt#2<<2]
j	.__linx_vbody_vblock_body.0.L5
.__linx_vbody_vblock_body.0.L4:         #  EH_LABEL
v.sw.brg	ri7, [ri6, lc0<<2, vt#2<<2]
j	.__linx_vbody_vblock_body.0.L5
.__linx_vbody_vblock_body.0.L5:         #  EH_LABEL
v.add	vt#1, ri8,	->vt#4
v.cmp.ltu	vt#4, ri9,	->p
b.nz	.__linx_vbody_vblock_body.0.L_end
j	.__linx_vbody_vblock_body.0.L5_exit1
.__linx_vbody_vblock_body.0.L5_exit1:   #  EH_LABEL
j	.__linx_vbody_vblock_body.0.L_end
.__linx_vbody_vblock_body.0.L_end:      #  EH_LABEL
C.BSTOP
.__linx_vblock_body.0.end:              #  EH_LABEL
.Lfunc_end0:
	.size	vector_nested_if, .Lfunc_end0-vector_nested_if
                                        #  -- End function
	.section	".note.GNU-stack","",@progbits
