/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 */

#ifndef MC_PP_SEQ_H
#define MC_PP_SEQ_H

#include <mc/pp/cat.h>

// A "seq" is (a)(b)(c) — each element is parenthesized.

// --- SEQ_SIZE ---
// MC_PP_SEQ_SIZE((a)(b)(c)) => 3
// Technique: chain-matching. Each SIZE_N matches one element and produces SIZE_{N+1}.
// Final cat produces the number.
#define MC_PP_SEQ_SIZE(seq) MC_PP_CAT(MC_PP_SEQ_SIZE_, MC_PP_SEQ_SIZE_0 seq)

#define MC_PP_SEQ_SIZE_0(_) MC_PP_SEQ_SIZE_1
#define MC_PP_SEQ_SIZE_1(_) MC_PP_SEQ_SIZE_2
#define MC_PP_SEQ_SIZE_2(_) MC_PP_SEQ_SIZE_3
#define MC_PP_SEQ_SIZE_3(_) MC_PP_SEQ_SIZE_4
#define MC_PP_SEQ_SIZE_4(_) MC_PP_SEQ_SIZE_5
#define MC_PP_SEQ_SIZE_5(_) MC_PP_SEQ_SIZE_6
#define MC_PP_SEQ_SIZE_6(_) MC_PP_SEQ_SIZE_7
#define MC_PP_SEQ_SIZE_7(_) MC_PP_SEQ_SIZE_8
#define MC_PP_SEQ_SIZE_8(_) MC_PP_SEQ_SIZE_9
#define MC_PP_SEQ_SIZE_9(_) MC_PP_SEQ_SIZE_10
#define MC_PP_SEQ_SIZE_10(_) MC_PP_SEQ_SIZE_11
#define MC_PP_SEQ_SIZE_11(_) MC_PP_SEQ_SIZE_12
#define MC_PP_SEQ_SIZE_12(_) MC_PP_SEQ_SIZE_13
#define MC_PP_SEQ_SIZE_13(_) MC_PP_SEQ_SIZE_14
#define MC_PP_SEQ_SIZE_14(_) MC_PP_SEQ_SIZE_15
#define MC_PP_SEQ_SIZE_15(_) MC_PP_SEQ_SIZE_16
#define MC_PP_SEQ_SIZE_16(_) MC_PP_SEQ_SIZE_17
#define MC_PP_SEQ_SIZE_17(_) MC_PP_SEQ_SIZE_18
#define MC_PP_SEQ_SIZE_18(_) MC_PP_SEQ_SIZE_19
#define MC_PP_SEQ_SIZE_19(_) MC_PP_SEQ_SIZE_20
#define MC_PP_SEQ_SIZE_20(_) MC_PP_SEQ_SIZE_21
#define MC_PP_SEQ_SIZE_21(_) MC_PP_SEQ_SIZE_22
#define MC_PP_SEQ_SIZE_22(_) MC_PP_SEQ_SIZE_23
#define MC_PP_SEQ_SIZE_23(_) MC_PP_SEQ_SIZE_24
#define MC_PP_SEQ_SIZE_24(_) MC_PP_SEQ_SIZE_25
#define MC_PP_SEQ_SIZE_25(_) MC_PP_SEQ_SIZE_26
#define MC_PP_SEQ_SIZE_26(_) MC_PP_SEQ_SIZE_27
#define MC_PP_SEQ_SIZE_27(_) MC_PP_SEQ_SIZE_28
#define MC_PP_SEQ_SIZE_28(_) MC_PP_SEQ_SIZE_29
#define MC_PP_SEQ_SIZE_29(_) MC_PP_SEQ_SIZE_30
#define MC_PP_SEQ_SIZE_30(_) MC_PP_SEQ_SIZE_31
#define MC_PP_SEQ_SIZE_31(_) MC_PP_SEQ_SIZE_32
#define MC_PP_SEQ_SIZE_32(_) MC_PP_SEQ_SIZE_33
#define MC_PP_SEQ_SIZE_33(_) MC_PP_SEQ_SIZE_34
#define MC_PP_SEQ_SIZE_34(_) MC_PP_SEQ_SIZE_35
#define MC_PP_SEQ_SIZE_35(_) MC_PP_SEQ_SIZE_36
#define MC_PP_SEQ_SIZE_36(_) MC_PP_SEQ_SIZE_37
#define MC_PP_SEQ_SIZE_37(_) MC_PP_SEQ_SIZE_38
#define MC_PP_SEQ_SIZE_38(_) MC_PP_SEQ_SIZE_39
#define MC_PP_SEQ_SIZE_39(_) MC_PP_SEQ_SIZE_40
#define MC_PP_SEQ_SIZE_40(_) MC_PP_SEQ_SIZE_41
#define MC_PP_SEQ_SIZE_41(_) MC_PP_SEQ_SIZE_42
#define MC_PP_SEQ_SIZE_42(_) MC_PP_SEQ_SIZE_43
#define MC_PP_SEQ_SIZE_43(_) MC_PP_SEQ_SIZE_44
#define MC_PP_SEQ_SIZE_44(_) MC_PP_SEQ_SIZE_45
#define MC_PP_SEQ_SIZE_45(_) MC_PP_SEQ_SIZE_46
#define MC_PP_SEQ_SIZE_46(_) MC_PP_SEQ_SIZE_47
#define MC_PP_SEQ_SIZE_47(_) MC_PP_SEQ_SIZE_48
#define MC_PP_SEQ_SIZE_48(_) MC_PP_SEQ_SIZE_49
#define MC_PP_SEQ_SIZE_49(_) MC_PP_SEQ_SIZE_50
#define MC_PP_SEQ_SIZE_50(_) MC_PP_SEQ_SIZE_51
#define MC_PP_SEQ_SIZE_51(_) MC_PP_SEQ_SIZE_52
#define MC_PP_SEQ_SIZE_52(_) MC_PP_SEQ_SIZE_53
#define MC_PP_SEQ_SIZE_53(_) MC_PP_SEQ_SIZE_54
#define MC_PP_SEQ_SIZE_54(_) MC_PP_SEQ_SIZE_55
#define MC_PP_SEQ_SIZE_55(_) MC_PP_SEQ_SIZE_56
#define MC_PP_SEQ_SIZE_56(_) MC_PP_SEQ_SIZE_57
#define MC_PP_SEQ_SIZE_57(_) MC_PP_SEQ_SIZE_58
#define MC_PP_SEQ_SIZE_58(_) MC_PP_SEQ_SIZE_59
#define MC_PP_SEQ_SIZE_59(_) MC_PP_SEQ_SIZE_60
#define MC_PP_SEQ_SIZE_60(_) MC_PP_SEQ_SIZE_61
#define MC_PP_SEQ_SIZE_61(_) MC_PP_SEQ_SIZE_62
#define MC_PP_SEQ_SIZE_62(_) MC_PP_SEQ_SIZE_63
#define MC_PP_SEQ_SIZE_63(_) MC_PP_SEQ_SIZE_64
#define MC_PP_SEQ_SIZE_64(_) MC_PP_SEQ_SIZE_65
#define MC_PP_SEQ_SIZE_65(_) MC_PP_SEQ_SIZE_66
#define MC_PP_SEQ_SIZE_66(_) MC_PP_SEQ_SIZE_67
#define MC_PP_SEQ_SIZE_67(_) MC_PP_SEQ_SIZE_68
#define MC_PP_SEQ_SIZE_68(_) MC_PP_SEQ_SIZE_69
#define MC_PP_SEQ_SIZE_69(_) MC_PP_SEQ_SIZE_70
#define MC_PP_SEQ_SIZE_70(_) MC_PP_SEQ_SIZE_71
#define MC_PP_SEQ_SIZE_71(_) MC_PP_SEQ_SIZE_72
#define MC_PP_SEQ_SIZE_72(_) MC_PP_SEQ_SIZE_73
#define MC_PP_SEQ_SIZE_73(_) MC_PP_SEQ_SIZE_74
#define MC_PP_SEQ_SIZE_74(_) MC_PP_SEQ_SIZE_75
#define MC_PP_SEQ_SIZE_75(_) MC_PP_SEQ_SIZE_76
#define MC_PP_SEQ_SIZE_76(_) MC_PP_SEQ_SIZE_77
#define MC_PP_SEQ_SIZE_77(_) MC_PP_SEQ_SIZE_78
#define MC_PP_SEQ_SIZE_78(_) MC_PP_SEQ_SIZE_79
#define MC_PP_SEQ_SIZE_79(_) MC_PP_SEQ_SIZE_80
#define MC_PP_SEQ_SIZE_80(_) MC_PP_SEQ_SIZE_81
#define MC_PP_SEQ_SIZE_81(_) MC_PP_SEQ_SIZE_82
#define MC_PP_SEQ_SIZE_82(_) MC_PP_SEQ_SIZE_83
#define MC_PP_SEQ_SIZE_83(_) MC_PP_SEQ_SIZE_84
#define MC_PP_SEQ_SIZE_84(_) MC_PP_SEQ_SIZE_85
#define MC_PP_SEQ_SIZE_85(_) MC_PP_SEQ_SIZE_86
#define MC_PP_SEQ_SIZE_86(_) MC_PP_SEQ_SIZE_87
#define MC_PP_SEQ_SIZE_87(_) MC_PP_SEQ_SIZE_88
#define MC_PP_SEQ_SIZE_88(_) MC_PP_SEQ_SIZE_89
#define MC_PP_SEQ_SIZE_89(_) MC_PP_SEQ_SIZE_90
#define MC_PP_SEQ_SIZE_90(_) MC_PP_SEQ_SIZE_91
#define MC_PP_SEQ_SIZE_91(_) MC_PP_SEQ_SIZE_92
#define MC_PP_SEQ_SIZE_92(_) MC_PP_SEQ_SIZE_93
#define MC_PP_SEQ_SIZE_93(_) MC_PP_SEQ_SIZE_94
#define MC_PP_SEQ_SIZE_94(_) MC_PP_SEQ_SIZE_95
#define MC_PP_SEQ_SIZE_95(_) MC_PP_SEQ_SIZE_96
#define MC_PP_SEQ_SIZE_96(_) MC_PP_SEQ_SIZE_97
#define MC_PP_SEQ_SIZE_97(_) MC_PP_SEQ_SIZE_98
#define MC_PP_SEQ_SIZE_98(_) MC_PP_SEQ_SIZE_99
#define MC_PP_SEQ_SIZE_99(_) MC_PP_SEQ_SIZE_100
#define MC_PP_SEQ_SIZE_100(_) MC_PP_SEQ_SIZE_101
#define MC_PP_SEQ_SIZE_101(_) MC_PP_SEQ_SIZE_102
#define MC_PP_SEQ_SIZE_102(_) MC_PP_SEQ_SIZE_103
#define MC_PP_SEQ_SIZE_103(_) MC_PP_SEQ_SIZE_104
#define MC_PP_SEQ_SIZE_104(_) MC_PP_SEQ_SIZE_105
#define MC_PP_SEQ_SIZE_105(_) MC_PP_SEQ_SIZE_106
#define MC_PP_SEQ_SIZE_106(_) MC_PP_SEQ_SIZE_107
#define MC_PP_SEQ_SIZE_107(_) MC_PP_SEQ_SIZE_108
#define MC_PP_SEQ_SIZE_108(_) MC_PP_SEQ_SIZE_109
#define MC_PP_SEQ_SIZE_109(_) MC_PP_SEQ_SIZE_110
#define MC_PP_SEQ_SIZE_110(_) MC_PP_SEQ_SIZE_111
#define MC_PP_SEQ_SIZE_111(_) MC_PP_SEQ_SIZE_112
#define MC_PP_SEQ_SIZE_112(_) MC_PP_SEQ_SIZE_113
#define MC_PP_SEQ_SIZE_113(_) MC_PP_SEQ_SIZE_114
#define MC_PP_SEQ_SIZE_114(_) MC_PP_SEQ_SIZE_115
#define MC_PP_SEQ_SIZE_115(_) MC_PP_SEQ_SIZE_116
#define MC_PP_SEQ_SIZE_116(_) MC_PP_SEQ_SIZE_117
#define MC_PP_SEQ_SIZE_117(_) MC_PP_SEQ_SIZE_118
#define MC_PP_SEQ_SIZE_118(_) MC_PP_SEQ_SIZE_119
#define MC_PP_SEQ_SIZE_119(_) MC_PP_SEQ_SIZE_120
#define MC_PP_SEQ_SIZE_120(_) MC_PP_SEQ_SIZE_121
#define MC_PP_SEQ_SIZE_121(_) MC_PP_SEQ_SIZE_122
#define MC_PP_SEQ_SIZE_122(_) MC_PP_SEQ_SIZE_123
#define MC_PP_SEQ_SIZE_123(_) MC_PP_SEQ_SIZE_124
#define MC_PP_SEQ_SIZE_124(_) MC_PP_SEQ_SIZE_125
#define MC_PP_SEQ_SIZE_125(_) MC_PP_SEQ_SIZE_126
#define MC_PP_SEQ_SIZE_126(_) MC_PP_SEQ_SIZE_127
#define MC_PP_SEQ_SIZE_127(_) MC_PP_SEQ_SIZE_128

