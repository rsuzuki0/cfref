/* cfref -- A C function crossrefernce generator */

/* PROGRAMED BY  Ryuji Suzuki (JF7WEX) wex@jf7wex.sdj.miyagi.prug.or.jp */
/* (c) July, 1992. */

/* 3rd   July 1992   specification */
/* 5th   July 1992   first test version */
/* 8th   July 1992   2nd test version */ 


#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <setjmp.h>
#include <stdlib.h>

typedef enum { false = 0, true = 1 } boolean;
/* typedef char uchar; */
typedef unsigned char uchar;
enum functions { declare, prototype, call };

uchar const	*version = "1.00a",
			*command = "cfref";

#define DEBUGMODE false

unsigned brace = 0;

struct {
	char *filename;
	boolean verbose;
	boolean iglib;
	boolean igpro;
	boolean igprolib;
	boolean fileout;
} flag = {(char *)NULL, DEBUGMODE, true, true, true, false};


struct funclist {
	struct funclist *next; /* all categories' list */
	struct funclist *forward; /* list without calling */
	char *symbol;
	int filenum;
	enum functions category;
	boolean islib;
} funclisttop = {(struct funclist *)NULL, (struct funclist *)NULL, "", 0, 0, false}; 


struct liblist {
	struct liblist *next;
	char *symbol;
} liblisttop = {(struct liblist *)NULL, (char *)NULL};


boolean
islibfunc(char const *ptr)
{
	struct liblist *p;

	for (p = liblisttop.next; p; p = p->next)
		if (!strcmp(p->symbol, ptr)) return true;
	return false;
}


boolean
isreserved(char const  *ptr)
{
	static uchar const *table[] = {
		"auto", "break", "case", "char", "const", "double", "else",
		"enum", "extern", "float", "int", "long", "register", "return",
		"short", "struct", "switch", "typedef", "union", "unsigned",
		"contine", "default", "do", "for", "goto", "if", "signed",
		"sizeof", "static", "void", "volatile", "while", (uchar *)NULL,
	};
	int i;
		for (i = 0;table[i]; i++)
			if (!strcmp(table[i], ptr))
				return true;

	return false;
}


jmp_buf jmpbuf;

int
gec(FILE *fp)
{
	int c;
	if (EOF == (c = getc(fp))) longjmp(jmpbuf, 1);
	return c;
}


boolean
analyze_calling(char buf[], FILE *fp, int filenum);


boolean
iscalling(FILE *fp, int filenum)
{
	auto int i, j;
	auto uchar buf[35];

	j = 0;
	while (EOF != (i = gec(fp))) {
		if (isspace(i)) continue;
		if (i != '(' && i != '_' && !isalpha(i)) { /*)*/
			ungetc(i, fp);
			return true;
		}
		if (i == '(') { /*)*/
			return analyze_calling(buf, fp, filenum);
		}
		buf[j++] = i;
		buf[j] = '\0';
	}
	return false;
}	


#define gc (c = gec(fp))


boolean
analyze_calling(char buf[], FILE *fp, int filenum)
{
	int bracket = 1;
	int c = 0, i = 0;

	for (gc; !feof(fp); gc) {
		struct funclist *p, *q;
		if (isspace(c)) continue;
		if (c == '/') if (gc == '*') { /* comment */
			for (;;) {
				while (gc != '*')
					;
				if (gc == '/') break;
			}
			continue;
		}
		if (c == '\"' || c == '\'') { /* quote */
			while ((i = gec(fp)) == '\\') gec(fp);
			while (i != c)
				while ((i = gec(fp)) == '\\') gec(fp);
			continue;
		}
		if (c == '(')
			++bracket;
		else if (c == ')')
			--bracket;
		if (bracket == 0) {
			if (isspace(gc)) while (isspace(gc)) ;
			if (isreserved(buf) || (flag.iglib && islibfunc(buf))) {
				ungetc(c, fp);
				return true;
			}
			p = malloc(sizeof(struct funclist));
			if (p == NULL) return false;
			p->symbol = strdup(buf);
			p->next = NULL;
			p->forward = NULL;
			p->filenum = filenum;
			switch (c) {
				case '{': /* decraletion */
					/* } dummy */
					p->category = declare;
					break;
				case ';': /* prototype */
				default : /* call */
					if (brace) {
						p->category = call;
					} else {
						p->category = prototype;
					}	
					break;
			}
			ungetc(c, fp);
			for (q = &funclisttop; q->next; q = q->next)
				;
			q->next = p;
			return true;
		} else if (c == '_' || isalpha(c)) {
			ungetc(c, fp);
			iscalling(fp, filenum);
		}
	}
	return false;
}			


