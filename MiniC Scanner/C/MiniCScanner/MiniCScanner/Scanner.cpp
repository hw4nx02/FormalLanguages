/***************************************************************
*      scanner routine for Mini C language                    *
***************************************************************/

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "Scanner.h"

extern FILE *sourceFile;                       // miniC source program

static int superLetter(int ch);
static int superLetterOrDigit(int ch);
static int hexValue(int ch);
static int readChar();
static void unreadChar(int ch);
static void initToken(struct tokenType *token, int lineNumber, int columnNumber);
static void setToken(struct tokenType *token, int number, const char *value);
static void copyText(char *target, int targetSize, const char *source);
static int appendChar(char *target, int *length, int ch);
static void describeChar(int ch, char *buffer, int bufferSize);
static void lexicalError(int n, int lineNumber, int columnNumber, const char *detail);
static void scanNumber(int firstCharacter, struct tokenType *token);
static void scanDotStartedDouble(struct tokenType *token);
static int scanOptionalExponent(char *buff, int *length, int lineNumber, int columnNumber);
static void scanStringLiteral(struct tokenType *token);
static void scanCharacterLiteral(struct tokenType *token);
static void scanBlockComment(struct tokenType *token, int documented);
static void scanLineComment(struct tokenType *token, int documented);

/** Token의 이름: Token Number -> 문자열 변환을 위함 */
const char *tokenName[] = {
	"!",        "!=",      "%",       "%=",     "%ident",   "%number",
	/* 0          1           2         3          4          5        */
	"&&",       "(",       ")",       "*",      "*=",       "+",
	/* 6          7           8         9         10         11        */
	"++",       "+=",      ",",       "-",      "--",	    "-=",
	/* 12         13         14        15         16         17        */
	"/",        "/=",      ";",       "<",      "<=",       "=",
	/* 18         19         20        21         22         23        */
	"==",       ">",       ">=",      "[",      "]",        "eof",
	/* 24         25         26        27         28         29        */
	//   ...........    word symbols ................................. //
	/* 30         31         32        33         34         35        */
	"const",    "else",     "if",      "int",     "return",  "void",
	/* 36         37         38        39                              */
	"while",    "{",       "||",      "}",
	//   ...........    추가 키워드 ................................. //
	/* 40         41         42        43         44         45        */
	"char",     "double",  "for",     "do",     "goto",     "switch",
	/* 46         47         48        */
	"case",     "break",   "default",
	//   ...........    추가 연산자 ................................. //
	/* 49         */
	":",
	//   ...........    추가 인식 리터럴 ................................. //
	/* 50         51          52        */
	"%character", "%string", "%double",
	//   ...........    문서화 주석 ................................. //
	/* 53         */
	"%documented-comment"
};

/** 키워드 테이블 */
const char *keyword[NO_KEYWORD] = {
	"const",  "else",    "if",    "int",    "return",  "void",    "while",
	"char",   "double",  "for",   "do",     "goto",    "switch",  "case",
	"break",  "default"
};

/** 키워드-Token Number 매핑 테이블 */
enum tsymbol tnum[NO_KEYWORD] = {
	tconst,    telse,     tif,     tint,     treturn,   tvoid,     twhile,
	tchar,     tdouble,   tfor,    tdo,      tgoto,     tswitch,   tcase,
	tbreak,    tdefault
};

static char scannerFileName[FILE_NAME_LENGTH] = "";
static int currentLineNumber = 1;
static int currentColumnNumber = 0;
static int lastReadLineNumber = 1;
static int lastReadColumnNumber = 0;

void initScanner(const char *fileName)
{
	copyText(scannerFileName, FILE_NAME_LENGTH, fileName);
	currentLineNumber = 1;
	currentColumnNumber = 0;
	lastReadLineNumber = 1;
	lastReadColumnNumber = 0;
}