#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_0 0
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_1 1
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_2 2
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_3 3
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_4 4
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_5 5
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_6 6
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_7 7
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_8 8
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_9 9
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_10 10
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_11 11
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_12 12
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_13 13
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_14 14
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_15 15
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_16 16
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_17 17
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_18 18
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_19 19
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_20 20
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_21 21
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_22 22
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_23 23
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_24 24
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_25 25
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_26 26
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_27 27
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_28 28
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_29 29
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_30 30
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_31 31
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_32 32
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_33 33
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_34 34
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_35 35
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_36 36
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_37 37
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_38 38
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_39 39
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_40 40
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_41 41
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_42 42
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_43 43
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_44 44
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_45 45
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_46 46
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_47 47
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_48 48
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_49 49
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_50 50
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_51 51
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_52 52
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_53 53
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_54 54
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_55 55
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_56 56
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_57 57
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_58 58
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_59 59
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_60 60
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_61 61
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_62 62
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_63 63
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_64 64
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_65 65
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_66 66
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_67 67
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_68 68
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_69 69
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_70 70
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_71 71
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_72 72
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_73 73
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_74 74
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_75 75
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_76 76
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_77 77
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_78 78
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_79 79
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_80 80
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_81 81
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_82 82
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_83 83
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_84 84
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_85 85
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_86 86
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_87 87
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_88 88
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_89 89
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_90 90
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_91 91
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_92 92
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_93 93
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_94 94
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_95 95
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_96 96
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_97 97
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_98 98
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_99 99
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_100 100
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_101 101
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_102 102
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_103 103
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_104 104
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_105 105
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_106 106
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_107 107
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_108 108
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_109 109
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_110 110
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_111 111
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_112 112
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_113 113
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_114 114
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_115 115
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_116 116
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_117 117
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_118 118
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_119 119
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_120 120
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_121 121
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_122 122
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_123 123
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_124 124
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_125 125
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_126 126
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_127 127
#define MC_PP_SEQ_SIZE_MC_PP_SEQ_SIZE_128 128

