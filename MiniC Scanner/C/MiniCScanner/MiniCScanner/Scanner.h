/***************************************************************
*      scanner routine for Mini C language                    *
*                                   2003. 3. 10               *
***************************************************************/

#pragma once

/**
 * NO_KEYWORD: keyword 테이블 크기
 * ID_LENGTH: 변수명, 함수명 등의 최대 길이
 */
#define NO_KEYWORD 16
#define ID_LENGTH 12

/**
 * TOKEN_VALUE_LENGTH: Token Value 길이
 * FILE_NAME_LENGTH: 파일명 길이
 */
#define TOKEN_VALUE_LENGTH 4096
#define FILE_NAME_LENGTH 260

/**
 * Token 정보
 */
struct tokenType {
	int number; // Token Number
	char value[TOKEN_VALUE_LENGTH]; // Token Value
	char fileName[FILE_NAME_LENGTH];
	int lineNumber;
	int columnNumber;
};

/**
 * Scanner가 인식할 수 있는 Symbol 정의
 */
enum tsymbol {
	tnull = -1,
	tnot, tnotequ, tremainder, tremAssign, tident, tnumber,
	/* 0          1            2         3            4          5     */
	tand, tlparen, trparen, tmul, tmulAssign, tplus,
	/* 6          7            8         9           10         11     */
	tinc, taddAssign, tcomma, tminus, tdec, tsubAssign,
	/* 12         13          14        15           16         17     */
	tdiv, tdivAssign, tsemicolon, tless, tlesse, tassign,
	/* 18         19          20        21           22         23     */
	tequal, tgreat, tgreate, tlbracket, trbracket, teof,
	/* 24         25          26        27           28         29     */
	//   ...........    word symbols ................................. //
	/* 30         31          32        33           34         35     */
	tconst, telse, tif, tint, treturn, tvoid,
	/* 36         37          38        39                             */
	twhile, tlbrace, tor, trbrace,
	/* 40         41          42        43          44         45      */
	tchar, tdouble, tfor, tdo, tgoto, tswitch,
	/* 46         47          48        49          50         51      */
	tcase, tbreak, tdefault, tcolon, tcharLiteral, tstringLiteral,
	/* 52         53                                                */
	tdoubleLiteral, tdocumentedComment
};

/** 함수 원형 */
void initScanner(const char *fileName); // Scanner 초기화: 파일명, 위치 정보
struct tokenType scanner(); // Scanner.cpp에 구현
void printToken(struct tokenType token); // Token 정보 출력