struct tokenType scanner()
{
	struct tokenType token;
	int i, index;
	int ch; // scanner가 읽어들인 글자
	char id[ID_LENGTH]; // Token value
	int tooLong; // 제한 길이 넘김 여부

	// Token Number 초기화
	token.number = tnull;

	do {

		/** 공백 문자 건너뛰기 */
		do {
			ch = readChar();
		} while (ch != EOF && isspace((unsigned char)ch));

		/** Token 정보 초기화: Token 시작 위치 저장 */
		initToken(&token, lastReadLineNumber, lastReadColumnNumber);

		/** 시작 문자 구분 */
		/** Identifier Recognition */
		if (superLetter(ch)) { // 첫 문자: 문자 or _ -> 키워드/식별자
			i = 0;
			tooLong = 0;
			do {
				// 제한 길이 넘더라도 Token 끝까지 읽음
				if (i < ID_LENGTH - 1)
					id[i++] = (char)ch;
				else
					tooLong = 1; // 제한 길이 넘기면 플래그 표시
				ch = readChar();
			} while (superLetterOrDigit(ch)); // 문자, 숫자, _만나면 루프 반복
			id[i] = '\0';
			unreadChar(ch);  // 방금 인식한 키워드/식별자를 구성하는 문자가 아닌 문자를 다시 파일 스트림으로
			// 키워드/식별자 인식 완

			if (tooLong) // 제한 길이 넘기면 Lexical Error 발생
				lexicalError(1, token.lineNumber, token.columnNumber, id);

			// 방금 인식한 Token이 키워드인지 식별자인지 구분
			for (index = 0; index < NO_KEYWORD; index++)
				if (!strcmp(id, keyword[index])) break;
			if (index < NO_KEYWORD)    // 키워드 테이블에서 찾으면 키워드 형태로 Token 저장
				setToken(&token, tnum[index], keyword[index]);
			else {                     // 키워드 테이블에서 찾지 못하면 식별자 형태로 Token 저장
				setToken(&token, tident, id);
			}
		}  // end of identifier or keyword

		/** Integer number Recognition */
		else if (isdigit((unsigned char)ch)) {  // 첫 문자 정수 -> 정수
			scanNumber(ch, &token);
		}

		/** 특수문자 인식 */
		else switch (ch) {
		case '/':
			ch = readChar();

			if (ch == '*') {
				ch = readChar();
				if (ch == '*') // Documented(/** ... */) Comments
					scanBlockComment(&token, 1);
				else { // /* ... */
					unreadChar(ch);
					scanBlockComment(&token, 0);
				}
			}
			
			else if (ch == '/') {
				ch = readChar();
				if (ch == '/') // Single line documented(/// ...) Comments
					scanLineComment(&token, 1);
				else { // // ...
					unreadChar(ch);
					scanLineComment(&token, 0);
				}
			}
			
			else if (ch == '=') // 연산자 /= 인식
				setToken(&token, tdivAssign, tokenName[tdivAssign]);
			else { // 연산자 / 인식
				setToken(&token, tdiv, tokenName[tdiv]);
				unreadChar(ch);
			}
			break;
		case '!':
			ch = readChar();
			if (ch == '=') // !=
				setToken(&token, tnotequ, tokenName[tnotequ]);
			else { // !
				setToken(&token, tnot, tokenName[tnot]);
				unreadChar(ch);
			}
			break;
		case '%':
			ch = readChar();
			if (ch == '=') // %=
				setToken(&token, tremAssign, tokenName[tremAssign]);
			else { // %
				setToken(&token, tremainder, tokenName[tremainder]);
				unreadChar(ch);
			}
			break;
		case '&':
			ch = readChar();
			if (ch == '&') // &&
				setToken(&token, tand, tokenName[tand]);
			else { // & -> Lexical Error
				lexicalError(2, token.lineNumber, token.columnNumber, NULL);
				unreadChar(ch);
			}
			break;
		case '*':
			ch = readChar();
			if (ch == '=') // *=
				setToken(&token, tmulAssign, tokenName[tmulAssign]);
			else { // *
				setToken(&token, tmul, tokenName[tmul]);
				unreadChar(ch);
			}
			break;
		case '+':
			ch = readChar();
			if (ch == '+') // ++
				setToken(&token, tinc, tokenName[tinc]);
			else if (ch == '=') // +=
				setToken(&token, taddAssign, tokenName[taddAssign]);
			else { // +
				setToken(&token, tplus, tokenName[tplus]);
				unreadChar(ch);
			}
			break;
		case '-':
			ch = readChar();
			if (ch == '-') // --
				setToken(&token, tdec, tokenName[tdec]);
			else if (ch == '=') // -=
				setToken(&token, tsubAssign, tokenName[tsubAssign]);
			else { // -
				setToken(&token, tminus, tokenName[tminus]);
				unreadChar(ch);
			}
			break;
		case '<':
			ch = readChar();
			if (ch == '=') // <=
				setToken(&token, tlesse, tokenName[tlesse]);
			else { // <
				setToken(&token, tless, tokenName[tless]);
				unreadChar(ch);
			}
			break;
		case '=':
			ch = readChar();
			if (ch == '=') // ==
				setToken(&token, tequal, tokenName[tequal]);
			else { // =
				setToken(&token, tassign, tokenName[tassign]);
				unreadChar(ch);
			}
			break;
		case '>':
			ch = readChar();
			if (ch == '=') // >=
				setToken(&token, tgreate, tokenName[tgreate]);
			else { // >
				setToken(&token, tgreat, tokenName[tgreat]);
				unreadChar(ch);
			}
			break;
		case '|':
			ch = readChar();
			if (ch == '|') // ||
				setToken(&token, tor, tokenName[tor]);
			else { // | -> Lexical Error
				lexicalError(3, token.lineNumber, token.columnNumber, NULL);
				unreadChar(ch);
			}
			break;
		/** chracter literal 인식 - '로 시작 */
		case '\'': scanCharacterLiteral(&token);              break;
		/** String Constant Recognition */
		case '"':  scanStringLiteral(&token);                 break;
		/** Double literal - .으로 시작 */
		case '.':  scanDotStartedDouble(&token);              break;
		case ':':  setToken(&token, tcolon, tokenName[tcolon]); break;
		case '(':  setToken(&token, tlparen, tokenName[tlparen]); break;
		case ')':  setToken(&token, trparen, tokenName[trparen]); break;
		case ',':  setToken(&token, tcomma, tokenName[tcomma]); break;
		case ';':  setToken(&token, tsemicolon, tokenName[tsemicolon]); break;
		case '[':  setToken(&token, tlbracket, tokenName[tlbracket]); break;
		case ']':  setToken(&token, trbracket, tokenName[trbracket]); break;
		case '{':  setToken(&token, tlbrace, tokenName[tlbrace]); break;
		case '}':  setToken(&token, trbrace, tokenName[trbrace]); break;
		case EOF:  setToken(&token, teof, tokenName[teof]);      break;
		default: {
			char detail[32];
			describeChar(ch, detail, sizeof(detail));
			lexicalError(4, token.lineNumber, token.columnNumber, detail);
			break;
		}

		} // switch end
	} while (token.number == tnull);
	return token;
} // end of scanner