// --- SEQ_HEAD (with eater pattern) ---
// The naive HEAD_I seq technique leaves behind leftover (b)(c) tokens that
// concatenate with the result (e.g. "a(b)(c)" instead of "a").
// Fix: dispatch by size, extract first element, then use eater macros to
// consume the remaining N-1 elements without producing output.
#define MC_PP_SEQ_HEAD(seq) MC_PP_CAT(MC_PP_SH_, MC_PP_SEQ_SIZE(seq))(seq)

// Eater macros: each consumes one seq element and produces the next eater
#define MC_PP_SHE_1(...)
#define MC_PP_SHE_2(...) MC_PP_SHE_1
#define MC_PP_SHE_3(...) MC_PP_SHE_2
#define MC_PP_SHE_4(...) MC_PP_SHE_3
#define MC_PP_SHE_5(...) MC_PP_SHE_4
#define MC_PP_SHE_6(...) MC_PP_SHE_5
#define MC_PP_SHE_7(...) MC_PP_SHE_6
#define MC_PP_SHE_8(...) MC_PP_SHE_7
#define MC_PP_SHE_9(...) MC_PP_SHE_8
#define MC_PP_SHE_10(...) MC_PP_SHE_9
#define MC_PP_SHE_11(...) MC_PP_SHE_10
#define MC_PP_SHE_12(...) MC_PP_SHE_11
#define MC_PP_SHE_13(...) MC_PP_SHE_12
#define MC_PP_SHE_14(...) MC_PP_SHE_13
#define MC_PP_SHE_15(...) MC_PP_SHE_14
#define MC_PP_SHE_16(...) MC_PP_SHE_15
#define MC_PP_SHE_17(...) MC_PP_SHE_16
#define MC_PP_SHE_18(...) MC_PP_SHE_17
#define MC_PP_SHE_19(...) MC_PP_SHE_18
#define MC_PP_SHE_20(...) MC_PP_SHE_19
#define MC_PP_SHE_21(...) MC_PP_SHE_20
#define MC_PP_SHE_22(...) MC_PP_SHE_21
#define MC_PP_SHE_23(...) MC_PP_SHE_22
#define MC_PP_SHE_24(...) MC_PP_SHE_23
#define MC_PP_SHE_25(...) MC_PP_SHE_24
#define MC_PP_SHE_26(...) MC_PP_SHE_25
#define MC_PP_SHE_27(...) MC_PP_SHE_26
#define MC_PP_SHE_28(...) MC_PP_SHE_27
#define MC_PP_SHE_29(...) MC_PP_SHE_28
#define MC_PP_SHE_30(...) MC_PP_SHE_29
#define MC_PP_SHE_31(...) MC_PP_SHE_30
#define MC_PP_SHE_32(...) MC_PP_SHE_31
#define MC_PP_SHE_33(...) MC_PP_SHE_32
#define MC_PP_SHE_34(...) MC_PP_SHE_33
#define MC_PP_SHE_35(...) MC_PP_SHE_34
#define MC_PP_SHE_36(...) MC_PP_SHE_35
#define MC_PP_SHE_37(...) MC_PP_SHE_36
#define MC_PP_SHE_38(...) MC_PP_SHE_37
#define MC_PP_SHE_39(...) MC_PP_SHE_38
#define MC_PP_SHE_40(...) MC_PP_SHE_39
#define MC_PP_SHE_41(...) MC_PP_SHE_40
#define MC_PP_SHE_42(...) MC_PP_SHE_41
#define MC_PP_SHE_43(...) MC_PP_SHE_42
#define MC_PP_SHE_44(...) MC_PP_SHE_43
#define MC_PP_SHE_45(...) MC_PP_SHE_44
#define MC_PP_SHE_46(...) MC_PP_SHE_45
#define MC_PP_SHE_47(...) MC_PP_SHE_46
#define MC_PP_SHE_48(...) MC_PP_SHE_47
#define MC_PP_SHE_49(...) MC_PP_SHE_48
#define MC_PP_SHE_50(...) MC_PP_SHE_49
#define MC_PP_SHE_51(...) MC_PP_SHE_50
#define MC_PP_SHE_52(...) MC_PP_SHE_51
#define MC_PP_SHE_53(...) MC_PP_SHE_52
#define MC_PP_SHE_54(...) MC_PP_SHE_53
#define MC_PP_SHE_55(...) MC_PP_SHE_54
#define MC_PP_SHE_56(...) MC_PP_SHE_55
#define MC_PP_SHE_57(...) MC_PP_SHE_56
#define MC_PP_SHE_58(...) MC_PP_SHE_57
#define MC_PP_SHE_59(...) MC_PP_SHE_58
#define MC_PP_SHE_60(...) MC_PP_SHE_59
#define MC_PP_SHE_61(...) MC_PP_SHE_60
#define MC_PP_SHE_62(...) MC_PP_SHE_61
#define MC_PP_SHE_63(...) MC_PP_SHE_62
#define MC_PP_SHE_64(...) MC_PP_SHE_63
#define MC_PP_SHE_65(...) MC_PP_SHE_64
#define MC_PP_SHE_66(...) MC_PP_SHE_65
#define MC_PP_SHE_67(...) MC_PP_SHE_66
#define MC_PP_SHE_68(...) MC_PP_SHE_67
#define MC_PP_SHE_69(...) MC_PP_SHE_68
#define MC_PP_SHE_70(...) MC_PP_SHE_69
#define MC_PP_SHE_71(...) MC_PP_SHE_70
#define MC_PP_SHE_72(...) MC_PP_SHE_71
#define MC_PP_SHE_73(...) MC_PP_SHE_72
#define MC_PP_SHE_74(...) MC_PP_SHE_73
#define MC_PP_SHE_75(...) MC_PP_SHE_74
#define MC_PP_SHE_76(...) MC_PP_SHE_75
#define MC_PP_SHE_77(...) MC_PP_SHE_76
#define MC_PP_SHE_78(...) MC_PP_SHE_77
#define MC_PP_SHE_79(...) MC_PP_SHE_78
#define MC_PP_SHE_80(...) MC_PP_SHE_79
#define MC_PP_SHE_81(...) MC_PP_SHE_80
#define MC_PP_SHE_82(...) MC_PP_SHE_81
#define MC_PP_SHE_83(...) MC_PP_SHE_82
#define MC_PP_SHE_84(...) MC_PP_SHE_83
#define MC_PP_SHE_85(...) MC_PP_SHE_84
#define MC_PP_SHE_86(...) MC_PP_SHE_85
#define MC_PP_SHE_87(...) MC_PP_SHE_86
#define MC_PP_SHE_88(...) MC_PP_SHE_87
#define MC_PP_SHE_89(...) MC_PP_SHE_88
#define MC_PP_SHE_90(...) MC_PP_SHE_89
#define MC_PP_SHE_91(...) MC_PP_SHE_90
#define MC_PP_SHE_92(...) MC_PP_SHE_91
#define MC_PP_SHE_93(...) MC_PP_SHE_92
#define MC_PP_SHE_94(...) MC_PP_SHE_93
#define MC_PP_SHE_95(...) MC_PP_SHE_94
#define MC_PP_SHE_96(...) MC_PP_SHE_95
#define MC_PP_SHE_97(...) MC_PP_SHE_96
#define MC_PP_SHE_98(...) MC_PP_SHE_97
#define MC_PP_SHE_99(...) MC_PP_SHE_98
#define MC_PP_SHE_100(...) MC_PP_SHE_99
#define MC_PP_SHE_101(...) MC_PP_SHE_100
#define MC_PP_SHE_102(...) MC_PP_SHE_101
#define MC_PP_SHE_103(...) MC_PP_SHE_102
#define MC_PP_SHE_104(...) MC_PP_SHE_103
#define MC_PP_SHE_105(...) MC_PP_SHE_104
#define MC_PP_SHE_106(...) MC_PP_SHE_105
#define MC_PP_SHE_107(...) MC_PP_SHE_106
#define MC_PP_SHE_108(...) MC_PP_SHE_107
#define MC_PP_SHE_109(...) MC_PP_SHE_108
#define MC_PP_SHE_110(...) MC_PP_SHE_109
#define MC_PP_SHE_111(...) MC_PP_SHE_110
#define MC_PP_SHE_112(...) MC_PP_SHE_111
#define MC_PP_SHE_113(...) MC_PP_SHE_112
#define MC_PP_SHE_114(...) MC_PP_SHE_113
#define MC_PP_SHE_115(...) MC_PP_SHE_114
#define MC_PP_SHE_116(...) MC_PP_SHE_115
#define MC_PP_SHE_117(...) MC_PP_SHE_116
#define MC_PP_SHE_118(...) MC_PP_SHE_117
#define MC_PP_SHE_119(...) MC_PP_SHE_118
#define MC_PP_SHE_120(...) MC_PP_SHE_119
#define MC_PP_SHE_121(...) MC_PP_SHE_120
#define MC_PP_SHE_122(...) MC_PP_SHE_121
#define MC_PP_SHE_123(...) MC_PP_SHE_122
#define MC_PP_SHE_124(...) MC_PP_SHE_123
#define MC_PP_SHE_125(...) MC_PP_SHE_124
#define MC_PP_SHE_126(...) MC_PP_SHE_125
#define MC_PP_SHE_127(...) MC_PP_SHE_126
#define MC_PP_SHE_128(...) MC_PP_SHE_127

