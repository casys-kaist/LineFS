/* A Bison parser, made by GNU Bison 3.5.1.  */

/* Bison interface for Yacc-like parsers in C

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

/* Undocumented macros, especially those whose name start with YY_,
   are private implementation details.  Do not rely on them.  */

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

#line 284 "parser_gram.h"

};
typedef union YYSTYPE YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif


extern YYSTYPE yylval;

int yyparse (void);

#endif /* !YY_YY_PARSER_GRAM_H_INCLUDED  */
