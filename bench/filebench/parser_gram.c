/* A Bison parser, made by GNU Bison 3.5.1.  */

/* Bison implementation for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015, 2018-2020 Free Software Foundation,
   Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* C LALR(1) parser skeleton written by Richard Stallman, by
   simplifying the original so-called "semantic" parser.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Undocumented macros, especially those whose name start with YY_,
   are private implementation details.  Do not rely on them.  */

/* Identify Bison output.  */
#define YYBISON 1

/* Bison version.  */
#define YYBISON_VERSION "3.5.1"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 0

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1




/* First part of user prologue.  */
#line 29 "parser_gram.y"


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <float.h>
#include <limits.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <locale.h>
#include <sys/utsname.h>
#include <sys/statvfs.h>
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include "parsertypes.h"
#include "filebench.h"
#include "utils.h"
#include "stats.h"
#include "vars.h"
#include "eventgen.h"
#include "aslr.h"
#include "multi_client_sync.h"

#include "mlfs/mlfs_interface.h"

/* yacc and lex externals */
extern FILE *yyin;
extern int yydebug;
extern void yyerror(char *s);
extern int yylex(void);

/* executable name to execute worker processes later */
char *execname;

/* utilities */
static cmd_t *alloc_cmd(void);
static attr_t *alloc_attr(void);
static attr_t *alloc_lvar_attr(var_t *var);
static attr_t *get_attr(cmd_t *cmd, int64_t name);
static void get_attr_lvars(cmd_t *cmd, flowop_t *flowop);
static list_t *alloc_list();
static probtabent_t *alloc_probtabent(void);
static void add_lvar_to_list(var_t *newlvar, var_t **lvar_list);

/* Info Commands */
static void parser_fileset_list(cmd_t *);
static void parser_flowop_list(cmd_t *);

/* Define Commands */
static void parser_proc_define(cmd_t *);
static void parser_thread_define(cmd_t *, procflow_t *);
static void parser_flowop_define(cmd_t *, threadflow_t *, flowop_t **, int);
static void parser_composite_flowop_define(cmd_t *);
static void parser_file_define(cmd_t *);
static void parser_fileset_define(cmd_t *);
static void parser_var_assign_random(char *, cmd_t *);
static void parser_var_assign_custom(char *, cmd_t *);

/* Create Commands */
static void parser_fileset_create(cmd_t *);

/* Digest Commands */
static void parser_digest(cmd_t *);

/* Run Commands */
static void parser_run(cmd_t *cmd);
static void parser_run_variable(cmd_t *cmd);
static void parser_psrun(cmd_t *cmd);

/* Shutdown (Quit) Commands */
static void parser_filebench_shutdown(cmd_t *cmd);

/* Other Commands */
static void parser_echo(cmd_t *cmd);
static void parser_system(cmd_t *cmd);
static void parser_eventgen(cmd_t *cmd);
static void parser_enable_mc(cmd_t *cmd);
static void parser_domultisync(cmd_t *cmd);
static void parser_sleep(cmd_t *cmd);
static void parser_sleep_variable(cmd_t *cmd);
static void parser_version(cmd_t *cmd);
static void parser_enable_lathist(cmd_t *cmd);

int mlfs = 0;
int nic_offload = 0;


#line 163 "parser_gram.c"

# ifndef YY_CAST
#  ifdef __cplusplus
#   define YY_CAST(Type, Val) static_cast<Type> (Val)
#   define YY_REINTERPRET_CAST(Type, Val) reinterpret_cast<Type> (Val)
#  else
#   define YY_CAST(Type, Val) ((Type) (Val))
#   define YY_REINTERPRET_CAST(Type, Val) ((Type) (Val))
#  endif
# endif
# ifndef YY_NULLPTR
#  if defined __cplusplus
#   if 201103L <= __cplusplus
#    define YY_NULLPTR nullptr
#   else
#    define YY_NULLPTR 0
#   endif
#  else
#   define YY_NULLPTR ((void*)0)
#  endif
# endif

/* Enabling verbose error messages.  */
#ifdef YYERROR_VERBOSE
# undef YYERROR_VERBOSE
# define YYERROR_VERBOSE 1
#else
# define YYERROR_VERBOSE 0
#endif

/* Use api.header.include to #include this header
   instead of duplicating it here.  */
#ifndef YY_YY_PARSER_GRAM_H_INCLUDED
# define YY_YY_PARSER_GRAM_H_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif
#if YYDEBUG
extern int yydebug;
#endif

/* Token type.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
  enum yytokentype
  {
    FSC_LIST = 258,
    FSC_DEFINE = 259,
    FSC_QUIT = 260,
    FSC_DEBUG = 261,
    FSC_CREATE = 262,
    FSC_SLEEP = 263,
    FSC_SET = 264,
    FSC_DIGEST = 265,
    FSC_SYSTEM = 266,
    FSC_EVENTGEN = 267,
    FSC_ECHO = 268,
    FSC_RUN = 269,
    FSC_PSRUN = 270,
    FSC_VERSION = 271,
    FSC_ENABLE = 272,
    FSC_DOMULTISYNC = 273,
    FSV_STRING = 274,
    FSV_VAL_POSINT = 275,
    FSV_VAL_NEGINT = 276,
    FSV_VAL_BOOLEAN = 277,
    FSV_VARIABLE = 278,
    FSV_WHITESTRING = 279,
    FSV_RANDUNI = 280,
    FSV_RANDTAB = 281,
    FSV_URAND = 282,
    FSV_RAND48 = 283,
    FSE_FILE = 284,
    FSE_FILES = 285,
    FSE_FILESET = 286,
    FSE_PROC = 287,
    FSE_THREAD = 288,
    FSE_FLOWOP = 289,
    FSE_CVAR = 290,
    FSE_RAND = 291,
    FSE_MODE = 292,
    FSE_MULTI = 293,
    FSK_SEPLST = 294,
    FSK_OPENLST = 295,
    FSK_CLOSELST = 296,
    FSK_OPENPAR = 297,
    FSK_CLOSEPAR = 298,
    FSK_ASSIGN = 299,
    FSK_IN = 300,
    FSK_QUOTE = 301,
    FSA_SIZE = 302,
    FSA_PREALLOC = 303,
    FSA_PARALLOC = 304,
    FSA_PATH = 305,
    FSA_REUSE = 306,
    FSA_MEMSIZE = 307,
    FSA_RATE = 308,
    FSA_READONLY = 309,
    FSA_TRUSTTREE = 310,
    FSA_IOSIZE = 311,
    FSA_FILENAME = 312,
    FSA_WSS = 313,
    FSA_NAME = 314,
    FSA_RANDOM = 315,
    FSA_INSTANCES = 316,
    FSA_DSYNC = 317,
    FSA_TARGET = 318,
    FSA_ITERS = 319,
    FSA_NICE = 320,
    FSA_VALUE = 321,
    FSA_BLOCKING = 322,
    FSA_HIGHWATER = 323,
    FSA_DIRECTIO = 324,
    FSA_DIRWIDTH = 325,
    FSA_FD = 326,
    FSA_SRCFD = 327,
    FSA_ROTATEFD = 328,
    FSA_ENTRIES = 329,
    FSA_DIRDEPTHRV = 330,
    FSA_DIRGAMMA = 331,
    FSA_USEISM = 332,
    FSA_TYPE = 333,
    FSA_LEAFDIRS = 334,
    FSA_INDEXED = 335,
    FSA_RANDTABLE = 336,
    FSA_RANDSRC = 337,
    FSA_ROUND = 338,
    FSA_RANDSEED = 339,
    FSA_RANDGAMMA = 340,
    FSA_RANDMEAN = 341,
    FSA_MIN = 342,
    FSA_MAX = 343,
    FSA_MASTER = 344,
    FSA_CLIENT = 345,
    FSS_TYPE = 346,
    FSS_SEED = 347,
    FSS_GAMMA = 348,
    FSS_MEAN = 349,
    FSS_MIN = 350,
    FSS_SRC = 351,
    FSS_ROUND = 352,
    FSA_LVAR_ASSIGN = 353,
    FSA_ALLDONE = 354,
    FSA_FIRSTDONE = 355,
    FSA_TIMEOUT = 356,
    FSA_LATHIST = 357,
    FSA_NOREADAHEAD = 358,
    FSA_IOPRIO = 359,
    FSA_WRITEONLY = 360,
    FSA_PARAMETERS = 361,
    FSA_NOUSESTATS = 362,
    FSA_MLFS_DEV_ID = 363
  };
#endif
/* Tokens.  */
#define FSC_LIST 258
#define FSC_DEFINE 259
#define FSC_QUIT 260
#define FSC_DEBUG 261
#define FSC_CREATE 262
#define FSC_SLEEP 263
#define FSC_SET 264
#define FSC_DIGEST 265
#define FSC_SYSTEM 266
#define FSC_EVENTGEN 267
#define FSC_ECHO 268
#define FSC_RUN 269
#define FSC_PSRUN 270
#define FSC_VERSION 271
#define FSC_ENABLE 272
#define FSC_DOMULTISYNC 273
#define FSV_STRING 274
#define FSV_VAL_POSINT 275
#define FSV_VAL_NEGINT 276
#define FSV_VAL_BOOLEAN 277
#define FSV_VARIABLE 278
#define FSV_WHITESTRING 279
#define FSV_RANDUNI 280
#define FSV_RANDTAB 281
#define FSV_URAND 282
#define FSV_RAND48 283
#define FSE_FILE 284
#define FSE_FILES 285
#define FSE_FILESET 286
#define FSE_PROC 287
#define FSE_THREAD 288
#define FSE_FLOWOP 289
#define FSE_CVAR 290
#define FSE_RAND 291
#define FSE_MODE 292
#define FSE_MULTI 293
#define FSK_SEPLST 294
#define FSK_OPENLST 295
#define FSK_CLOSELST 296
#define FSK_OPENPAR 297
#define FSK_CLOSEPAR 298
#define FSK_ASSIGN 299
#define FSK_IN 300
#define FSK_QUOTE 301
#define FSA_SIZE 302
#define FSA_PREALLOC 303
#define FSA_PARALLOC 304
#define FSA_PATH 305
#define FSA_REUSE 306
#define FSA_MEMSIZE 307
#define FSA_RATE 308
#define FSA_READONLY 309
#define FSA_TRUSTTREE 310
#define FSA_IOSIZE 311
#define FSA_FILENAME 312
#define FSA_WSS 313
#define FSA_NAME 314
#define FSA_RANDOM 315
#define FSA_INSTANCES 316
#define FSA_DSYNC 317
#define FSA_TARGET 318
#define FSA_ITERS 319
#define FSA_NICE 320
#define FSA_VALUE 321
#define FSA_BLOCKING 322
#define FSA_HIGHWATER 323
#define FSA_DIRECTIO 324
#define FSA_DIRWIDTH 325
#define FSA_FD 326
#define FSA_SRCFD 327
#define FSA_ROTATEFD 328
#define FSA_ENTRIES 329
#define FSA_DIRDEPTHRV 330
#define FSA_DIRGAMMA 331
#define FSA_USEISM 332
#define FSA_TYPE 333
#define FSA_LEAFDIRS 334
#define FSA_INDEXED 335
#define FSA_RANDTABLE 336
#define FSA_RANDSRC 337
#define FSA_ROUND 338
#define FSA_RANDSEED 339
#define FSA_RANDGAMMA 340
#define FSA_RANDMEAN 341
#define FSA_MIN 342
#define FSA_MAX 343
#define FSA_MASTER 344
#define FSA_CLIENT 345
#define FSS_TYPE 346
#define FSS_SEED 347
#define FSS_GAMMA 348
#define FSS_MEAN 349
#define FSS_MIN 350
#define FSS_SRC 351
#define FSS_ROUND 352
#define FSA_LVAR_ASSIGN 353
#define FSA_ALLDONE 354
#define FSA_FIRSTDONE 355
#define FSA_TIMEOUT 356
#define FSA_LATHIST 357
#define FSA_NOREADAHEAD 358
#define FSA_IOPRIO 359
#define FSA_WRITEONLY 360
#define FSA_PARAMETERS 361
#define FSA_NOUSESTATS 362
#define FSA_MLFS_DEV_ID 363

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
union YYSTYPE
{
#line 122 "parser_gram.y"

	int64_t		 ival;
	unsigned char	 bval;
	char *		 sval;
	avd_t		 avd;
	cmd_t		*cmd;
	attr_t		*attr;
	list_t		*list;
	probtabent_t	*rndtb;

#line 442 "parser_gram.c"

};
typedef union YYSTYPE YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif


extern YYSTYPE yylval;

int yyparse (void);

#endif /* !YY_YY_PARSER_GRAM_H_INCLUDED  */



#ifdef short
# undef short
#endif

/* On compilers that do not define __PTRDIFF_MAX__ etc., make sure
   <limits.h> and (if available) <stdint.h> are included
   so that the code can choose integer types of a good width.  */

#ifndef __PTRDIFF_MAX__
# include <limits.h> /* INFRINGES ON USER NAME SPACE */
# if defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stdint.h> /* INFRINGES ON USER NAME SPACE */
#  define YY_STDINT_H
# endif
#endif

/* Narrow types that promote to a signed type and that can represent a
   signed or unsigned integer of at least N bits.  In tables they can
   save space and decrease cache pressure.  Promoting to a signed type
   helps avoid bugs in integer arithmetic.  */

#ifdef __INT_LEAST8_MAX__
typedef __INT_LEAST8_TYPE__ yytype_int8;
#elif defined YY_STDINT_H
typedef int_least8_t yytype_int8;
#else
typedef signed char yytype_int8;
#endif

#ifdef __INT_LEAST16_MAX__
typedef __INT_LEAST16_TYPE__ yytype_int16;
#elif defined YY_STDINT_H
typedef int_least16_t yytype_int16;
#else
typedef short yytype_int16;
#endif

#if defined __UINT_LEAST8_MAX__ && __UINT_LEAST8_MAX__ <= __INT_MAX__
typedef __UINT_LEAST8_TYPE__ yytype_uint8;
#elif (!defined __UINT_LEAST8_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST8_MAX <= INT_MAX)
typedef uint_least8_t yytype_uint8;
#elif !defined __UINT_LEAST8_MAX__ && UCHAR_MAX <= INT_MAX
typedef unsigned char yytype_uint8;
#else
typedef short yytype_uint8;
#endif

#if defined __UINT_LEAST16_MAX__ && __UINT_LEAST16_MAX__ <= __INT_MAX__
typedef __UINT_LEAST16_TYPE__ yytype_uint16;
#elif (!defined __UINT_LEAST16_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST16_MAX <= INT_MAX)
typedef uint_least16_t yytype_uint16;
#elif !defined __UINT_LEAST16_MAX__ && USHRT_MAX <= INT_MAX
typedef unsigned short yytype_uint16;
#else
typedef int yytype_uint16;
#endif

#ifndef YYPTRDIFF_T
# if defined __PTRDIFF_TYPE__ && defined __PTRDIFF_MAX__
#  define YYPTRDIFF_T __PTRDIFF_TYPE__
#  define YYPTRDIFF_MAXIMUM __PTRDIFF_MAX__
# elif defined PTRDIFF_MAX
#  ifndef ptrdiff_t
#   include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  endif
#  define YYPTRDIFF_T ptrdiff_t
#  define YYPTRDIFF_MAXIMUM PTRDIFF_MAX
# else
#  define YYPTRDIFF_T long
#  define YYPTRDIFF_MAXIMUM LONG_MAX
# endif
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned
# endif
#endif

#define YYSIZE_MAXIMUM                                  \
  YY_CAST (YYPTRDIFF_T,                                 \
           (YYPTRDIFF_MAXIMUM < YY_CAST (YYSIZE_T, -1)  \
            ? YYPTRDIFF_MAXIMUM                         \
            : YY_CAST (YYSIZE_T, -1)))

#define YYSIZEOF(X) YY_CAST (YYPTRDIFF_T, sizeof (X))

/* Stored state numbers (used for stacks). */
typedef yytype_int16 yy_state_t;

/* State numbers in computations.  */
typedef int yy_state_fast_t;

#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(Msgid) dgettext ("bison-runtime", Msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(Msgid) Msgid
# endif
#endif

#ifndef YY_ATTRIBUTE_PURE
# if defined __GNUC__ && 2 < __GNUC__ + (96 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_PURE __attribute__ ((__pure__))
# else
#  define YY_ATTRIBUTE_PURE
# endif
#endif

#ifndef YY_ATTRIBUTE_UNUSED
# if defined __GNUC__ && 2 < __GNUC__ + (7 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_UNUSED __attribute__ ((__unused__))
# else
#  define YY_ATTRIBUTE_UNUSED
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YYUSE(E) ((void) (E))
#else
# define YYUSE(E) /* empty */
#endif

#if defined __GNUC__ && ! defined __ICC && 407 <= __GNUC__ * 100 + __GNUC_MINOR__
/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                            \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")              \
    _Pragma ("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
# define YY_IGNORE_MAYBE_UNINITIALIZED_END      \
    _Pragma ("GCC diagnostic pop")
#else
# define YY_INITIAL_VALUE(Value) Value
#endif
#ifndef YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_END
#endif
#ifndef YY_INITIAL_VALUE
# define YY_INITIAL_VALUE(Value) /* Nothing. */
#endif

#if defined __cplusplus && defined __GNUC__ && ! defined __ICC && 6 <= __GNUC__
# define YY_IGNORE_USELESS_CAST_BEGIN                          \
    _Pragma ("GCC diagnostic push")                            \
    _Pragma ("GCC diagnostic ignored \"-Wuseless-cast\"")
# define YY_IGNORE_USELESS_CAST_END            \
    _Pragma ("GCC diagnostic pop")
#endif
#ifndef YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_END
#endif


#define YY_ASSERT(E) ((void) (0 && (E)))

#if ! defined yyoverflow || YYERROR_VERBOSE

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   elif defined __BUILTIN_VA_ARG_INCR
#    include <alloca.h> /* INFRINGES ON USER NAME SPACE */
#   elif defined _AIX
#    define YYSTACK_ALLOC __alloca
#   elif defined _MSC_VER
#    include <malloc.h> /* INFRINGES ON USER NAME SPACE */
#    define alloca _alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if ! defined _ALLOCA_H && ! defined EXIT_SUCCESS
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
      /* Use EXIT_SUCCESS as a witness for stdlib.h.  */
#     ifndef EXIT_SUCCESS
#      define EXIT_SUCCESS 0
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's 'empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (0)
#  ifndef YYSTACK_ALLOC_MAXIMUM
    /* The OS might guarantee only one guard page at the bottom of the stack,
       and a page size can be as small as 4096 bytes.  So we cannot safely
       invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
       to allow for a few compiler-allocated temporary stack slots.  */