// Size-specific HEAD: extract element, eat the rest
#define MC_PP_SH_1(s) MC_PP_SH_1_I s
#define MC_PP_SH_2(s) MC_PP_SH_2_I s
#define MC_PP_SH_3(s) MC_PP_SH_3_I s
#define MC_PP_SH_4(s) MC_PP_SH_4_I s
#define MC_PP_SH_5(s) MC_PP_SH_5_I s
#define MC_PP_SH_6(s) MC_PP_SH_6_I s
#define MC_PP_SH_7(s) MC_PP_SH_7_I s
#define MC_PP_SH_8(s) MC_PP_SH_8_I s
#define MC_PP_SH_9(s) MC_PP_SH_9_I s
#define MC_PP_SH_10(s) MC_PP_SH_10_I s
#define MC_PP_SH_11(s) MC_PP_SH_11_I s
#define MC_PP_SH_12(s) MC_PP_SH_12_I s
#define MC_PP_SH_13(s) MC_PP_SH_13_I s
#define MC_PP_SH_14(s) MC_PP_SH_14_I s
#define MC_PP_SH_15(s) MC_PP_SH_15_I s
#define MC_PP_SH_16(s) MC_PP_SH_16_I s
#define MC_PP_SH_17(s) MC_PP_SH_17_I s
#define MC_PP_SH_18(s) MC_PP_SH_18_I s
#define MC_PP_SH_19(s) MC_PP_SH_19_I s
#define MC_PP_SH_20(s) MC_PP_SH_20_I s
#define MC_PP_SH_21(s) MC_PP_SH_21_I s
#define MC_PP_SH_22(s) MC_PP_SH_22_I s
#define MC_PP_SH_23(s) MC_PP_SH_23_I s
#define MC_PP_SH_24(s) MC_PP_SH_24_I s
#define MC_PP_SH_25(s) MC_PP_SH_25_I s
#define MC_PP_SH_26(s) MC_PP_SH_26_I s
#define MC_PP_SH_27(s) MC_PP_SH_27_I s
#define MC_PP_SH_28(s) MC_PP_SH_28_I s
#define MC_PP_SH_29(s) MC_PP_SH_29_I s
#define MC_PP_SH_30(s) MC_PP_SH_30_I s
#define MC_PP_SH_31(s) MC_PP_SH_31_I s
#define MC_PP_SH_32(s) MC_PP_SH_32_I s
#define MC_PP_SH_33(s) MC_PP_SH_33_I s
#define MC_PP_SH_34(s) MC_PP_SH_34_I s
#define MC_PP_SH_35(s) MC_PP_SH_35_I s
#define MC_PP_SH_36(s) MC_PP_SH_36_I s
#define MC_PP_SH_37(s) MC_PP_SH_37_I s
#define MC_PP_SH_38(s) MC_PP_SH_38_I s
#define MC_PP_SH_39(s) MC_PP_SH_39_I s
#define MC_PP_SH_40(s) MC_PP_SH_40_I s
#define MC_PP_SH_41(s) MC_PP_SH_41_I s
#define MC_PP_SH_42(s) MC_PP_SH_42_I s
#define MC_PP_SH_43(s) MC_PP_SH_43_I s
#define MC_PP_SH_44(s) MC_PP_SH_44_I s
#define MC_PP_SH_45(s) MC_PP_SH_45_I s
#define MC_PP_SH_46(s) MC_PP_SH_46_I s
#define MC_PP_SH_47(s) MC_PP_SH_47_I s
#define MC_PP_SH_48(s) MC_PP_SH_48_I s
#define MC_PP_SH_49(s) MC_PP_SH_49_I s
#define MC_PP_SH_50(s) MC_PP_SH_50_I s
#define MC_PP_SH_51(s) MC_PP_SH_51_I s
#define MC_PP_SH_52(s) MC_PP_SH_52_I s
#define MC_PP_SH_53(s) MC_PP_SH_53_I s
#define MC_PP_SH_54(s) MC_PP_SH_54_I s
#define MC_PP_SH_55(s) MC_PP_SH_55_I s
#define MC_PP_SH_56(s) MC_PP_SH_56_I s
#define MC_PP_SH_57(s) MC_PP_SH_57_I s
#define MC_PP_SH_58(s) MC_PP_SH_58_I s
#define MC_PP_SH_59(s) MC_PP_SH_59_I s
#define MC_PP_SH_60(s) MC_PP_SH_60_I s
#define MC_PP_SH_61(s) MC_PP_SH_61_I s
#define MC_PP_SH_62(s) MC_PP_SH_62_I s
#define MC_PP_SH_63(s) MC_PP_SH_63_I s
#define MC_PP_SH_64(s) MC_PP_SH_64_I s
#define MC_PP_SH_65(s) MC_PP_SH_65_I s
#define MC_PP_SH_66(s) MC_PP_SH_66_I s
#define MC_PP_SH_67(s) MC_PP_SH_67_I s
#define MC_PP_SH_68(s) MC_PP_SH_68_I s
#define MC_PP_SH_69(s) MC_PP_SH_69_I s
#define MC_PP_SH_70(s) MC_PP_SH_70_I s
#define MC_PP_SH_71(s) MC_PP_SH_71_I s
#define MC_PP_SH_72(s) MC_PP_SH_72_I s
#define MC_PP_SH_73(s) MC_PP_SH_73_I s
#define MC_PP_SH_74(s) MC_PP_SH_74_I s
#define MC_PP_SH_75(s) MC_PP_SH_75_I s
#define MC_PP_SH_76(s) MC_PP_SH_76_I s
#define MC_PP_SH_77(s) MC_PP_SH_77_I s
#define MC_PP_SH_78(s) MC_PP_SH_78_I s
#define MC_PP_SH_79(s) MC_PP_SH_79_I s
#define MC_PP_SH_80(s) MC_PP_SH_80_I s
#define MC_PP_SH_81(s) MC_PP_SH_81_I s
#define MC_PP_SH_82(s) MC_PP_SH_82_I s
#define MC_PP_SH_83(s) MC_PP_SH_83_I s
#define MC_PP_SH_84(s) MC_PP_SH_84_I s
#define MC_PP_SH_85(s) MC_PP_SH_85_I s
#define MC_PP_SH_86(s) MC_PP_SH_86_I s
#define MC_PP_SH_87(s) MC_PP_SH_87_I s
#define MC_PP_SH_88(s) MC_PP_SH_88_I s
#define MC_PP_SH_89(s) MC_PP_SH_89_I s
#define MC_PP_SH_90(s) MC_PP_SH_90_I s
#define MC_PP_SH_91(s) MC_PP_SH_91_I s
#define MC_PP_SH_92(s) MC_PP_SH_92_I s
#define MC_PP_SH_93(s) MC_PP_SH_93_I s
#define MC_PP_SH_94(s) MC_PP_SH_94_I s
#define MC_PP_SH_95(s) MC_PP_SH_95_I s
#define MC_PP_SH_96(s) MC_PP_SH_96_I s
#define MC_PP_SH_97(s) MC_PP_SH_97_I s
#define MC_PP_SH_98(s) MC_PP_SH_98_I s
#define MC_PP_SH_99(s) MC_PP_SH_99_I s
#define MC_PP_SH_100(s) MC_PP_SH_100_I s
#define MC_PP_SH_101(s) MC_PP_SH_101_I s
#define MC_PP_SH_102(s) MC_PP_SH_102_I s
#define MC_PP_SH_103(s) MC_PP_SH_103_I s
#define MC_PP_SH_104(s) MC_PP_SH_104_I s
#define MC_PP_SH_105(s) MC_PP_SH_105_I s
#define MC_PP_SH_106(s) MC_PP_SH_106_I s
#define MC_PP_SH_107(s) MC_PP_SH_107_I s
#define MC_PP_SH_108(s) MC_PP_SH_108_I s
#define MC_PP_SH_109(s) MC_PP_SH_109_I s
#define MC_PP_SH_110(s) MC_PP_SH_110_I s
#define MC_PP_SH_111(s) MC_PP_SH_111_I s
#define MC_PP_SH_112(s) MC_PP_SH_112_I s
#define MC_PP_SH_113(s) MC_PP_SH_113_I s
#define MC_PP_SH_114(s) MC_PP_SH_114_I s
#define MC_PP_SH_115(s) MC_PP_SH_115_I s
#define MC_PP_SH_116(s) MC_PP_SH_116_I s
#define MC_PP_SH_117(s) MC_PP_SH_117_I s
#define MC_PP_SH_118(s) MC_PP_SH_118_I s
#define MC_PP_SH_119(s) MC_PP_SH_119_I s
#define MC_PP_SH_120(s) MC_PP_SH_120_I s
#define MC_PP_SH_121(s) MC_PP_SH_121_I s
#define MC_PP_SH_122(s) MC_PP_SH_122_I s
#define MC_PP_SH_123(s) MC_PP_SH_123_I s
#define MC_PP_SH_124(s) MC_PP_SH_124_I s
#define MC_PP_SH_125(s) MC_PP_SH_125_I s
#define MC_PP_SH_126(s) MC_PP_SH_126_I s
#define MC_PP_SH_127(s) MC_PP_SH_127_I s
#define MC_PP_SH_128(s) MC_PP_SH_128_I s

