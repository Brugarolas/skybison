from .opcodebase import Opcode

opcode: Opcode = Opcode()
opcode.def_op("POP_TOP", 1)
opcode.def_op("ROT_TWO", 2)
opcode.def_op("ROT_THREE", 3)
opcode.def_op("DUP_TOP", 4)
opcode.def_op("DUP_TOP_TWO", 5)
opcode.def_op("NOP", 9)
opcode.def_op("UNARY_POSITIVE", 10)
opcode.def_op("UNARY_NEGATIVE", 11)
opcode.def_op("UNARY_NOT", 12)
opcode.def_op("UNARY_INVERT", 15)
opcode.def_op("BINARY_MATRIX_MULTIPLY", 16)
opcode.def_op("INPLACE_MATRIX_MULTIPLY", 17)
opcode.def_op("BINARY_POWER", 19)
opcode.def_op("BINARY_MULTIPLY", 20)
opcode.def_op("BINARY_MODULO", 22)
opcode.def_op("BINARY_ADD", 23)
opcode.def_op("BINARY_SUBTRACT", 24)
opcode.def_op("BINARY_SUBSCR", 25)
opcode.def_op("BINARY_FLOOR_DIVIDE", 26)
opcode.def_op("BINARY_TRUE_DIVIDE", 27)
opcode.def_op("INPLACE_FLOOR_DIVIDE", 28)
opcode.def_op("INPLACE_TRUE_DIVIDE", 29)
opcode.def_op("GET_AITER", 50)
opcode.def_op("GET_ANEXT", 51)
opcode.def_op("BEFORE_ASYNC_WITH", 52)
opcode.def_op("INPLACE_ADD", 55)
opcode.def_op("INPLACE_SUBTRACT", 56)
opcode.def_op("INPLACE_MULTIPLY", 57)
opcode.def_op("INPLACE_MODULO", 59)
opcode.def_op("STORE_SUBSCR", 60)
opcode.def_op("DELETE_SUBSCR", 61)
opcode.def_op("BINARY_LSHIFT", 62)
opcode.def_op("BINARY_RSHIFT", 63)
opcode.def_op("BINARY_AND", 64)
opcode.def_op("BINARY_XOR", 65)
opcode.def_op("BINARY_OR", 66)
opcode.def_op("INPLACE_POWER", 67)
opcode.def_op("GET_ITER", 68)
opcode.def_op("GET_YIELD_FROM_ITER", 69)
opcode.def_op("PRINT_EXPR", 70)
opcode.def_op("LOAD_BUILD_CLASS", 71)
opcode.def_op("YIELD_FROM", 72)
opcode.def_op("GET_AWAITABLE", 73)
opcode.def_op("INPLACE_LSHIFT", 75)
opcode.def_op("INPLACE_RSHIFT", 76)
opcode.def_op("INPLACE_AND", 77)
opcode.def_op("INPLACE_XOR", 78)
opcode.def_op("INPLACE_OR", 79)
opcode.def_op("BREAK_LOOP", 80)
opcode.def_op("WITH_CLEANUP_START", 81)
opcode.def_op("WITH_CLEANUP_FINISH", 82)
opcode.def_op("RETURN_VALUE", 83)
opcode.def_op("IMPORT_STAR", 84)
opcode.def_op("SETUP_ANNOTATIONS", 85)
opcode.def_op("YIELD_VALUE", 86)
opcode.def_op("POP_BLOCK", 87)
opcode.def_op("END_FINALLY", 88)
opcode.def_op("POP_EXCEPT", 89)
# Opcodes from here have an argument:
opcode.name_op("STORE_NAME", 90)  # Index in name list
opcode.name_op("DELETE_NAME", 91)  # ""
opcode.def_op("UNPACK_SEQUENCE", 92)  # Number of tuple items
opcode.jrel_op("FOR_ITER", 93)
opcode.def_op("UNPACK_EX", 94)
opcode.name_op("STORE_ATTR", 95)  # Index in name list
opcode.name_op("DELETE_ATTR", 96)  # ""
opcode.name_op("STORE_GLOBAL", 97)  # ""
opcode.name_op("DELETE_GLOBAL", 98)  # ""
opcode.def_op("LOAD_CONST", 100)  # Index in const list
opcode.hasconst.add(100)
opcode.name_op("LOAD_NAME", 101)  # Index in name list
opcode.def_op("BUILD_TUPLE", 102)  # Number of tuple items
opcode.def_op("BUILD_LIST", 103)  # Number of list items
opcode.def_op("BUILD_SET", 104)  # Number of set items
opcode.def_op("BUILD_MAP", 105)  # Number of dict entries
opcode.name_op("LOAD_ATTR", 106)  # Index in name list
opcode.def_op("COMPARE_OP", 107)  # Comparison operator
opcode.hascompare.add(107)
opcode.name_op("IMPORT_NAME", 108)  # Index in name list
opcode.name_op("IMPORT_FROM", 109)  # Index in name list
opcode.jrel_op("JUMP_FORWARD", 110)  # Number of bytes to skip
opcode.jabs_op("JUMP_IF_FALSE_OR_POP", 111)  # Target byte offset from beginning of code
opcode.jabs_op("JUMP_IF_TRUE_OR_POP", 112)  # ""
opcode.jabs_op("JUMP_ABSOLUTE", 113)  # ""
opcode.jabs_op("POP_JUMP_IF_FALSE", 114)  # ""
opcode.jabs_op("POP_JUMP_IF_TRUE", 115)  # ""
opcode.name_op("LOAD_GLOBAL", 116)  # Index in name list
opcode.jabs_op("CONTINUE_LOOP", 119)  # Target address
opcode.jrel_op("SETUP_LOOP", 120)  # Distance to target address
opcode.jrel_op("SETUP_EXCEPT", 121)  # ""
opcode.jrel_op("SETUP_FINALLY", 122)  # ""
opcode.def_op("LOAD_FAST", 124)  # Local variable number
opcode.haslocal.add(124)
opcode.def_op("STORE_FAST", 125)  # Local variable number
opcode.haslocal.add(125)
opcode.def_op("DELETE_FAST", 126)  # Local variable number
opcode.haslocal.add(126)
opcode.name_op("STORE_ANNOTATION", 127)  # Index in name list
opcode.def_op("RAISE_VARARGS", 130)  # Number of raise arguments (1, 2, or 3)
opcode.def_op("CALL_FUNCTION", 131)  # #args
opcode.def_op("MAKE_FUNCTION", 132)  # Flags
opcode.def_op("BUILD_SLICE", 133)  # Number of items
opcode.def_op("LOAD_CLOSURE", 135)
opcode.hasfree.add(135)
opcode.def_op("LOAD_DEREF", 136)
opcode.hasfree.add(136)
opcode.def_op("STORE_DEREF", 137)
opcode.hasfree.add(137)
opcode.def_op("DELETE_DEREF", 138)
opcode.hasfree.add(138)
opcode.def_op("CALL_FUNCTION_KW", 141)  # #args + #kwargs
opcode.def_op("CALL_FUNCTION_EX", 142)  # Flags
opcode.jrel_op("SETUP_WITH", 143)
opcode.def_op("LIST_APPEND", 145)
opcode.def_op("SET_ADD", 146)
opcode.def_op("MAP_ADD", 147)
opcode.def_op("LOAD_CLASSDEREF", 148)
opcode.hasfree.add(148)
opcode.def_op("EXTENDED_ARG", 144)
opcode.def_op("BUILD_LIST_UNPACK", 149)
opcode.def_op("BUILD_MAP_UNPACK", 150)
opcode.def_op("BUILD_MAP_UNPACK_WITH_CALL", 151)
opcode.def_op("BUILD_TUPLE_UNPACK", 152)
opcode.def_op("BUILD_SET_UNPACK", 153)
opcode.jrel_op("SETUP_ASYNC_WITH", 154)
opcode.def_op("FORMAT_VALUE", 155)
opcode.def_op("BUILD_CONST_KEY_MAP", 156)
opcode.def_op("BUILD_STRING", 157)
opcode.def_op("BUILD_TUPLE_UNPACK_WITH_CALL", 158)