/** 한 글자 읽는 함수 */
static int readChar()
{
	// 파일에서 문자 하나 읽기
	int ch = fgetc(sourceFile);

	// EOF 도달 시
	if (ch == EOF) {
		lastReadLineNumber = currentLineNumber;
		lastReadColumnNumber = currentColumnNumber + 1;
		return EOF;
	}

	// 한 글자 읽었으므로, 현재 위치를 이전 위치 정보에 저장
	lastReadLineNumber = currentLineNumber;
	lastReadColumnNumber = currentColumnNumber + 1;

	// 읽은 글자에 따라 line number, column number 다르게 갱신
	if (ch == '\n') { //  줄바꿈 문자일 시
		currentLineNumber++;
		currentColumnNumber = 0;
	}
	else // 일반 문자일 시
		currentColumnNumber++;

	// 읽은 문자 반환
	return ch;
}

/** 한 글자 읽었던 것 취소하는 함수 */
static void unreadChar(int ch)
{
	if (ch == EOF) // 마지막으로 읽은 것이 EOF이면 되돌릴 필요 없음
		return;

	// 마지막으로 읽은 문자를 다시 파일 스트림에 넣음
	ungetc(ch, sourceFile);
	
	// readChar하기 전으로 line number, column number 복원
	currentLineNumber = lastReadLineNumber;
	currentColumnNumber = lastReadColumnNumber - 1;
}