#define MC_PP_SH_1_I(...)   __VA_ARGS__
#define MC_PP_SH_2_I(a)     a MC_PP_SHE_1
#define MC_PP_SH_3_I(a)     a MC_PP_SHE_2
#define MC_PP_SH_4_I(a)     a MC_PP_SHE_3
#define MC_PP_SH_5_I(a)     a MC_PP_SHE_4
#define MC_PP_SH_6_I(a)     a MC_PP_SHE_5
#define MC_PP_SH_7_I(a)     a MC_PP_SHE_6
#define MC_PP_SH_8_I(a)     a MC_PP_SHE_7
#define MC_PP_SH_9_I(a)     a MC_PP_SHE_8
#define MC_PP_SH_10_I(a)     a MC_PP_SHE_9
#define MC_PP_SH_11_I(a)     a MC_PP_SHE_10
#define MC_PP_SH_12_I(a)     a MC_PP_SHE_11
#define MC_PP_SH_13_I(a)     a MC_PP_SHE_12
#define MC_PP_SH_14_I(a)     a MC_PP_SHE_13
#define MC_PP_SH_15_I(a)     a MC_PP_SHE_14
#define MC_PP_SH_16_I(a)     a MC_PP_SHE_15
#define MC_PP_SH_17_I(a)     a MC_PP_SHE_16
#define MC_PP_SH_18_I(a)     a MC_PP_SHE_17
#define MC_PP_SH_19_I(a)     a MC_PP_SHE_18
#define MC_PP_SH_20_I(a)     a MC_PP_SHE_19
#define MC_PP_SH_21_I(a)     a MC_PP_SHE_20
#define MC_PP_SH_22_I(a)     a MC_PP_SHE_21
#define MC_PP_SH_23_I(a)     a MC_PP_SHE_22
#define MC_PP_SH_24_I(a)     a MC_PP_SHE_23
#define MC_PP_SH_25_I(a)     a MC_PP_SHE_24
#define MC_PP_SH_26_I(a)     a MC_PP_SHE_25
#define MC_PP_SH_27_I(a)     a MC_PP_SHE_26
#define MC_PP_SH_28_I(a)     a MC_PP_SHE_27
#define MC_PP_SH_29_I(a)     a MC_PP_SHE_28
#define MC_PP_SH_30_I(a)     a MC_PP_SHE_29
#define MC_PP_SH_31_I(a)     a MC_PP_SHE_30
#define MC_PP_SH_32_I(a)     a MC_PP_SHE_31
#define MC_PP_SH_33_I(a)     a MC_PP_SHE_32
#define MC_PP_SH_34_I(a)     a MC_PP_SHE_33
#define MC_PP_SH_35_I(a)     a MC_PP_SHE_34
#define MC_PP_SH_36_I(a)     a MC_PP_SHE_35
#define MC_PP_SH_37_I(a)     a MC_PP_SHE_36
#define MC_PP_SH_38_I(a)     a MC_PP_SHE_37
#define MC_PP_SH_39_I(a)     a MC_PP_SHE_38
#define MC_PP_SH_40_I(a)     a MC_PP_SHE_39
#define MC_PP_SH_41_I(a)     a MC_PP_SHE_40
#define MC_PP_SH_42_I(a)     a MC_PP_SHE_41
#define MC_PP_SH_43_I(a)     a MC_PP_SHE_42
#define MC_PP_SH_44_I(a)     a MC_PP_SHE_43
#define MC_PP_SH_45_I(a)     a MC_PP_SHE_44
#define MC_PP_SH_46_I(a)     a MC_PP_SHE_45
#define MC_PP_SH_47_I(a)     a MC_PP_SHE_46
#define MC_PP_SH_48_I(a)     a MC_PP_SHE_47
#define MC_PP_SH_49_I(a)     a MC_PP_SHE_48
#define MC_PP_SH_50_I(a)     a MC_PP_SHE_49
#define MC_PP_SH_51_I(a)     a MC_PP_SHE_50
#define MC_PP_SH_52_I(a)     a MC_PP_SHE_51
#define MC_PP_SH_53_I(a)     a MC_PP_SHE_52
#define MC_PP_SH_54_I(a)     a MC_PP_SHE_53
#define MC_PP_SH_55_I(a)     a MC_PP_SHE_54
#define MC_PP_SH_56_I(a)     a MC_PP_SHE_55
#define MC_PP_SH_57_I(a)     a MC_PP_SHE_56
#define MC_PP_SH_58_I(a)     a MC_PP_SHE_57
#define MC_PP_SH_59_I(a)     a MC_PP_SHE_58
#define MC_PP_SH_60_I(a)     a MC_PP_SHE_59
#define MC_PP_SH_61_I(a)     a MC_PP_SHE_60
#define MC_PP_SH_62_I(a)     a MC_PP_SHE_61
#define MC_PP_SH_63_I(a)     a MC_PP_SHE_62
#define MC_PP_SH_64_I(a)     a MC_PP_SHE_63
#define MC_PP_SH_65_I(a)     a MC_PP_SHE_64
#define MC_PP_SH_66_I(a)     a MC_PP_SHE_65
#define MC_PP_SH_67_I(a)     a MC_PP_SHE_66
#define MC_PP_SH_68_I(a)     a MC_PP_SHE_67
#define MC_PP_SH_69_I(a)     a MC_PP_SHE_68
#define MC_PP_SH_70_I(a)     a MC_PP_SHE_69
#define MC_PP_SH_71_I(a)     a MC_PP_SHE_70
#define MC_PP_SH_72_I(a)     a MC_PP_SHE_71
#define MC_PP_SH_73_I(a)     a MC_PP_SHE_72
#define MC_PP_SH_74_I(a)     a MC_PP_SHE_73
#define MC_PP_SH_75_I(a)     a MC_PP_SHE_74
#define MC_PP_SH_76_I(a)     a MC_PP_SHE_75
#define MC_PP_SH_77_I(a)     a MC_PP_SHE_76
#define MC_PP_SH_78_I(a)     a MC_PP_SHE_77
#define MC_PP_SH_79_I(a)     a MC_PP_SHE_78
#define MC_PP_SH_80_I(a)     a MC_PP_SHE_79
#define MC_PP_SH_81_I(a)     a MC_PP_SHE_80
#define MC_PP_SH_82_I(a)     a MC_PP_SHE_81
#define MC_PP_SH_83_I(a)     a MC_PP_SHE_82
#define MC_PP_SH_84_I(a)     a MC_PP_SHE_83
#define MC_PP_SH_85_I(a)     a MC_PP_SHE_84
#define MC_PP_SH_86_I(a)     a MC_PP_SHE_85
#define MC_PP_SH_87_I(a)     a MC_PP_SHE_86
#define MC_PP_SH_88_I(a)     a MC_PP_SHE_87
#define MC_PP_SH_89_I(a)     a MC_PP_SHE_88
#define MC_PP_SH_90_I(a)     a MC_PP_SHE_89
#define MC_PP_SH_91_I(a)     a MC_PP_SHE_90
#define MC_PP_SH_92_I(a)     a MC_PP_SHE_91
#define MC_PP_SH_93_I(a)     a MC_PP_SHE_92
#define MC_PP_SH_94_I(a)     a MC_PP_SHE_93
#define MC_PP_SH_95_I(a)     a MC_PP_SHE_94
#define MC_PP_SH_96_I(a)     a MC_PP_SHE_95
#define MC_PP_SH_97_I(a)     a MC_PP_SHE_96
#define MC_PP_SH_98_I(a)     a MC_PP_SHE_97
#define MC_PP_SH_99_I(a)     a MC_PP_SHE_98
#define MC_PP_SH_100_I(a)     a MC_PP_SHE_99
#define MC_PP_SH_101_I(a)     a MC_PP_SHE_100
#define MC_PP_SH_102_I(a)     a MC_PP_SHE_101
#define MC_PP_SH_103_I(a)     a MC_PP_SHE_102
#define MC_PP_SH_104_I(a)     a MC_PP_SHE_103
#define MC_PP_SH_105_I(a)     a MC_PP_SHE_104
#define MC_PP_SH_106_I(a)     a MC_PP_SHE_105
#define MC_PP_SH_107_I(a)     a MC_PP_SHE_106
#define MC_PP_SH_108_I(a)     a MC_PP_SHE_107
#define MC_PP_SH_109_I(a)     a MC_PP_SHE_108
#define MC_PP_SH_110_I(a)     a MC_PP_SHE_109
#define MC_PP_SH_111_I(a)     a MC_PP_SHE_110
#define MC_PP_SH_112_I(a)     a MC_PP_SHE_111
#define MC_PP_SH_113_I(a)     a MC_PP_SHE_112
#define MC_PP_SH_114_I(a)     a MC_PP_SHE_113
#define MC_PP_SH_115_I(a)     a MC_PP_SHE_114
#define MC_PP_SH_116_I(a)     a MC_PP_SHE_115
#define MC_PP_SH_117_I(a)     a MC_PP_SHE_116
#define MC_PP_SH_118_I(a)     a MC_PP_SHE_117
#define MC_PP_SH_119_I(a)     a MC_PP_SHE_118
#define MC_PP_SH_120_I(a)     a MC_PP_SHE_119
#define MC_PP_SH_121_I(a)     a MC_PP_SHE_120
#define MC_PP_SH_122_I(a)     a MC_PP_SHE_121
#define MC_PP_SH_123_I(a)     a MC_PP_SHE_122
#define MC_PP_SH_124_I(a)     a MC_PP_SHE_123
#define MC_PP_SH_125_I(a)     a MC_PP_SHE_124
#define MC_PP_SH_126_I(a)     a MC_PP_SHE_125
#define MC_PP_SH_127_I(a)     a MC_PP_SHE_126
#define MC_PP_SH_128_I(a)     a MC_PP_SHE_127

