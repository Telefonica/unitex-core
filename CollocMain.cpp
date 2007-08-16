
 /*
  * Unitex
  *
  * Copyright (C) 2001-2007 Université de Marne-la-Vallée <unitex@univ-mlv.fr>
  *
  * This library is free software; you can redistribute it and/or
  * modify it under the terms of the GNU Lesser General Public
  * License as published by the Free Software Foundation; either
  * version 2.1 of the License, or (at your option) any later version.
  *
  * This library is distributed in the hope that it will be useful,
  * but WITHOUT ANY WARRANTY; without even the implied warranty of
  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  * Lesser General Public License for more details.
  * 
  * You should have received a copy of the GNU Lesser General Public
  * License along with this library; if not, write to the Free Software
  * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA.
  *
  */
  
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>
#include <limits.h>
#include "CollocMain.h"
#include "Unicode.h"
#include "Text_tokens.h"
#include "Alphabet.h"
#include "Collocation.h"
#include "Copyright.h"
#include "Error.h"
#include "Snt.h"
#include "defines.h"

typedef Pvoid_t judy;

static void usage(int header) {

	if (header) {
		u_printf("%S",COPYRIGHT);
		u_printf(
			"Tries to extract multi word expressions using several stochastic"
				"and/or deterministic algorithms. It works on the normalized text automata.\n"
			"\n"
		);
	}

	u_printf(
		"Usage: Colloc [OPTIONS] <snt directory>\n"
		"\n"
		"Parameters:\n"
		"     -?, --help                  Shows this message                                  \n"    
		"     -w, --words-only            Ignores non-word states.                    / n \\  \n"          
		"     -c, --combination-length    The length of word combinations. The p in C |   |   \n"
		"                                 Not yet implemented                         \\ p /  \n" 
		"     -l, --linear-width          The limit in which the token combinations are formed.\n"
		"                                 Not yet implemented\n"
		"\n"
	); 
}

/* 
 * This function behaves in the same way as an int main(), except that it does
 * not invoke the setBufferMode function.
 */

int main_Colloc(int argc, char **argv) {

	char ch;
	int option_index = 0;
	
	const struct option longopts[] = {
		 {               "help",       no_argument, NULL, '?' }
		,{         "words-only",       no_argument, NULL, 'w' }
		,{ "combination-length", required_argument, NULL, 'c' }
		,{       "linear-width", required_argument, NULL, 'l' }
		,{NULL, 0, NULL, 0}
	};

	colloc_opt option;
	option.wonly   = 0; 
	option.clength = 0; 
	option.lwidth  = 0; 
	
	while ((ch = getopt_long(argc, argv, "?wc:l:", longopts, &option_index)) != -1) {
		switch (ch) {
	
		case '?':
			usage(1);
			exit(EXIT_SUCCESS);
			break;	
	
		case 'w':
			option.wonly=1;
			break;

		case 'c':
			STRINGINT(optarg, option.clength);
			if (option.clength <= 0) {
				u_printf("combination length must be > 0\n\n");
				exit (EXIT_FAILURE);
			}

			break;

		case 'l':
			STRINGINT(optarg, option.lwidth);
			if (option.lwidth <= 0) {
				u_printf("linear context width must be > 0\n\n");
				exit (EXIT_FAILURE);
			}
			break;
		
		default:
			exit (EXIT_FAILURE);
		
		}
	}
	
	
	char text_snt[FILENAME_MAX];
	if (optind < argc) {
		if (strlen (argv[optind]) > FILENAME_MAX) {
			u_fprintf(stderr, "`%s' is too long for a file name (max=%d)", argv[optind], FILENAME_MAX);
			exit (EXIT_FAILURE);
		}
		else {
			get_path ( argv[optind], text_snt );
	    }
	}
	else { 
		usage(1);
		u_fprintf(stderr, "Error: no snt directory specified\n\n");
		exit(EXIT_FAILURE);
	}
	
	struct snt_files* snt_files=NULL;
	snt_files = new_snt_files_from_path(text_snt);

	judy candidates;
	
	candidates=colloc_candidates(snt_files, option);
	
	u_printf("Done.\n");
	
	return 0;
}