/** Token 초기화 */
static void initToken(struct tokenType *token, int lineNumber, int columnNumber)
{
	token->number = tnull;
	token->value[0] = '\0';
	copyText(token->fileName, FILE_NAME_LENGTH, scannerFileName);
	token->lineNumber = lineNumber;
	token->columnNumber = columnNumber;
}

/** Token 정보 저장 */
static void setToken(struct tokenType *token, int number, const char *value)
{
	token->number = number;
	copyText(token->value, TOKEN_VALUE_LENGTH, value);
}

/** target에 source를 복사: target에 source 값을 저장하기 위해 사용 */
static void copyText(char *target, int targetSize, const char *source)
{
	int i;

	if (targetSize <= 0)
		return;
	if (source == NULL)
		source = "";

	for (i = 0; i < targetSize - 1 && source[i] != '\0'; i++)
		target[i] = source[i];
	target[i] = '\0';
}

/** target에 문자 ch 추가하고 length++ */
static int appendChar(char *target, int *length, int ch)
{
	if (*length >= TOKEN_VALUE_LENGTH - 1)
		return 0;

	target[*length] = (char)ch;
	(*length)++;
	target[*length] = '\0';
	return 1;
}

/** buffer에 ch가 의미하는 문자열 자체를 저장 */
static void describeChar(int ch, char *buffer, int bufferSize)
{
	if (ch == EOF) // EOF 만났을 시
		copyText(buffer, bufferSize, "EOF"); // buffer에 "EOF"라는 문자열 저장
	else if (ch == '\n') // 줄바꿈 만났을 시
		copyText(buffer, bufferSize, "\\n"); // buffer에 "\n" 문자열 저장
	else if (ch == '\t') // 탭 만났을 시
		copyText(buffer, bufferSize, "\\t"); // buffer에 "\t" 문자열 저장
	else if (isprint((unsigned char)ch)) // 출력 가능한 문자 만났을 시
		sprintf_s(buffer, bufferSize, "'%c'", ch); // buffer에 "'문자'"를 저장
	else // 아무에도 해당하지 않는, 눈에 보이지 않는 문자 만났을 시
		sprintf_s(buffer, bufferSize, "ASCII %d", ch); // 문자의 ASCII 코드값을 저장 
}

static void lexicalError(int n, int lineNumber, int columnNumber, const char *detail)
{
	printf(" *** Lexical Error (%s:%d:%d) : ", scannerFileName, lineNumber, columnNumber);
	switch (n) {
	case 1: printf("an identifier length must be less than 12.\n");
		break;
	case 2: printf("next character must be &\n");
		break;
	case 3: printf("next character must be |\n");
		break;
	case 4: printf("invalid character\n");
		break;
	case 5: printf("unterminated comment\n");
		break;
	case 6: printf("unterminated string literal\n");
		break;
	case 7: printf("invalid character literal\n");
		break;
	case 8: printf("invalid double literal\n");
		break;
	case 9: printf("hexadecimal integer needs at least one hex digit\n");
		break;
	case 10: printf("token text is too long and was truncated\n");
		break;
	case 11: printf("octal integer can contain only digits 0 through 7\n");
		break;
	default: printf("unknown scanner error\n");
		break;
	}
	if (detail != NULL && detail[0] != '\0')
		printf(" [%s]", detail);
	printf("\n");
}