// --- SEQ_TAIL ---
#define MC_PP_SEQ_TAIL(seq)   MC_PP_SEQ_TAIL_I seq
#define MC_PP_SEQ_TAIL_I(...)

// --- SEQ_POP_FRONT ---
#define MC_PP_SEQ_POP_FRONT(seq) MC_PP_SEQ_TAIL(seq)

// --- SEQ_ENUM ---
// MC_PP_SEQ_ENUM((a)(b)(c)) => a, b, c
#define MC_PP_SEQ_ENUM(seq) MC_PP_CAT(MC_PP_SE_, MC_PP_SEQ_SIZE(seq))(seq)

#define MC_PP_SE_0(s)
#define MC_PP_SE_1(s) MC_PP_SEQ_HEAD(s)
#define MC_PP_SE_2(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_1(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_3(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_2(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_4(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_3(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_5(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_4(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_6(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_5(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_7(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_6(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_8(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_7(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_9(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_8(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_10(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_9(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_11(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_10(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_12(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_11(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_13(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_12(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_14(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_13(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_15(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_14(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_16(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_15(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_17(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_16(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_18(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_17(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_19(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_18(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_20(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_19(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_21(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_20(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_22(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_21(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_23(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_22(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_24(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_23(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_25(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_24(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_26(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_25(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_27(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_26(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_28(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_27(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_29(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_28(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_30(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_29(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_31(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_30(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_32(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_31(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_33(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_32(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_34(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_33(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_35(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_34(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_36(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_35(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_37(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_36(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_38(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_37(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_39(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_38(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_40(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_39(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_41(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_40(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_42(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_41(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_43(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_42(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_44(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_43(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_45(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_44(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_46(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_45(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_47(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_46(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_48(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_47(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_49(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_48(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_50(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_49(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_51(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_50(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_52(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_51(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_53(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_52(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_54(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_53(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_55(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_54(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_56(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_55(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_57(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_56(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_58(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_57(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_59(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_58(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_60(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_59(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_61(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_60(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_62(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_61(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_63(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_62(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_64(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_63(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_65(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_64(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_66(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_65(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_67(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_66(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_68(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_67(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_69(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_68(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_70(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_69(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_71(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_70(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_72(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_71(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_73(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_72(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_74(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_73(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_75(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_74(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_76(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_75(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_77(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_76(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_78(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_77(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_79(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_78(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_80(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_79(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_81(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_80(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_82(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_81(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_83(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_82(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_84(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_83(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_85(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_84(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_86(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_85(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_87(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_86(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_88(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_87(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_89(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_88(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_90(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_89(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_91(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_90(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_92(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_91(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_93(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_92(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_94(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_93(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_95(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_94(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_96(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_95(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_97(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_96(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_98(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_97(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_99(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_98(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_100(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_99(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_101(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_100(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_102(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_101(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_103(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_102(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_104(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_103(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_105(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_104(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_106(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_105(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_107(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_106(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_108(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_107(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_109(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_108(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_110(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_109(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_111(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_110(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_112(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_111(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_113(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_112(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_114(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_113(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_115(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_114(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_116(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_115(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_117(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_116(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_118(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_117(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_119(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_118(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_120(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_119(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_121(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_120(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_122(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_121(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_123(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_122(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_124(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_123(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_125(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_124(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_126(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_125(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_127(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_126(MC_PP_SEQ_TAIL(s))
#define MC_PP_SE_128(s) MC_PP_SEQ_HEAD(s), MC_PP_SE_127(MC_PP_SEQ_TAIL(s))

