/*
*   Copyright (c) 1996-2003, Darren Hiebert
*
*   This source code is released into the public domain.
*
*   This module contains functions for reading tag files.
*/

#include "readtags.h"
#include <string.h>		/* strerror */
#include <stdlib.h>		/* exit */
#include <stdio.h>		/* stderr */

static const char *TagFileName = "tags";
static const char *ProgramName;
static int extensionFields;
static int SortOverride;
static sortType SortMethod;
static int allowPrintLineNumber;
static int debugMode;
static int escaping;
#ifdef READTAGS_DSL
#include "dsl/qualifier.h"
static QCode *Qualifier;
#endif

static void ultostr (char dst [21], unsigned long d)
{
	int o [20];
	int i;

	if (d == 0)
	{
		dst [0] = '0';
		dst [1] = '\0';
		return;
	}

	for (i = 0; d != 0; i++, d = d/10)
		o [i] = d % 10;

	for (int j = i - 1; j >= 0; j--)
		dst [i - j - 1] = o[j] + '0';
	dst [i] = '\0';
}

static void printValue (const char *val, int printingWithEscaping,
						int  (* print_str) (const char *, void *),
						int  (* print_char) (int, void *),
						void *outfp)
{
	if (printingWithEscaping)
	{
		for(; *val != '\0'; val++)
		{
			switch (*val)
			{
				case '\t': print_str ("\\t",  outfp); break;
				case '\r': print_str ("\\r",  outfp); break;
				case '\n': print_str ("\\n",  outfp); break;
				case '\\': print_str ("\\\\", outfp); break;
					/* Universal-CTags extensions */
				case '\a': print_str ("\\a", outfp); break;
				case '\b': print_str ("\\b", outfp); break;
				case '\v': print_str ("\\v", outfp); break;
				case '\f': print_str ("\\f", outfp); break;
				default:
					if ((0x01 <= *val && *val <= 0x1F) || *val == 0x7F)
					{
						char c[5] = {
							[0] = '\\',
							[1] = 'x',
						};
						c [2] = (*val / 16) % 16;
#if 0
						if (c [2] == 0)
						{
							c [2] = *val % 16;
							c [2] += ( c [2] < 10 )? '0': 'A' - 9;
							c [3] = '\0';
						}
						else
#endif
						{
							c [2] += ( c [2] < 10 )? '0': 'A' - 9;
							c [3] = *val % 16;
							c [3] += ( c [3] < 10 )? '0': 'A' - 9;
							c [4] = '\0';
						}
						print_str (c, outfp);
					}
					else
						print_char (*val, outfp);
			}
		}
	}
	else
		print_str (val, outfp);
}

static void tagsPrintTag (const tagEntry *entry,
						  int printingExtensionFields,
						  int printingLineNumber,
						  int printingWithEscaping,
						  int pseudoTag,
						  int  (* print_str) (const char *, void *),
						  int  (* print_char) (int, void *),
						  void *outfp)
{
	int i;
	int first = 1;
	const char* separator = ";\"";
	const char* const empty = "";
/* "sep" returns a value only the first time it is evaluated */
#define sep (first ? (first = 0, separator) : empty)

	if (entry->name == NULL
		|| entry->file == NULL
		|| entry->address.pattern == NULL)
		return;
	if (pseudoTag)
		print_str (entry->name, outfp);
	else if (*entry->name == '!' && printingWithEscaping)
	{
		print_str ("\\x21", outfp);
		printValue (entry->name + 1, printingWithEscaping,
					print_str, print_char, outfp);
	}
	else if (*entry->name == ' ' && printingWithEscaping)
	{
		print_str ("\\x20", outfp);
		printValue (entry->name + 1, printingWithEscaping,
					print_str, print_char, outfp);
	}
	else
		printValue (entry->name, printingWithEscaping,
					print_str, print_char, outfp);

	print_char ('\t', outfp);
	printValue  (entry->file, printingWithEscaping,
				 print_str, print_char, outfp);
	print_char ('\t', outfp);
	print_str (entry->address.pattern, outfp);

	if (printingExtensionFields)
	{
		if (entry->kind != NULL  &&  entry->kind [0] != '\0')
		{
			print_str (sep, outfp);
			print_str ("\tkind:", outfp);
			printValue (entry->kind, printingWithEscaping,
						print_str, print_char, outfp);
			first = 0;
		}
		if (entry->fileScope)
		{
			print_str (sep, outfp);
			print_str ("\tfile:", outfp);
			first = 0;
		}
		if (printingLineNumber && entry->address.lineNumber > 0)
		{
			print_str (sep, outfp);
			print_str ("\tline:", outfp);
			char buf [20 + 1];	/* 20 comes from UINNT64_MAX, 1 is for \0. */
			ultostr (buf, entry->address.lineNumber);
			print_str (buf, outfp);
			first = 0;
		}
		for (i = 0  ;  i < entry->fields.count  ;  ++i)
		{
			if (entry->fields.list [i].key)
			{
				print_str (sep, outfp);
				print_char ('\t', outfp);
				print_str (entry->fields.list [i].key, outfp);
				print_char (':', outfp);
				if (entry->fields.list  [i].value)
					printValue (entry->fields.list [i].value,
								printingWithEscaping, print_str, print_char, outfp);
				first = 0;
			}
		}
	}
	print_char ('\n', outfp);
#undef sep
}

