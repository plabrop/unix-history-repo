/* Generated automatically by the program `gencodes'
from the machine description file `md'.  */

#ifndef MAX_INSN_CODE

enum insn_code {
  CODE_FOR_tstsi_1 = 0,
  CODE_FOR_tstsi = 1,
  CODE_FOR_tsthi_1 = 2,
  CODE_FOR_tsthi = 3,
  CODE_FOR_tstqi_1 = 4,
  CODE_FOR_tstqi = 5,
  CODE_FOR_tstsf_cc = 6,
  CODE_FOR_tstsf = 7,
  CODE_FOR_tstdf_cc = 8,
  CODE_FOR_tstdf = 9,
  CODE_FOR_tstxf_cc = 10,
  CODE_FOR_tstxf = 11,
  CODE_FOR_cmpsi_1 = 12,
  CODE_FOR_cmpsi = 13,
  CODE_FOR_cmphi_1 = 14,
  CODE_FOR_cmphi = 15,
  CODE_FOR_cmpqi_1 = 16,
  CODE_FOR_cmpqi = 17,
  CODE_FOR_cmpsf_cc_1 = 30,
  CODE_FOR_cmpxf = 34,
  CODE_FOR_cmpdf = 35,
  CODE_FOR_cmpsf = 36,
  CODE_FOR_cmpxf_cc = 37,
  CODE_FOR_cmpxf_ccfpeq = 38,
  CODE_FOR_cmpdf_cc = 39,
  CODE_FOR_cmpdf_ccfpeq = 40,
  CODE_FOR_cmpsf_cc = 41,
  CODE_FOR_cmpsf_ccfpeq = 42,
  CODE_FOR_movsi = 49,
  CODE_FOR_movhi = 54,
  CODE_FOR_movstricthi = 56,
  CODE_FOR_movqi = 61,
  CODE_FOR_movstrictqi = 63,
  CODE_FOR_movsf = 65,
  CODE_FOR_movsf_push_nomove = 66,
  CODE_FOR_movsf_push = 67,
  CODE_FOR_movsf_mem = 68,
  CODE_FOR_movsf_normal = 69,
  CODE_FOR_swapsf = 70,
  CODE_FOR_movdf = 71,
  CODE_FOR_movdf_push_nomove = 72,
  CODE_FOR_movdf_push = 73,
  CODE_FOR_movdf_mem = 74,
  CODE_FOR_movdf_normal = 75,
  CODE_FOR_swapdf = 76,
  CODE_FOR_movxf = 77,
  CODE_FOR_movxf_push_nomove = 78,
  CODE_FOR_movxf_push = 79,
  CODE_FOR_movxf_mem = 80,
  CODE_FOR_movxf_normal = 81,
  CODE_FOR_swapxf = 82,
  CODE_FOR_movdi = 84,
  CODE_FOR_zero_extendhisi2 = 85,
  CODE_FOR_zero_extendqihi2 = 86,
  CODE_FOR_zero_extendqisi2 = 87,
  CODE_FOR_zero_extendsidi2 = 88,
  CODE_FOR_extendsidi2 = 89,
  CODE_FOR_extendhisi2 = 90,
  CODE_FOR_extendqihi2 = 91,
  CODE_FOR_extendqisi2 = 92,
  CODE_FOR_extendsfdf2 = 93,
  CODE_FOR_extenddfxf2 = 94,
  CODE_FOR_extendsfxf2 = 95,
  CODE_FOR_truncdfsf2 = 96,
  CODE_FOR_truncxfsf2 = 98,
  CODE_FOR_truncxfdf2 = 99,
  CODE_FOR_fixuns_truncxfsi2 = 100,
  CODE_FOR_fixuns_truncdfsi2 = 101,
  CODE_FOR_fixuns_truncsfsi2 = 102,
  CODE_FOR_fix_truncxfdi2 = 103,
  CODE_FOR_fix_truncdfdi2 = 104,
  CODE_FOR_fix_truncsfdi2 = 105,
  CODE_FOR_fix_truncxfsi2 = 109,
  CODE_FOR_fix_truncdfsi2 = 110,
  CODE_FOR_fix_truncsfsi2 = 111,
  CODE_FOR_floatsisf2 = 115,
  CODE_FOR_floatdisf2 = 116,
  CODE_FOR_floatsidf2 = 117,
  CODE_FOR_floatdidf2 = 118,
  CODE_FOR_floatsixf2 = 119,
  CODE_FOR_floatdixf2 = 120,
  CODE_FOR_adddi3 = 127,
  CODE_FOR_addsi3 = 128,
  CODE_FOR_addhi3 = 129,
  CODE_FOR_addqi3 = 130,
  CODE_FOR_movsi_lea = 131,
  CODE_FOR_addxf3 = 132,
  CODE_FOR_adddf3 = 133,
  CODE_FOR_addsf3 = 134,
  CODE_FOR_subdi3 = 135,
  CODE_FOR_subsi3 = 136,
  CODE_FOR_subhi3 = 137,
  CODE_FOR_subqi3 = 138,
  CODE_FOR_subxf3 = 139,
  CODE_FOR_subdf3 = 140,
  CODE_FOR_subsf3 = 141,
  CODE_FOR_mulhi3 = 143,
  CODE_FOR_mulsi3 = 145,
  CODE_FOR_umulqihi3 = 146,
  CODE_FOR_mulqihi3 = 147,
  CODE_FOR_umulsidi3 = 148,
  CODE_FOR_mulsidi3 = 149,
  CODE_FOR_umulsi3_highpart = 150,
  CODE_FOR_smulsi3_highpart = 151,
  CODE_FOR_mulxf3 = 152,
  CODE_FOR_muldf3 = 153,
  CODE_FOR_mulsf3 = 154,
  CODE_FOR_divqi3 = 155,
  CODE_FOR_udivqi3 = 156,
  CODE_FOR_divxf3 = 157,
  CODE_FOR_divdf3 = 158,
  CODE_FOR_divsf3 = 159,
  CODE_FOR_divmodsi4 = 160,
  CODE_FOR_divmodhi4 = 161,
  CODE_FOR_udivmodsi4 = 162,
  CODE_FOR_udivmodhi4 = 163,
  CODE_FOR_andsi3 = 164,
  CODE_FOR_andhi3 = 165,
  CODE_FOR_andqi3 = 166,
  CODE_FOR_iorsi3 = 167,
  CODE_FOR_iorhi3 = 168,
  CODE_FOR_iorqi3 = 169,
  CODE_FOR_xorsi3 = 170,
  CODE_FOR_xorhi3 = 171,
  CODE_FOR_xorqi3 = 172,
  CODE_FOR_negdi2 = 173,
  CODE_FOR_negsi2 = 174,
  CODE_FOR_neghi2 = 175,
  CODE_FOR_negqi2 = 176,
  CODE_FOR_negsf2 = 177,
  CODE_FOR_negdf2 = 178,
  CODE_FOR_negxf2 = 180,
  CODE_FOR_abssf2 = 182,
  CODE_FOR_absdf2 = 183,
  CODE_FOR_absxf2 = 185,
  CODE_FOR_sqrtsf2 = 187,
  CODE_FOR_sqrtdf2 = 188,
  CODE_FOR_sqrtxf2 = 190,
  CODE_FOR_sindf2 = 193,
  CODE_FOR_sinsf2 = 194,
  CODE_FOR_cosdf2 = 196,
  CODE_FOR_cossf2 = 197,
  CODE_FOR_one_cmplsi2 = 199,
  CODE_FOR_one_cmplhi2 = 200,
  CODE_FOR_one_cmplqi2 = 201,
  CODE_FOR_ashldi3 = 202,
  CODE_FOR_ashldi3_const_int = 203,
  CODE_FOR_ashldi3_non_const_int = 204,
  CODE_FOR_ashlsi3 = 205,
  CODE_FOR_ashlhi3 = 206,
  CODE_FOR_ashlqi3 = 207,
  CODE_FOR_ashrdi3 = 208,
  CODE_FOR_ashrdi3_const_int = 209,
  CODE_FOR_ashrdi3_non_const_int = 210,
  CODE_FOR_ashrsi3 = 211,
  CODE_FOR_ashrhi3 = 212,
  CODE_FOR_ashrqi3 = 213,
  CODE_FOR_lshrdi3 = 214,
  CODE_FOR_lshrdi3_const_int = 215,
  CODE_FOR_lshrdi3_non_const_int = 216,
  CODE_FOR_lshrsi3 = 217,
  CODE_FOR_lshrhi3 = 218,
  CODE_FOR_lshrqi3 = 219,
  CODE_FOR_rotlsi3 = 220,
  CODE_FOR_rotlhi3 = 221,
  CODE_FOR_rotlqi3 = 222,
  CODE_FOR_rotrsi3 = 223,
  CODE_FOR_rotrhi3 = 224,
  CODE_FOR_rotrqi3 = 225,
  CODE_FOR_seq = 232,
  CODE_FOR_sne = 234,
  CODE_FOR_sgt = 236,
  CODE_FOR_sgtu = 238,
  CODE_FOR_slt = 240,
  CODE_FOR_sltu = 242,
  CODE_FOR_sge = 244,
  CODE_FOR_sgeu = 246,
  CODE_FOR_sle = 248,
  CODE_FOR_sleu = 250,
  CODE_FOR_beq = 252,
  CODE_FOR_bne = 254,
  CODE_FOR_bgt = 256,
  CODE_FOR_bgtu = 258,
  CODE_FOR_blt = 260,
  CODE_FOR_bltu = 262,
  CODE_FOR_bge = 264,
  CODE_FOR_bgeu = 266,
  CODE_FOR_ble = 268,
  CODE_FOR_bleu = 270,
  CODE_FOR_jump = 282,
  CODE_FOR_indirect_jump = 283,
  CODE_FOR_casesi = 284,
  CODE_FOR_tablejump = 286,
  CODE_FOR_call_pop = 287,
  CODE_FOR_call = 290,
  CODE_FOR_call_value_pop = 293,
  CODE_FOR_call_value = 296,
  CODE_FOR_untyped_call = 299,
  CODE_FOR_blockage = 300,
  CODE_FOR_return = 301,
  CODE_FOR_nop = 302,
  CODE_FOR_movstrsi = 303,
  CODE_FOR_cmpstrsi = 305,
  CODE_FOR_ffssi2 = 308,
  CODE_FOR_ffshi2 = 310,
  CODE_FOR_strlensi = 325,
  CODE_FOR_nothing };

#define MAX_INSN_CODE ((int) CODE_FOR_nothing)
#endif /* MAX_INSN_CODE */