#   define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2006 */
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#  endif
#  if (defined __cplusplus && ! defined EXIT_SUCCESS \
       && ! ((defined YYMALLOC || defined malloc) \
             && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef EXIT_SUCCESS
#    define EXIT_SUCCESS 0
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined EXIT_SUCCESS
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined EXIT_SUCCESS
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* ! defined yyoverflow || YYERROR_VERBOSE */


#if (! defined yyoverflow \
     && (! defined __cplusplus \
         || (defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yy_state_t yyss_alloc;
  YYSTYPE yyvs_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (YYSIZEOF (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (YYSIZEOF (yy_state_t) + YYSIZEOF (YYSTYPE)) \
      + YYSTACK_GAP_MAXIMUM)

# define YYCOPY_NEEDED 1

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack_alloc, Stack)                           \
    do                                                                  \
      {                                                                 \
        YYPTRDIFF_T yynewbytes;                                         \
        YYCOPY (&yyptr->Stack_alloc, Stack, yysize);                    \
        Stack = &yyptr->Stack_alloc;                                    \
        yynewbytes = yystacksize * YYSIZEOF (*Stack) + YYSTACK_GAP_MAXIMUM; \
        yyptr += yynewbytes / YYSIZEOF (*yyptr);                        \
      }                                                                 \
    while (0)

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
/* Copy COUNT objects from SRC to DST.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(Dst, Src, Count) \
      __builtin_memcpy (Dst, Src, YY_CAST (YYSIZE_T, (Count)) * sizeof (*(Src)))
#  else
#   define YYCOPY(Dst, Src, Count)              \
      do                                        \
        {                                       \
          YYPTRDIFF_T yyi;                      \
          for (yyi = 0; yyi < (Count); yyi++)   \
            (Dst)[yyi] = (Src)[yyi];            \
        }                                       \
      while (0)
#  endif
# endif
#endif /* !YYCOPY_NEEDED */

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  2
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   258

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  109
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  72
/* YYNRULES -- Number of rules.  */
#define YYNRULES  212
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  298

#define YYUNDEFTOK  2
#define YYMAXUTOK   363


/* YYTRANSLATE(TOKEN-NUM) -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex, with out-of-bounds checking.  */
#define YYTRANSLATE(YYX)                                                \
  (0 <= (YYX) && (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex.  */
static const yytype_int8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,    59,    60,    61,    62,    63,    64,
      65,    66,    67,    68,    69,    70,    71,    72,    73,    74,
      75,    76,    77,    78,    79,    80,    81,    82,    83,    84,
      85,    86,    87,    88,    89,    90,    91,    92,    93,    94,
      95,    96,    97,    98,    99,   100,   101,   102,   103,   104,
     105,   106,   107,   108
};

#if YYDEBUG
  /* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_int16 yyrline[] =
{
       0,   200,   200,   207,   211,   214,   215,   216,   217,   218,
     219,   220,   221,   222,   223,   224,   225,   226,   227,   228,
     229,   230,   231,   233,   239,   244,   253,   262,   269,   278,
     286,   295,   302,   310,   328,   345,   363,   380,   383,   388,
     394,   401,   416,   416,   416,   416,   418,   429,   439,   449,
     460,   472,   484,   494,   504,   514,   526,   533,   536,   554,
     566,   569,   587,   597,   605,   615,   624,   632,   639,   647,
     654,   661,   669,   677,   686,   695,   703,   712,   718,   723,
     725,   729,   742,   746,   759,   764,   774,   779,   790,   794,
     807,   826,   831,   838,   843,   849,   859,   863,   877,   881,
     895,   900,   910,   914,   928,   933,   942,   946,   959,   973,
     978,   987,   991,  1005,  1010,  1019,  1023,  1037,  1043,  1053,
    1054,  1055,  1056,  1059,  1060,  1061,  1062,  1063,  1064,  1065,
    1066,  1067,  1070,  1071,  1072,  1073,  1074,  1075,  1076,  1077,
    1078,  1079,  1080,  1081,  1082,  1083,  1086,  1087,  1088,  1089,
    1090,  1091,  1093,  1101,  1102,  1103,  1105,  1113,  1114,  1117,
    1118,  1119,  1120,  1121,  1124,  1125,  1126,  1127,  1128,  1131,
    1132,  1133,  1134,  1135,  1136,  1137,  1138,  1139,  1140,  1141,
    1142,  1143,  1144,  1145,  1146,  1147,  1150,  1153,  1154,  1156,
    1160,  1173,  1187,  1193,  1198,  1203,  1208,  1213,  1218,  1224,
    1232,  1238,  1242,  1256,  1263,  1264,  1266,  1272,  1277,  1282,
    1287,  1294,  1297
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || 0
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "FSC_LIST", "FSC_DEFINE", "FSC_QUIT",
  "FSC_DEBUG", "FSC_CREATE", "FSC_SLEEP", "FSC_SET", "FSC_DIGEST",
  "FSC_SYSTEM", "FSC_EVENTGEN", "FSC_ECHO", "FSC_RUN", "FSC_PSRUN",
  "FSC_VERSION", "FSC_ENABLE", "FSC_DOMULTISYNC", "FSV_STRING",
  "FSV_VAL_POSINT", "FSV_VAL_NEGINT", "FSV_VAL_BOOLEAN", "FSV_VARIABLE",
  "FSV_WHITESTRING", "FSV_RANDUNI", "FSV_RANDTAB", "FSV_URAND",
  "FSV_RAND48", "FSE_FILE", "FSE_FILES", "FSE_FILESET", "FSE_PROC",
  "FSE_THREAD", "FSE_FLOWOP", "FSE_CVAR", "FSE_RAND", "FSE_MODE",
  "FSE_MULTI", "FSK_SEPLST", "FSK_OPENLST", "FSK_CLOSELST", "FSK_OPENPAR",
  "FSK_CLOSEPAR", "FSK_ASSIGN", "FSK_IN", "FSK_QUOTE", "FSA_SIZE",
  "FSA_PREALLOC", "FSA_PARALLOC", "FSA_PATH", "FSA_REUSE", "FSA_MEMSIZE",
  "FSA_RATE", "FSA_READONLY", "FSA_TRUSTTREE", "FSA_IOSIZE",
  "FSA_FILENAME", "FSA_WSS", "FSA_NAME", "FSA_RANDOM", "FSA_INSTANCES",
  "FSA_DSYNC", "FSA_TARGET", "FSA_ITERS", "FSA_NICE", "FSA_VALUE",
  "FSA_BLOCKING", "FSA_HIGHWATER", "FSA_DIRECTIO", "FSA_DIRWIDTH",
  "FSA_FD", "FSA_SRCFD", "FSA_ROTATEFD", "FSA_ENTRIES", "FSA_DIRDEPTHRV",
  "FSA_DIRGAMMA", "FSA_USEISM", "FSA_TYPE", "FSA_LEAFDIRS", "FSA_INDEXED",
  "FSA_RANDTABLE", "FSA_RANDSRC", "FSA_ROUND", "FSA_RANDSEED",
  "FSA_RANDGAMMA", "FSA_RANDMEAN", "FSA_MIN", "FSA_MAX", "FSA_MASTER",
  "FSA_CLIENT", "FSS_TYPE", "FSS_SEED", "FSS_GAMMA", "FSS_MEAN", "FSS_MIN",
  "FSS_SRC", "FSS_ROUND", "FSA_LVAR_ASSIGN", "FSA_ALLDONE",
  "FSA_FIRSTDONE", "FSA_TIMEOUT", "FSA_LATHIST", "FSA_NOREADAHEAD",
  "FSA_IOPRIO", "FSA_WRITEONLY", "FSA_PARAMETERS", "FSA_NOUSESTATS",
  "FSA_MLFS_DEV_ID", "$accept", "commands", "command", "eventgen_command",
  "system_command", "echo_command", "version_command", "enable_command",
  "multisync_command", "whitevar_string", "whitevar_string_list",
  "list_command", "debug_command", "set_command", "set_variable",
  "set_random_variable", "set_custom_variable", "set_mode", "quit_command",
  "flowop_list", "thread", "thread_list", "proc_define_command",
  "files_define_command", "digest_define_command", "create_command",
  "sleep_command", "run_command", "psrun_command", "flowop_command",
  "name", "file_attr_ops", "fileset_attr_ops", "file_attr_op",
  "fileset_attr_op", "randvar_attr_ops", "randvar_attr_op", "probtabentry",
  "probtabentry_list", "p_attr_ops", "p_attr_op", "t_attr_ops",
  "t_attr_op", "fo_attr_ops", "fo_attr_op", "ev_attr_ops", "ev_attr_op",
  "enable_multi_ops", "enable_multi_op", "multisync_op",
  "attrs_define_proc", "attrs_define_file", "attrs_define_fileset",
  "randvar_attr_name", "randvar_attr_typop", "randtype_name",
  "randvar_attr_srcop", "randsrc_name", "cvar_attr_name",
  "attrs_define_thread", "attrs_flowop", "attrs_eventgen", "em_attr_name",
  "comp_attr_ops", "comp_attr_op", "comp_lvar_def",
  "flowop_define_command", "cvar_attr_ops", "cvar_attr_op",
  "attrs_define_comp", "attr_value", "var_int_val", YY_NULLPTR
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[NUM] -- (External) token number corresponding to the
   (internal) symbol number NUM (which must be that of a token).  */
static const yytype_int16 yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274,
     275,   276,   277,   278,   279,   280,   281,   282,   283,   284,
     285,   286,   287,   288,   289,   290,   291,   292,   293,   294,
     295,   296,   297,   298,   299,   300,   301,   302,   303,   304,
     305,   306,   307,   308,   309,   310,   311,   312,   313,   314,
     315,   316,   317,   318,   319,   320,   321,   322,   323,   324,
     325,   326,   327,   328,   329,   330,   331,   332,   333,   334,
     335,   336,   337,   338,   339,   340,   341,   342,   343,   344,
     345,   346,   347,   348,   349,   350,   351,   352,   353,   354,
     355,   356,   357,   358,   359,   360,   361,   362,   363
};
# endif

#define YYPACT_NINF (-175)

#define yypact_value_is_default(Yyn) \
  ((Yyn) == YYPACT_NINF)

#define YYTABLE_NINF (-1)

#define yytable_value_is_error(Yyn) \
  0

  /* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
     STATE-NUM.  */
static const yytype_int16 yypact[] =
{
    -175,   180,  -175,  -175,    88,   143,  -175,    11,   -11,   137,
     -10,    13,  -175,    13,   141,    63,  -175,   -31,    -2,  -175,
      71,  -175,  -175,  -175,  -175,  -175,  -175,  -175,  -175,  -175,
    -175,  -175,  -175,  -175,  -175,  -175,  -175,  -175,  -175,  -175,
    -175,   -27,  -175,  -175,  -175,   -25,    18,   -47,   -27,  -175,
    -175,  -175,  -175,    87,    -4,   146,    12,    66,    66,  -175,
    -175,   115,   151,   110,  -175,   135,  -175,  -175,   168,  -175,
     167,  -175,  -175,   171,  -175,   169,  -175,  -175,  -175,  -175,
    -175,  -175,  -175,  -175,  -175,   173,  -175,   170,  -175,  -175,
    -175,  -175,  -175,  -175,  -175,  -175,  -175,  -175,  -175,  -175,
    -175,  -175,   176,  -175,   172,  -175,  -175,  -175,  -175,   162,
    -175,   174,   164,   132,    -1,  -175,  -175,  -175,  -175,  -175,
    -175,  -175,  -175,  -175,  -175,  -175,  -175,  -175,   178,  -175,
     175,    56,    71,    56,    -7,    56,   -25,    56,    18,    56,
     -47,   157,    56,   186,  -175,  -175,  -175,   134,   179,   198,
    -175,  -175,  -175,   110,    56,  -175,  -175,  -175,  -175,   199,
    -175,  -175,  -175,   182,  -175,  -175,  -175,  -175,  -175,  -175,
    -175,  -175,   -44,  -175,   -13,  -175,   205,    22,    70,     8,
      62,   183,  -175,  -175,   184,   136,  -175,  -175,  -175,  -175,
    -175,   166,  -175,   187,  -175,  -175,  -175,  -175,  -175,    70,
    -175,  -175,  -175,  -175,  -175,  -175,  -175,  -175,  -175,  -175,
    -175,  -175,  -175,  -175,  -175,  -175,  -175,   188,  -175,   189,
    -175,  -175,  -175,  -175,  -175,   190,     4,  -175,  -175,   191,
     192,  -175,  -175,  -175,  -175,  -175,    77,  -175,   193,  -175,
    -175,  -175,  -175,  -175,  -175,   201,   -44,   186,    56,   -18,
      56,    56,     8,  -175,   -15,   181,    23,  -175,    56,   194,
    -175,    40,  -175,  -175,  -175,  -175,  -175,  -175,  -175,  -175,
    -175,  -175,  -175,  -175,  -175,  -175,  -175,   195,  -175,  -175,
    -175,  -175,   202,   203,   142,  -175,    72,  -175,  -175,   206,
     203,  -175,   142,  -175,   207,   142,   197,  -175
};

  /* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
     Performed when YYTABLE does not specify something else to do.  Zero
     means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       4,     0,     1,     3,     0,     0,    56,     0,     0,     0,
       0,     0,    23,     0,    71,    72,    27,     0,     0,     2,
      10,    19,    12,    20,    21,    22,    13,     9,    16,    42,
      43,    44,    45,    17,     5,     7,     6,    11,    18,    14,
      15,     8,    39,    40,    65,     0,     0,     0,     0,    41,
      66,    67,    68,     0,     0,     0,     0,    25,    26,    69,
      70,    74,    73,     0,    29,     0,    30,   186,    24,   111,
     114,   204,   205,   200,   189,     0,   125,   126,   127,   124,
     128,   130,   129,   123,   131,    63,    80,    85,   135,   136,
     137,   133,   138,   140,   139,   132,   142,   134,   143,   144,
     145,   141,    64,    82,    87,   119,   120,   121,   122,     0,
      98,   101,     0,     0,     0,    55,    31,    32,    34,    33,
      38,    36,    35,    37,    76,    75,   187,   188,    28,   115,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    49,    46,    47,     0,     0,     0,
      53,    54,    52,     0,     0,   206,   208,   209,   210,     0,
     118,   112,   113,   198,   190,   191,   192,    81,    84,    83,
      86,    99,     0,    60,     0,   100,     0,     0,    57,     0,
       0,     0,   116,   117,     0,     0,   165,   164,   167,   166,
     168,     0,   102,   105,    62,    61,    79,    77,   199,    58,
     184,   170,   169,   171,   172,   176,   179,   180,   181,   182,
     183,   177,   173,   174,   175,   178,   185,    78,   106,   110,
     159,   163,   161,   162,   160,     0,     0,   201,   146,     0,
       0,   151,   147,   148,   149,   150,     0,    88,    92,    48,
     207,   196,   194,   193,   197,     0,     0,     0,     0,     0,
       0,     0,     0,    51,     0,     0,     0,    50,     0,     0,
     103,     0,   104,   107,   108,   109,   203,   202,   153,   154,
     155,    93,   152,   157,   158,    94,   156,     0,    89,    91,
     195,    59,     0,     0,     0,    96,     0,   211,   212,     0,
       0,    90,     0,    97,     0,     0,     0,    95
};

  /* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -175,  -175,  -175,  -175,  -175,  -175,  -175,  -175,  -175,  -175,
     215,  -175,  -175,  -175,  -175,  -175,  -175,  -175,  -175,    -6,
      58,  -175,  -175,  -175,  -175,  -175,  -175,  -175,  -175,  -174,
    -175,  -175,  -175,   108,   109,  -175,    -8,   -41,  -175,  -175,
     111,  -175,     6,  -175,     1,  -175,   121,  -175,   101,  -175,
    -175,  -175,  -175,  -175,  -175,  -175,  -175,  -175,  -175,  -175,
    -175,  -175,  -175,   208,   123,     9,  -175,  -175,     3,  -175,
    -133,  -129
};

  /* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,     1,    19,    20,    21,    22,    23,    24,    25,    56,
      57,    26,    27,    28,    29,    30,    31,    32,    33,   177,
     173,   174,    34,    35,    36,    37,    38,    39,    40,   178,
     197,    85,   102,    86,   103,   236,   237,   285,   286,   109,
     110,   191,   192,   217,   218,    68,    69,   128,   129,    66,
     111,    87,   104,   238,   271,   272,   275,   276,   225,   193,
     219,    70,   130,    73,    74,   165,    41,   226,   227,    75,
     160,   289
};

  /* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
     positive, shift that token.  If negative, reduce the rule whose
     number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int16 yytable[] =
{
     162,   114,   166,   199,   168,   163,   170,    63,   186,   175,
     268,   269,   105,    53,   106,   187,   163,   188,   107,    50,
     172,   183,    76,    77,    78,    79,    80,    54,   194,    81,
      82,    49,    71,   189,    83,   118,   119,    72,   200,   201,
     202,   203,   204,   252,   205,   206,   207,   253,   208,   209,
     210,   211,    71,   212,   213,   214,   176,    72,   120,    55,
     190,   108,   215,   198,    65,    88,    89,    90,    91,    92,
     270,    64,    93,    94,   176,   155,   156,    95,   157,   158,
      84,   281,   228,    61,    62,   216,   220,   199,    96,   121,
     122,   221,    97,    98,    99,   222,   223,   100,   150,   151,
     152,   229,   159,   115,   277,   230,   231,   232,   233,   234,
     235,   290,   123,   291,   224,   262,   256,   265,   266,    42,
     257,   228,    43,   101,    67,   279,   200,   201,   202,   203,
     204,   113,   205,   206,   207,   124,   208,   209,   210,   211,
     229,   212,   213,   214,   230,   231,   232,   233,   234,   235,
     215,   144,   145,    44,   146,   241,   242,    51,   243,   244,
      52,    59,   287,   294,    60,   288,   296,   147,   148,   116,
     117,   125,    45,   216,    46,    47,   179,    48,   149,   131,
       2,     3,   245,     4,     5,     6,     7,     8,     9,    10,
     172,    11,    12,    13,    14,    15,    16,    17,    18,   126,
     127,   140,   141,   134,   143,   246,   247,   132,   273,   274,
     134,   133,   136,   135,   137,   138,   139,   153,   142,   154,
     176,   180,   181,   184,   196,   259,   185,   249,    58,   239,
     240,   248,   195,   250,   251,   254,   255,   258,   297,   282,
     280,   261,   283,   284,   167,   292,   295,   169,   278,   293,
     263,   171,   260,   161,   182,   267,   112,   164,   264
};

static const yytype_int16 yycheck[] =
{
     133,     5,   135,   177,   137,    23,   139,    38,    52,   142,
      25,    26,    59,    23,    61,    59,    23,    61,    65,    30,
      33,   154,    47,    48,    49,    50,    51,    37,    41,    54,
      55,    20,    59,    77,    59,    23,    24,    64,    56,    57,
      58,    59,    60,    39,    62,    63,    64,    43,    66,    67,
      68,    69,    59,    71,    72,    73,    34,    64,    46,    46,
     104,   108,    80,    41,    66,    47,    48,    49,    50,    51,
      85,   102,    54,    55,    34,    19,    20,    59,    22,    23,
     105,    41,    59,    20,    21,   103,    78,   261,    70,    23,
      24,    83,    74,    75,    76,    87,    88,    79,    99,   100,
     101,    78,    46,   107,    81,    82,    83,    84,    85,    86,
      87,    39,    46,    41,   106,   248,    39,   250,   251,    31,
      43,    59,    34,   105,    53,   258,    56,    57,    58,    59,
      60,    44,    62,    63,    64,    20,    66,    67,    68,    69,
      78,    71,    72,    73,    82,    83,    84,    85,    86,    87,
      80,    19,    20,    10,    22,    19,    20,    20,    22,    23,
      23,    20,    20,   292,    23,    23,   295,    35,    36,    23,
      24,    20,    29,   103,    31,    32,    42,    34,    46,    44,
       0,     1,    46,     3,     4,     5,     6,     7,     8,     9,
      33,    11,    12,    13,    14,    15,    16,    17,    18,    89,
      90,    39,    40,    39,    40,    39,    40,    39,    27,    28,
      39,    44,    39,    44,    44,    39,    44,    39,    44,    44,
      34,    42,    24,    24,    19,    24,    44,    39,    13,    46,
      46,    44,   174,    44,    44,    44,    44,    44,    41,    44,
      46,   247,    40,    40,   136,    39,    39,   138,   256,   290,
     249,   140,   246,   132,   153,   252,    48,   134,   249
};

  /* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
     symbol of state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,   110,     0,     1,     3,     4,     5,     6,     7,     8,
       9,    11,    12,    13,    14,    15,    16,    17,    18,   111,
     112,   113,   114,   115,   116,   117,   120,   121,   122,   123,
     124,   125,   126,   127,   131,   132,   133,   134,   135,   136,
     137,   175,    31,    34,    10,    29,    31,    32,    34,    20,
      30,    20,    23,    23,    37,    46,   118,   119,   119,    20,
      23,    20,    21,    38,   102,    66,   158,    53,   154,   155,
     170,    59,    64,   172,   173,   178,    47,    48,    49,    50,
      51,    54,    55,    59,   105,   140,   142,   160,    47,    48,
      49,    50,    51,    54,    55,    59,    70,    74,    75,    76,
      79,   105,   141,   143,   161,    59,    61,    65,   108,   148,
     149,   159,   172,    44,     5,   107,    23,    24,    23,    24,
      46,    23,    24,    46,    20,    20,    89,    90,   156,   157,
     171,    44,    39,    44,    39,    44,    39,    44,    39,    44,
      39,    40,    44,    40,    19,    20,    22,    35,    36,    46,
      99,   100,   101,    39,    44,    19,    20,    22,    23,    46,
     179,   155,   179,    23,   173,   174,   179,   142,   179,   143,
     179,   149,    33,   129,   130,   179,    34,   128,   138,    42,
      42,    24,   157,   179,    24,    44,    52,    59,    61,    77,
     104,   150,   151,   168,    41,   129,    19,   139,    41,   138,
      56,    57,    58,    59,    60,    62,    63,    64,    66,    67,
      68,    69,    71,    72,    73,    80,   103,   152,   153,   169,
      78,    83,    87,    88,   106,   167,   176,   177,    59,    78,
      82,    83,    84,    85,    86,    87,   144,   145,   162,    46,
      46,    19,    20,    22,    23,    46,    39,    40,    44,    39,
      44,    44,    39,    43,    44,    44,    39,    43,    44,    24,
     151,   128,   179,   153,   174,   179,   179,   177,    25,    26,
      85,   163,   164,    27,    28,   165,   166,    81,   145,   179,
      46,    41,    44,    40,    40,   146,   147,    20,    23,   180,
      39,    41,    39,   146,   180,    39,   180,    41
};

  /* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint8 yyr1[] =
{
       0,   109,   110,   110,   110,   111,   111,   111,   111,   111,
     111,   111,   111,   111,   111,   111,   111,   111,   111,   111,
     111,   111,   111,   112,   112,   113,   114,   115,   116,   116,
     117,   118,   118,   119,   119,   119,   119,   119,   119,   120,
     120,   121,   122,   122,   122,   122,   123,   123,   123,   123,
     124,   125,   126,   126,   126,   126,   127,   128,   128,   129,
     130,   130,   131,   132,   132,   133,   134,   135,   135,   136,
     136,   136,   137,   137,   137,   137,   137,   138,   138,   139,
     140,   140,   141,   141,   142,   142,   143,   143,   144,   144,
     144,   145,   145,   145,   145,   146,   147,   147,   148,   148,
     149,   149,   150,   150,   151,   151,   152,   152,   152,   153,
     153,   154,   154,   155,   155,   156,   156,   157,   158,   159,
     159,   159,   159,   160,   160,   160,   160,   160,   160,   160,
     160,   160,   161,   161,   161,   161,   161,   161,   161,   161,
     161,   161,   161,   161,   161,   161,   162,   162,   162,   162,
     162,   162,   163,   164,   164,   164,   165,   166,   166,   167,
     167,   167,   167,   167,   168,   168,   168,   168,   168,   169,
     169,   169,   169,   169,   169,   169,   169,   169,   169,   169,
     169,   169,   169,   169,   169,   169,   170,   171,   171,   172,
     172,   172,   173,   174,   174,   174,   174,   174,   174,   175,
     175,   176,   176,   177,   178,   178,   179,   179,   179,   179,
     179,   180,   180
};

  /* YYR2[YYN] -- Number of symbols on the right hand side of rule YYN.  */
static const yytype_int8 yyr2[] =
{
       0,     2,     2,     2,     0,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     2,     2,     2,     1,     3,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     1,     1,     1,     1,     4,     4,     6,     4,
       7,     7,     4,     4,     4,     3,     1,     1,     2,     5,
       1,     2,     6,     3,     3,     2,     2,     2,     2,     2,
       2,     1,     1,     2,     2,     3,     3,     2,     2,     1,
       1,     3,     1,     3,     3,     1,     3,     1,     1,     3,
       7,     3,     1,     3,     3,     7,     1,     3,     1,     3,
       3,     1,     1,     3,     3,     1,     1,     3,     3,     3,
       1,     1,     3,     3,     1,     1,     3,     3,     3,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       3,     3,     3,     3,     3,     5,     3,     3,     1,     6,
       2,     1,     3,     3,     1,     1,     1,     3,     1,     1,
       1,     1,     1
};


#define yyerrok         (yyerrstatus = 0)
#define yyclearin       (yychar = YYEMPTY)
#define YYEMPTY         (-2)
#define YYEOF           0

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab


#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)                                    \
  do                                                              \
    if (yychar == YYEMPTY)                                        \
      {                                                           \
        yychar = (Token);                                         \
        yylval = (Value);                                         \
        YYPOPSTACK (yylen);                                       \
        yystate = *yyssp;                                         \
        goto yybackup;                                            \
      }                                                           \
    else                                                          \
      {                                                           \
        yyerror (YY_("syntax error: cannot back up")); \
        YYERROR;                                                  \
      }                                                           \
  while (0)

/* Error token number */
#define YYTERROR        1
#define YYERRCODE       256



/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)                        \
do {                                            \
  if (yydebug)                                  \
    YYFPRINTF Args;                             \
} while (0)

/* This macro is provided for backward compatibility. */
#ifndef YY_LOCATION_PRINT
# define YY_LOCATION_PRINT(File, Loc) ((void) 0)
#endif


# define YY_SYMBOL_PRINT(Title, Type, Value, Location)                    \
do {                                                                      \
  if (yydebug)                                                            \
    {                                                                     \
      YYFPRINTF (stderr, "%s ", Title);                                   \
      yy_symbol_print (stderr,                                            \
                  Type, Value); \
      YYFPRINTF (stderr, "\n");                                           \
    }                                                                     \
} while (0)


/*-----------------------------------.
| Print this symbol's value on YYO.  |
`-----------------------------------*/

static void
yy_symbol_value_print (FILE *yyo, int yytype, YYSTYPE const * const yyvaluep)
{
  FILE *yyoutput = yyo;
  YYUSE (yyoutput);
  if (!yyvaluep)
    return;
# ifdef YYPRINT
  if (yytype < YYNTOKENS)
    YYPRINT (yyo, yytoknum[yytype], *yyvaluep);
# endif
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YYUSE (yytype);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}


/*---------------------------.
| Print this symbol on YYO.  |
`---------------------------*/

static void
yy_symbol_print (FILE *yyo, int yytype, YYSTYPE const * const yyvaluep)
{
  YYFPRINTF (yyo, "%s %s (",
             yytype < YYNTOKENS ? "token" : "nterm", yytname[yytype]);

  yy_symbol_value_print (yyo, yytype, yyvaluep);
  YYFPRINTF (yyo, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

static void
yy_stack_print (yy_state_t *yybottom, yy_state_t *yytop)
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)                            \
do {                                                            \
  if (yydebug)                                                  \
    yy_stack_print ((Bottom), (Top));                           \
} while (0)


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

static void
yy_reduce_print (yy_state_t *yyssp, YYSTYPE *yyvsp, int yyrule)
{
  int yylno = yyrline[yyrule];
  int yynrhs = yyr2[yyrule];
  int yyi;
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %d):\n",
             yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr,
                       yystos[+yyssp[yyi + 1 - yynrhs]],
                       &yyvsp[(yyi + 1) - (yynrhs)]
                                              );
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)          \
do {                                    \
  if (yydebug)                          \
    yy_reduce_print (yyssp, yyvsp, Rule); \
} while (0)

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args)
# define YY_SYMBOL_PRINT(Title, Type, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !YYDEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   YYSTACK_ALLOC_MAXIMUM < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif


#if YYERROR_VERBOSE

# ifndef yystrlen
#  if defined __GLIBC__ && defined _STRING_H
#   define yystrlen(S) (YY_CAST (YYPTRDIFF_T, strlen (S)))
#  else
/* Return the length of YYSTR.  */
static YYPTRDIFF_T
yystrlen (const char *yystr)
{
  YYPTRDIFF_T yylen;
  for (yylen = 0; yystr[yylen]; yylen++)
    continue;
  return yylen;
}
#  endif
# endif

# ifndef yystpcpy
#  if defined __GLIBC__ && defined _STRING_H && defined _GNU_SOURCE
#   define yystpcpy stpcpy
#  else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
static char *
yystpcpy (char *yydest, const char *yysrc)
{
  char *yyd = yydest;
  const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
#  endif
# endif

# ifndef yytnamerr
/* Copy to YYRES the contents of YYSTR after stripping away unnecessary
   quotes and backslashes, so that it's suitable for yyerror.  The
   heuristic is that double-quoting is unnecessary unless the string
   contains an apostrophe, a comma, or backslash (other than
   backslash-backslash).  YYSTR is taken from yytname.  If YYRES is
   null, do not copy; instead, return the length of what the result
   would have been.  */
static YYPTRDIFF_T
yytnamerr (char *yyres, const char *yystr)
{
  if (*yystr == '"')
    {
      YYPTRDIFF_T yyn = 0;
      char const *yyp = yystr;

      for (;;)
        switch (*++yyp)
          {
          case '\'':
          case ',':
            goto do_not_strip_quotes;

          case '\\':
            if (*++yyp != '\\')
              goto do_not_strip_quotes;
            else
              goto append;

          append:
          default:
            if (yyres)
              yyres[yyn] = *yyp;
            yyn++;
            break;

          case '"':
            if (yyres)
              yyres[yyn] = '\0';
            return yyn;
          }
    do_not_strip_quotes: ;
    }

  if (yyres)
    return yystpcpy (yyres, yystr) - yyres;
  else
    return yystrlen (yystr);
}
# endif

/* Copy into *YYMSG, which is of size *YYMSG_ALLOC, an error message
   about the unexpected token YYTOKEN for the state stack whose top is
   YYSSP.

   Return 0 if *YYMSG was successfully written.  Return 1 if *YYMSG is
   not large enough to hold the message.  In that case, also set
   *YYMSG_ALLOC to the required number of bytes.  Return 2 if the
   required number of bytes is too large to store.  */
static int
yysyntax_error (YYPTRDIFF_T *yymsg_alloc, char **yymsg,
                yy_state_t *yyssp, int yytoken)
{
  enum { YYERROR_VERBOSE_ARGS_MAXIMUM = 5 };
  /* Internationalized format string. */
  const char *yyformat = YY_NULLPTR;
  /* Arguments of yyformat: reported tokens (one for the "unexpected",
     one per "expected"). */
  char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];
  /* Actual size of YYARG. */
  int yycount = 0;
  /* Cumulated lengths of YYARG.  */
  YYPTRDIFF_T yysize = 0;

  /* There are many possibilities here to consider:
     - If this state is a consistent state with a default action, then
       the only way this function was invoked is if the default action
       is an error action.  In that case, don't check for expected
       tokens because there are none.
     - The only way there can be no lookahead present (in yychar) is if
       this state is a consistent state with a default action.  Thus,
       detecting the absence of a lookahead is sufficient to determine
       that there is no unexpected or expected token to report.  In that
       case, just report a simple "syntax error".
     - Don't assume there isn't a lookahead just because this state is a
       consistent state with a default action.  There might have been a
       previous inconsistent state, consistent state with a non-default
       action, or user semantic action that manipulated yychar.
     - Of course, the expected token list depends on states to have
       correct lookahead information, and it depends on the parser not
       to perform extra reductions after fetching a lookahead from the
       scanner and before detecting a syntax error.  Thus, state merging
       (from LALR or IELR) and default reductions corrupt the expected
       token list.  However, the list is correct for canonical LR with
       one exception: it will still contain any token that will not be
       accepted due to an error action in a later state.
  */
  if (yytoken != YYEMPTY)
    {
      int yyn = yypact[+*yyssp];
      YYPTRDIFF_T yysize0 = yytnamerr (YY_NULLPTR, yytname[yytoken]);
      yysize = yysize0;
      yyarg[yycount++] = yytname[yytoken];
      if (!yypact_value_is_default (yyn))
        {
          /* Start YYX at -YYN if negative to avoid negative indexes in
             YYCHECK.  In other words, skip the first -YYN actions for
             this state because they are default actions.  */
          int yyxbegin = yyn < 0 ? -yyn : 0;
          /* Stay within bounds of both yycheck and yytname.  */
          int yychecklim = YYLAST - yyn + 1;
          int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
          int yyx;

          for (yyx = yyxbegin; yyx < yyxend; ++yyx)
            if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR
                && !yytable_value_is_error (yytable[yyx + yyn]))
              {
                if (yycount == YYERROR_VERBOSE_ARGS_MAXIMUM)
                  {
                    yycount = 1;
                    yysize = yysize0;
                    break;
                  }
                yyarg[yycount++] = yytname[yyx];
                {
                  YYPTRDIFF_T yysize1
                    = yysize + yytnamerr (YY_NULLPTR, yytname[yyx]);
                  if (yysize <= yysize1 && yysize1 <= YYSTACK_ALLOC_MAXIMUM)
                    yysize = yysize1;
                  else
                    return 2;
                }
              }
        }
    }

  switch (yycount)
    {
# define YYCASE_(N, S)                      \
      case N:                               \
        yyformat = S;                       \
      break
    default: /* Avoid compiler warnings. */
      YYCASE_(0, YY_("syntax error"));
      YYCASE_(1, YY_("syntax error, unexpected %s"));
      YYCASE_(2, YY_("syntax error, unexpected %s, expecting %s"));
      YYCASE_(3, YY_("syntax error, unexpected %s, expecting %s or %s"));
      YYCASE_(4, YY_("syntax error, unexpected %s, expecting %s or %s or %s"));
      YYCASE_(5, YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s"));
# undef YYCASE_
    }

  {
    /* Don't count the "%s"s in the final size, but reserve room for
       the terminator.  */
    YYPTRDIFF_T yysize1 = yysize + (yystrlen (yyformat) - 2 * yycount) + 1;
    if (yysize <= yysize1 && yysize1 <= YYSTACK_ALLOC_MAXIMUM)
      yysize = yysize1;
    else
      return 2;
  }

  if (*yymsg_alloc < yysize)
    {
      *yymsg_alloc = 2 * yysize;
      if (! (yysize <= *yymsg_alloc
             && *yymsg_alloc <= YYSTACK_ALLOC_MAXIMUM))
        *yymsg_alloc = YYSTACK_ALLOC_MAXIMUM;
      return 1;
    }

  /* Avoid sprintf, as that infringes on the user's name space.
     Don't have undefined behavior even if the translation
     produced a string with the wrong number of "%s"s.  */
  {
    char *yyp = *yymsg;
    int yyi = 0;
    while ((*yyp = *yyformat) != '\0')
      if (*yyp == '%' && yyformat[1] == 's' && yyi < yycount)
        {
          yyp += yytnamerr (yyp, yyarg[yyi++]);
          yyformat += 2;
        }
      else
        {
          ++yyp;
          ++yyformat;
        }
  }
  return 0;
}
#endif /* YYERROR_VERBOSE */

/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

static void
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep)
{
  YYUSE (yyvaluep);
  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yytype, yyvaluep, yylocationp);

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YYUSE (yytype);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}




/* The lookahead symbol.  */
int yychar;

/* The semantic value of the lookahead symbol.  */
YYSTYPE yylval;
/* Number of syntax errors so far.  */
int yynerrs;


/*----------.
| yyparse.  |
`----------*/

int
yyparse (void)
{
    yy_state_fast_t yystate;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus;

    /* The stacks and their tools:
       'yyss': related to states.
       'yyvs': related to semantic values.

       Refer to the stacks through separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* The state stack.  */
    yy_state_t yyssa[YYINITDEPTH];
    yy_state_t *yyss;
    yy_state_t *yyssp;

    /* The semantic value stack.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs;
    YYSTYPE *yyvsp;

    YYPTRDIFF_T yystacksize;

  int yyn;
  int yyresult;
  /* Lookahead token as an internal (translated) token number.  */
  int yytoken = 0;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;

#if YYERROR_VERBOSE
  /* Buffer for error messages, and its allocated size.  */
  char yymsgbuf[128];
  char *yymsg = yymsgbuf;
  YYPTRDIFF_T yymsg_alloc = sizeof yymsgbuf;
#endif

#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  yyssp = yyss = yyssa;
  yyvsp = yyvs = yyvsa;
  yystacksize = YYINITDEPTH;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY; /* Cause a token to be read.  */
  goto yysetstate;


/*------------------------------------------------------------.
| yynewstate -- push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;


/*--------------------------------------------------------------------.
| yysetstate -- set current state (the top of the stack) to yystate.  |
`--------------------------------------------------------------------*/
yysetstate:
  YYDPRINTF ((stderr, "Entering state %d\n", yystate));
  YY_ASSERT (0 <= yystate && yystate < YYNSTATES);
  YY_IGNORE_USELESS_CAST_BEGIN
  *yyssp = YY_CAST (yy_state_t, yystate);
  YY_IGNORE_USELESS_CAST_END

  if (yyss + yystacksize - 1 <= yyssp)
#if !defined yyoverflow && !defined YYSTACK_RELOCATE
    goto yyexhaustedlab;
#else
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYPTRDIFF_T yysize = yyssp - yyss + 1;

# if defined yyoverflow
      {
        /* Give user a chance to reallocate the stack.  Use copies of
           these so that the &'s don't force the real ones into
           memory.  */
        yy_state_t *yyss1 = yyss;
        YYSTYPE *yyvs1 = yyvs;

        /* Each stack pointer address is followed by the size of the
           data in use in that stack, in bytes.  This used to be a
           conditional around just the two extra args, but that might
           be undefined if yyoverflow is a macro.  */
        yyoverflow (YY_("memory exhausted"),
                    &yyss1, yysize * YYSIZEOF (*yyssp),
                    &yyvs1, yysize * YYSIZEOF (*yyvsp),
                    &yystacksize);
        yyss = yyss1;
        yyvs = yyvs1;
      }
# else /* defined YYSTACK_RELOCATE */
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
        goto yyexhaustedlab;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
        yystacksize = YYMAXDEPTH;

      {
        yy_state_t *yyss1 = yyss;
        union yyalloc *yyptr =
          YY_CAST (union yyalloc *,
                   YYSTACK_ALLOC (YY_CAST (YYSIZE_T, YYSTACK_BYTES (yystacksize))));
        if (! yyptr)
          goto yyexhaustedlab;
        YYSTACK_RELOCATE (yyss_alloc, yyss);
        YYSTACK_RELOCATE (yyvs_alloc, yyvs);
# undef YYSTACK_RELOCATE
        if (yyss1 != yyssa)
          YYSTACK_FREE (yyss1);
      }
# endif

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;

      YY_IGNORE_USELESS_CAST_BEGIN
      YYDPRINTF ((stderr, "Stack size increased to %ld\n",
                  YY_CAST (long, yystacksize)));
      YY_IGNORE_USELESS_CAST_END

      if (yyss + yystacksize - 1 <= yyssp)
        YYABORT;
    }
#endif /* !defined yyoverflow && !defined YYSTACK_RELOCATE */

  if (yystate == YYFINAL)
    YYACCEPT;

  goto yybackup;


/*-----------.
| yybackup.  |
`-----------*/
yybackup:
  /* Do appropriate processing given the current state.  Read a
     lookahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to lookahead token.  */
  yyn = yypact[yystate];
  if (yypact_value_is_default (yyn))
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either YYEMPTY or YYEOF or a valid lookahead symbol.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token: "));
      yychar = yylex ();
    }

  if (yychar <= YYEOF)
    {
      yychar = yytoken = YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yytable_value_is_error (yyn))
        goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the lookahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);
  yystate = yyn;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END

  /* Discard the shifted token.  */
  yychar = YYEMPTY;
  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     '$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];


  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
  case 2:
#line 201 "parser_gram.y"
{
	if ((yyvsp[0].cmd)->cmd)
		(yyvsp[0].cmd)->cmd((yyvsp[0].cmd));

	free((yyvsp[0].cmd));
}
#line 1885 "parser_gram.c"
    break;

  case 3:
#line 208 "parser_gram.y"
{
	YYABORT;
}
#line 1893 "parser_gram.c"
    break;

  case 23:
#line 234 "parser_gram.y"
{
	if (((yyval.cmd) = alloc_cmd()) == NULL)
		YYERROR;
	(yyval.cmd)->cmd = &parser_eventgen;
}
#line 1903 "parser_gram.c"
    break;

  case 24:
#line 240 "parser_gram.y"
{
	(yyvsp[-1].cmd)->cmd_attr_list = (yyvsp[0].attr);
}
#line 1911 "parser_gram.c"
    break;

  case 25:
#line 245 "parser_gram.y"
{
	if (((yyval.cmd) = alloc_cmd()) == NULL)
		YYERROR;

	(yyval.cmd)->cmd_param_list = (yyvsp[0].list);
	(yyval.cmd)->cmd = parser_system;
}
#line 1923 "parser_gram.c"
    break;

  case 26:
#line 254 "parser_gram.y"
{
	if (((yyval.cmd) = alloc_cmd()) == NULL)
		YYERROR;

	(yyval.cmd)->cmd_param_list = (yyvsp[0].list);
	(yyval.cmd)->cmd = parser_echo;
}
#line 1935 "parser_gram.c"
    break;

  case 27:
#line 263 "parser_gram.y"
{
	if (((yyval.cmd) = alloc_cmd()) == NULL)
		YYERROR;
	(yyval.cmd)->cmd = parser_version;
}
#line 1945 "parser_gram.c"
    break;

  case 28:
#line 270 "parser_gram.y"
{

	if (((yyval.cmd) = alloc_cmd()) == NULL)
		YYERROR;

	(yyval.cmd)->cmd = parser_enable_mc;
	(yyval.cmd)->cmd_attr_list = (yyvsp[0].attr);
}
#line 1958 "parser_gram.c"
    break;

  case 29:
#line 279 "parser_gram.y"
{
	if (((yyval.cmd) = alloc_cmd()) == NULL)
		YYERROR;

	(yyval.cmd)->cmd = parser_enable_lathist;
}
#line 1969 "parser_gram.c"
    break;

  case 30:
#line 287 "parser_gram.y"
{
	if (((yyval.cmd) = alloc_cmd()) == NULL)
		YYERROR;

	(yyval.cmd)->cmd = parser_domultisync;
	(yyval.cmd)->cmd_attr_list = (yyvsp[0].attr);
}
#line 1981 "parser_gram.c"
    break;

  case 31:
#line 296 "parser_gram.y"
{
	if (((yyval.list) = alloc_list()) == NULL)
			YYERROR;

	(yyval.list)->list_string = avd_str_alloc((yyvsp[0].sval));
}
#line 1992 "parser_gram.c"
    break;

  case 32:
#line 303 "parser_gram.y"
{
	if (((yyval.list) = alloc_list()) == NULL)
			YYERROR;

	(yyval.list)->list_string = avd_str_alloc((yyvsp[0].sval));
}
#line 2003 "parser_gram.c"
    break;

  case 33:
#line 311 "parser_gram.y"
{
	list_t *list = NULL;
	list_t *list_end = NULL;

	/* Add string */
	if (((yyval.list) = alloc_list()) == NULL)
		YYERROR;

	(yyval.list)->list_string = avd_str_alloc((yyvsp[0].sval));

	/* Find end of list */
	for (list = (yyvsp[-1].list); list != NULL;
	    list = list->list_next)
		list_end = list;
	list_end->list_next = (yyval.list);
	(yyval.list) = (yyvsp[-1].list);

}
#line 2026 "parser_gram.c"
    break;

  case 34:
#line 329 "parser_gram.y"
{
	list_t *list = NULL;
	list_t *list_end = NULL;

	/* Add variable */
	if (((yyval.list) = alloc_list()) == NULL)
		YYERROR;

	(yyval.list)->list_string = avd_str_alloc((yyvsp[0].sval));

	/* Find end of list */
	for (list = (yyvsp[-1].list); list != NULL;
	    list = list->list_next)
		list_end = list;
	list_end->list_next = (yyval.list);
	(yyval.list) = (yyvsp[-1].list);
}
#line 2048 "parser_gram.c"
    break;

  case 35:
#line 346 "parser_gram.y"
{
	list_t *list = NULL;
	list_t *list_end = NULL;

	/* Add string */
	if (((yyval.list) = alloc_list()) == NULL)
		YYERROR;

	(yyval.list)->list_string = avd_str_alloc((yyvsp[0].sval));

	/* Find end of list */
	for (list = (yyvsp[-1].list); list != NULL;
	    list = list->list_next)
		list_end = list;
	list_end->list_next = (yyval.list);
	(yyval.list) = (yyvsp[-1].list);

}
#line 2071 "parser_gram.c"
    break;

  case 36:
#line 364 "parser_gram.y"
{
	list_t *list = NULL;
	list_t *list_end = NULL;

	/* Add variable */
	if (((yyval.list) = alloc_list()) == NULL)
		YYERROR;

	(yyval.list)->list_string = avd_str_alloc((yyvsp[0].sval));

	/* Find end of list */
	for (list = (yyvsp[-1].list); list != NULL;
	    list = list->list_next)
		list_end = list;
	list_end->list_next = (yyval.list);
	(yyval.list) = (yyvsp[-1].list);
}
#line 2093 "parser_gram.c"
    break;

  case 37:
#line 381 "parser_gram.y"
{
	(yyval.list) = (yyvsp[-1].list);
}
#line 2101 "parser_gram.c"
    break;

  case 38:
#line 384 "parser_gram.y"
{
	(yyval.list) = (yyvsp[-1].list);
}
#line 2109 "parser_gram.c"
    break;

  case 39:
#line 389 "parser_gram.y"
{
	if (((yyval.cmd) = alloc_cmd()) == NULL)
		YYERROR;
	(yyval.cmd)->cmd = &parser_fileset_list;
}
#line 2119 "parser_gram.c"
    break;

  case 40:
#line 395 "parser_gram.y"
{
	if (((yyval.cmd) = alloc_cmd()) == NULL)
		YYERROR;
	(yyval.cmd)->cmd = &parser_flowop_list;
}
#line 2129 "parser_gram.c"
    break;

  case 41:
#line 402 "parser_gram.y"
{
	if (((yyval.cmd) = alloc_cmd()) == NULL)
		YYERROR;
	(yyval.cmd)->cmd = NULL;
	filebench_shm->shm_debug_level = (yyvsp[0].ival);
	if (filebench_shm->shm_debug_level > 10) {
		filebench_log(LOG_ERROR, "Debug level set out of range."
					"  Adjusting to 10.");
			filebench_shm->shm_debug_level = 10;
		}
	if (filebench_shm->shm_debug_level > 9)
		yydebug = 1;
}
#line 2147 "parser_gram.c"
    break;

  case 46:
#line 419 "parser_gram.y"
{
	(yyval.cmd) = alloc_cmd();
	if (!(yyval.cmd))
		YYERROR;

	var_assign_integer((yyvsp[-2].sval), (yyvsp[0].ival));

	(yyval.cmd)->cmd = NULL;
}
#line 2161 "parser_gram.c"
    break;

  case 47:
#line 430 "parser_gram.y"
{
	(yyval.cmd) = alloc_cmd();
	if (!(yyval.cmd))
		YYERROR;

	var_assign_boolean((yyvsp[-2].sval), (yyvsp[0].bval));

	(yyval.cmd)->cmd = NULL;
}
#line 2175 "parser_gram.c"
    break;

  case 48:
#line 440 "parser_gram.y"
{
	(yyval.cmd) = alloc_cmd();
	if (!(yyval.cmd))
		YYERROR;

	var_assign_string((yyvsp[-4].sval), (yyvsp[-1].sval));

	(yyval.cmd)->cmd = NULL;
}
#line 2189 "parser_gram.c"
    break;

  case 49:
#line 450 "parser_gram.y"
{
	(yyval.cmd) = alloc_cmd();
	if (!(yyval.cmd))
		YYERROR;

	var_assign_string((yyvsp[-2].sval), (yyvsp[0].sval));

	(yyval.cmd)->cmd = NULL;
}
#line 2203 "parser_gram.c"
    break;

  case 50:
#line 461 "parser_gram.y"
{
	(yyval.cmd) = alloc_cmd();
	if (!(yyval.cmd))
		YYERROR;

	(yyval.cmd)->cmd_attr_list = (yyvsp[-1].attr);
	(yyval.cmd)->cmd = NULL;

	parser_var_assign_random((yyvsp[-5].sval), (yyval.cmd));
}
#line 2218 "parser_gram.c"
    break;

  case 51:
#line 473 "parser_gram.y"
{
	(yyval.cmd) = alloc_cmd();
	if (!(yyval.cmd))
		YYERROR;

	(yyval.cmd)->cmd_attr_list = (yyvsp[-1].attr);
	(yyval.cmd)->cmd = NULL;

	parser_var_assign_custom((yyvsp[-5].sval), (yyval.cmd));
}
#line 2233 "parser_gram.c"
    break;

  case 52:
#line 485 "parser_gram.y"
{
	(yyval.cmd) = alloc_cmd();
	if (!(yyval.cmd))
		YYERROR;

	filebench_shm->shm_rmode = FILEBENCH_MODE_TIMEOUT;

	(yyval.cmd)->cmd = NULL;
}
#line 2247 "parser_gram.c"
    break;

  case 53:
#line 495 "parser_gram.y"
{
	(yyval.cmd) = alloc_cmd();
	if (!(yyval.cmd))
		YYERROR;

	filebench_shm->shm_rmode = FILEBENCH_MODE_QALLDONE;

	(yyval.cmd)->cmd = NULL;
}
#line 2261 "parser_gram.c"
    break;

  case 54:
#line 505 "parser_gram.y"
{
	(yyval.cmd) = alloc_cmd();
	if (!(yyval.cmd))
		YYERROR;

	filebench_shm->shm_rmode = FILEBENCH_MODE_Q1STDONE;

	(yyval.cmd)->cmd = NULL;
}
#line 2275 "parser_gram.c"
    break;

  case 55:
#line 515 "parser_gram.y"
{
	(yyval.cmd) = alloc_cmd();
	if (!(yyval.cmd))
		YYERROR;

	filebench_log(LOG_INFO, "Disabling CPU usage statistics");
	filebench_shm->shm_mmode |= FILEBENCH_MODE_NOUSAGE;

	(yyval.cmd)->cmd = NULL;
}
#line 2290 "parser_gram.c"
    break;

  case 56:
#line 527 "parser_gram.y"
{
	if (((yyval.cmd) = alloc_cmd()) == NULL)
		YYERROR;
	(yyval.cmd)->cmd = parser_filebench_shutdown;
}
#line 2300 "parser_gram.c"
    break;

  case 57:
#line 534 "parser_gram.y"
{
	(yyval.cmd) = (yyvsp[0].cmd);
}
#line 2308 "parser_gram.c"
    break;

  case 58:
#line 537 "parser_gram.y"
{
	cmd_t *list = NULL;
	cmd_t *list_end = NULL;

	/* Find end of list */
	for (list = (yyvsp[-1].cmd); list != NULL;
	    list = list->cmd_next)
		list_end = list;

	list_end->cmd_next = (yyvsp[0].cmd);

	filebench_log(LOG_DEBUG_IMPL,
	    "flowop_list adding cmd %zx to list %zx", (yyvsp[0].cmd), (yyvsp[-1].cmd));

	(yyval.cmd) = (yyvsp[-1].cmd);
}
#line 2329 "parser_gram.c"
    break;

  case 59:
#line 555 "parser_gram.y"
{
	/*
	 * Allocate a cmd node per thread, with a
	 * list of flowops attached to the cmd_list
	 */
	if (((yyval.cmd) = alloc_cmd()) == NULL)
		YYERROR;
	(yyval.cmd)->cmd_list = (yyvsp[-1].cmd);
	(yyval.cmd)->cmd_attr_list = (yyvsp[-3].attr);
}
#line 2344 "parser_gram.c"
    break;

  case 60:
#line 567 "parser_gram.y"
{
	(yyval.cmd) = (yyvsp[0].cmd);
}
#line 2352 "parser_gram.c"
    break;

  case 61:
#line 570 "parser_gram.y"
{
	cmd_t *list = NULL;
	cmd_t *list_end = NULL;

	/* Find end of list */
	for (list = (yyvsp[-1].cmd); list != NULL;
	    list = list->cmd_next)
		list_end = list;

	list_end->cmd_next = (yyvsp[0].cmd);

	filebench_log(LOG_DEBUG_IMPL,
	    "thread_list adding cmd %zx to list %zx", (yyvsp[0].cmd), (yyvsp[-1].cmd));

	(yyval.cmd) = (yyvsp[-1].cmd);
}
#line 2373 "parser_gram.c"
    break;

  case 62:
#line 588 "parser_gram.y"
{
	(yyval.cmd) = alloc_cmd();
	if (!(yyval.cmd))
		YYERROR;
	(yyval.cmd)->cmd = &parser_proc_define;
	(yyval.cmd)->cmd_list = (yyvsp[-1].cmd);
	(yyval.cmd)->cmd_attr_list = (yyvsp[-3].attr);
}
#line 2386 "parser_gram.c"
    break;

  case 63:
#line 598 "parser_gram.y"
{
	(yyval.cmd) = alloc_cmd();
	if (!(yyval.cmd))
		YYERROR;

	(yyval.cmd)->cmd = &parser_file_define;
	(yyval.cmd)->cmd_attr_list = (yyvsp[0].attr);
}
#line 2399 "parser_gram.c"
    break;

  case 64:
#line 606 "parser_gram.y"
{
	(yyval.cmd) = alloc_cmd();
	if (!(yyval.cmd))
		YYERROR;

	(yyval.cmd)->cmd = &parser_fileset_define;
	(yyval.cmd)->cmd_attr_list = (yyvsp[0].attr);
}
#line 2412 "parser_gram.c"
    break;

  case 65:
#line 616 "parser_gram.y"
{
	(yyval.cmd) = alloc_cmd();
	if (!(yyval.cmd))
		YYERROR;

	(yyval.cmd)->cmd = &parser_digest;
}
#line 2424 "parser_gram.c"
    break;

  case 66:
#line 625 "parser_gram.y"
{
	if (((yyval.cmd) = alloc_cmd()) == NULL)
		YYERROR;

	(yyval.cmd)->cmd = &parser_fileset_create;
}
#line 2435 "parser_gram.c"
    break;

  case 67:
#line 633 "parser_gram.y"
{
	if (((yyval.cmd) = alloc_cmd()) == NULL)
		YYERROR;
	(yyval.cmd)->cmd = parser_sleep;
	(yyval.cmd)->cmd_qty = (yyvsp[0].ival);
}
#line 2446 "parser_gram.c"
    break;

  case 68:
#line 640 "parser_gram.y"
{
	if (((yyval.cmd) = alloc_cmd()) == NULL)
		YYERROR;
	(yyval.cmd)->cmd = parser_sleep_variable;
	(yyval.cmd)->cmd_tgt1 = fb_stralloc((yyvsp[0].sval));
}
#line 2457 "parser_gram.c"
    break;

  case 69:
#line 648 "parser_gram.y"
{
	if (((yyval.cmd) = alloc_cmd()) == NULL)
		YYERROR;
	(yyval.cmd)->cmd = parser_run;
	(yyval.cmd)->cmd_qty = (yyvsp[0].ival);
}
#line 2468 "parser_gram.c"
    break;

  case 70:
#line 655 "parser_gram.y"
{
	if (((yyval.cmd) = alloc_cmd()) == NULL)
		YYERROR;
	(yyval.cmd)->cmd = parser_run_variable;
	(yyval.cmd)->cmd_tgt1 = fb_stralloc((yyvsp[0].sval));
}
#line 2479 "parser_gram.c"
    break;

  case 71:
#line 662 "parser_gram.y"
{
	if (((yyval.cmd) = alloc_cmd()) == NULL)
		YYERROR;
	(yyval.cmd)->cmd = parser_run;
	(yyval.cmd)->cmd_qty = 0;
}
#line 2490 "parser_gram.c"
    break;

  case 72:
#line 670 "parser_gram.y"
{
	if (((yyval.cmd) = alloc_cmd()) == NULL)
		YYERROR;
	(yyval.cmd)->cmd = parser_psrun;
	(yyval.cmd)->cmd_qty1 = 0;
	(yyval.cmd)->cmd_qty = 0;
}
#line 2502 "parser_gram.c"
    break;

  case 73:
#line 678 "parser_gram.y"
{
	if (((yyval.cmd) = alloc_cmd()) == NULL)
		YYERROR;
	(yyval.cmd)->cmd = parser_psrun;
	(yyval.cmd)->cmd_qty1 = (yyvsp[0].ival);
	(yyval.cmd)->cmd_qty = 0;

}
#line 2515 "parser_gram.c"
    break;

  case 74:
#line 687 "parser_gram.y"
{
	if (((yyval.cmd) = alloc_cmd()) == NULL)
		YYERROR;
	(yyval.cmd)->cmd = parser_psrun;
	(yyval.cmd)->cmd_qty1 = (yyvsp[0].ival);
	(yyval.cmd)->cmd_qty = 0;

}
#line 2528 "parser_gram.c"
    break;

  case 75:
#line 696 "parser_gram.y"
{
	if (((yyval.cmd) = alloc_cmd()) == NULL)
		YYERROR;
	(yyval.cmd)->cmd = parser_psrun;
	(yyval.cmd)->cmd_qty1 = (yyvsp[-1].ival);
	(yyval.cmd)->cmd_qty = (yyvsp[0].ival);
}
#line 2540 "parser_gram.c"
    break;

  case 76:
#line 704 "parser_gram.y"
{
	if (((yyval.cmd) = alloc_cmd()) == NULL)
		YYERROR;
	(yyval.cmd)->cmd = parser_psrun;
	(yyval.cmd)->cmd_qty1 = (yyvsp[-1].ival);
	(yyval.cmd)->cmd_qty = (yyvsp[0].ival);
}
#line 2552 "parser_gram.c"
    break;

  case 77:
#line 713 "parser_gram.y"
{
	if (((yyval.cmd) = alloc_cmd()) == NULL)
		YYERROR;
	(yyval.cmd)->cmd_name = fb_stralloc((yyvsp[0].sval));
}
#line 2562 "parser_gram.c"
    break;

  case 78:
#line 719 "parser_gram.y"
{
	(yyvsp[-1].cmd)->cmd_attr_list = (yyvsp[0].attr);
}
#line 2570 "parser_gram.c"
    break;

  case 80:
#line 726 "parser_gram.y"
{
	(yyval.attr) = (yyvsp[0].attr);
}
#line 2578 "parser_gram.c"
    break;

  case 81:
#line 730 "parser_gram.y"
{
	attr_t *attr = NULL;
	attr_t *list_end = NULL;

	for (attr = (yyvsp[-2].attr); attr; attr = attr->attr_next)
		list_end = attr;

	list_end->attr_next = (yyvsp[0].attr);

	(yyval.attr) = (yyvsp[-2].attr);
}
#line 2594 "parser_gram.c"
    break;

  case 82:
#line 743 "parser_gram.y"
{
	(yyval.attr) = (yyvsp[0].attr);
}
#line 2602 "parser_gram.c"
    break;

  case 83:
#line 747 "parser_gram.y"
{
	attr_t *attr = NULL;
	attr_t *list_end = NULL;

	for (attr = (yyvsp[-2].attr); attr; attr = attr->attr_next)
		list_end = attr;

	list_end->attr_next = (yyvsp[0].attr);

	(yyval.attr) = (yyvsp[-2].attr);
}
#line 2618 "parser_gram.c"
    break;

  case 84:
#line 760 "parser_gram.y"
{
	(yyval.attr) = (yyvsp[0].attr);
	(yyval.attr)->attr_name = (yyvsp[-2].ival);
}
#line 2627 "parser_gram.c"
    break;

  case 85:
#line 765 "parser_gram.y"
{
	(yyval.attr) = alloc_attr();
	if (!(yyval.attr))
		YYERROR;

	(yyval.attr)->attr_name = (yyvsp[0].ival);
	(yyval.attr)->attr_avd = avd_bool_alloc(TRUE);
}
#line 2640 "parser_gram.c"
    break;

  case 86:
#line 775 "parser_gram.y"
{
	(yyval.attr) = (yyvsp[0].attr);
	(yyval.attr)->attr_name = (yyvsp[-2].ival);
}
#line 2649 "parser_gram.c"
    break;

  case 87:
#line 780 "parser_gram.y"
{
	(yyval.attr) = alloc_attr();
	if (!(yyval.attr))
		YYERROR;

	(yyval.attr)->attr_name = (yyvsp[0].ival);
	(yyval.attr)->attr_avd = avd_bool_alloc(TRUE);
}
#line 2662 "parser_gram.c"
    break;

  case 88:
#line 791 "parser_gram.y"
{
	(yyval.attr) = (yyvsp[0].attr);
}
#line 2670 "parser_gram.c"
    break;

  case 89:
#line 795 "parser_gram.y"
{
	attr_t *attr = NULL;
	attr_t *list_end = NULL;

	for (attr = (yyvsp[-2].attr); attr != NULL;
	    attr = attr->attr_next)
		list_end = attr; /* Find end of list */

	list_end->attr_next = (yyvsp[0].attr);

	(yyval.attr) = (yyvsp[-2].attr);
}
#line 2687 "parser_gram.c"
    break;

  case 90:
#line 808 "parser_gram.y"
{
	attr_t *attr = NULL;
	attr_t *list_end = NULL;

	for (attr = (yyvsp[-6].attr); attr != NULL;
	    attr = attr->attr_next)
		list_end = attr; /* Find end of list */


	if ((attr = alloc_attr()) == NULL)
		YYERROR;

	attr->attr_name = FSA_RANDTABLE;
	attr->attr_obj = (void *)(yyvsp[-1].rndtb);
	list_end->attr_next = attr;
	(yyval.attr) = (yyvsp[-6].attr);
}
#line 2709 "parser_gram.c"
    break;

  case 91:
#line 827 "parser_gram.y"
{
	(yyval.attr) = (yyvsp[0].attr);
	(yyval.attr)->attr_name = (yyvsp[-2].ival);
}
#line 2718 "parser_gram.c"
    break;

  case 92:
#line 832 "parser_gram.y"
{
	if (((yyval.attr) = alloc_attr()) == NULL)
		YYERROR;
	(yyval.attr)->attr_name = (yyvsp[0].ival);
	(yyval.attr)->attr_avd = avd_bool_alloc(TRUE);
}
#line 2729 "parser_gram.c"
    break;

  case 93:
#line 839 "parser_gram.y"
{
	(yyval.attr) = (yyvsp[0].attr);
	(yyval.attr)->attr_name = FSA_TYPE;
}
#line 2738 "parser_gram.c"
    break;

  case 94:
#line 844 "parser_gram.y"
{
	(yyval.attr) = (yyvsp[0].attr);
	(yyval.attr)->attr_name = FSA_RANDSRC;
}
#line 2747 "parser_gram.c"
    break;

  case 95:
#line 850 "parser_gram.y"
{
	if (((yyval.rndtb) = alloc_probtabent()) == NULL)
		YYERROR;
	(yyval.rndtb)->pte_percent = (yyvsp[-5].avd);
	(yyval.rndtb)->pte_segmin  = (yyvsp[-3].avd);
	(yyval.rndtb)->pte_segmax  = (yyvsp[-1].avd);
}
#line 2759 "parser_gram.c"
    break;

  case 96:
#line 860 "parser_gram.y"
{
	(yyval.rndtb) = (yyvsp[0].rndtb);
}
#line 2767 "parser_gram.c"
    break;

  case 97:
#line 864 "parser_gram.y"
{
	probtabent_t *pte = NULL;
	probtabent_t *ptelist_end = NULL;

	for (pte = (yyvsp[-2].rndtb); pte != NULL;
	    pte = pte->pte_next)
		ptelist_end = pte; /* Find end of prob table entry list */

	ptelist_end->pte_next = (yyvsp[0].rndtb);

	(yyval.rndtb) = (yyvsp[-2].rndtb);
}
#line 2784 "parser_gram.c"
    break;

  case 98:
#line 878 "parser_gram.y"
{
	(yyval.attr) = (yyvsp[0].attr);
}
#line 2792 "parser_gram.c"
    break;

  case 99:
#line 882 "parser_gram.y"
{
	attr_t *attr = NULL;
	attr_t *list_end = NULL;

	for (attr = (yyvsp[-2].attr); attr != NULL;
	    attr = attr->attr_next)
		list_end = attr; /* Find end of list */

	list_end->attr_next = (yyvsp[0].attr);

	(yyval.attr) = (yyvsp[-2].attr);
}
#line 2809 "parser_gram.c"
    break;

  case 100:
#line 896 "parser_gram.y"
{
	(yyval.attr) = (yyvsp[0].attr);
	(yyval.attr)->attr_name = (yyvsp[-2].ival);
}
#line 2818 "parser_gram.c"
    break;

  case 101:
#line 901 "parser_gram.y"
{
	(yyval.attr) = alloc_attr();
	if (!(yyval.attr))
		YYERROR;
	(yyval.attr)->attr_name = (yyvsp[0].ival);
	(yyval.attr)->attr_avd = avd_bool_alloc(TRUE);
}
#line 2830 "parser_gram.c"
    break;

  case 102:
#line 911 "parser_gram.y"
{
	(yyval.attr) = (yyvsp[0].attr);
}
#line 2838 "parser_gram.c"
    break;

  case 103:
#line 915 "parser_gram.y"
{
	attr_t *attr = NULL;
	attr_t *list_end = NULL;

	for (attr = (yyvsp[-2].attr); attr != NULL;
	    attr = attr->attr_next)
		list_end = attr; /* Find end of list */

	list_end->attr_next = (yyvsp[0].attr);

	(yyval.attr) = (yyvsp[-2].attr);
}
#line 2855 "parser_gram.c"
    break;

  case 104:
#line 929 "parser_gram.y"
{
	(yyval.attr) = (yyvsp[0].attr);
	(yyval.attr)->attr_name = (yyvsp[-2].ival);
}
#line 2864 "parser_gram.c"
    break;

  case 105:
#line 934 "parser_gram.y"
{
	if (((yyval.attr) = alloc_attr()) == NULL)
		YYERROR;
	(yyval.attr)->attr_name = (yyvsp[0].ival);
	(yyval.attr)->attr_avd = avd_bool_alloc(TRUE);
}
#line 2875 "parser_gram.c"
    break;

  case 106:
#line 943 "parser_gram.y"
{
	(yyval.attr) = (yyvsp[0].attr);
}
#line 2883 "parser_gram.c"
    break;

  case 107:
#line 947 "parser_gram.y"
{
	attr_t *attr = NULL;
	attr_t *list_end = NULL;

	for (attr = (yyvsp[-2].attr); attr != NULL;
	    attr = attr->attr_next)
		list_end = attr; /* Find end of list */

	list_end->attr_next = (yyvsp[0].attr);

	(yyval.attr) = (yyvsp[-2].attr);
}
#line 2900 "parser_gram.c"
    break;

  case 108:
#line 960 "parser_gram.y"
{
	attr_t *attr = NULL;
	attr_t *list_end = NULL;

	for (attr = (yyvsp[-2].attr); attr != NULL;
	    attr = attr->attr_next)
		list_end = attr; /* Find end of list */

	list_end->attr_next = (yyvsp[0].attr);

	(yyval.attr) = (yyvsp[-2].attr);
}
#line 2917 "parser_gram.c"
    break;

  case 109:
#line 974 "parser_gram.y"
{
	(yyval.attr) = (yyvsp[0].attr);
	(yyval.attr)->attr_name = (yyvsp[-2].ival);
}
#line 2926 "parser_gram.c"
    break;

  case 110:
#line 979 "parser_gram.y"
{
	if (((yyval.attr) = alloc_attr()) == NULL)
		YYERROR;
	(yyval.attr)->attr_name = (yyvsp[0].ival);
	(yyval.attr)->attr_avd = avd_bool_alloc(TRUE);
}
#line 2937 "parser_gram.c"
    break;

  case 111:
#line 988 "parser_gram.y"
{
	(yyval.attr) = (yyvsp[0].attr);
}
#line 2945 "parser_gram.c"
    break;

  case 112:
#line 992 "parser_gram.y"
{
	attr_t *attr = NULL;
	attr_t *list_end = NULL;

	for (attr = (yyvsp[-2].attr); attr != NULL;
	    attr = attr->attr_next)
		list_end = attr; /* Find end of list */

	list_end->attr_next = (yyvsp[0].attr);

	(yyval.attr) = (yyvsp[-2].attr);
}
#line 2962 "parser_gram.c"
    break;

  case 113:
#line 1006 "parser_gram.y"
{
	(yyval.attr) = (yyvsp[0].attr);
	(yyval.attr)->attr_name = (yyvsp[-2].ival);
}
#line 2971 "parser_gram.c"
    break;

  case 114:
#line 1011 "parser_gram.y"
{
	if (((yyval.attr) = alloc_attr()) == NULL)
		YYERROR;
	(yyval.attr)->attr_name = (yyvsp[0].ival);
	(yyval.attr)->attr_avd = avd_bool_alloc(TRUE);
}
#line 2982 "parser_gram.c"
    break;

  case 115:
#line 1020 "parser_gram.y"
{
	(yyval.attr) = (yyvsp[0].attr);
}
#line 2990 "parser_gram.c"
    break;

  case 116:
#line 1024 "parser_gram.y"
{
	attr_t *attr = NULL;
	attr_t *list_end = NULL;

	for (attr = (yyvsp[-2].attr); attr != NULL;
	    attr = attr->attr_next)
		list_end = attr; /* Find end of list */

	list_end->attr_next = (yyvsp[0].attr);

	(yyval.attr) = (yyvsp[-2].attr);
}
#line 3007 "parser_gram.c"
    break;

  case 117:
#line 1038 "parser_gram.y"
{
	(yyval.attr) = (yyvsp[0].attr);
	(yyval.attr)->attr_name = (yyvsp[-2].ival);
}
#line 3016 "parser_gram.c"
    break;

  case 118:
#line 1044 "parser_gram.y"
{
	(yyval.attr) = (yyvsp[0].attr);
	(yyval.attr)->attr_name = FSA_VALUE;
}
#line 3025 "parser_gram.c"
    break;

  case 119:
#line 1053 "parser_gram.y"
           { (yyval.ival) = FSA_NAME;}
#line 3031 "parser_gram.c"
    break;

  case 120:
#line 1054 "parser_gram.y"
                { (yyval.ival) = FSA_INSTANCES;}
#line 3037 "parser_gram.c"
    break;

  case 121:
#line 1055 "parser_gram.y"
           { (yyval.ival) = FSA_NICE;}
#line 3043 "parser_gram.c"
    break;

  case 122:
#line 1056 "parser_gram.y"
                  { (yyval.ival) = FSA_MLFS_DEV_ID;}
#line 3049 "parser_gram.c"
    break;

  case 123:
#line 1059 "parser_gram.y"
           { (yyval.ival) = FSA_NAME;}
#line 3055 "parser_gram.c"
    break;

  case 124:
#line 1060 "parser_gram.y"
           { (yyval.ival) = FSA_PATH;}
#line 3061 "parser_gram.c"
    break;

  case 125:
#line 1061 "parser_gram.y"
           { (yyval.ival) = FSA_SIZE;}
#line 3067 "parser_gram.c"
    break;

  case 126:
#line 1062 "parser_gram.y"
               { (yyval.ival) = FSA_PREALLOC;}
#line 3073 "parser_gram.c"
    break;

  case 127:
#line 1063 "parser_gram.y"
               { (yyval.ival) = FSA_PARALLOC;}
#line 3079 "parser_gram.c"
    break;

  case 128:
#line 1064 "parser_gram.y"
            { (yyval.ival) = FSA_REUSE;}
#line 3085 "parser_gram.c"
    break;

  case 129:
#line 1065 "parser_gram.y"
                { (yyval.ival) = FSA_TRUSTTREE;}
#line 3091 "parser_gram.c"
    break;

  case 130:
#line 1066 "parser_gram.y"
               { (yyval.ival) = FSA_READONLY;}
#line 3097 "parser_gram.c"
    break;

  case 131:
#line 1067 "parser_gram.y"
                { (yyval.ival) = FSA_WRITEONLY;}
#line 3103 "parser_gram.c"
    break;

  case 132:
#line 1070 "parser_gram.y"
           { (yyval.ival) = FSA_NAME;}
#line 3109 "parser_gram.c"
    break;

  case 133:
#line 1071 "parser_gram.y"
           { (yyval.ival) = FSA_PATH;}
#line 3115 "parser_gram.c"
    break;

  case 134:
#line 1072 "parser_gram.y"
              { (yyval.ival) = FSA_ENTRIES;}
#line 3121 "parser_gram.c"
    break;

  case 135:
#line 1073 "parser_gram.y"
           { (yyval.ival) = FSA_SIZE;}
#line 3127 "parser_gram.c"
    break;

  case 136:
#line 1074 "parser_gram.y"
               { (yyval.ival) = FSA_PREALLOC;}
#line 3133 "parser_gram.c"
    break;

  case 137:
#line 1075 "parser_gram.y"
               { (yyval.ival) = FSA_PARALLOC;}
#line 3139 "parser_gram.c"
    break;

  case 138:
#line 1076 "parser_gram.y"
            { (yyval.ival) = FSA_REUSE;}
#line 3145 "parser_gram.c"
    break;

  case 139:
#line 1077 "parser_gram.y"
                { (yyval.ival) = FSA_TRUSTTREE;}
#line 3151 "parser_gram.c"
    break;

  case 140:
#line 1078 "parser_gram.y"
               { (yyval.ival) = FSA_READONLY;}
#line 3157 "parser_gram.c"
    break;

  case 141:
#line 1079 "parser_gram.y"
                { (yyval.ival) = FSA_WRITEONLY;}
#line 3163 "parser_gram.c"
    break;

  case 142:
#line 1080 "parser_gram.y"
               { (yyval.ival) = FSA_DIRWIDTH;}
#line 3169 "parser_gram.c"
    break;

  case 143:
#line 1081 "parser_gram.y"
                 { (yyval.ival) = FSA_DIRDEPTHRV;}
#line 3175 "parser_gram.c"
    break;

  case 144:
#line 1082 "parser_gram.y"
               { (yyval.ival) = FSA_DIRGAMMA;}
#line 3181 "parser_gram.c"
    break;

  case 145:
#line 1083 "parser_gram.y"
               { (yyval.ival) = FSA_LEAFDIRS;}
#line 3187 "parser_gram.c"
    break;

  case 146:
#line 1086 "parser_gram.y"
           { (yyval.ival) = FSA_NAME;}
#line 3193 "parser_gram.c"
    break;

  case 147:
#line 1087 "parser_gram.y"
               { (yyval.ival) = FSA_RANDSEED;}
#line 3199 "parser_gram.c"
    break;

  case 148:
#line 1088 "parser_gram.y"
                { (yyval.ival) = FSA_RANDGAMMA;}
#line 3205 "parser_gram.c"
    break;

  case 149:
#line 1089 "parser_gram.y"
               { (yyval.ival) = FSA_RANDMEAN;}
#line 3211 "parser_gram.c"
    break;

  case 150:
#line 1090 "parser_gram.y"
          { (yyval.ival) = FSA_MIN;}
#line 3217 "parser_gram.c"
    break;

  case 151:
#line 1091 "parser_gram.y"
            { (yyval.ival) = FSA_ROUND;}
#line 3223 "parser_gram.c"
    break;

  case 152:
#line 1094 "parser_gram.y"
{
	if (((yyval.attr) = alloc_attr()) == NULL)
		YYERROR;
	(yyval.attr)->attr_avd = avd_int_alloc((yyvsp[0].ival));
}
#line 3233 "parser_gram.c"
    break;

  case 153:
#line 1101 "parser_gram.y"
              { (yyval.ival) = FSV_RANDUNI;}
#line 3239 "parser_gram.c"
    break;

  case 154:
#line 1102 "parser_gram.y"
              { (yyval.ival) = FSV_RANDTAB;}
#line 3245 "parser_gram.c"
    break;

  case 155:
#line 1103 "parser_gram.y"
                { (yyval.ival) = FSA_RANDGAMMA;}
#line 3251 "parser_gram.c"
    break;

  case 156:
#line 1106 "parser_gram.y"
{
	if (((yyval.attr) = alloc_attr()) == NULL)
		YYERROR;
	(yyval.attr)->attr_avd = avd_int_alloc((yyvsp[0].ival));
}
#line 3261 "parser_gram.c"
    break;

  case 157:
#line 1113 "parser_gram.y"
            { (yyval.ival) = FSV_URAND;}
#line 3267 "parser_gram.c"
    break;

  case 158:
#line 1114 "parser_gram.y"
             { (yyval.ival) = FSV_RAND48;}
#line 3273 "parser_gram.c"
    break;

  case 159:
#line 1117 "parser_gram.y"
           { (yyval.ival) = FSA_TYPE;}
#line 3279 "parser_gram.c"
    break;

  case 160:
#line 1118 "parser_gram.y"
                 { (yyval.ival) = FSA_PARAMETERS;}
#line 3285 "parser_gram.c"
    break;

  case 161:
#line 1119 "parser_gram.y"
          { (yyval.ival) = FSA_MIN;}
#line 3291 "parser_gram.c"
    break;

  case 162:
#line 1120 "parser_gram.y"
          { (yyval.ival) = FSA_MAX;}
#line 3297 "parser_gram.c"
    break;

  case 163:
#line 1121 "parser_gram.y"
            { (yyval.ival) = FSA_ROUND;}
#line 3303 "parser_gram.c"
    break;

  case 164:
#line 1124 "parser_gram.y"
           { (yyval.ival) = FSA_NAME;}
#line 3309 "parser_gram.c"
    break;

  case 165:
#line 1125 "parser_gram.y"
              { (yyval.ival) = FSA_MEMSIZE;}
#line 3315 "parser_gram.c"
    break;

  case 166:
#line 1126 "parser_gram.y"
             { (yyval.ival) = FSA_USEISM;}
#line 3321 "parser_gram.c"
    break;

  case 167:
#line 1127 "parser_gram.y"
                { (yyval.ival) = FSA_INSTANCES;}
#line 3327 "parser_gram.c"
    break;

  case 168:
#line 1128 "parser_gram.y"
             { (yyval.ival) = FSA_IOPRIO;}
#line 3333 "parser_gram.c"
    break;

  case 169:
#line 1131 "parser_gram.y"
          { (yyval.ival) = FSA_WSS;}
#line 3339 "parser_gram.c"
    break;

  case 170:
#line 1132 "parser_gram.y"
               { (yyval.ival) = FSA_FILENAME;}
#line 3345 "parser_gram.c"
    break;

  case 171:
#line 1133 "parser_gram.y"
           { (yyval.ival) = FSA_NAME;}
#line 3351 "parser_gram.c"
    break;

  case 172:
#line 1134 "parser_gram.y"
             { (yyval.ival) = FSA_RANDOM;}
#line 3357 "parser_gram.c"
    break;

  case 173:
#line 1135 "parser_gram.y"
         { (yyval.ival) = FSA_FD;}
#line 3363 "parser_gram.c"
    break;

  case 174:
#line 1136 "parser_gram.y"
            { (yyval.ival) = FSA_SRCFD;}
#line 3369 "parser_gram.c"
    break;

  case 175:
#line 1137 "parser_gram.y"
               { (yyval.ival) = FSA_ROTATEFD;}
#line 3375 "parser_gram.c"
    break;

  case 176:
#line 1138 "parser_gram.y"
            { (yyval.ival) = FSA_DSYNC;}
#line 3381 "parser_gram.c"
    break;

  case 177:
#line 1139 "parser_gram.y"
               { (yyval.ival) = FSA_DIRECTIO;}
#line 3387 "parser_gram.c"
    break;

  case 178:
#line 1140 "parser_gram.y"
              { (yyval.ival) = FSA_INDEXED;}
#line 3393 "parser_gram.c"
    break;

  case 179:
#line 1141 "parser_gram.y"
             { (yyval.ival) = FSA_TARGET;}
#line 3399 "parser_gram.c"
    break;

  case 180:
#line 1142 "parser_gram.y"
            { (yyval.ival) = FSA_ITERS;}
#line 3405 "parser_gram.c"
    break;

  case 181:
#line 1143 "parser_gram.y"
            { (yyval.ival) = FSA_VALUE;}
#line 3411 "parser_gram.c"
    break;

  case 182:
#line 1144 "parser_gram.y"
               { (yyval.ival) = FSA_BLOCKING;}
#line 3417 "parser_gram.c"
    break;

  case 183:
#line 1145 "parser_gram.y"
                { (yyval.ival) = FSA_HIGHWATER;}
#line 3423 "parser_gram.c"
    break;

  case 184:
#line 1146 "parser_gram.y"
             { (yyval.ival) = FSA_IOSIZE;}
#line 3429 "parser_gram.c"
    break;

  case 185:
#line 1147 "parser_gram.y"
                  { (yyval.ival) = FSA_NOREADAHEAD;}
#line 3435 "parser_gram.c"
    break;

  case 186:
#line 1150 "parser_gram.y"
           { (yyval.ival) = FSA_RATE;}
#line 3441 "parser_gram.c"
    break;

  case 187:
#line 1153 "parser_gram.y"
             { (yyval.ival) = FSA_MASTER;}
#line 3447 "parser_gram.c"
    break;

  case 188:
#line 1154 "parser_gram.y"
             { (yyval.ival) = FSA_CLIENT;}
#line 3453 "parser_gram.c"
    break;

  case 189:
#line 1157 "parser_gram.y"
{
	(yyval.attr) = (yyvsp[0].attr);
}
#line 3461 "parser_gram.c"
    break;

  case 190:
#line 1161 "parser_gram.y"
{
	attr_t *attr = NULL;
	attr_t *list_end = NULL;

	for (attr = (yyvsp[-2].attr); attr != NULL;
	    attr = attr->attr_next)
		list_end = attr; /* Find end of list */

	list_end->attr_next = (yyvsp[0].attr);

	(yyval.attr) = (yyvsp[-2].attr);
}
#line 3478 "parser_gram.c"
    break;

  case 191:
#line 1174 "parser_gram.y"
{
	attr_t *attr = NULL;
	attr_t *list_end = NULL;

	for (attr = (yyvsp[-2].attr); attr != NULL;
	    attr = attr->attr_next)
		list_end = attr; /* Find end of list */

	list_end->attr_next = (yyvsp[0].attr);

	(yyval.attr) = (yyvsp[-2].attr);
}
#line 3495 "parser_gram.c"
    break;

  case 192:
#line 1188 "parser_gram.y"
{
	(yyval.attr) = (yyvsp[0].attr);
	(yyval.attr)->attr_name = (yyvsp[-2].ival);
}
#line 3504 "parser_gram.c"
    break;

  case 193:
#line 1194 "parser_gram.y"
{
	if (((yyval.attr) = alloc_lvar_attr(var_lvar_assign_boolean((yyvsp[-2].sval), (yyvsp[0].bval)))) == NULL)
		YYERROR;
}
#line 3513 "parser_gram.c"
    break;

  case 194:
#line 1199 "parser_gram.y"
{
	if (((yyval.attr) = alloc_lvar_attr(var_lvar_assign_integer((yyvsp[-2].sval), (yyvsp[0].ival)))) == NULL)
		YYERROR;
}
#line 3522 "parser_gram.c"
    break;

  case 195:
#line 1204 "parser_gram.y"
{
	if (((yyval.attr) = alloc_lvar_attr(var_lvar_assign_string((yyvsp[-4].sval), (yyvsp[-1].sval)))) == NULL)
		YYERROR;
}
#line 3531 "parser_gram.c"
    break;

  case 196:
#line 1209 "parser_gram.y"
{
	if (((yyval.attr) = alloc_lvar_attr(var_lvar_assign_string((yyvsp[-2].sval), (yyvsp[0].sval)))) == NULL)
		YYERROR;
}
#line 3540 "parser_gram.c"
    break;

  case 197:
#line 1214 "parser_gram.y"
{
	if (((yyval.attr) = alloc_lvar_attr(var_lvar_assign_var((yyvsp[-2].sval), (yyvsp[0].sval)))) == NULL)
		YYERROR;
}
#line 3549 "parser_gram.c"
    break;

  case 198:
#line 1219 "parser_gram.y"
{
	if (((yyval.attr) = alloc_lvar_attr(var_lvar_alloc_local((yyvsp[0].sval)))) == NULL)
		YYERROR;
}
#line 3558 "parser_gram.c"
    break;

  case 199:
#line 1225 "parser_gram.y"
{
	if (((yyval.cmd) = alloc_cmd()) == NULL)
		YYERROR;
	(yyval.cmd)->cmd = &parser_composite_flowop_define;
	(yyval.cmd)->cmd_list = (yyvsp[-1].cmd);
	(yyval.cmd)->cmd_attr_list = (yyvsp[-3].attr);
}
#line 3570 "parser_gram.c"
    break;

  case 200:
#line 1233 "parser_gram.y"
{
	(yyvsp[-1].cmd)->cmd_attr_list = (yyvsp[0].attr);
}
#line 3578 "parser_gram.c"
    break;

  case 201:
#line 1239 "parser_gram.y"
{
	(yyval.attr) = (yyvsp[0].attr);
}
#line 3586 "parser_gram.c"
    break;

  case 202:
#line 1243 "parser_gram.y"
{
	attr_t *attr = NULL;
	attr_t *list_end = NULL;

	for (attr = (yyvsp[-2].attr); attr != NULL;
	    attr = attr->attr_next)
		list_end = attr; /* Find end of list */

	list_end->attr_next = (yyvsp[0].attr);

	(yyval.attr) = (yyvsp[-2].attr);
}
#line 3603 "parser_gram.c"
    break;

  case 203:
#line 1257 "parser_gram.y"
{
	(yyval.attr) = (yyvsp[0].attr);
	(yyval.attr)->attr_name = (yyvsp[-2].ival);
}
#line 3612 "parser_gram.c"
    break;

  case 204:
#line 1263 "parser_gram.y"
           { (yyval.ival) = FSA_NAME;}
#line 3618 "parser_gram.c"
    break;

  case 205:
#line 1264 "parser_gram.y"
            { (yyval.ival) = FSA_ITERS;}
#line 3624 "parser_gram.c"
    break;

  case 206:
#line 1267 "parser_gram.y"
{
	(yyval.attr) = alloc_attr();
	if (!(yyval.attr))
		YYERROR;
	(yyval.attr)->attr_avd = avd_str_alloc((yyvsp[0].sval));
}
#line 3635 "parser_gram.c"
    break;

  case 207:
#line 1272 "parser_gram.y"
                                        {
	(yyval.attr) = alloc_attr();
	if (!(yyval.attr))
		YYERROR;
	(yyval.attr)->attr_avd = avd_str_alloc((yyvsp[-1].sval));
}
#line 3646 "parser_gram.c"
    break;

  case 208:
#line 1277 "parser_gram.y"
                   {
	(yyval.attr) = alloc_attr();
	if (!(yyval.attr))
		YYERROR;
	(yyval.attr)->attr_avd = avd_int_alloc((yyvsp[0].ival));
}
#line 3657 "parser_gram.c"
    break;

  case 209:
#line 1282 "parser_gram.y"
                    {
	(yyval.attr) = alloc_attr();
	if (!(yyval.attr))
		YYERROR;
	(yyval.attr)->attr_avd = avd_bool_alloc((yyvsp[0].bval));
}
#line 3668 "parser_gram.c"
    break;

  case 210:
#line 1287 "parser_gram.y"
                 {
	(yyval.attr) = alloc_attr();
	if (!(yyval.attr))
		YYERROR;
	(yyval.attr)->attr_avd = avd_var_alloc((yyvsp[0].sval));
}
#line 3679 "parser_gram.c"
    break;

  case 211:
#line 1295 "parser_gram.y"
{
	(yyval.avd) = avd_int_alloc((yyvsp[0].ival));
}
#line 3687 "parser_gram.c"
    break;

  case 212:
#line 1298 "parser_gram.y"
{
	(yyval.avd) = avd_var_alloc((yyvsp[0].sval));
}
#line 3695 "parser_gram.c"
    break;


#line 3699 "parser_gram.c"

      default: break;
    }
  /* User semantic actions sometimes alter yychar, and that requires
     that yytoken be updated with the new translation.  We take the
     approach of translating immediately before every use of yytoken.
     One alternative is translating here after every semantic action,
     but that translation would be missed if the semantic action invokes
     YYABORT, YYACCEPT, or YYERROR immediately after altering yychar or
     if it invokes YYBACKUP.  In the case of YYABORT or YYACCEPT, an
     incorrect destructor might then be invoked immediately.  In the
     case of YYERROR or YYBACKUP, subsequent parser actions might lead
     to an incorrect destructor call or verbose syntax error message
     before the lookahead is translated.  */
  YY_SYMBOL_PRINT ("-> $$ =", yyr1[yyn], &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);

  *++yyvsp = yyval;

  /* Now 'shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */
  {
    const int yylhs = yyr1[yyn] - YYNTOKENS;
    const int yyi = yypgoto[yylhs] + *yyssp;
    yystate = (0 <= yyi && yyi <= YYLAST && yycheck[yyi] == *yyssp
               ? yytable[yyi]
               : yydefgoto[yylhs]);
  }

  goto yynewstate;


/*--------------------------------------.
| yyerrlab -- here on detecting error.  |
`--------------------------------------*/
yyerrlab:
  /* Make sure we have latest lookahead translation.  See comments at
     user semantic actions for why this is necessary.  */
  yytoken = yychar == YYEMPTY ? YYEMPTY : YYTRANSLATE (yychar);

  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
#if ! YYERROR_VERBOSE
      yyerror (YY_("syntax error"));
#else
# define YYSYNTAX_ERROR yysyntax_error (&yymsg_alloc, &yymsg, \
                                        yyssp, yytoken)
      {
        char const *yymsgp = YY_("syntax error");
        int yysyntax_error_status;
        yysyntax_error_status = YYSYNTAX_ERROR;
        if (yysyntax_error_status == 0)
          yymsgp = yymsg;
        else if (yysyntax_error_status == 1)
          {
            if (yymsg != yymsgbuf)
              YYSTACK_FREE (yymsg);
            yymsg = YY_CAST (char *, YYSTACK_ALLOC (YY_CAST (YYSIZE_T, yymsg_alloc)));
            if (!yymsg)
              {
                yymsg = yymsgbuf;
                yymsg_alloc = sizeof yymsgbuf;
                yysyntax_error_status = 2;
              }
            else
              {
                yysyntax_error_status = YYSYNTAX_ERROR;
                yymsgp = yymsg;
              }
          }
        yyerror (yymsgp);
        if (yysyntax_error_status == 2)
          goto yyexhaustedlab;
      }
# undef YYSYNTAX_ERROR
#endif
    }



  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
         error, discard it.  */

      if (yychar <= YYEOF)
        {
          /* Return failure if at end of input.  */
          if (yychar == YYEOF)
            YYABORT;
        }
      else
        {
          yydestruct ("Error: discarding",
                      yytoken, &yylval);
          yychar = YYEMPTY;
        }
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:
  /* Pacify compilers when the user code never invokes YYERROR and the
     label yyerrorlab therefore never appears in user code.  */
  if (0)
    YYERROR;

  /* Do not reclaim the symbols of the rule whose action triggered
     this YYERROR.  */
  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);
  yystate = *yyssp;
  goto yyerrlab1;


/*-------------------------------------------------------------.
| yyerrlab1 -- common code for both syntax error and YYERROR.  |
`-------------------------------------------------------------*/
yyerrlab1:
  yyerrstatus = 3;      /* Each real token shifted decrements this.  */

  for (;;)
    {
      yyn = yypact[yystate];
      if (!yypact_value_is_default (yyn))
        {
          yyn += YYTERROR;
          if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYTERROR)
            {
              yyn = yytable[yyn];
              if (0 < yyn)
                break;
            }
        }

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
        YYABORT;


      yydestruct ("Error: popping",
                  yystos[yystate], yyvsp);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END


  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", yystos[yyn], yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturn;


/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturn;


#if !defined yyoverflow || YYERROR_VERBOSE
/*-------------------------------------------------.
| yyexhaustedlab -- memory exhaustion comes here.  |
`-------------------------------------------------*/
yyexhaustedlab:
  yyerror (YY_("memory exhausted"));
  yyresult = 2;
  /* Fall through.  */
#endif


/*-----------------------------------------------------.
| yyreturn -- parsing is finished, return the result.  |
`-----------------------------------------------------*/
yyreturn:
  if (yychar != YYEMPTY)
    {
      /* Make sure we have latest lookahead translation.  See comments at
         user semantic actions for why this is necessary.  */
      yytoken = YYTRANSLATE (yychar);
      yydestruct ("Cleanup: discarding lookahead",
                  yytoken, &yylval);
    }
  /* Do not reclaim the symbols of the rule whose action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
                  yystos[+*yyssp], yyvsp);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
#if YYERROR_VERBOSE
  if (yymsg != yymsgbuf)
    YYSTACK_FREE (yymsg);
#endif
  return yyresult;
}
#line 1302 "parser_gram.y"


/*
 * The following C routines implement the various commands defined in the above
 * yacc parser code. The yacc portion checks the syntax of the commands found
 * in a workload file and parses the commands' parameters into lists. The lists
 * are then passed in a cmd_t struct for each command to its related routine in
 * the following section for actual execution.  This section also includes a
 * few utility routines and the main entry point for the program.
 */

#define	USAGE \
"Usage: " \
"filebench {-f <wmlscript> | -h | -c [cvartype]}\n\n" \
"  Filebench version " FILEBENCH_VERSION "\n\n" \
"  Filebench is a file system and storage benchmark that interprets a script\n" \
"  written in its Workload Model Language (WML), and procees to generate the\n" \
"  specified workload. Refer to the README for more details.\n\n" \
"  Visit github.com/filebench/filebench for WML definition and tutorials.\n\n" \
"Options:\n" \
"   -f <wmlscript> generate workload from the specified file\n" \
"   -h             display this help message\n" \
"   -c             display supported cvar types\n" \
"   -c [cvartype]  display options of the specific cvar type\n\n"

static void
usage_exit(int ret, const char *msg)
{
	if (ret) {
		(void)fprintf(stderr, "Usage error: %s\n\n", msg);
		(void)fprintf(stderr, USAGE);
	} else
		printf(USAGE);
	exit(ret);
}

struct fbparams {
	char *execname;
	char *fscriptname;
	char *procname;
	char *shmaddr;
	char *shmpath;
	int instance;
	char *cvartype;
};

static void
init_fbparams(struct fbparams *fbparams)
{
	memset(fbparams, 0, sizeof(*fbparams));
	fbparams->instance = -1;
}

#define FB_MODE_NONE		0
#define FB_MODE_HELP		1
#define FB_MODE_MASTER		2
#define FB_MODE_WORKER		3
#define FB_MODE_CVARS		4

static int
parse_options(int argc, char *argv[], struct fbparams *fbparams)
{
	const char cmd_options[] = "m:s:a:i:hf:c:";
	int mode = FB_MODE_NONE;
	int opt;

	init_fbparams(fbparams);
	fbparams->execname = argv[0];

 	/*
	 * We don't want getopt() to print error messages because
	 * sometimes what it percieves as an error is actually not
	 * an error.  For example, "-c" option might have or might
	 * not have an argument.  If opterr is non-zero, getopt()
	 * prints an error message when "-c"'s argument is missing.
	 */
	opterr = 0;

	/* Either
		(-f <wmlscript>) or
		(-a and -s and -m and -i) or
		(-c [cvartype]) or
		(-h)
	   must be specified */
	while ((opt = getopt(argc, argv, cmd_options)) > 0) {
		switch (opt) {
		/* public parameters */
		case 'h':
			if (mode != FB_MODE_NONE)
				usage_exit(1, "Too many options specified");
			mode = FB_MODE_HELP;
			break;
		case 'c':
			if (mode != FB_MODE_NONE)
				usage_exit(1, "Too many options specified");
			mode = FB_MODE_CVARS;
			fbparams->cvartype = optarg;
			break;
		case 'f':
			if (mode != FB_MODE_NONE)
				usage_exit(1, "Too many options specified");
			mode = FB_MODE_MASTER;
			fbparams->fscriptname = optarg;
			break;
		/* private parameters: when filebench calls itself */
		case 'a':
			if (mode != FB_MODE_NONE &&
				(mode != FB_MODE_WORKER || fbparams->procname))
					usage_exit(1, "Too many options");
			mode = FB_MODE_WORKER;
			fbparams->procname = optarg;
			break;
		case 's':
			if (mode != FB_MODE_NONE &&
				(mode != FB_MODE_WORKER || fbparams->shmaddr))
					usage_exit(1, "Too many options");
			mode = FB_MODE_WORKER;
			sscanf(optarg, "%p", &fbparams->shmaddr);
			break;
		case 'm':
			if (mode != FB_MODE_NONE &&
				(mode != FB_MODE_WORKER || fbparams->shmpath))
					usage_exit(1, "Too many options");
			mode = FB_MODE_WORKER;
			fbparams->shmpath = optarg;
			break;
		case 'i':
			if (mode != FB_MODE_NONE &&
				(mode != FB_MODE_WORKER || fbparams->instance != -1))
					usage_exit(1, "Too many options");
			mode = FB_MODE_WORKER;
			sscanf(optarg, "%d", &fbparams->instance);
			break;
		case '?':
			if (optopt == 'c') {
				if (mode != FB_MODE_NONE)
					usage_exit(1, "Too many options");
				mode = FB_MODE_CVARS;
				break;
			}
		default:
			usage_exit(1, "Unrecognized option");
			break;
		}
	}

	if (mode == FB_MODE_NONE)
		usage_exit(1, "No runtime options specified");

	if (mode == FB_MODE_WORKER) {
		if (!fbparams->procname ||
			!fbparams->shmaddr ||
			!fbparams->shmpath ||
			fbparams->instance == -1)
			usage_exit(1, "Invalid worker settings");
	}

	return mode;
}

static void
worker_mode(struct fbparams *fbparams)
{
	int ret;

	ret = ipc_attach(fbparams->shmaddr, fbparams->shmpath);
	if (ret < 0) {
		filebench_log(LOG_FATAL, "Cannot attach shm for %s",
		    fbparams->procname);
		exit(1);
	}

	/* get correct function pointer for each working process */
	flowop_init(0);

	/* load custom variable libraries and revalidate handles */
	ret = init_cvar_libraries();
	if (ret)
		exit(1);

	ret = revalidate_cvar_handles();
	if (ret)
		exit(1);

	/* execute corresponding procflow */
	ret = procflow_exec(fbparams->procname, fbparams->instance);
	if (ret < 0) {
		filebench_log(LOG_FATAL, "Cannot startup process %s",
		    fbparams->procname);
		exit(1);
	}

	exit(0);
}

void parser_list_cvar_types(void)
{
	cvar_library_info_t *t;

	if (!filebench_shm->shm_cvar_lib_info_list) {
		printf("No custom variables supported.\n");
		return;
	}

	printf("Custom variable types supported:\n");
	for (t = filebench_shm->shm_cvar_lib_info_list; t; t = t->next)
		printf("  %s\n", t->type);

	return;
}

void parser_list_cvar_type_parameters(char *type)
{
	const char *version = NULL;
	const char *usage = NULL;

	cvar_library_info_t *t;

	for (t = filebench_shm->shm_cvar_lib_info_list; t != NULL; t = t->next) {
		if (!strcmp(type, t->type))
			break;
	}

	if (!t) {
		printf("Unknown custom variable type %s.\n", type);
		return;
	}

	printf("Custom variable type: %s\n", t->type);
	printf("Supporting library: %s\n", t->filename);

	if (cvar_libraries[t->index]->cvar_op.cvar_version)
		version = cvar_libraries[t->index]->cvar_op.cvar_version();

	if (cvar_libraries[t->index]->cvar_op.cvar_usage)
		usage = cvar_libraries[t->index]->cvar_op.cvar_usage();


	if (version)
		printf("Version: %s\n", version);
	else
		printf("Oops. No version information provided.\n");

	if (usage)
		printf("Usage:\n%s\n", usage);
	else
		printf("Oops. No usage information provided.\n");

	return;
}

static void
cvars_mode(struct fbparams *fbparams)
{
	int ret;

	ipc_init();

	ret = init_cvar_library_info(FBDATADIR "/cvars");
	if (ret)
		filebench_shutdown(1);

	ret = init_cvar_libraries();
	if (ret)
		filebench_shutdown(1);

	if (fbparams->cvartype)
		parser_list_cvar_type_parameters(fbparams->cvartype);
	else
		parser_list_cvar_types();

	ipc_fini();

	exit(0);
}

/*
 * Shutdown filebench.
 */
static void
parser_abort(int arg)
{
	(void) sigignore(SIGINT);
	filebench_log(LOG_INFO, "Aborting...");
	filebench_shutdown(1);
}

static void
master_mode(struct fbparams *fbparams) {
	int ret;

	printf("Filebench Version %s\n", FILEBENCH_VERSION);

	yyin = fopen(fbparams->fscriptname, "r");
	if (!yyin) {
		filebench_log(LOG_FATAL,
			"Cannot open file %s!", fbparams->fscriptname);
		exit(1);
	}

	execname = fbparams->execname;
	fb_set_shmmax();

	ipc_init();

	/* Below we initialize things that depend on IPC */
	(void)strcpy(filebench_shm->shm_fscriptname,
				fbparams->fscriptname);

	flowop_init(1);
	eventgen_init();

	/* Initialize custom variables. */
	ret = init_cvar_library_info(FBDATADIR "/cvars");
	if (ret)
		filebench_shutdown(1);

	ret = init_cvar_libraries();
	if (ret)
		filebench_shutdown(1);

	signal(SIGINT, parser_abort);

	/* yyparse() after it parsed complete grammar */
	yyparse();

	/* We only get here if there was no
	   run (or similar) command in the
	   end of the WML script. */
	printf("Warning: no run command in the WML script!\n");
	parser_filebench_shutdown((cmd_t *)0);
}

static void
init_common()
{
	disable_aslr();
	my_pid = getpid();
	fb_set_rlimit();
}

/*
 * Entry point for Filebench. Processes command line arguments. The -f option
 * will read in a workload file (the full name and extension must must be
 * given). The -a, -s, -m and -i options are used by the worker process to
 * receive the name, the base address of shared memory, its path, and the
 * process' instance number, respectively. This information is supplied by the
 * master process when it execs worker processes. If the worker process
 * arguments are passed then main will call the procflow_exec() routine which
 * creates worker threadflows and flowops and executes the procflow's portion of
 * the workload model until completion. If worker process arguments are not
 * passed to the process, then it becomes the master process for a filebench
 * run. It initializes the various filebench components and either executes the
 * supplied workload file, or enters interactive mode.
 */
int
main(int argc, char *argv[])
{
	struct fbparams fbparams;
	int mode;

	/* parse_options() exits if detects wrong usage */
	mode = parse_options(argc, argv, &fbparams);

	if (mode == FB_MODE_HELP)
		usage_exit(0, NULL);

	if (mode == FB_MODE_CVARS)
		cvars_mode(&fbparams);

	init_common();

	/* MLFS: Initialize */
	{
		const char *shell_mlfs;
		const char *nic;
		shell_mlfs = getenv("MLFS");
		nic = getenv("NIC_OFFLOAD");
		if (nic) {
			nic_offload = 1;
		}

		if (shell_mlfs) {
			mlfs = 1;
			printf("initialize mlfs\n");
			if (mode == FB_MODE_MASTER)
				init_fs();
			else if (mode == FB_MODE_WORKER)
				init_fs();
		}	
	}

	if (mode == FB_MODE_MASTER)
		master_mode(&fbparams);

	if (mode == FB_MODE_WORKER)
		worker_mode(&fbparams);

	/* We should never reach this point */
	return 0;
}

/*
 * Converts a list of var_strings or ordinary strings to a single ordinary
 * string. It returns a pointer to the string (in malloc'd memory) if found,
 * or NULL otherwise.
 */
char *
parser_list2string(list_t *list)
{
	list_t *l;
	char *string;
	char *tmp;

	string = malloc(MAXPATHLEN);
	if (!string) {
		filebench_log(LOG_ERROR, "Failed to allocate memory");
		return NULL;
	}

	*string = 0;

	/* Format args */
	for (l = list; l != NULL; l = l->list_next) {

		char *lstr = avd_get_str(l->list_string);

		filebench_log(LOG_DEBUG_SCRIPT, "converting string '%s'", lstr);

		/* see if it is a random variable */
		if (l->list_integer) {
			fbint_t param_name;

			tmp = NULL;
			param_name = avd_get_int(l->list_integer);

			switch (param_name) {
			case FSS_TYPE:
				tmp = var_randvar_to_string(lstr,
				    RAND_PARAM_TYPE);
				break;

			case FSS_SRC:
				tmp = var_randvar_to_string(lstr,
				    RAND_PARAM_SRC);
				break;

			case FSS_SEED:
				tmp = var_randvar_to_string(lstr,
				    RAND_PARAM_SEED);
				break;

			case FSS_MIN:
				tmp = var_randvar_to_string(lstr,
				    RAND_PARAM_MIN);
				break;

			case FSS_MEAN:
				tmp = var_randvar_to_string(lstr,
				    RAND_PARAM_MEAN);
				break;

			case FSS_GAMMA:
				tmp = var_randvar_to_string(lstr,
				    RAND_PARAM_GAMMA);
				break;

			case FSS_ROUND:
				tmp = var_randvar_to_string(lstr,
				    RAND_PARAM_ROUND);
				break;
			}

			if (tmp) {
				(void) strcat(string, tmp);
				free(tmp);
			} else {
				(void) strcat(string, lstr);
			}
		} else {
			/* perhaps a normal variable? */
			if ((tmp = var_to_string(lstr)) != NULL) {
				(void) strcat(string, tmp);
				free(tmp);
			} else {
				(void) strcat(string, lstr);
			}
		}
	}

	return string;
}

/*
 * If the list just contains a single string starting with '$', then find
 * or create the named var and return the var's var_string component.
 * Otherwise, convert the list to a string, and allocate a var_string
 * containing a copy of that string. On failure either returns NULL
 * or shuts down the run.
 */
avd_t
parser_list2varstring(list_t *list)
{
	char *lstr = avd_get_str(list->list_string);

	/* Special case - variable name */
	if ((list->list_next == NULL) && (*lstr == '$'))
		return avd_var_alloc(lstr);

	return (avd_str_alloc(parser_list2string(list)));
}

/*
 * Looks for the var named in list_string of the first element of the
 * supplied list. If found, returns the var_val portion of the var in
 * an attribute value descriptor. If the var is not found, cannot be
 * allocated, the supplied list is NULL, or the list_string filed is
 * empty, returns NULL.
 */
avd_t
parser_list2avd(list_t *list)
{
	avd_t avd;
	char *lstr;

	if (list && ((lstr = avd_get_str(list->list_string)) != NULL)) {
		avd = avd_var_alloc(lstr);
		return (avd);
	}

	return (NULL);
}

/*
 * Sets the event generator rate from the attribute supplied with the
 * command. If the attribute doesn't exist the routine does nothing.
 */
static void
parser_eventgen(cmd_t *cmd)
{
	attr_t *attr;

	/* Get the rate from attribute */
	if ((attr = get_attr(cmd, FSA_RATE))) {
		if (attr->attr_avd) {
			eventgen_setrate(attr->attr_avd);
		}
	}
}