static int superLetter(int ch)
{
	if (ch != EOF && (isalpha((unsigned char)ch) || ch == '_')) return 1;
	else return 0;
}

static int superLetterOrDigit(int ch)
{
	if (ch != EOF && (isalnum((unsigned char)ch) || ch == '_')) return 1;
	else return 0;
}

/**
 * Interger Recognition
 * - 16진수
 * - 8진수
 * - 10진수
 * - Real number Recognition; 정수로 시작하는 형태
 */
static void scanNumber(int firstCharacter, struct tokenType *token)
{
	char buff[TOKEN_VALUE_LENGTH]; // 현재 읽고 있는 문자열 임시 저장 버퍼
	int length = 0; // 버퍼에 저장된 글자 수
	int ch; // scanner가 읽은 글자
	int hexDigits = 0; // 16진수 값 길이 (0x 제외)
	int tooLong = 0; // 제한 길이 초과 여부

	/* buff에 읽은 글자 append */
	appendChar(buff, &length, firstCharacter);

	/* 글자 읽기 */
	ch = readChar();
	
	/* 16진수 인식 */
	if (firstCharacter == '0' && (ch == 'X' || ch == 'x')) { // 0x, 0X 인식
		appendChar(buff, &length, ch);
		ch = readChar();
		while (hexValue(ch) != -1) { // hexa digit 반복해서 인식
			if (!appendChar(buff, &length, ch))
				tooLong = 1;
			hexDigits++;
			ch = readChar();
		}
		unreadChar(ch);

		if (hexDigits == 0) {
			lexicalError(9, token->lineNumber, token->columnNumber, buff);
			return;
		}
		if (tooLong)
			lexicalError(10, token->lineNumber, token->columnNumber, buff);
		setToken(token, tnumber, buff);
		return;
	}

	/** 8진수 인식 */
	if (firstCharacter == '0' && ch >= '0' && ch <= '7') { // 0o 인식
		do {
			if (!appendChar(buff, &length, ch))
				tooLong = 1;
			ch = readChar();
		} while (ch >= '0' && ch <= '7'); // octal digit 반복해서 인식

		if (ch == '8' || ch == '9') { // octal digit은 8, 9가 나오면 Lexical Error
			do {
				if (!appendChar(buff, &length, ch))
					tooLong = 1;
				ch = readChar();
			} while (isdigit((unsigned char)ch));
			unreadChar(ch);
			lexicalError(11, token->lineNumber, token->columnNumber, buff);
			return;
		}

		unreadChar(ch);
		if (tooLong)
			lexicalError(10, token->lineNumber, token->columnNumber, buff);
		setToken(token, tnumber, buff);
		return;
	}

	if (firstCharacter == '0' && (ch == '8' || ch == '9')) { // 08, 09 -> Lexical Error
		do {
			if (!appendChar(buff, &length, ch))
				tooLong = 1;
			ch = readChar();
		} while (isdigit((unsigned char)ch));
		unreadChar(ch);
		lexicalError(11, token->lineNumber, token->columnNumber, buff);
		return;
	}

	/** 10진수 인식 */
	while (isdigit((unsigned char)ch)) {
		if (!appendChar(buff, &length, ch))
			tooLong = 1;
		ch = readChar();
	}

	/** Real number Recognition */
	if (ch == '.') {
		do {
			if (!appendChar(buff, &length, ch))
				tooLong = 1;
			ch = readChar();
		} while (isdigit((unsigned char)ch)); // digit 반복 인식

		unreadChar(ch);

		/** 지수부 검사 */
		if (!scanOptionalExponent(buff, &length, token->lineNumber, token->columnNumber))
			return;

		if (tooLong)
			lexicalError(10, token->lineNumber, token->columnNumber, buff);
		setToken(token, tdoubleLiteral, buff);
		return;
	}

	unreadChar(ch);
	if (tooLong)
		lexicalError(10, token->lineNumber, token->columnNumber, buff);
	setToken(token, tnumber, buff);
}