static void tagsPrintTagToFILE (FILE *outfp, const tagEntry *entry,
								int printingExtensionFields,
								int printingLineNumber,
								int printingWithEscaping,
								int pseudoTag)
{
	tagsPrintTag (entry,
				  printingExtensionFields, printingLineNumber, printingWithEscaping, pseudoTag,
				  (int  (*) (const char *, void *))fputs,
				  (int  (*) (const int, void *))fputc,
				  outfp);
}

static void printTag (const tagEntry *entry)
{
	tagsPrintTagToFILE (stdout, entry, extensionFields, allowPrintLineNumber, escaping, 0);
}

static void printPseudoTag (const tagEntry *entry)
{
	tagsPrintTagToFILE (stdout, entry, extensionFields, allowPrintLineNumber, escaping, 1);
}

static void walkTags (tagFile *const file, tagEntry *first_entry,
					  tagResult (* nextfn) (tagFile *const, tagEntry *),
					  void (* actionfn) (const tagEntry *))
{
	do
	{
#ifdef READTAGS_DSL
		if (Qualifier)
		{
			int i = q_is_acceptable (Qualifier, first_entry);
			switch (i)
			{
			case Q_REJECT:
				continue;
			case Q_ERROR:
				exit (1);
			}
		}
#endif
		(* actionfn) (first_entry);
	} while ( (*nextfn) (file, first_entry) == TagSuccess);
}

static void findTag (const char *const name, const int options)
{
	tagFileInfo info;
	tagEntry entry;
	tagFile *const file = tagsOpen (TagFileName, &info);
	if (file == NULL)
	{
		fprintf (stderr, "%s: cannot open tag file: %s: %s\n",
				ProgramName, strerror (info.status.error_number), TagFileName);
		exit (1);
	}
	else
	{
		if (SortOverride)
			tagsSetSortType (file, SortMethod);
		if (debugMode)
			fprintf (stderr, "%s: searching for \"%s\" in \"%s\"\n",
					 ProgramName, name, TagFileName);
		if (tagsFind (file, &entry, name, options) == TagSuccess)
			walkTags (file, &entry, tagsFindNext, printTag);
		tagsClose (file);
	}
}

static void listTags (int pseudoTags)
{
	tagFileInfo info;
	tagEntry entry;
	tagFile *const file = tagsOpen (TagFileName, &info);
	if (file == NULL)
	{
		fprintf (stderr, "%s: cannot open tag file: %s: %s\n",
				ProgramName, strerror (info.status.error_number), TagFileName);
		exit (1);
	}
	else if (pseudoTags)
	{
		if (tagsFirstPseudoTag (file, &entry) == TagSuccess)
			walkTags (file, &entry, tagsNextPseudoTag, printPseudoTag);
		tagsClose (file);
	}
	else
	{
		if (tagsFirst (file, &entry) == TagSuccess)
			walkTags (file, &entry, tagsNext, printTag);
		tagsClose (file);
	}
}

static const char *const Usage =
	"Find tag file entries matching specified names.\n\n"
	"Usage: \n"
	"    %s -h | --help\n"
	"        Print this help message.\n"