/*
 * Lists the fileset name, path name and average size for all defined
 * filesets.
 */
static void
parser_fileset_list(cmd_t *cmd)
{
	(void) fileset_iter(fileset_print);
}

/*
 * Lists the flowop name and instance number for all flowops.
 */
static void
parser_flowop_list(cmd_t *cmd)
{
	flowop_printall();
}

/*
 * Calls procflow_define() to allocate "instances" number of  procflow(s)
 * (processes) with the supplied name. The default number of instances is
 * one. An optional priority level attribute can be supplied and is stored in
 * pf_nice. Finally the routine loops through the list of inner commands, if
 * any, which are defines for threadflows, and passes them one at a time to
 * parser_thread_define() to allocate threadflow entities for the process(es).
 */
static void
parser_proc_define(cmd_t *cmd)
{
	procflow_t *procflow;
	char *name = NULL;
	attr_t *attr;
	avd_t var_instances;
	fbint_t instances;
	cmd_t *inner_cmd;

	attr = get_attr(cmd, FSA_NAME);
	if (attr)
		name = avd_get_str(attr->attr_avd);
	else {
		filebench_log(LOG_ERROR, "process specifies no name");
		filebench_shutdown(1);
	}

	attr = get_attr(cmd, FSA_INSTANCES);
	if (attr) {
		var_instances = attr->attr_avd;
		instances = avd_get_int(var_instances);
		filebench_log(LOG_DEBUG_IMPL,
		    "Setting instances = %llu", (u_longlong_t)instances);
	} else {
		filebench_log(LOG_DEBUG_IMPL,
		    "Defaulting to instances = 1");
		var_instances = avd_int_alloc(1);
		instances = 1;
	}

	procflow = procflow_define(name, var_instances);
	if (!procflow) {
		filebench_log(LOG_ERROR,
		    "Failed to instantiate %d %s process(es)\n",
		    instances, name);
		filebench_shutdown(1);
	}

	attr = get_attr(cmd, FSA_NICE);
	if (attr) {
		filebench_log(LOG_DEBUG_IMPL, "Setting pri = %llu",
			    (u_longlong_t)avd_get_int(attr->attr_avd));
		procflow->pf_nice = attr->attr_avd;
	} else
		procflow->pf_nice = avd_int_alloc(0);

	attr = get_attr(cmd, FSA_MLFS_DEV_ID);
	if(attr) {
		filebench_log(LOG_DEBUG_IMPL, "Setting DEV_ID = %d",
			(int)avd_get_int(attr->attr_avd));
		procflow->mlfs_dev_id = (int)avd_get_int(attr->attr_avd);
	} else
		procflow->mlfs_dev_id = 4;

	/* Create the list of threads for this process  */
	for (inner_cmd = cmd->cmd_list; inner_cmd;
	    	inner_cmd = inner_cmd->cmd_next)
		parser_thread_define(inner_cmd, procflow);
}

/*
 * Calls threadflow_define() to allocate "instances" number of  threadflow(s)
 * (threads) with the supplied name. The default number of instances is
 * one. Two other optional attributes may be supplied, one to set the memory
 * size, stored in tf_memsize, and to select the use of Interprocess Shared
 * Memory, which sets the THREADFLOW_USEISM flag in tf_attrs. Finally
 * the routine loops through the list of inner commands, if any, which are
 * defines for flowops, and passes them one at a time to
 * parser_flowop_define() to allocate flowop entities for the threadflows.
 */
static void
parser_thread_define(cmd_t *cmd, procflow_t *procflow)
{
	threadflow_t *threadflow, template;
	attr_t *attr;
	avd_t instances;
	cmd_t *inner_cmd;
	char *name = NULL;

	memset(&template, 0, sizeof (threadflow_t));

	attr = get_attr(cmd, FSA_NAME);
	if (attr)
		name = avd_get_str(attr->attr_avd);
	else {
		filebench_log(LOG_ERROR,
		    "thread in process %s specifies no name",
		    procflow->pf_name);
		filebench_shutdown(1);
	}

	attr = get_attr(cmd, FSA_INSTANCES);
	if (attr)
		instances = attr->attr_avd;
	else
		instances = avd_int_alloc(1);

	attr = get_attr(cmd, FSA_MEMSIZE);
	if (attr)
		template.tf_memsize = attr->attr_avd;
	else /* XXX: really, memsize zero is default?.. */
		template.tf_memsize = avd_int_alloc(0);
	
	attr = get_attr(cmd, FSA_IOPRIO);
	if (attr)
		template.tf_ioprio = attr->attr_avd;
	else /* XXX: really, ioprio is 8 by default?.. */
		template.tf_ioprio = avd_int_alloc(8);


	threadflow = threadflow_define(procflow, name, &template, instances);
	if (!threadflow) {
		filebench_log(LOG_ERROR,
		    "failed to instantiate thread\n");
		filebench_shutdown(1);
	}

	attr = get_attr(cmd, FSA_USEISM);
	if (attr)
		threadflow->tf_attrs |= THREADFLOW_USEISM;

	/* create the list of flowops */
	for (inner_cmd = cmd->cmd_list; inner_cmd;
	    inner_cmd = inner_cmd->cmd_next)
		parser_flowop_define(inner_cmd, threadflow,
		    &threadflow->tf_thrd_fops, FLOW_MASTER);
}

/*
 * Fills in the attributes for a newly allocated flowop
 */
static void
parser_flowop_get_attrs(cmd_t *cmd, flowop_t *flowop)
{
	attr_t *attr;

	/* Get the filename from attribute */
	if ((attr = get_attr(cmd, FSA_FILENAME))) {
		flowop->fo_filename = attr->attr_avd;
		if (flowop->fo_filename == NULL) {
			filebench_log(LOG_ERROR,
			    "define flowop: no filename specfied");
			filebench_shutdown(1);
		}
	} else {
		/* no filename attribute specified */
		flowop->fo_filename = NULL;
	}

	/* Get the iosize of the op */
	if ((attr = get_attr(cmd, FSA_IOSIZE)))
		flowop->fo_iosize = attr->attr_avd;
	else
		flowop->fo_iosize = avd_int_alloc(0);

	/* Get the working set size of the op */
	if ((attr = get_attr(cmd, FSA_WSS)))
		flowop->fo_wss = attr->attr_avd;
	else
		flowop->fo_wss = avd_int_alloc(0);

	/* Random I/O? */
	if ((attr = get_attr(cmd, FSA_RANDOM)))
		flowop->fo_random = attr->attr_avd;
	else
		flowop->fo_random = avd_bool_alloc(FALSE);

	/* Sync I/O? */
	if ((attr = get_attr(cmd, FSA_DSYNC)))
		flowop->fo_dsync = attr->attr_avd;
	else
		flowop->fo_dsync = avd_bool_alloc(FALSE);

	/* Target, for wakeup etc */
	if ((attr = get_attr(cmd, FSA_TARGET)))
		(void) strcpy(flowop->fo_targetname,
		    avd_get_str(attr->attr_avd));

	/* Value */
	if ((attr = get_attr(cmd, FSA_VALUE)))
		flowop->fo_value = attr->attr_avd;
	else
		flowop->fo_value = avd_int_alloc(0);

	/* FD */
	if ((attr = get_attr(cmd, FSA_FD))) {
		flowop->fo_fdnumber = avd_get_int(attr->attr_avd);
		if (flowop->fo_filename != NULL)
			filebench_log(LOG_DEBUG_SCRIPT, "It is not "
			    "advisable to supply both an fd number "
			    "and a fileset name in most cases");
	}

	/* Rotatefd? */
	if ((attr = get_attr(cmd, FSA_ROTATEFD)))
		flowop->fo_rotatefd = attr->attr_avd;
	else
		flowop->fo_rotatefd = avd_bool_alloc(FALSE);

	/* SRC FD, for copies etc... */
	if ((attr = get_attr(cmd, FSA_SRCFD)))
		flowop->fo_srcfdnumber = avd_get_int(attr->attr_avd);

	/* Blocking operation? */
	if ((attr = get_attr(cmd, FSA_BLOCKING)))
		flowop->fo_blocking = attr->attr_avd;
	else
		flowop->fo_blocking = avd_bool_alloc(FALSE);

	/* Direct I/O Operation */
	if ((attr = get_attr(cmd, FSA_DIRECTIO)))
		flowop->fo_directio = attr->attr_avd;
	else
		flowop->fo_directio = avd_bool_alloc(FALSE);

	/* Highwater mark */
	if ((attr = get_attr(cmd, FSA_HIGHWATER))) {
		flowop->fo_highwater = attr->attr_avd;
		if (AVD_IS_RANDOM(attr->attr_avd)) {
			filebench_log(LOG_ERROR,
			    "define flowop: Highwater attr cannot be random");
			filebench_shutdown(1);
		}
	} else {
		flowop->fo_highwater = avd_int_alloc(1);
	}

	/* find file or leaf directory by index number */
	if ((attr = get_attr(cmd, FSA_INDEXED)))
		flowop->fo_fileindex = attr->attr_avd;
	else
		flowop->fo_fileindex = NULL;

	/* Read Ahead Diable */
	if ((attr = get_attr(cmd, FSA_NOREADAHEAD)))
		flowop->fo_noreadahead = attr->attr_avd;
	else
		flowop->fo_noreadahead = avd_bool_alloc(FALSE);


}

/*
 * defines the FLOW_MASTER flowops within a FLOW_MASTER instance of
 * a composit flowop. Default attributes from the FLOW_INNER_DEF instances
 * of the composit flowop's inner flowops are used if set. Otherwise
 * default attributes from the FLOW_MASTER instance of the composit flowop
 * are used, which may include defaults from the original FLOW_DEFINITION
 * of the composit flowop.
 */
static void
parser_inner_flowop_define(threadflow_t *thread, flowop_t *comp0_flow,
			   flowop_t *comp_mstr_flow)
{
	flowop_t *inner_flowtype, *inner_flowop;

	/* follow flowop list, creating composit names */
	inner_flowtype = comp0_flow->fo_comp_fops;
	comp_mstr_flow->fo_comp_fops = NULL;

	while (inner_flowtype) {
		char fullname[MAXPATHLEN];

		/* create composite_name.name for new flowop */
		snprintf(fullname, MAXPATHLEN, "%s.%s",
		    comp_mstr_flow->fo_name, inner_flowtype->fo_name);

		if ((inner_flowop = flowop_define(thread, fullname,
		    inner_flowtype, &comp_mstr_flow->fo_comp_fops,
		    FLOW_MASTER, 0)) == NULL) {
			filebench_log(LOG_ERROR,
			    "define flowop: Failed to instantiate flowop %s\n",
			    fullname);
			filebench_shutdown(1);
		}

		/* if applicable, update filename attribute */
		if (inner_flowop->fo_filename) {
			char *name;

			/* fix up avd_t */
			avd_update(&inner_flowop->fo_filename,
			    comp_mstr_flow->fo_lvar_list);

			/* see if ready to get the file or fileset */
			name = avd_get_str(inner_flowop->fo_filename);
			if (name) {

				inner_flowop->fo_fileset = fileset_find(name);

				if (inner_flowop->fo_fileset == NULL) {
					filebench_log(LOG_ERROR,
					    "inr flowop %s: file %s not found",
					    inner_flowop->fo_name, name);
					filebench_shutdown(1);
				}
			}
		}

		/* update attributes from local variables */
		avd_update(&inner_flowop->fo_iters,
		    comp_mstr_flow->fo_lvar_list);

		/* if the inner flowop is a composit flowop, recurse */
		if (inner_flowtype->fo_type == FLOW_TYPE_COMPOSITE) {
			var_t *newlvar, *proto_lvars, *lvar_ptr;

			proto_lvars = inner_flowop->fo_lvar_list;
			inner_flowop->fo_lvar_list = 0;

			for (lvar_ptr = inner_flowtype->fo_lvar_list; lvar_ptr;
			    lvar_ptr = lvar_ptr->var_next) {

				if ((newlvar = var_lvar_alloc_local(
				    lvar_ptr->var_name)) != NULL) {

					add_lvar_to_list(newlvar,
					    &inner_flowop->fo_lvar_list);

					var_update_comp_lvars(newlvar,
					    proto_lvars,
					    comp_mstr_flow->fo_lvar_list);
				}
			}

			parser_inner_flowop_define(thread,
			    inner_flowtype,
			    inner_flowop);

			inner_flowtype = inner_flowtype->fo_exec_next;
			continue;
		}

		avd_update(&inner_flowop->fo_iosize,
		    comp_mstr_flow->fo_lvar_list);
		avd_update(&inner_flowop->fo_wss,
		    comp_mstr_flow->fo_lvar_list);
		avd_update(&inner_flowop->fo_iters,
		    comp_mstr_flow->fo_lvar_list);
		avd_update(&inner_flowop->fo_value,
		    comp_mstr_flow->fo_lvar_list);
		avd_update(&inner_flowop->fo_random,
		    comp_mstr_flow->fo_lvar_list);
		avd_update(&inner_flowop->fo_dsync,
		    comp_mstr_flow->fo_lvar_list);
		avd_update(&inner_flowop->fo_rotatefd,
		    comp_mstr_flow->fo_lvar_list);
		avd_update(&inner_flowop->fo_blocking,
		    comp_mstr_flow->fo_lvar_list);
		avd_update(&inner_flowop->fo_directio,
		    comp_mstr_flow->fo_lvar_list);
		avd_update(&inner_flowop->fo_highwater,
		    comp_mstr_flow->fo_lvar_list);

		inner_flowtype = inner_flowtype->fo_exec_next;
	}
}

/*
 * Calls flowop_define() to allocate a flowop with the supplied name.
 * The allocated flowop inherits attributes from a base flowop of the
 * same type.  If the new flowop has a file or fileset attribute specified,
 * it must specify a defined fileobj or fileset or an error will be logged.
 * The new flowop may  also have the following attributes set by
 * the program:
 *  - file size (fo_iosize)
 *  - working set size (fo_wss)
 *  - do random io (fo_random)
 *  - do synchronous io (fo_dsync)
 *  - perform each operation multiple times before advancing (fo_iter)
 *  - target name (fo_targetname)
 *  - An integer value (fo_value)
 *  - a file descriptor (fo_fd)
 *  - specify to rotate file descriptors (fo_rotatefd)
 *  - a source fd (fo_srcfdnumber)
 *  - specify a blocking operation (fo_blocking)
 *  - specify a highwater mark (fo_highwater)
 *
 * After all the supplied attributes are stored in their respective locations
 * in the flowop object, the flowop's init function is called. No errors are
 * returned, but the filebench run will be terminated if the flowtype is not
 * specified, a name for the new flowop is not supplied, the flowop_define
 * call fails, or a file or fileset name is supplied but the corresponding
 * fileobj or fileset cannot be located.
 */
static void
parser_flowop_define(cmd_t *cmd, threadflow_t *thread,
    flowop_t **flowoplist_hdp, int category)
{
	flowop_t *flowop, *flowop_type;
	char *type = (char *)cmd->cmd_name;
	char *name = NULL;
	attr_t *attr;

	/* Get the inherited flowop */
	flowop_type = flowop_find(type);
	if (flowop_type == NULL) {
		filebench_log(LOG_ERROR,
		    "define flowop: flowop type %s not found",
		    type);
		filebench_shutdown(1);
	}

	/* Get the name of the flowop */
	if ((attr = get_attr(cmd, FSA_NAME))) {
		name = avd_get_str(attr->attr_avd);
	} else {
		filebench_log(LOG_ERROR,
		    "define flowop: flowop %s specifies no name",
		    flowop_type->fo_name);
		filebench_shutdown(1);
	}

	if ((flowop = flowop_define(thread, name,
	    flowop_type, flowoplist_hdp, category, 0)) == NULL) {
		filebench_log(LOG_ERROR,
		    "define flowop: Failed to instantiate flowop %s\n",
		    cmd->cmd_name);
		filebench_shutdown(1);
	}

	/* Iterations */
	if ((attr = get_attr(cmd, FSA_ITERS)))
		flowop->fo_iters = attr->attr_avd;
	else
		flowop->fo_iters = avd_int_alloc(1);


	/* if this is a use of a composit flowop, create inner FLOW MASTERS */
	if (flowop_type->fo_type == FLOW_TYPE_COMPOSITE) {
		get_attr_lvars(cmd, flowop);
		if (category == FLOW_MASTER)
			parser_inner_flowop_define(thread,
			    flowop_type, flowop);
	}
	else {
		parser_flowop_get_attrs(cmd, flowop);
	}
}

static void
parser_composite_flowop_define(cmd_t *cmd)
{
	flowop_t *flowop;
	cmd_t *inner_cmd;
	char *name = NULL;
	attr_t *attr;

	/* Get the name of the flowop */
	if ((attr = get_attr(cmd, FSA_NAME))) {
		name = avd_get_str(attr->attr_avd);
	} else {
		filebench_log(LOG_ERROR,
		    "define flowop: Composit flowop specifies no name");

		filebench_shutdown(1);
	}

	if ((flowop = flowop_new_composite_define(name)) == NULL) {
		filebench_log(LOG_ERROR,
		    "define flowop: Failed to instantiate flowop %s\n",
		    cmd->cmd_name);
		filebench_shutdown(1);
	}

	/* place any local var_t variables on the flowop's local list */
	get_attr_lvars(cmd, flowop);

	/* Iterations */
	if ((attr = get_attr(cmd, FSA_ITERS)))
		flowop->fo_iters = attr->attr_avd;
	else
		flowop->fo_iters = avd_int_alloc(1);

	/* define inner flowops */
	for (inner_cmd = cmd->cmd_list; inner_cmd != NULL;
	    inner_cmd = inner_cmd->cmd_next) {
		parser_flowop_define(inner_cmd, NULL,
		    &flowop->fo_comp_fops, FLOW_INNER_DEF);
	}
}


/*
 * First, we verify that mandatory attributes - name and path - are specified.
 * Then allocate a fileset structure and setup its fields. Notice, at this
 * point we should not verify if AVD type makes sense, because AVD type can
 * change as variables are set to other values after fileset definition.
*/
static fileset_t *
parser_fileset_define_common(cmd_t *cmd)
{
	fileset_t *fileset;
	attr_t *attr;
	avd_t name;
	avd_t path;

	attr = get_attr(cmd, FSA_NAME);
	if (attr)
		name = attr->attr_avd;
	else {
		filebench_log(LOG_ERROR, "file[set] specifies no name");
		return NULL;
	}

	attr = get_attr(cmd, FSA_PATH);
	if (attr)
		path = attr->attr_avd;
	else {
		filebench_log(LOG_ERROR, "file[set] specifies no path");
		return NULL;
	}

	fileset = fileset_define(name, path);
	if (!fileset) {
		filebench_log(LOG_ERROR, "failed to instantiate file[set] %s\n",
		    		avd_get_str(name));
		return NULL;
	}

	attr = get_attr(cmd, FSA_PREALLOC);
	if (attr)
		fileset->fs_preallocpercent = attr->attr_avd;
	else
		fileset->fs_preallocpercent = avd_int_alloc(0);

	attr = get_attr(cmd, FSA_PARALLOC);
	if (attr)
		fileset->fs_paralloc = attr->attr_avd;
	else
		fileset->fs_paralloc = avd_bool_alloc(FALSE);

	attr = get_attr(cmd, FSA_READONLY);
	if (attr)
		fileset->fs_readonly = attr->attr_avd;
	else
		fileset->fs_readonly = avd_bool_alloc(FALSE);

	attr = get_attr(cmd, FSA_WRITEONLY);
	if (attr)
		fileset->fs_writeonly = attr->attr_avd;
	else
		fileset->fs_writeonly = avd_bool_alloc(FALSE);

	attr = get_attr(cmd, FSA_REUSE);
	if (attr)
		fileset->fs_reuse = attr->attr_avd;
	else
		fileset->fs_reuse = avd_bool_alloc(FALSE);

	/* Should we check for files actual existance? */
	attr = get_attr(cmd, FSA_TRUSTTREE);
	if (attr )
		fileset->fs_trust_tree = attr->attr_avd;
	else
		fileset->fs_trust_tree = avd_bool_alloc(FALSE);

	attr = get_attr(cmd, FSA_SIZE);
	if (attr)
		fileset->fs_size = attr->attr_avd;
	else
		fileset->fs_size = avd_int_alloc(1024);

	return fileset;
}

static void
parser_digest(cmd_t *cmd)
{
/*
	make_digest_request_sync(0);
	wait_on_digesting();
*/
}

static void
parser_file_define(cmd_t *cmd)
{
	fileset_t *fileset;

	fileset = parser_fileset_define_common(cmd);
	if (!fileset) {
		filebench_log(LOG_ERROR, "failed to instantiate file");
		filebench_shutdown(1);
		return;
	}

	/* fileset is emulating a single file */
	fileset->fs_attrs = FILESET_IS_FILE;
	fileset->fs_entries = avd_int_alloc(1);
	/* Set the mean dir width to more than 1 */
	fileset->fs_dirwidth = avd_int_alloc(10);
	fileset->fs_dirgamma = avd_int_alloc(0);
	fileset->fs_leafdirs = avd_int_alloc(0);
}