boolean
analyze(FILE *fp, int filenum)
{
	int c = 0, i = 0;
	uchar buf[35];

	for (gc; !feof(fp); gc) {
		if (c == '/') if (gc == '*') {
			for (;;) {
				while (gc != '*')
					;
				if (gc == '/') break;
			}
			i = 0;
			continue;
		}
		if (c == '\"' || c == '\'') {
			while ((i = gec(fp)) == '\\') gec(fp);
			while (i != c)
				while ((i = gec(fp)) == '\\') gec(fp);
			i = 0;
			continue;
		}
		if (c == '#') {
			for (gc; c != '\n'; gc)
				for (; c == '\\'; gc) ;
			i = c =  0;
			continue;
		}
		if (c == '{') {
			brace++;
		} else if (c == '}') {
			brace --;
		}	
		if (c == '\n' || c == ' ' || isspace(c)) {
			while (isspace(gc)) ;
			if (c != '(') {   /*)*/
				ungetc(c, fp);
				i = c = 0;
				continue;
			}
		}
		if (!i && c != '_' && !isalpha(c)) continue;
		if (c != '(' && c != '_' && !isalpha(c)) { /*)*/
			i = 0;
			continue;
		}
		if (i && c == '(' && isreserved(buf)) { /*)*/
			i = c = 0;
			continue;
		}
		if (i && c == '(') { /*)*/
			analyze_calling(buf, fp, filenum);
		}
		if (c == 0 || isspace(c)) {
			continue;
		}
		buf[i++] = (uchar)c;
		buf[i] = (uchar)NULL;
	}
	return false;
}


boolean
addliblist(char const *srcfile)
{
	FILE *fp;
	uchar buf[512];
	struct liblist *q;

	if ((FILE *)NULL == (fp = fopen(srcfile, "rt"))) {
		fprintf(stderr, "cfref: libfile \'%s\' not found.\n", srcfile);
		return false;
	}
	while (fgets(buf, 512, fp)) {
		if (*buf == '#' || *buf == ';' || (*buf != '_' && !isalpha(*buf)))
			continue;
		buf[strspn(buf, "_abcdefghijklmnopqrstuvwxyz"
						"ABCDEFGHIJKLMNOPQRSTUVWXYZ")] = (uchar)NULL;
		q = malloc(sizeof(struct liblist));
		if (!q) return false;
		q->symbol = strdup(buf);
		if (!q->symbol) return false;
		q->next = liblisttop.next;
		liblisttop.next = q;
	}
	fclose(fp);
	return true;	
}


boolean
run(char const *srcfile, int filenum)
{
	FILE *fp;
	brace = 0;
	if (setjmp(jmpbuf)) {
		fclose(fp);
		return true;
	}	
	if (NULL != (fp = fopen(srcfile, "rt"))) {
		while (analyze(fp, filenum))
			;
		fclose(fp);
		return true;	
	} else fprintf(stderr, "cfref: file \'%s\' not found.\n", srcfile);
	return false;
}


void
remake(void)
{
	struct funclist *p, *q;

	for (p = q = &funclisttop; p; p = p->next) {
		if (p->category != call) {
			q->forward = p;
			q = p;
		}
	}
	q->forward = (struct funclist *)NULL;
}

struct {
	int prototype;
	int declare;
} searched = {0, 0};