/** Double literal 인식 - .으로 시작 */
static void scanDotStartedDouble(struct tokenType *token)
{
	char buff[TOKEN_VALUE_LENGTH];
	int length = 0;
	int ch;
	int tooLong = 0;

	appendChar(buff, &length, '.');
	ch = readChar();

	if (!isdigit((unsigned char)ch)) {
		char detail[32];
		unreadChar(ch);
		describeChar('.', detail, sizeof(detail));
		lexicalError(4, token->lineNumber, token->columnNumber, detail);
		return;
	}

	while (isdigit((unsigned char)ch)) {
		if (!appendChar(buff, &length, ch))
			tooLong = 1;
		ch = readChar();
	}
	unreadChar(ch);

	/** 지수부 검사 */
	if (!scanOptionalExponent(buff, &length, token->lineNumber, token->columnNumber))
		return;

	if (tooLong)
		lexicalError(10, token->lineNumber, token->columnNumber, buff);
	setToken(token, tdoubleLiteral, buff);
}

/** 지수부 처리 */
static int scanOptionalExponent(char *buff, int *length, int lineNumber, int columnNumber)
{
	int ch = readChar();
	int exponentDigits = 0;

	/** 지수부 없는 경우 */
	if (ch != 'e' && ch != 'E') {
		unreadChar(ch);
		return 1;
	}

	/** 지수부 있는 경우 */
	appendChar(buff, length, ch);
	
	ch = readChar();
	if (ch == '+' || ch == '-') { // +, - 인식
		appendChar(buff, length, ch);
		ch = readChar();
	}

	// digit 반복해서 인식
	while (isdigit((unsigned char)ch)) {
		appendChar(buff, length, ch);
		exponentDigits++;
		ch = readChar();
	}
	unreadChar(ch);

	/** 지수부 표시만 되어 있고 비어있으면 Lexical Error -> 종결 상태 아님 */
	if (exponentDigits == 0) {
		lexicalError(8, lineNumber, columnNumber, buff);
		return 0;
	}
	return 1;
}

/** String Constant Recognition */
static void scanStringLiteral(struct tokenType *token)
{
	char buff[TOKEN_VALUE_LENGTH];
	int length = 0;
	int ch;
	int tooLong = 0;

	/** 문자열 시작 - "" */
	appendChar(buff, &length, '"');
	
	while (1) {
		ch = readChar();
		if (ch == EOF || ch == '\n') { // "만 나오고 끝 -> Lexical Error
			lexicalError(6, token->lineNumber, token->columnNumber, buff);
			return;
		}

		if (!appendChar(buff, &length, ch)) // 문자 저장
			tooLong = 1;

		if (ch == '"') { // 닫는 " 인식
			if (tooLong)
				lexicalError(10, token->lineNumber, token->columnNumber, buff);
			setToken(token, tstringLiteral, buff);
			return;
		}

		if (ch == '\\') { // escape 인식
			ch = readChar();
			if (ch == EOF || ch == '\n') {
				lexicalError(6, token->lineNumber, token->columnNumber, buff);
				return;
			}
			if (!appendChar(buff, &length, ch)) // 문자 저장
				tooLong = 1;
		}
	}
}

