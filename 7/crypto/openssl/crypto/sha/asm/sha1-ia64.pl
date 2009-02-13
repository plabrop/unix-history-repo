#!/usr/bin/env perl
#
# ====================================================================
# Written by Andy Polyakov <appro@fy.chalmers.se> for the OpenSSL
# project. Rights for redistribution and usage in source and binary
# forms are granted according to the OpenSSL license.
# ====================================================================
#
# Eternal question is what's wrong with compiler generated code? The
# trick is that it's possible to reduce the number of shifts required
# to perform rotations by maintaining copy of 32-bit value in upper
# bits of 64-bit register. Just follow mux2 and shrp instructions...
# Performance under big-endian OS such as HP-UX is 179MBps*1GHz, which
# is >50% better than HP C and >2x better than gcc. As of this moment
# performance under little-endian OS such as Linux and Windows will be
# a bit lower, because data has to be picked in reverse byte-order.
# It's possible to resolve this issue by implementing third function,
# sha1_block_asm_data_order_aligned, which would temporarily flip
# BE field in User Mask register...

$code=<<___;
.ident  \"sha1-ia64.s, version 1.0\"
.ident  \"IA-64 ISA artwork by Andy Polyakov <appro\@fy.chalmers.se>\"
.explicit

___


if ($^O eq "hpux") {
    $ADDP="addp4";
    for (@ARGV) { $ADDP="add" if (/[\+DD|\-mlp]64/); }
} else { $ADDP="add"; }
for (@ARGV) {	$big_endian=1 if (/\-DB_ENDIAN/);
		$big_endian=0 if (/\-DL_ENDIAN/);   }
if (!defined($big_endian))
	    {	$big_endian=(unpack('L',pack('N',1))==1);   }

#$human=1;
if ($human) {	# useful for visual code auditing...
	($A,$B,$C,$D,$E,$T)   = ("A","B","C","D","E","T");
	($h0,$h1,$h2,$h3,$h4) = ("h0","h1","h2","h3","h4");
	($K_00_19, $K_20_39, $K_40_59, $K_60_79) =
	    (	"K_00_19","K_20_39","K_40_59","K_60_79"	);
	@X= (	"X0", "X1", "X2", "X3", "X4", "X5", "X6", "X7",
		"X8", "X9","X10","X11","X12","X13","X14","X15"	);
}
else {
	($A,$B,$C,$D,$E,$T)   = ("loc0","loc1","loc2","loc3","loc4","loc5");
	($h0,$h1,$h2,$h3,$h4) = ("loc6","loc7","loc8","loc9","loc10");
	($K_00_19, $K_20_39, $K_40_59, $K_60_79) =
	    (	"r14", "r15", "loc11", "loc12"	);
	@X= (	"r16", "r17", "r18", "r19", "r20", "r21", "r22", "r23",
		"r24", "r25", "r26", "r27", "r28", "r29", "r30", "r31"	);
}