FVC_MASK = 0x3
FVC_NONE = 0x0
FVC_STR = 0x1
FVC_REPR = 0x2
FVC_ASCII = 0x3
FVS_MASK = 0x4
FVS_HAVE_SPEC = 0x4

opcode.stack_effects.update(
    NOP=0,
    POP_TOP=-1,
    ROT_TWO=0,
    ROT_THREE=0,
    DUP_TOP=1,
    DUP_TOP_TWO=2,
    UNARY_POSITIVE=0,
    UNARY_NEGATIVE=0,
    UNARY_NOT=0,
    UNARY_INVERT=0,
    SET_ADD=-1,
    LIST_APPEND=-1,
    MAP_ADD=-2,
    BINARY_POWER=-1,
    BINARY_MULTIPLY=-1,
    BINARY_MATRIX_MULTIPLY=-1,
    BINARY_MODULO=-1,
    BINARY_ADD=-1,
    BINARY_SUBTRACT=-1,
    BINARY_SUBSCR=-1,
    BINARY_FLOOR_DIVIDE=-1,
    BINARY_TRUE_DIVIDE=-1,
    INPLACE_FLOOR_DIVIDE=-1,
    INPLACE_TRUE_DIVIDE=-1,
    INPLACE_ADD=-1,
    INPLACE_SUBTRACT=-1,
    INPLACE_MULTIPLY=-1,
    INPLACE_MATRIX_MULTIPLY=-1,
    INPLACE_MODULO=-1,
    STORE_SUBSCR=-3,
    DELETE_SUBSCR=-2,
    BINARY_LSHIFT=-1,
    BINARY_RSHIFT=-1,
    BINARY_AND=-1,
    BINARY_XOR=-1,
    BINARY_OR=-1,
    INPLACE_POWER=-1,
    GET_ITER=0,
    PRINT_EXPR=-1,
    LOAD_BUILD_CLASS=1,
    INPLACE_LSHIFT=-1,
    INPLACE_RSHIFT=-1,
    INPLACE_AND=-1,
    INPLACE_XOR=-1,
    INPLACE_OR=-1,
    BREAK_LOOP=0,
    SETUP_WITH=7,
    WITH_CLEANUP_START=1,
    WITH_CLEANUP_FINISH=-1,  # XXX Sometimes more
    RETURN_VALUE=-1,
    IMPORT_STAR=-1,
    SETUP_ANNOTATIONS=0,
    YIELD_VALUE=0,
    YIELD_FROM=-1,
    POP_BLOCK=0,
    POP_EXCEPT=0,  # -3 except if bad bytecode
    END_FINALLY=-1,  # or -2 or -3 if exception occurred
    STORE_NAME=-1,
    DELETE_NAME=0,
    UNPACK_SEQUENCE=lambda oparg, jmp=0: oparg - 1,
    UNPACK_EX=lambda oparg, jmp=0: (oparg & 0xFF) + (oparg >> 8),
    FOR_ITER=1,  # or -1, at end of iterator
    STORE_ATTR=-2,
    DELETE_ATTR=-1,
    STORE_GLOBAL=-1,
    DELETE_GLOBAL=0,
    LOAD_CONST=1,
    LOAD_NAME=1,
    BUILD_TUPLE=lambda oparg, jmp=0: 1 - oparg,
    BUILD_LIST=lambda oparg, jmp=0: 1 - oparg,
    BUILD_SET=lambda oparg, jmp=0: 1 - oparg,
    BUILD_STRING=lambda oparg, jmp=0: 1 - oparg,
    BUILD_LIST_UNPACK=lambda oparg, jmp=0: 1 - oparg,
    BUILD_TUPLE_UNPACK=lambda oparg, jmp=0: 1 - oparg,
    BUILD_TUPLE_UNPACK_WITH_CALL=lambda oparg, jmp=0: 1 - oparg,
    BUILD_SET_UNPACK=lambda oparg, jmp=0: 1 - oparg,
    BUILD_MAP_UNPACK=lambda oparg, jmp=0: 1 - oparg,
    BUILD_MAP_UNPACK_WITH_CALL=lambda oparg, jmp=0: 1 - oparg,
    BUILD_MAP=lambda oparg, jmp=0: 1 - 2 * oparg,
    BUILD_CONST_KEY_MAP=lambda oparg, jmp=0: -oparg,
    LOAD_ATTR=0,
    COMPARE_OP=-1,
    IMPORT_NAME=-1,
    IMPORT_FROM=1,
    JUMP_FORWARD=0,
    JUMP_IF_TRUE_OR_POP=0,  # -1 if jump not taken
    JUMP_IF_FALSE_OR_POP=0,  # ""
    JUMP_ABSOLUTE=0,
    POP_JUMP_IF_FALSE=-1,
    POP_JUMP_IF_TRUE=-1,
    LOAD_GLOBAL=1,
    CONTINUE_LOOP=0,
    SETUP_LOOP=0,
    # close enough...
    SETUP_EXCEPT=6,
    SETUP_FINALLY=6,  # can push 3 values for the new exception
    # + 3 others for the previous exception state
    LOAD_FAST=1,
    STORE_FAST=-1,
    DELETE_FAST=0,
    STORE_ANNOTATION=-1,
    RAISE_VARARGS=lambda oparg, jmp=0: -oparg,
    CALL_FUNCTION=lambda oparg, jmp=0: -oparg,
    CALL_FUNCTION_KW=lambda oparg, jmp=0: -oparg - 1,
    CALL_FUNCTION_EX=lambda oparg, jmp=0: -1 - ((oparg & 0x01) != 0),
    MAKE_FUNCTION=lambda oparg, jmp=0: -1
    - ((oparg & 0x01) != 0)
    - ((oparg & 0x02) != 0)
    - ((oparg & 0x04) != 0)
    - ((oparg & 0x08) != 0),
    BUILD_SLICE=lambda oparg, jmp=0: -2 if oparg == 3 else -1,
    LOAD_CLOSURE=1,
    LOAD_DEREF=1,
    LOAD_CLASSDEREF=1,
    STORE_DEREF=-1,
    DELETE_DEREF=0,
    GET_AWAITABLE=0,
    SETUP_ASYNC_WITH=6,
    BEFORE_ASYNC_WITH=1,
    GET_AITER=0,
    GET_ANEXT=1,
    GET_YIELD_FROM_ITER=0,
    # If there's a fmt_spec on the stack, we go from 2->1,
    # else 1->1.
    FORMAT_VALUE=lambda oparg, jmp=0: -1 if (oparg & FVS_MASK) == FVS_HAVE_SPEC else 0,
    SET_LINENO=0,
    EXTENDED_ARG=0,
)