/** Character Literal 인식 */
static void scanCharacterLiteral(struct tokenType *token)
{
	char buff[TOKEN_VALUE_LENGTH];
	int length = 0;
	int ch;
	int closing;

	/** '로 시작 */
	appendChar(buff, &length, '\'');

	/** 한 글자 인식 */
	ch = readChar();
	if (ch == EOF || ch == '\n' || ch == '\'') { // ', '' -> Lexical Error
		lexicalError(7, token->lineNumber, token->columnNumber, buff);
		return;
	}
	appendChar(buff, &length, ch);

	if (ch == '\\') { // escape 문자
		ch = readChar();
		if (ch == EOF || ch == '\n') { // '\만 나오고 끝날 때 -> Lexical Error
			lexicalError(7, token->lineNumber, token->columnNumber, buff);
			return;
		}
		appendChar(buff, &length, ch);
	}

	/** 닫는 ' */
	closing = readChar();
	if (closing != '\'') { // 닫는 따옴표 안 오면 -> Lexical Error
		while (closing != EOF && closing != '\n' && closing != '\'')
			closing = readChar();
		lexicalError(7, token->lineNumber, token->columnNumber, buff);
		return;
	}
	appendChar(buff, &length, closing);
	
	setToken(token, tcharLiteral, buff);
}

/** 블록 주석 인식
 * documented: /-*-* 형태
 */
static void scanBlockComment(struct tokenType *token, int documented)
{
	char contents[TOKEN_VALUE_LENGTH]; // 주석 내용
	int length = 0; // 주석 내용 길이
	int ch;
	int previousWasStar = documented;  // 직전에 읽은 문자가 *인지 여부
	int tooLong = 0; // 제한 길이 초과 여부

	contents[0] = '\0'; // 버퍼 초기화
	while (1) {
		ch = readChar();
		if (ch == EOF) { // 주석 시작하고 바로 EOF -> Lexical Error
			lexicalError(5, token->lineNumber, token->columnNumber, NULL);
			return;
		}

		if (previousWasStar && ch == '/') { // 직전이 *이고 / 만날 시
			if (documented && length > 0 && contents[length - 1] == '*') // 버파가 /**...*인 상태에서 / 만난 상황
				contents[--length] = '\0';  // 직전에 나온 *가 내용이 아니라 주석 닫는 문자임을 파악
			if (documented) // 주석 인식 종료: 일반 블록 주석 형태는 따로 Token 저장 X
				setToken(token, tdocumentedComment, contents);
			return;
		}

		if (documented && !appendChar(contents, &length, ch)) // 버퍼 가득 찼을 경우
			tooLong = 1;
		previousWasStar = (ch == '*'); // * 만나면 직전 문자 * 여부 플래그 on

		if (tooLong) {
			lexicalError(10, token->lineNumber, token->columnNumber, contents);
			tooLong = 0; // 주석 닫는 부분 찾기 위해 다시 루프 돌림
		}
	}
}

/** 라인 주석 인식
 * documented: ///
 */
static void scanLineComment(struct tokenType *token, int documented)
{
	char contents[TOKEN_VALUE_LENGTH]; // 주석 내용
	int length = 0;
	int ch;
	int tooLong = 0;

	contents[0] = '\0';
	ch = readChar();
	while (ch != EOF && ch != '\n') {
		if (documented && !appendChar(contents, &length, ch))
			tooLong = 1;
		ch = readChar();
	}

	if (documented) {
		if (tooLong)
			lexicalError(10, token->lineNumber, token->columnNumber, contents);
		setToken(token, tdocumentedComment, contents);
	}
}

/** hexa digit 정의 */
static int hexValue(int ch)
{
	switch (ch) {
	case '0': case '1': case '2': case '3': case '4':
	case '5': case '6': case '7': case '8': case '9':
		return (ch - '0');
	case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
		return (ch - 'A' + 10);
	case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
		return (ch - 'a' + 10);
	default: return -1;
	}
}

/** Token 정보 출력 */
void printToken(struct tokenType token)
{
	if (token.number == tdocumentedComment)
		printf("Documented Comments ------> %s\n", token.value);
	else
		printf("Token -----> %s (%d, %s, %s, %d, %d)\n",
			token.value, token.number, token.value, token.fileName,
			token.lineNumber, token.columnNumber);
}