sub BODY_00_15 {
local	*code=shift;
local	($i,$a,$b,$c,$d,$e,$f,$unaligned)=@_;

if ($unaligned) {
	$code.=<<___;
{ .mmi;	ld1	tmp0=[inp],2		    // MSB
	ld1	tmp1=[tmp3],2		};;
{ .mmi;	ld1	tmp2=[inp],2
	ld1	$X[$i&0xf]=[tmp3],2	    // LSB
	dep	tmp1=tmp0,tmp1,8,8	};;
{ .mii;	cmp.ne	p16,p0=r0,r0		    // no misaligned prefetch
	dep	$X[$i&0xf]=tmp2,$X[$i&0xf],8,8;;
	dep	$X[$i&0xf]=tmp1,$X[$i&0xf],16,16	};;
{ .mmi;	nop.m	0
___
	}
elsif ($i<15) {
	$code.=<<___;
{ .mmi;	ld4	$X[($i+1)&0xf]=[inp],4	// prefetch
___
	}
else	{
	$code.=<<___;
{ .mmi;	nop.m	0
___
	}
if ($i<15) {
	$code.=<<___;
	and	tmp0=$c,$b
	dep.z	tmp5=$a,5,27		}   // a<<5
{ .mmi;	andcm	tmp1=$d,$b
	add	tmp4=$e,$K_00_19	};;
{ .mmi;	or	tmp0=tmp0,tmp1		    // F_00_19(b,c,d)=(b&c)|(~b&d)
	add	$f=tmp4,$X[$i&0xf]	    // f=xi+e+K_00_19
	extr.u	tmp1=$a,27,5		};; // a>>27
{ .mib;	add	$f=$f,tmp0		    // f+=F_00_19(b,c,d)
	shrp	$b=tmp6,tmp6,2		}   // b=ROTATE(b,30)
{ .mib;	or	tmp1=tmp1,tmp5		    // ROTATE(a,5)
	mux2	tmp6=$a,0x44		};; // see b in next iteration
{ .mii;	add	$f=$f,tmp1		    // f+=ROTATE(a,5)
	mux2	$X[$i&0xf]=$X[$i&0xf],0x44
	nop.i	0			};;

___
	}
else	{
	$code.=<<___;
	and	tmp0=$c,$b
	dep.z	tmp5=$a,5,27		}   // a<<5 ;;?
{ .mmi;	andcm	tmp1=$d,$b
	add	tmp4=$e,$K_00_19	};;
{ .mmi;	or	tmp0=tmp0,tmp1		    // F_00_19(b,c,d)=(b&c)|(~b&d)
	add	$f=tmp4,$X[$i&0xf]	    // f=xi+e+K_00_19
	extr.u	tmp1=$a,27,5		}   // a>>27
{ .mmi;	xor	tmp2=$X[($i+0+1)&0xf],$X[($i+2+1)&0xf]	// +1
	xor	tmp3=$X[($i+8+1)&0xf],$X[($i+13+1)&0xf] // +1
	nop.i	0			};;
{ .mmi;	add	$f=$f,tmp0		    // f+=F_00_19(b,c,d)
	xor	tmp2=tmp2,tmp3		    // +1
	shrp	$b=tmp6,tmp6,2		}   // b=ROTATE(b,30)
{ .mmi; or	tmp1=tmp1,tmp5		    // ROTATE(a,5)
	mux2	tmp6=$a,0x44		};; // see b in next iteration
{ .mii;	add	$f=$f,tmp1		    // f+=ROTATE(a,5)
	shrp	$e=tmp2,tmp2,31		    // f+1=ROTATE(x[0]^x[2]^x[8]^x[13],1)
	mux2	$X[$i&0xf]=$X[$i&0xf],0x44  };;

___
	}
}

sub BODY_16_19 {
local	*code=shift;
local	($i,$a,$b,$c,$d,$e,$f)=@_;

$code.=<<___;
{ .mmi;	mov	$X[$i&0xf]=$f		    // Xupdate
	and	tmp0=$c,$b
	dep.z	tmp5=$a,5,27		}   // a<<5
{ .mmi;	andcm	tmp1=$d,$b
	add	tmp4=$e,$K_00_19	};;
{ .mmi;	or	tmp0=tmp0,tmp1		    // F_00_19(b,c,d)=(b&c)|(~b&d)
	add	$f=$f,tmp4		    // f+=e+K_00_19
	extr.u	tmp1=$a,27,5		}   // a>>27
{ .mmi;	xor	tmp2=$X[($i+0+1)&0xf],$X[($i+2+1)&0xf]	// +1
	xor	tmp3=$X[($i+8+1)&0xf],$X[($i+13+1)&0xf]	// +1
	nop.i	0			};;
{ .mmi;	add	$f=$f,tmp0		    // f+=F_00_19(b,c,d)
	xor	tmp2=tmp2,tmp3		    // +1
	shrp	$b=tmp6,tmp6,2		}   // b=ROTATE(b,30)
{ .mmi;	or	tmp1=tmp1,tmp5		    // ROTATE(a,5)
	mux2	tmp6=$a,0x44		};; // see b in next iteration
{ .mii;	add	$f=$f,tmp1		    // f+=ROTATE(a,5)
	shrp	$e=tmp2,tmp2,31		    // f+1=ROTATE(x[0]^x[2]^x[8]^x[13],1)
	nop.i	0			};;

___
}

sub BODY_20_39 {
local	*code=shift;
local	($i,$a,$b,$c,$d,$e,$f,$Konst)=@_;
	$Konst = $K_20_39 if (!defined($Konst));

if ($i<79) {
$code.=<<___;
{ .mib;	mov	$X[$i&0xf]=$f		    // Xupdate
	dep.z	tmp5=$a,5,27		}   // a<<5
{ .mib;	xor	tmp0=$c,$b
	add	tmp4=$e,$Konst		};;
{ .mmi;	xor	tmp0=tmp0,$d		    // F_20_39(b,c,d)=b^c^d
	add	$f=$f,tmp4		    // f+=e+K_20_39
	extr.u	tmp1=$a,27,5		}   // a>>27
{ .mmi;	xor	tmp2=$X[($i+0+1)&0xf],$X[($i+2+1)&0xf]	// +1
	xor	tmp3=$X[($i+8+1)&0xf],$X[($i+13+1)&0xf]	// +1
	nop.i	0			};;
{ .mmi;	add	$f=$f,tmp0		    // f+=F_20_39(b,c,d)
	xor	tmp2=tmp2,tmp3		    // +1
	shrp	$b=tmp6,tmp6,2		}   // b=ROTATE(b,30)
{ .mmi;	or	tmp1=tmp1,tmp5		    // ROTATE(a,5)
	mux2	tmp6=$a,0x44		};; // see b in next iteration
{ .mii;	add	$f=$f,tmp1		    // f+=ROTATE(a,5)
	shrp	$e=tmp2,tmp2,31		    // f+1=ROTATE(x[0]^x[2]^x[8]^x[13],1)
	nop.i	0			};;

___
}
else {
$code.=<<___;
{ .mib;	mov	$X[$i&0xf]=$f		    // Xupdate
	dep.z	tmp5=$a,5,27		}   // a<<5
{ .mib;	xor	tmp0=$c,$b
	add	tmp4=$e,$Konst		};;
{ .mib;	xor	tmp0=tmp0,$d		    // F_20_39(b,c,d)=b^c^d
	extr.u	tmp1=$a,27,5		}   // a>>27
{ .mib;	add	$f=$f,tmp4		    // f+=e+K_20_39
	add	$h1=$h1,$a		};; // wrap up
{ .mmi;
(p16)	ld4.s	$X[0]=[inp],4		    // non-faulting prefetch
	add	$f=$f,tmp0		    // f+=F_20_39(b,c,d)
	shrp	$b=tmp6,tmp6,2		}   // b=ROTATE(b,30) ;;?
{ .mmi;	or	tmp1=tmp1,tmp5		    // ROTATE(a,5)
	add	$h3=$h3,$c		};; // wrap up
{ .mib;	add	tmp3=1,inp		    // used in unaligned codepath
	add	$f=$f,tmp1		}   // f+=ROTATE(a,5)
{ .mib;	add	$h2=$h2,$b		    // wrap up
	add	$h4=$h4,$d		};; // wrap up

___
}
}

sub BODY_40_59 {
local	*code=shift;
local	($i,$a,$b,$c,$d,$e,$f)=@_;

$code.=<<___;
{ .mmi;	mov	$X[$i&0xf]=$f		    // Xupdate
	and	tmp0=$c,$b
	dep.z	tmp5=$a,5,27		}   // a<<5
{ .mmi;	and	tmp1=$d,$b
	add	tmp4=$e,$K_40_59	};;
{ .mmi;	or	tmp0=tmp0,tmp1		    // (b&c)|(b&d)
	add	$f=$f,tmp4		    // f+=e+K_40_59
	extr.u	tmp1=$a,27,5		}   // a>>27
{ .mmi;	and	tmp4=$c,$d
	xor	tmp2=$X[($i+0+1)&0xf],$X[($i+2+1)&0xf]	// +1
	xor	tmp3=$X[($i+8+1)&0xf],$X[($i+13+1)&0xf]	// +1
	};;
{ .mmi;	or	tmp1=tmp1,tmp5		    // ROTATE(a,5)
	xor	tmp2=tmp2,tmp3		    // +1
	shrp	$b=tmp6,tmp6,2		}   // b=ROTATE(b,30)
{ .mmi;	or	tmp0=tmp0,tmp4		    // F_40_59(b,c,d)=(b&c)|(b&d)|(c&d)
	mux2	tmp6=$a,0x44		};; // see b in next iteration
{ .mii;	add	$f=$f,tmp0		    // f+=F_40_59(b,c,d)
	shrp	$e=tmp2,tmp2,31;;	    // f+1=ROTATE(x[0]^x[2]^x[8]^x[13],1)
	add	$f=$f,tmp1		};; // f+=ROTATE(a,5)

___
}
sub BODY_60_79	{ &BODY_20_39(@_,$K_60_79); }

$code.=<<___;
.text

tmp0=r8;
tmp1=r9;
tmp2=r10;
tmp3=r11;
ctx=r32;	// in0
inp=r33;	// in1

// void sha1_block_asm_host_order(SHA_CTX *c,const void *p,size_t num);
.global	sha1_block_asm_host_order#
.proc	sha1_block_asm_host_order#
.align	32
sha1_block_asm_host_order:
	.prologue
	.fframe	0
	.save	ar.pfs,r0
	.save	ar.lc,r3
{ .mmi;	alloc	tmp1=ar.pfs,3,15,0,0
	$ADDP	tmp0=4,ctx
	mov	r3=ar.lc		}
{ .mmi;	$ADDP	ctx=0,ctx
	$ADDP	inp=0,inp
	mov	r2=pr			};;
tmp4=in2;
tmp5=loc13;
tmp6=loc14;
	.body
{ .mlx;	ld4	$h0=[ctx],8
	movl	$K_00_19=0x5a827999	}
{ .mlx;	ld4	$h1=[tmp0],8
	movl	$K_20_39=0x6ed9eba1	};;
{ .mlx;	ld4	$h2=[ctx],8
	movl	$K_40_59=0x8f1bbcdc	}
{ .mlx;	ld4	$h3=[tmp0]
	movl	$K_60_79=0xca62c1d6	};;
{ .mmi;	ld4	$h4=[ctx],-16
	add	in2=-1,in2		    // adjust num for ar.lc
	mov	ar.ec=1			};;
{ .mmi;	ld4	$X[0]=[inp],4		    // prefetch
	cmp.ne	p16,p0=r0,in2		    // prefecth at loop end
	mov	ar.lc=in2		};; // brp.loop.imp: too far

.Lhtop:
{ .mmi;	mov	$A=$h0
	mov	$B=$h1
	mux2	tmp6=$h1,0x44		}
{ .mmi;	mov	$C=$h2
	mov	$D=$h3
	mov	$E=$h4			};;

___

	&BODY_00_15(\$code, 0,$A,$B,$C,$D,$E,$T);
	&BODY_00_15(\$code, 1,$T,$A,$B,$C,$D,$E);
	&BODY_00_15(\$code, 2,$E,$T,$A,$B,$C,$D);
	&BODY_00_15(\$code, 3,$D,$E,$T,$A,$B,$C);
	&BODY_00_15(\$code, 4,$C,$D,$E,$T,$A,$B);
	&BODY_00_15(\$code, 5,$B,$C,$D,$E,$T,$A);
	&BODY_00_15(\$code, 6,$A,$B,$C,$D,$E,$T);
	&BODY_00_15(\$code, 7,$T,$A,$B,$C,$D,$E);
	&BODY_00_15(\$code, 8,$E,$T,$A,$B,$C,$D);
	&BODY_00_15(\$code, 9,$D,$E,$T,$A,$B,$C);
	&BODY_00_15(\$code,10,$C,$D,$E,$T,$A,$B);
	&BODY_00_15(\$code,11,$B,$C,$D,$E,$T,$A);
	&BODY_00_15(\$code,12,$A,$B,$C,$D,$E,$T);
	&BODY_00_15(\$code,13,$T,$A,$B,$C,$D,$E);
	&BODY_00_15(\$code,14,$E,$T,$A,$B,$C,$D);
	&BODY_00_15(\$code,15,$D,$E,$T,$A,$B,$C);

	&BODY_16_19(\$code,16,$C,$D,$E,$T,$A,$B);
	&BODY_16_19(\$code,17,$B,$C,$D,$E,$T,$A);
	&BODY_16_19(\$code,18,$A,$B,$C,$D,$E,$T);
	&BODY_16_19(\$code,19,$T,$A,$B,$C,$D,$E);

	&BODY_20_39(\$code,20,$E,$T,$A,$B,$C,$D);
	&BODY_20_39(\$code,21,$D,$E,$T,$A,$B,$C);
	&BODY_20_39(\$code,22,$C,$D,$E,$T,$A,$B);
	&BODY_20_39(\$code,23,$B,$C,$D,$E,$T,$A);
	&BODY_20_39(\$code,24,$A,$B,$C,$D,$E,$T);
	&BODY_20_39(\$code,25,$T,$A,$B,$C,$D,$E);
	&BODY_20_39(\$code,26,$E,$T,$A,$B,$C,$D);
	&BODY_20_39(\$code,27,$D,$E,$T,$A,$B,$C);
	&BODY_20_39(\$code,28,$C,$D,$E,$T,$A,$B);
	&BODY_20_39(\$code,29,$B,$C,$D,$E,$T,$A);
	&BODY_20_39(\$code,30,$A,$B,$C,$D,$E,$T);
	&BODY_20_39(\$code,31,$T,$A,$B,$C,$D,$E);
	&BODY_20_39(\$code,32,$E,$T,$A,$B,$C,$D);
	&BODY_20_39(\$code,33,$D,$E,$T,$A,$B,$C);
	&BODY_20_39(\$code,34,$C,$D,$E,$T,$A,$B);
	&BODY_20_39(\$code,35,$B,$C,$D,$E,$T,$A);
	&BODY_20_39(\$code,36,$A,$B,$C,$D,$E,$T);
	&BODY_20_39(\$code,37,$T,$A,$B,$C,$D,$E);
	&BODY_20_39(\$code,38,$E,$T,$A,$B,$C,$D);
	&BODY_20_39(\$code,39,$D,$E,$T,$A,$B,$C);

	&BODY_40_59(\$code,40,$C,$D,$E,$T,$A,$B);
	&BODY_40_59(\$code,41,$B,$C,$D,$E,$T,$A);
	&BODY_40_59(\$code,42,$A,$B,$C,$D,$E,$T);
	&BODY_40_59(\$code,43,$T,$A,$B,$C,$D,$E);
	&BODY_40_59(\$code,44,$E,$T,$A,$B,$C,$D);
	&BODY_40_59(\$code,45,$D,$E,$T,$A,$B,$C);
	&BODY_40_59(\$code,46,$C,$D,$E,$T,$A,$B);
	&BODY_40_59(\$code,47,$B,$C,$D,$E,$T,$A);
	&BODY_40_59(\$code,48,$A,$B,$C,$D,$E,$T);
	&BODY_40_59(\$code,49,$T,$A,$B,$C,$D,$E);
	&BODY_40_59(\$code,50,$E,$T,$A,$B,$C,$D);
	&BODY_40_59(\$code,51,$D,$E,$T,$A,$B,$C);
	&BODY_40_59(\$code,52,$C,$D,$E,$T,$A,$B);
	&BODY_40_59(\$code,53,$B,$C,$D,$E,$T,$A);
	&BODY_40_59(\$code,54,$A,$B,$C,$D,$E,$T);
	&BODY_40_59(\$code,55,$T,$A,$B,$C,$D,$E);
	&BODY_40_59(\$code,56,$E,$T,$A,$B,$C,$D);
	&BODY_40_59(\$code,57,$D,$E,$T,$A,$B,$C);
	&BODY_40_59(\$code,58,$C,$D,$E,$T,$A,$B);
	&BODY_40_59(\$code,59,$B,$C,$D,$E,$T,$A);

	&BODY_60_79(\$code,60,$A,$B,$C,$D,$E,$T);
	&BODY_60_79(\$code,61,$T,$A,$B,$C,$D,$E);
	&BODY_60_79(\$code,62,$E,$T,$A,$B,$C,$D);
	&BODY_60_79(\$code,63,$D,$E,$T,$A,$B,$C);
	&BODY_60_79(\$code,64,$C,$D,$E,$T,$A,$B);
	&BODY_60_79(\$code,65,$B,$C,$D,$E,$T,$A);
	&BODY_60_79(\$code,66,$A,$B,$C,$D,$E,$T);
	&BODY_60_79(\$code,67,$T,$A,$B,$C,$D,$E);
	&BODY_60_79(\$code,68,$E,$T,$A,$B,$C,$D);
	&BODY_60_79(\$code,69,$D,$E,$T,$A,$B,$C);
	&BODY_60_79(\$code,70,$C,$D,$E,$T,$A,$B);
	&BODY_60_79(\$code,71,$B,$C,$D,$E,$T,$A);
	&BODY_60_79(\$code,72,$A,$B,$C,$D,$E,$T);
	&BODY_60_79(\$code,73,$T,$A,$B,$C,$D,$E);
	&BODY_60_79(\$code,74,$E,$T,$A,$B,$C,$D);
	&BODY_60_79(\$code,75,$D,$E,$T,$A,$B,$C);
	&BODY_60_79(\$code,76,$C,$D,$E,$T,$A,$B);
	&BODY_60_79(\$code,77,$B,$C,$D,$E,$T,$A);
	&BODY_60_79(\$code,78,$A,$B,$C,$D,$E,$T);
	&BODY_60_79(\$code,79,$T,$A,$B,$C,$D,$E);

$code.=<<___;
{ .mmb;	add	$h0=$h0,$E
	nop.m	0
	br.ctop.dptk.many	.Lhtop	};;
.Lhend:
{ .mmi;	add	tmp0=4,ctx
	mov	ar.lc=r3		};;
{ .mmi;	st4	[ctx]=$h0,8
	st4	[tmp0]=$h1,8		};;
{ .mmi;	st4	[ctx]=$h2,8
	st4	[tmp0]=$h3		};;
{ .mib;	st4	[ctx]=$h4,-16
	mov	pr=r2,0x1ffff
	br.ret.sptk.many	b0	};;
.endp	sha1_block_asm_host_order#
___


$code.=<<___;
// void sha1_block_asm_data_order(SHA_CTX *c,const void *p,size_t num);
.global	sha1_block_asm_data_order#
.proc	sha1_block_asm_data_order#
.align	32
sha1_block_asm_data_order:
___
$code.=<<___ if ($big_endian);
{ .mmi;	and	r2=3,inp				};;
{ .mib;	cmp.eq	p6,p0=r0,r2
(p6)	br.dptk.many	sha1_block_asm_host_order	};;
___
$code.=<<___;
	.prologue
	.fframe	0
	.save	ar.pfs,r0
	.save	ar.lc,r3
{ .mmi;	alloc	tmp1=ar.pfs,3,15,0,0
	$ADDP	tmp0=4,ctx
	mov	r3=ar.lc		}
{ .mmi;	$ADDP	ctx=0,ctx
	$ADDP	inp=0,inp
	mov	r2=pr			};;
tmp4=in2;
tmp5=loc13;
tmp6=loc14;
	.body
{ .mlx;	ld4	$h0=[ctx],8
	movl	$K_00_19=0x5a827999	}
{ .mlx;	ld4	$h1=[tmp0],8
	movl	$K_20_39=0x6ed9eba1	};;
{ .mlx;	ld4	$h2=[ctx],8
	movl	$K_40_59=0x8f1bbcdc	}
{ .mlx;	ld4	$h3=[tmp0]
	movl	$K_60_79=0xca62c1d6	};;
{ .mmi;	ld4	$h4=[ctx],-16
	add	in2=-1,in2		    // adjust num for ar.lc
	mov	ar.ec=1			};;
{ .mmi;	nop.m	0
	add	tmp3=1,inp
	mov	ar.lc=in2		};; // brp.loop.imp: too far

.Ldtop:
{ .mmi;	mov	$A=$h0
	mov	$B=$h1
	mux2	tmp6=$h1,0x44		}
{ .mmi;	mov	$C=$h2
	mov	$D=$h3
	mov	$E=$h4			};;

___

	&BODY_00_15(\$code, 0,$A,$B,$C,$D,$E,$T,1);
	&BODY_00_15(\$code, 1,$T,$A,$B,$C,$D,$E,1);
	&BODY_00_15(\$code, 2,$E,$T,$A,$B,$C,$D,1);
	&BODY_00_15(\$code, 3,$D,$E,$T,$A,$B,$C,1);
	&BODY_00_15(\$code, 4,$C,$D,$E,$T,$A,$B,1);
	&BODY_00_15(\$code, 5,$B,$C,$D,$E,$T,$A,1);
	&BODY_00_15(\$code, 6,$A,$B,$C,$D,$E,$T,1);
	&BODY_00_15(\$code, 7,$T,$A,$B,$C,$D,$E,1);
	&BODY_00_15(\$code, 8,$E,$T,$A,$B,$C,$D,1);
	&BODY_00_15(\$code, 9,$D,$E,$T,$A,$B,$C,1);
	&BODY_00_15(\$code,10,$C,$D,$E,$T,$A,$B,1);
	&BODY_00_15(\$code,11,$B,$C,$D,$E,$T,$A,1);
	&BODY_00_15(\$code,12,$A,$B,$C,$D,$E,$T,1);
	&BODY_00_15(\$code,13,$T,$A,$B,$C,$D,$E,1);
	&BODY_00_15(\$code,14,$E,$T,$A,$B,$C,$D,1);
	&BODY_00_15(\$code,15,$D,$E,$T,$A,$B,$C,1);

	&BODY_16_19(\$code,16,$C,$D,$E,$T,$A,$B);
	&BODY_16_19(\$code,17,$B,$C,$D,$E,$T,$A);
	&BODY_16_19(\$code,18,$A,$B,$C,$D,$E,$T);
	&BODY_16_19(\$code,19,$T,$A,$B,$C,$D,$E);

	&BODY_20_39(\$code,20,$E,$T,$A,$B,$C,$D);
	&BODY_20_39(\$code,21,$D,$E,$T,$A,$B,$C);
	&BODY_20_39(\$code,22,$C,$D,$E,$T,$A,$B);
	&BODY_20_39(\$code,23,$B,$C,$D,$E,$T,$A);
	&BODY_20_39(\$code,24,$A,$B,$C,$D,$E,$T);
	&BODY_20_39(\$code,25,$T,$A,$B,$C,$D,$E);
	&BODY_20_39(\$code,26,$E,$T,$A,$B,$C,$D);
	&BODY_20_39(\$code,27,$D,$E,$T,$A,$B,$C);
	&BODY_20_39(\$code,28,$C,$D,$E,$T,$A,$B);
	&BODY_20_39(\$code,29,$B,$C,$D,$E,$T,$A);
	&BODY_20_39(\$code,30,$A,$B,$C,$D,$E,$T);
	&BODY_20_39(\$code,31,$T,$A,$B,$C,$D,$E);
	&BODY_20_39(\$code,32,$E,$T,$A,$B,$C,$D);
	&BODY_20_39(\$code,33,$D,$E,$T,$A,$B,$C);
	&BODY_20_39(\$code,34,$C,$D,$E,$T,$A,$B);
	&BODY_20_39(\$code,35,$B,$C,$D,$E,$T,$A);
	&BODY_20_39(\$code,36,$A,$B,$C,$D,$E,$T);
	&BODY_20_39(\$code,37,$T,$A,$B,$C,$D,$E);
	&BODY_20_39(\$code,38,$E,$T,$A,$B,$C,$D);
	&BODY_20_39(\$code,39,$D,$E,$T,$A,$B,$C);

	&BODY_40_59(\$code,40,$C,$D,$E,$T,$A,$B);
	&BODY_40_59(\$code,41,$B,$C,$D,$E,$T,$A);
	&BODY_40_59(\$code,42,$A,$B,$C,$D,$E,$T);
	&BODY_40_59(\$code,43,$T,$A,$B,$C,$D,$E);
	&BODY_40_59(\$code,44,$E,$T,$A,$B,$C,$D);
	&BODY_40_59(\$code,45,$D,$E,$T,$A,$B,$C);
	&BODY_40_59(\$code,46,$C,$D,$E,$T,$A,$B);
	&BODY_40_59(\$code,47,$B,$C,$D,$E,$T,$A);
	&BODY_40_59(\$code,48,$A,$B,$C,$D,$E,$T);
	&BODY_40_59(\$code,49,$T,$A,$B,$C,$D,$E);
	&BODY_40_59(\$code,50,$E,$T,$A,$B,$C,$D);
	&BODY_40_59(\$code,51,$D,$E,$T,$A,$B,$C);
	&BODY_40_59(\$code,52,$C,$D,$E,$T,$A,$B);
	&BODY_40_59(\$code,53,$B,$C,$D,$E,$T,$A);
	&BODY_40_59(\$code,54,$A,$B,$C,$D,$E,$T);
	&BODY_40_59(\$code,55,$T,$A,$B,$C,$D,$E);
	&BODY_40_59(\$code,56,$E,$T,$A,$B,$C,$D);
	&BODY_40_59(\$code,57,$D,$E,$T,$A,$B,$C);
	&BODY_40_59(\$code,58,$C,$D,$E,$T,$A,$B);
	&BODY_40_59(\$code,59,$B,$C,$D,$E,$T,$A);

	&BODY_60_79(\$code,60,$A,$B,$C,$D,$E,$T);
	&BODY_60_79(\$code,61,$T,$A,$B,$C,$D,$E);
	&BODY_60_79(\$code,62,$E,$T,$A,$B,$C,$D);
	&BODY_60_79(\$code,63,$D,$E,$T,$A,$B,$C);
	&BODY_60_79(\$code,64,$C,$D,$E,$T,$A,$B);
	&BODY_60_79(\$code,65,$B,$C,$D,$E,$T,$A);
	&BODY_60_79(\$code,66,$A,$B,$C,$D,$E,$T);
	&BODY_60_79(\$code,67,$T,$A,$B,$C,$D,$E);
	&BODY_60_79(\$code,68,$E,$T,$A,$B,$C,$D);
	&BODY_60_79(\$code,69,$D,$E,$T,$A,$B,$C);
	&BODY_60_79(\$code,70,$C,$D,$E,$T,$A,$B);
	&BODY_60_79(\$code,71,$B,$C,$D,$E,$T,$A);
	&BODY_60_79(\$code,72,$A,$B,$C,$D,$E,$T);
	&BODY_60_79(\$code,73,$T,$A,$B,$C,$D,$E);
	&BODY_60_79(\$code,74,$E,$T,$A,$B,$C,$D);
	&BODY_60_79(\$code,75,$D,$E,$T,$A,$B,$C);
	&BODY_60_79(\$code,76,$C,$D,$E,$T,$A,$B);
	&BODY_60_79(\$code,77,$B,$C,$D,$E,$T,$A);
	&BODY_60_79(\$code,78,$A,$B,$C,$D,$E,$T);
	&BODY_60_79(\$code,79,$T,$A,$B,$C,$D,$E);

$code.=<<___;
{ .mmb;	add	$h0=$h0,$E
	nop.m	0
	br.ctop.dptk.many	.Ldtop	};;
.Ldend:
{ .mmi;	add	tmp0=4,ctx
	mov	ar.lc=r3		};;
{ .mmi;	st4	[ctx]=$h0,8
	st4	[tmp0]=$h1,8		};;
{ .mmi;	st4	[ctx]=$h2,8
	st4	[tmp0]=$h3		};;
{ .mib;	st4	[ctx]=$h4,-16
	mov	pr=r2,0x1ffff
	br.ret.sptk.many	b0	};;
.endp	sha1_block_asm_data_order#
___

print $code;