// --- SEQ_FOR_EACH ---
// MC_PP_SEQ_FOR_EACH(macro, data, (a)(b)(c))
//   => macro(r, data, a) macro(r, data, b) macro(r, data, c)
#define MC_PP_SEQ_FOR_EACH(m, d, seq) MC_PP_CAT(MC_PP_SFE_, MC_PP_SEQ_SIZE(seq))(m, d, seq)

#define MC_PP_SFE_0(m, d, s)
#define MC_PP_SFE_1(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s))
#define MC_PP_SFE_2(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_1(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_3(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_2(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_4(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_3(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_5(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_4(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_6(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_5(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_7(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_6(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_8(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_7(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_9(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_8(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_10(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_9(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_11(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_10(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_12(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_11(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_13(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_12(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_14(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_13(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_15(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_14(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_16(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_15(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_17(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_16(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_18(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_17(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_19(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_18(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_20(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_19(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_21(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_20(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_22(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_21(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_23(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_22(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_24(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_23(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_25(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_24(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_26(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_25(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_27(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_26(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_28(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_27(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_29(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_28(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_30(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_29(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_31(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_30(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_32(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_31(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_33(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_32(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_34(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_33(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_35(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_34(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_36(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_35(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_37(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_36(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_38(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_37(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_39(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_38(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_40(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_39(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_41(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_40(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_42(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_41(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_43(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_42(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_44(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_43(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_45(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_44(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_46(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_45(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_47(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_46(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_48(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_47(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_49(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_48(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_50(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_49(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_51(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_50(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_52(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_51(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_53(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_52(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_54(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_53(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_55(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_54(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_56(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_55(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_57(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_56(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_58(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_57(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_59(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_58(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_60(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_59(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_61(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_60(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_62(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_61(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_63(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_62(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_64(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_63(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_65(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_64(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_66(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_65(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_67(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_66(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_68(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_67(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_69(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_68(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_70(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_69(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_71(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_70(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_72(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_71(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_73(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_72(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_74(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_73(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_75(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_74(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_76(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_75(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_77(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_76(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_78(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_77(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_79(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_78(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_80(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_79(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_81(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_80(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_82(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_81(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_83(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_82(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_84(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_83(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_85(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_84(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_86(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_85(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_87(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_86(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_88(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_87(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_89(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_88(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_90(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_89(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_91(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_90(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_92(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_91(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_93(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_92(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_94(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_93(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_95(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_94(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_96(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_95(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_97(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_96(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_98(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_97(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_99(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_98(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_100(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_99(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_101(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_100(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_102(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_101(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_103(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_102(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_104(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_103(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_105(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_104(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_106(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_105(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_107(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_106(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_108(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_107(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_109(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_108(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_110(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_109(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_111(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_110(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_112(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_111(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_113(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_112(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_114(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_113(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_115(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_114(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_116(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_115(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_117(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_116(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_118(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_117(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_119(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_118(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_120(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_119(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_121(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_120(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_122(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_121(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_123(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_122(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_124(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_123(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_125(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_124(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_126(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_125(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_127(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_126(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_SFE_128(m, d, s) m(r, d, MC_PP_SEQ_HEAD(s)) MC_PP_SFE_127(m, d, MC_PP_SEQ_TAIL(s))