void
search(char *func)
{
	struct funclist *p;

	searched.prototype = searched.declare = 0;
	for (p = &funclisttop; p; p = p->forward) {
		if (!strcmp(func, p->symbol)) {
			if (p->category == prototype) {
				searched.prototype = p->filenum;
			} else if (p->category == declare) {
				searched.declare = p->filenum;
			}
		}
	}
}


FILE *
change_outdevice(int filenum, char const *argv[])
{
	uchar *ptr;
	FILE *fp;
	uchar buf[128];
	if (!flag.fileout) return (stdout);
	strcpy(buf, argv[filenum]);
	ptr = strrchr(buf, '.');
	if (ptr) *ptr = '\0';
	strcat(buf, ".cf");
	fp = fopen(buf, "wt");
	if (!fp) {
		fprintf(stderr, "%s: Unable to write to %s.\a\n", command, buf);
		fp = (stdout);
	}
	return fp;
}	


void
report(char const *argv[])
{
	register struct funclist *p;
	register uchar *previous = "";
	boolean isinfunc;
	FILE *fpw;
	unsigned prefilenum;

	remake();
	fpw = (stdout);
	isinfunc = false;
	prefilenum = 0;
	for (p = funclisttop.next; p; p = p->next) {
		switch (p->category) { 
			case call:
				isinfunc = true;
				if (flag.fileout && p->filenum != prefilenum) {
					fclose(fpw);
					fpw = change_outdevice(p->filenum, argv);
					prefilenum = p->filenum;
				}
				if (!strcmp(previous, p->symbol)) {
					continue;
				}
				search(p->symbol);
				fprintf(fpw, "\t%s(", p->symbol); /* ) */
				if (searched.declare)
					fprintf(fpw, "+%s", argv[searched.declare]);
				if (searched.declare && searched.prototype)
					fputs(", ", fpw);	
				if (searched.prototype)
					fprintf(fpw, "-%s", argv[searched.declare]);
				/* ( */	
				fprintf(fpw, ");\n");
				previous = p->symbol;
				break;
			case prototype:
					/* { */
				if (isinfunc) fputs("}\n\n", fpw);
				isinfunc = false;
				if (flag.fileout && p->filenum != prefilenum) {
					fclose(fpw);
					fpw = change_outdevice(p->filenum, argv);
					prefilenum = p->filenum;
				}
				fprintf(fpw, "%s();\n\n", p->symbol);
				break;
			case declare:
					/* { */
				if (isinfunc) fputs("}\n\n", fpw);
				isinfunc = true;
				if (flag.fileout && p->filenum != prefilenum) {
					fclose(fpw);
					fpw = change_outdevice(p->filenum, argv);
					prefilenum = p->filenum;
				}
				fprintf(fpw, "%s()\n{\n", p->symbol); /* } */
				break;
			default:
				break;
		}
	}
		/* { */
	if (isinfunc) fputs("}\n\n", fpw);
}


int
main(int argc, const char *argv[])
{
	int i, j;

	for (i = 1; i < argc; i++) {
		if (argv[i][0] == '-') {
			switch (argv[i][1]) {
				case 'v':
					fprintf(stderr, "%s: version %s\n", command, version);
					exit(2);
				case 'L':
					addliblist("/usr/local/lib/funclist");
					break;
				case 'l':
					addliblist(argv[++i]);
					break;
				case 'f':
					flag.fileout = true;
					break;	
				case 'i':
					for (j = 0; argv[i][j]; j++) {
						switch (argv[i][j]) {
							case 'p':
								flag.igpro = true;
								break;
							case 'l':
								flag.iglib = true;
								break;
							defalult:
								fputs("cfref: incorrect level of \"-i\".\n", stderr);
								exit(1);
						}
					}
					break;
				case 'h':
				case '?':
				default:
					fprintf(stderr, "usage: %s [-v-h-?] [-L] [-ip-il] [-f] [-l file...]\n",
					command);
					exit(2);
			}
		} else run(argv[i], i);
	}
	report(argv);
	return 0;
}