#ifdef READTAGS_DSL
	"    %s -H POSTPROCESSOR | --help-expression POSTPROCESSOR\n"
	"        Print available terms that can be used in POSTPROCESSOR expression.\n"
	"        POSTPROCESSOR: filter\n"
#endif
	"    %s [OPTIONS] ACTION\n"
	"        Do the specified action.\n"
	"Actions:\n"
	"    -l | --list\n"
	"        List regular tags.\n"
	"    [-] NAME...\n"
	"        List regular tags matching NAME(s).\n"
	"        \"-\" indicates arguments after this as NAME(s) even if they start with -.\n"
	"    -D | --list-pseudo-tags\n"
	"        List pseudo tags.\n"
	"Options:\n"
	"    -d | --debug\n"
	"        Turn on debugging output.\n"
	"    -E | --escape\n"
	"        Escape characters like tab as described in tags(5).\n"
	"    -e | --extension-fields\n"
	"        Include extension fields in output.\n"
	"    -i | --icase-match\n"
	"        Perform case-insensitive matching in the NAME action.\n"
	"    -n | --line-number\n"
	"        Also include the line number field when -e option is given.\n"
	"    -p | --prefix-match\n"
	"        Perform prefix matching in the NAME action.\n"
	"    -t TAGFILE | --tag-file TAGFILE\n"
	"        Use specified tag file (default: \"tags\").\n"
	"    -s[0|1|2] | --override-sort-detection METHOD\n"
	"        Override sort detection of tag file.\n"
	"        METHOD: unsorted|sorted|foldcase\n"
#ifdef READTAGS_DSL
	"    -Q EXP | --filter EXP\n"
	"        Filter the tags listed by ACTION with EXP before printing.\n"
#endif
	;

static void printUsage(FILE* stream, int exitCode)
{
	fprintf (stream, Usage, ProgramName, ProgramName, ProgramName);
	exit (exitCode);
}

#ifdef READTAGS_DSL
static void printFilterExpression (FILE *stream, int exitCode)
{
	fprintf (stream, "Filter expression: \n");
	q_help (stream);
	exit (exitCode);
}