// --- SEQ_TRANSFORM ---
// MC_PP_SEQ_TRANSFORM(macro, data, (a)(b)(c))
//   => (macro(r,data,a))(macro(r,data,b))(macro(r,data,c))
#define MC_PP_SEQ_TRANSFORM(m, d, seq) MC_PP_CAT(MC_PP_ST_, MC_PP_SEQ_SIZE(seq))(m, d, seq)

#define MC_PP_ST_0(m, d, s)
#define MC_PP_ST_1(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s)))
#define MC_PP_ST_2(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_1(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_3(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_2(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_4(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_3(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_5(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_4(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_6(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_5(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_7(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_6(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_8(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_7(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_9(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_8(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_10(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_9(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_11(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_10(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_12(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_11(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_13(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_12(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_14(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_13(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_15(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_14(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_16(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_15(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_17(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_16(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_18(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_17(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_19(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_18(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_20(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_19(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_21(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_20(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_22(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_21(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_23(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_22(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_24(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_23(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_25(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_24(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_26(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_25(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_27(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_26(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_28(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_27(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_29(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_28(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_30(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_29(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_31(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_30(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_32(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_31(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_33(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_32(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_34(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_33(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_35(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_34(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_36(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_35(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_37(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_36(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_38(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_37(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_39(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_38(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_40(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_39(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_41(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_40(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_42(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_41(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_43(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_42(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_44(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_43(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_45(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_44(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_46(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_45(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_47(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_46(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_48(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_47(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_49(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_48(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_50(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_49(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_51(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_50(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_52(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_51(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_53(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_52(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_54(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_53(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_55(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_54(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_56(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_55(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_57(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_56(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_58(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_57(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_59(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_58(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_60(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_59(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_61(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_60(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_62(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_61(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_63(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_62(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_64(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_63(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_65(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_64(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_66(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_65(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_67(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_66(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_68(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_67(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_69(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_68(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_70(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_69(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_71(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_70(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_72(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_71(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_73(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_72(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_74(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_73(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_75(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_74(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_76(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_75(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_77(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_76(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_78(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_77(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_79(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_78(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_80(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_79(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_81(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_80(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_82(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_81(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_83(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_82(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_84(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_83(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_85(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_84(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_86(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_85(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_87(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_86(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_88(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_87(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_89(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_88(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_90(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_89(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_91(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_90(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_92(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_91(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_93(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_92(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_94(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_93(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_95(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_94(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_96(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_95(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_97(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_96(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_98(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_97(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_99(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_98(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_100(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_99(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_101(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_100(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_102(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_101(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_103(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_102(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_104(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_103(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_105(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_104(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_106(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_105(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_107(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_106(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_108(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_107(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_109(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_108(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_110(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_109(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_111(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_110(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_112(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_111(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_113(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_112(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_114(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_113(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_115(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_114(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_116(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_115(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_117(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_116(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_118(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_117(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_119(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_118(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_120(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_119(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_121(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_120(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_122(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_121(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_123(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_122(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_124(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_123(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_125(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_124(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_126(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_125(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_127(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_126(m, d, MC_PP_SEQ_TAIL(s))
#define MC_PP_ST_128(m, d, s) (m(r, d, MC_PP_SEQ_HEAD(s))) MC_PP_ST_127(m, d, MC_PP_SEQ_TAIL(s))

#endif // MC_PP_SEQ_H