static void
parser_fileset_define(cmd_t *cmd)
{
	fileset_t *fileset;
	attr_t *attr;

	fileset = parser_fileset_define_common(cmd);
	if (!fileset) {
		filebench_log(LOG_ERROR, "failed to instantiate fileset");
		filebench_shutdown(1);
		return;
	}

	attr = get_attr(cmd, FSA_ENTRIES);
	if (attr)
		fileset->fs_entries = attr->attr_avd;
	else
		fileset->fs_entries = avd_int_alloc(0);

	attr = get_attr(cmd, FSA_LEAFDIRS);
	if (attr)
		fileset->fs_leafdirs = attr->attr_avd;
	else
		fileset->fs_leafdirs = avd_int_alloc(0);

	attr = get_attr(cmd, FSA_DIRWIDTH);
	if (attr)
		fileset->fs_dirwidth = attr->attr_avd;
	else {
		filebench_log(LOG_ERROR, "Fileset has no directory width");
		fileset->fs_dirwidth = avd_int_alloc(0);
	}

	attr = get_attr(cmd, FSA_DIRDEPTHRV);
	if (attr)
		fileset->fs_dirdepthrv = attr->attr_avd;
	else
		fileset->fs_dirdepthrv = NULL;

	attr = get_attr(cmd, FSA_DIRGAMMA);
	if (attr)
		fileset->fs_dirgamma = attr->attr_avd;
	else
		fileset->fs_dirgamma = avd_int_alloc(1500);
}

/*
 * Calls fileset_createsets() to populate all filesets and create all
 * associated, initially existant,  files and subdirectories.
 * If errors are encountered, calls filebench_shutdown() to exit Filebench.
 */
static void
parser_fileset_create(cmd_t *cmd)
{
	int ret;

	ret = fileset_createsets(); 
	if (ret) {
		filebench_log(LOG_ERROR, "Failed to create filesets");
		filebench_shutdown(1);
	}
}

/*
 * Ends filebench run after first destoring any interprocess
 * shared memory. The call to filebench_shutdown()
 * also causes filebench to exit.
 */
static void
parser_filebench_shutdown(cmd_t *cmd)
{
	int f_abort = filebench_shm->shm_f_abort;

	ipc_fini();

	if (f_abort == FILEBENCH_ABORT_ERROR)
		filebench_shutdown(1);
	else
		filebench_shutdown(0);
}

/*
 * This is used for timing runs. Pauses the master thread in one second
 * intervals until the supplied ptime runs out or the f_abort flag
 * is raised. If given a time of zero, it will pause until f_abort is raised.
 */
static int
parser_pause(int ptime)
{
	int timeslept = 0;

	if (ptime) {
		while (timeslept < ptime) {
			(void) sleep(1);
			timeslept++;
			/*
			//print stats every 5 seconds, this cannot be too low since it Pauses
			//the system for a bit. If it's one the benchmark doesnt make progress
			// if (timeslept % 5 == 0) {
			// if (timeslept % 2 == 0) {
				stats_snap();
				printf("\n\n");
			// }
			*/

			if (filebench_shm->shm_f_abort)
				break;
		}
	} else {
		/* initial runtime of 0 means run till abort */
		/* CONSTCOND */
		while (1) {
			(void) sleep(1);
			timeslept++;
			if (filebench_shm->shm_f_abort)
				break;
		}
	}

	return (timeslept);
}

#define TIMED_RUNTIME_DEFAULT 60 /* In seconds */
#define PERIOD_DEFAULT 10 /* In seconds */

/*
 * Do a file bench run. Calls routines to create file sets, files, and
 * processes. It resets the statistics counters, then sleeps for the runtime
 * passed as an argument to it on the command line in 1 second increments.
 * When it is finished sleeping, it collects a snapshot of the statistics
 * and ends the run.
 */
static void
parser_run(cmd_t *cmd)
{
	int runtime;
	int timeslept;

	runtime = cmd->cmd_qty;

	parser_fileset_create(cmd);

	if (mlfs) {
		while(make_digest_request_async(100) == -EBUSY);
		if (!nic_offload)
			wait_on_digesting();
	}

	proc_create();

	/* check for startup errors */
	if (filebench_shm->shm_f_abort)
		return;

	filebench_log(LOG_INFO, "Running...");
	stats_clear();

	/* If it is a timed mode and timeout is not specified use default */
	if (filebench_shm->shm_rmode == FILEBENCH_MODE_TIMEOUT && !runtime)
		runtime = TIMED_RUNTIME_DEFAULT;

	timeslept = parser_pause(runtime);

	filebench_log(LOG_INFO, "Run took %d seconds...", timeslept);
	stats_snap();
	proc_shutdown();
	parser_filebench_shutdown((cmd_t *)0);
}

static void
parser_psrun(cmd_t *cmd)
{
	int runtime;
	int period;
	int timeslept = 0;
	int reset_stats = 0;

	runtime = cmd->cmd_qty;

	/*
	 * If period is negative then
	 * we want to reset statistics
	 * at the end of the every period
	 */
	if (cmd->cmd_qty1 < 0) {
		period = -cmd->cmd_qty1;
		reset_stats = 1;
	} else if (cmd->cmd_qty1 > 0) {
		period = cmd->cmd_qty1;
		reset_stats = 0;
	} else { /* (cmd->cmd_qty1) == 0 */
		period = PERIOD_DEFAULT;
		reset_stats = 0;
	}

	parser_fileset_create(cmd);
	proc_create();

	if (mlfs) {
		while(make_digest_request_async(100) == -EBUSY);
		if (!nic_offload)
			wait_on_digesting();
	}

	/* check for startup errors */
	if (filebench_shm->shm_f_abort)
		return;

	filebench_log(LOG_INFO, "Running...");
	stats_clear();

	/* If it is a timed mode and timeout is not specified use default */
	if (filebench_shm->shm_rmode == FILEBENCH_MODE_TIMEOUT && !runtime)
		runtime = TIMED_RUNTIME_DEFAULT;

	while (1) {
		/* sleep the remaining time if a period is smaller */
		if (filebench_shm->shm_rmode == FILEBENCH_MODE_TIMEOUT)
			period = period > (runtime - timeslept) ?
						(runtime - timeslept) : period;

		timeslept += parser_pause(period);

		if (filebench_shm->shm_f_abort)
			break;

		if (filebench_shm->shm_rmode == FILEBENCH_MODE_TIMEOUT &&
							timeslept >= runtime)
			break;

		stats_snap();

		if (reset_stats)
			stats_clear();
	}

	filebench_log(LOG_INFO, "Run took %d seconds...", timeslept);
	stats_snap();
	proc_shutdown();
	parser_filebench_shutdown((cmd_t *)0);
}

/*
 * Similar to parser_run, but gets the sleep time from a variable
 * whose name is supplied as an argument to the command.
 */
static void
parser_run_variable(cmd_t *cmd)
{
	avd_t integer = avd_var_alloc(cmd->cmd_tgt1);
	int runtime;
	int timeslept;

	if (integer == NULL) {
		filebench_log(LOG_ERROR, "Unknown variable %s",
		cmd->cmd_tgt1);
		return;
	}

	runtime = avd_get_int(integer);

	parser_fileset_create(cmd);

	if (mlfs) {
	  while(make_digest_request_async(100) == -EBUSY);
	  if (!nic_offload)
		wait_on_digesting();
	}

	proc_create();

	/* check for startup errors */
	if (filebench_shm->shm_f_abort)
		return;

	filebench_log(LOG_INFO, "Running...");
	stats_clear();

	/* If it is a timed mode and timeout is not specified use default */
	if (filebench_shm->shm_rmode == FILEBENCH_MODE_TIMEOUT && !runtime)
		runtime = TIMED_RUNTIME_DEFAULT;

	timeslept = parser_pause(runtime);

	filebench_log(LOG_INFO, "Run took %d seconds...", timeslept);
	stats_snap();
	proc_shutdown();
	parser_filebench_shutdown((cmd_t *)0);
}

/*
 * Establishes multi-client synchronization socket with synch server.
 */
static void
parser_enable_mc(cmd_t *cmd)
{
	attr_t *attr;
	char *master;
	char *client;

	if ((attr = get_attr(cmd, FSA_MASTER))) {
		master = avd_get_str(attr->attr_avd);
	} else {
		filebench_log(LOG_ERROR,
		    "enable multi: no master specified");
		return;
	}

	if ((attr = get_attr(cmd, FSA_CLIENT))) {
		client = avd_get_str(attr->attr_avd);
	} else {
		filebench_log(LOG_ERROR,
		    "enable multi: no client specified");
		return;
	}

	mc_sync_open_sock(master, 8001, client);
}

/*
 * Exchanges multi-client synchronization message with synch server.
 */
static void
parser_domultisync(cmd_t *cmd)
{
	attr_t *attr;
	fbint_t value;

	if ((attr = get_attr(cmd, FSA_VALUE)))
		value = avd_get_int(attr->attr_avd);
	else
		value = 1;

	mc_sync_synchronize((int)value);
}

/*
 * Sleeps for cmd->cmd_qty seconds, one second at a time.
 */
static void
parser_sleep(cmd_t *cmd)
{
	int sleeptime;
	int timeslept;

	/* check for startup errors */
	if (filebench_shm->shm_f_abort)
		return;

	sleeptime = cmd->cmd_qty;
	filebench_log(LOG_INFO, "Sleeping...");

	timeslept = parser_pause(sleeptime);

	filebench_log(LOG_INFO, "Slept for %d seconds...", timeslept);
}

/*
 * Same as parser_sleep, except the sleep time is obtained from a variable
 * whose name is passed to it as an argument on the command line.
 */
static void
parser_sleep_variable(cmd_t *cmd)
{
	avd_t integer = avd_var_alloc(cmd->cmd_tgt1);
	int sleeptime;
	int timeslept;

	if (integer == NULL) {
		filebench_log(LOG_ERROR, "Unknown variable %s",
		cmd->cmd_tgt1);
		return;
	}

	sleeptime = avd_get_int(integer);

	/* check for startup errors */
	if (filebench_shm->shm_f_abort)
		return;

	filebench_log(LOG_INFO, "Running...");

	timeslept = parser_pause(sleeptime);

	filebench_log(LOG_INFO, "Run took %d seconds...", timeslept);
}

/*
 * Launches a shell to run the unix command supplied in the argument.
 * The command should be enclosed in quotes, as in:
 * 	system "rm xyz"
 * which would run the "rm" utility to delete the file "xyz".
 */
static void
parser_system(cmd_t *cmd)
{
	char *string;

	if (!cmd->cmd_param_list)
		return;

	string = parser_list2string(cmd->cmd_param_list);

	if (!string)
		return;

	filebench_log(LOG_VERBOSE, "Running '%s'", string);

	if (system(string) < 0) {
		filebench_log(LOG_ERROR,
		    "system exec failed: %s",
		    strerror(errno));
		free(string);
		filebench_shutdown(1);
	}

	free(string);
}

/*
 * Echos string supplied with command to the log.
 */
static void
parser_echo(cmd_t *cmd)
{
	char *string;

	if (cmd->cmd_param_list == NULL)
		return;

	string = parser_list2string(cmd->cmd_param_list);

	if (string == NULL)
		return;

	filebench_log(LOG_INFO, "%s", string);
}

/*
 * Prints out the version of Filebench.
 */
static void
parser_version(cmd_t *cmd)
{
	filebench_log(LOG_INFO, "Filebench Version: %s", FILEBENCH_VERSION);
}

static void
parser_enable_lathist(cmd_t *cmd)
{
	filebench_shm->lathist_enabled = 1;
	filebench_log(LOG_INFO, "Latency histogram enabled");
}

/*
 * define a random variable and initialize the distribution parameters
 */
static void
parser_var_assign_random(char *name, cmd_t *cmd)
{
	randdist_t	*rndp;
	attr_t		*attr;

	rndp = randdist_alloc();
	if (!rndp) {
		filebench_log(LOG_ERROR,
			"failed to alloc random distribution object\n");
		return;
	}

	rndp->rnd_type = 0;

	/* Get the source of the random numbers */
	if ((attr = get_attr(cmd, FSA_RANDSRC))) {
		int randsrc = (int)avd_get_int(attr->attr_avd);

		switch (randsrc) {
		case FSV_URAND:
			rndp->rnd_type |= RAND_SRC_URANDOM;
			break;
		case FSV_RAND48:
			rndp->rnd_type |= RAND_SRC_GENERATOR;
			break;
		}
	} else {
		/* default to rand48 random number generator */
		rndp->rnd_type |= RAND_SRC_GENERATOR;
	}

	/* Get the min value of the random distribution */
	if ((attr = get_attr(cmd, FSA_MIN)))
		rndp->rnd_min = attr->attr_avd;
	else
		rndp->rnd_min = avd_int_alloc(0);

	/* Get the roundoff value for the random distribution */
	if ((attr = get_attr(cmd, FSA_ROUND)))
		rndp->rnd_round = attr->attr_avd;
	else
		rndp->rnd_round = avd_int_alloc(0);

	/* Get a tablular probablility distribution if there is one */
	if ((attr = get_attr(cmd, FSA_RANDTABLE))) {
		rndp->rnd_probtabs = (probtabent_t *)(attr->attr_obj);
		rndp->rnd_type |= RAND_TYPE_TABLE;

		/* no need for the rest of the attributes */
		goto randdist_init;
	} else {
		rndp->rnd_probtabs = NULL;
	}

	/* Get the type for the random variable */
	if ((attr = get_attr(cmd, FSA_TYPE))) {
		int disttype = (int)avd_get_int(attr->attr_avd);

		switch (disttype) {
		case FSV_RANDUNI:
			rndp->rnd_type |= RAND_TYPE_UNIFORM;
			break;
		case FSA_RANDGAMMA:
			rndp->rnd_type |= RAND_TYPE_GAMMA;
			break;
		case FSV_RANDTAB:
			filebench_log(LOG_ERROR,
			    "Table distribution type without prob table");
			break;
		}
	} else {
		/* default to gamma distribution type */
		rndp->rnd_type |= RAND_TYPE_GAMMA;
	}

	/* Get the seed for the random variable */
	if ((attr = get_attr(cmd, FSA_RANDSEED)))
		rndp->rnd_seed = attr->attr_avd;
	else
		rndp->rnd_seed = avd_int_alloc(0);

	/* Get the gamma value of the random distribution */
	if ((attr = get_attr(cmd, FSA_RANDGAMMA)))
		rndp->rnd_gamma = attr->attr_avd;
	else
		rndp->rnd_gamma = avd_int_alloc(1500);

	/* Get the mean value of the random distribution */
	if ((attr = get_attr(cmd, FSA_RANDMEAN))) {
		rndp->rnd_mean = attr->attr_avd;
	} else if ((rndp->rnd_type & RAND_TYPE_MASK) == RAND_TYPE_GAMMA) {
		rndp->rnd_mean = NULL;
	} else {
		rndp->rnd_mean = avd_int_alloc(0);
	}

	var_assign_random(name, rndp);

randdist_init:
	randdist_init(rndp);
}

/*
 * alloc_cmd() allocates the required resources for a cmd_t. On failure, a
 * filebench_log is issued and NULL is returned.
 */
static cmd_t *
alloc_cmd(void)
{
	cmd_t *cmd;

	cmd = malloc(sizeof(*cmd));
	if (!cmd) {
		filebench_log(LOG_ERROR, "Alloc cmd failed");
		return NULL;
	}

	memset(cmd, 0, sizeof (cmd_t));

	return cmd;
}

/*
 * Allocates an attr_t structure and zeros it. Returns NULL on failure, or
 * a pointer to the attr_t.
 */
static attr_t *
alloc_attr(void)
{
	attr_t *attr;

	attr = malloc(sizeof(*attr));
	if (!attr)
		return (NULL);

	(void) memset(attr, 0, sizeof(*attr));

	return (attr);
}

/*
 * Allocates a probtabent_t structure and zeros it. Returns NULL on failure, or
 * a pointer to the probtabent_t.
 */
static probtabent_t *
alloc_probtabent(void)
{
	probtabent_t *rte;

	if ((rte = malloc(sizeof (probtabent_t))) == NULL) {
		return (NULL);
	}

	(void) memset(rte, 0, sizeof (probtabent_t));
	return (rte);
}

/*
 * Allocates an attr_t structure and puts the supplied var_t into
 * its attr_avd location, and sets its name to FSA_LVAR_ASSIGN
 */
static attr_t *
alloc_lvar_attr(var_t *var)
{
	attr_t *attr;

	if ((attr = alloc_attr()) == NULL)
		return (NULL);

	attr->attr_name = FSA_LVAR_ASSIGN;
	attr->attr_avd = (avd_t)var;

	return (attr);
}

/*
 * Searches the attribute list for the command for the named attribute type.
 * The attribute list is created by the parser from the list of attributes
 * supplied with certain commands, such as the define and flowop commands.
 * Returns a pointer to the attribute structure if the named attribute is
 * found, otherwise returns NULL. If the attribute includes a parameter list,
 * the list is converted to a string and stored in the attr_avd field of
 * the returned attr_t struct.
 */
static attr_t *
get_attr(cmd_t *cmd, int64_t name)
{
	attr_t *attr;
	attr_t *rtn = NULL;

	for (attr = cmd->cmd_attr_list; attr != NULL;
	    attr = attr->attr_next) {

		filebench_log(LOG_DEBUG_IMPL,
		    "attr %d = %d %llx?",
		    attr->attr_name,
		    name,
		    attr->attr_avd);

		if (attr->attr_name == name)
			rtn = attr;
	}

	return rtn;
}

/*
 * removes the newly allocated local var from the shared local var
 * list, then puts it at the head of the private local var list
 * supplied as the second argument.
 */
static void
add_lvar_to_list(var_t *newlvar, var_t **lvar_list)
{
	var_t *prev;

	/* remove from shared local list, if there */
	if (newlvar == filebench_shm->shm_var_loc_list) {
		/* on top of list, just grap */
		filebench_shm->shm_var_loc_list = newlvar->var_next;
	} else {
		/* find newvar on list and remove */
		for (prev = filebench_shm->shm_var_loc_list; prev;
		    prev = prev->var_next) {
			if (prev->var_next == newlvar)
				prev->var_next = newlvar->var_next;
		}
	}
	newlvar->var_next = NULL;

	/* add to flowop private local list at head */
	newlvar->var_next = *lvar_list;
	*lvar_list = newlvar;
}

/*
 * Searches the attribute list for the command for any allocated local
 * variables. The attribute list is created by the parser from the list of
 * attributes supplied with certain commands, such as the define and flowop
 * commands. Places all found local vars onto the flowop's local variable
 * list.
 */
static void
get_attr_lvars(cmd_t *cmd, flowop_t *flowop)
{
	attr_t *attr;
	var_t *orig_lvar_list;

	/* save the local var list */
	orig_lvar_list = flowop->fo_lvar_list;

	for (attr = cmd->cmd_attr_list; attr != NULL;
	    attr = attr->attr_next) {

		if (attr->attr_name == FSA_LVAR_ASSIGN) {
			var_t *newvar;

			if ((newvar = (var_t *)attr->attr_avd) == NULL)
				continue;

			add_lvar_to_list(newvar, &flowop->fo_lvar_list);
			var_update_comp_lvars(newvar, orig_lvar_list, NULL);
		}
	}
}

/*
 * Allocates memory for a list_t structure, initializes it to zero, and
 * returns a pointer to it. On failure, returns NULL.
 */
static list_t *
alloc_list()
{
	list_t *list;

	if ((list = malloc(sizeof (list_t))) == NULL) {
		return (NULL);
	}

	(void) memset(list, 0, sizeof (list_t));
	return (list);
}

/*
 * Define a custom variable and validate its parameters.
 * TODO: Clean up state when things go wrong.
 */
static void
parser_var_assign_custom(char *name, cmd_t *cmd)
{
	cvar_t	*cvar;
	attr_t	*attr;
	char	*type;
	char	*parameters;
	int 	ret;

	attr = get_attr(cmd, FSA_TYPE);
	if (attr)
		type = avd_get_str(attr->attr_avd);
	else {
		filebench_log(LOG_ERROR, "define cvar: no type specified");
		filebench_shutdown(1);
		return;
	}

	cvar = cvar_alloc();
	if (!cvar) {
		filebench_log(LOG_ERROR, "Failed to allocate custom variable");
		filebench_shutdown(1);
		return;
	}

	/* Initialize the custom variable mutex. */
	(void) pthread_mutex_init(&cvar->cvar_lock,
			ipc_mutexattr(IPC_MUTEX_NORMAL));

	/* Get the min, max and round values for the custom variable. */
	if ((attr = get_attr(cmd, FSA_MIN)))
		cvar->min = avd_get_dbl(attr->attr_avd);
	else
		cvar->min = DBL_MIN;

	if ((attr = get_attr(cmd, FSA_MAX)))
		cvar->max = avd_get_dbl(attr->attr_avd);
	else
		cvar->max = DBL_MAX;

	if ((attr = get_attr(cmd, FSA_ROUND)))
		cvar->round = avd_get_dbl(attr->attr_avd);
	else
		cvar->round = 0;

	attr = get_attr(cmd, FSA_PARAMETERS);
	if (attr)
		parameters = avd_get_str(attr->attr_avd);
	else
		parameters = NULL;

	ret = init_cvar_handle(cvar, type, parameters);
	if (ret) {
		filebench_log(LOG_FATAL, "define cvar: failed for custom variable %s",
		    name);
		filebench_shutdown(1);
		return;
	}

	var_assign_custom(name, cvar);
}