static QCode *convertToQualifier(const char* exp)
{
	EsObject *sexp = es_read_from_string (exp, NULL);
	QCode *qcode;

	if (es_error_p (sexp))
	{
		fprintf (stderr,
			 "Failed to read the expression of qualifier: %s\n", exp);
		fprintf (stderr,
			 "Reason: %s\n", es_error_name (sexp));
		exit (1);
	}

	qcode = q_compile (sexp);
	if (qcode == NULL)
	{
		fprintf (stderr,
			 "Failed to compile the expression of qualifier: %s\n", exp);
		exit (1);
	}
	es_object_unref (sexp);
	return qcode;
}
#endif
extern int main (int argc, char **argv)
{
	int options = 0;
	int actionSupplied = 0;
	int i;
	int ignore_prefix = 0;

	ProgramName = argv [0];
	if (argc == 1)
		printUsage(stderr, 1);
	for (i = 1  ;  i < argc  ;  ++i)
	{
		const char *const arg = argv [i];
		if (ignore_prefix || arg [0] != '-')
		{
			findTag (arg, options);
			actionSupplied = 1;
		}
		else if (arg [0] == '-' && arg [1] == '\0')
			ignore_prefix = 1;
		else if (arg [0] == '-' && arg [1] == '-')
		{
			const char *optname = arg + 2;
			if (strcmp (optname, "debug") == 0)
				debugMode++;
			else if (strcmp (optname, "list-pseudo-tags") == 0)
			{
				listTags (1);
				actionSupplied = 1;
			}
			else if (strcmp (optname, "help") == 0)
				printUsage (stdout, 0);
#ifdef READTAGS_DSL
			else if (strcmp (optname, "help-expression") == 0)
			{
				if (i + 1 < argc)
				{
					const char *exp_klass = argv [++i];
					if (strcmp (exp_klass, "filter") == 0)
						printFilterExpression (stdout, 0);
					else
					{
						fprintf (stderr, "%s: unknown expression class for --%s option",
								 ProgramName, optname);
						exit (1);

					}
				}
				else
				{
					fprintf (stderr, "%s: missing expression class for --%s option",
							 ProgramName, optname);
					exit (1);
				}
			}
#endif
			else if (strcmp (optname, "escape") == 0)
				escaping = 1;
			else if (strcmp (optname, "extension-fields") == 0)
				extensionFields = 1;
			else if (strcmp (optname, "icase-match") == 0)
				options |= TAG_IGNORECASE;
			else if (strcmp (optname, "prefix-match") == 0)
				options |= TAG_PARTIALMATCH;
			else if (strcmp (optname, "list") == 0)
			{
				listTags (0);
				actionSupplied = 1;
			}
			else if (strcmp (optname, "line-number") == 0)
				allowPrintLineNumber = 1;
			else if (strcmp (optname, "tag-file") == 0)
			{
				if (i + 1 < argc)
					TagFileName = argv [++i];
				else
					printUsage (stderr, 1);
			}
			else if (strcmp (optname, "override-sort-detection") == 0)
			{
				if (i + 1 < argc)
				{
					const char *sort_spec = argv [++i];
					if (strcmp (sort_spec, "0") == 0
						|| strcmp (sort_spec, "unsorted") == 0)
						SortMethod = 0;
					else if (strcmp (sort_spec, "1") == 0
							 || strcmp (sort_spec, "sorted") == 0)
						SortMethod = 1;
					else if (strcmp (sort_spec, "2") == 0
							 || strcmp (sort_spec, "foldcase") == 0)
						SortMethod = 2;
					else
					{
						fprintf (stderr, "%s: unknown sort method for --%s option",
								 ProgramName, optname);
						exit (1);
					}
				}
				else
				{
					fprintf (stderr, "%s: missing sort method for --%s option",
							 ProgramName, optname);
					exit (1);
				}
			}
#ifdef READTAGS_DSL
			else if (strcmp (optname, "filter") == 0)
			{
				if (i + 1 < argc)
					Qualifier = convertToQualifier (argv[++i]);
				else
				{
					fprintf (stderr, "%s: missing filter expression for --%s option",
							 ProgramName, optname);
					exit (1);
				}
			}
#endif
			else
			{
				fprintf (stderr, "%s: unknown long options: --%s",
						 ProgramName, optname);
				exit (1);
				break;
			}
		}
		else
		{
			size_t j;
			for (j = 1  ;  arg [j] != '\0'  ;  ++j)
			{
				switch (arg [j])
				{
					case 'd': debugMode++; break;
					case 'D': listTags (1); actionSupplied = 1; break;
					case 'h': printUsage (stdout, 0); break;
#ifdef READTAGS_DSL
					case 'H':
						if (i + 1 < argc)
						{
							const char *exp_klass = argv [++i];
							if (strcmp (exp_klass, "filter") == 0)
								printFilterExpression (stdout, 0);
							else
								printUsage(stderr, 1);
						}
						else
							printUsage(stderr, 1);
#endif
					case 'E': escaping = 1; break;
					case 'e': extensionFields = 1;         break;
					case 'i': options |= TAG_IGNORECASE;   break;
					case 'p': options |= TAG_PARTIALMATCH; break;
					case 'l': listTags (0); actionSupplied = 1; break;
					case 'n': allowPrintLineNumber = 1; break;
					case 't':
						if (arg [j+1] != '\0')
						{
							TagFileName = arg + j + 1;
							j += strlen (TagFileName);
						}
						else if (i + 1 < argc)
							TagFileName = argv [++i];
						else
							printUsage(stderr, 1);
						break;
					case 's':
						SortOverride = 1;
						++j;
						if (arg [j] == '\0')
							SortMethod = TAG_SORTED;
						else if (strchr ("012", arg[j]) != NULL)
							SortMethod = (sortType) (arg[j] - '0');
						else
							printUsage(stderr, 1);
						break;
#ifdef READTAGS_DSL
					case 'Q':
						if (i + 1 == argc)
							printUsage(stderr, 1);
						Qualifier = convertToQualifier (argv[++i]);
						break;
#endif
					default:
						fprintf (stderr, "%s: unknown option: %c\n",
									ProgramName, arg[j]);
						exit (1);
						break;
				}
			}
		}
	}
	if (! actionSupplied)
	{
		fprintf (stderr,
			"%s: no action specified: specify one of NAME, -l or -D\n",
			ProgramName);
		exit (1);
	}
	return 0;
}
