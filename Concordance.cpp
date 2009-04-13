 /*
  * Unitex
  *
  * Copyright (C) 2001-2009 Universit� Paris-Est Marne-la-Vall�e <unitex@univ-mlv.fr>
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

#include "Concordance.h"
#include "Unicode.h"
#include "Matches.h"
#include "SortTxt.h"
#include "Error.h"
#include "Buffer.h"
#include "StringParsing.h"
#include "Thai.h"
#include "NewLineShifts.h"

int create_raw_text_concordance(U_FILE*,U_FILE*,U_FILE*,struct text_tokens*,int,int,
                                int*,int*,int,int,struct conc_opt*);
void compute_token_length(int*,struct text_tokens*);

void create_modified_text_file(U_FILE*,U_FILE*,struct text_tokens*,char*,int,int*);
void write_HTML_header(U_FILE*,int,struct conc_opt*);
void write_HTML_end(U_FILE*);
void reverse_initial_vowels_thai(unichar*);



/**
 * This function builds a concordance from a 'concord.ind' file
 * described by the 'concordance' parameter. 'text' is supposed to
 * represent the 'text.cod' file from which the concordance index was
 * computed. 'tokens' represents the associated 'tokens.txt'
 * file. 'option.sort_mode' is an integer that represents the sort mode to be
 * used for creating the concordance. This parameter will be ignored
 * if the function must modify the text instead of building a
 * concordance.  'option.left_context' and 'option.right_context' specify the length
 * of the contexts in visible characters (for Thai, this number is
 * different from the number of unicode characters because of
 * diacritics). 'option.fontname' and 'fontsize' are used to set the font
 * that will be used if the output is an HTML file (if not, these
 * parameters will be ignored). 'option.directory' represents the
 * working directory.  'option.result_mode' indicates the kind of
 * output that is expected. If it value is "html" or "text", the
 * function will build an HTML or text concordance. If its value is of
 * the form "glossanet=xxx", the result will be an HTML concordance
 * designed for the GlossaNet system (http://glossa.fltr.ucl.ac.be),
 * and "xxx" will be taken as a parameter given to GlossaNet. Any
 * other value will be considered as a file name to use for producing
 * a modified version of the text. 'option.sort_alphabet' is the name of the
 * "Alphabet_sort.txt" file to use for sorting the lines of the
 * concordance. This parameter will be ignored if the output is a
 * modified text file or if the sort mode is TEXT_ORDER.
 * 'n_enter_char' is the number of new lines in the text, and
 * 'enter_pos' is an array that contains the positions of these new
 * lines. If 'option.thai_mode' is set to a non zero value, it indicates that
 * the concordance is a Thai one. This information is used to compute
 * correctly the context sizes.
 *
 *
 * Modifications made by Patrick Watrin (pwatrin@gmail.com) allow to
 * build index and axis files.
 *
 * WHAT IS AN AXIS FILE ?
 * ----------------------
 * "SIMR requires axes to be formatted with one "tic mark" per line.
 * A tic mark consists of a semantic unit (a token) and its position
 * in the text. By convention, the position of a token is the position
 * of its median character (I conjecture that this also works best in
 * terms of accuracy). Thus, a token's position is always a multiple
 * of 0.5." (http://nlp.cs.nyu.edu/GMA/docs/HOWTO-axis)
 *
 * EXAMPLE :
 * ---------
 * 012345678901
 * This segment
 * 2.5  9
 */
void create_concordance(U_FILE* concordance,U_FILE* text,struct text_tokens* tokens,
                        int n_enter_char,int* enter_pos,struct conc_opt* option) {
U_FILE* out;
U_FILE* f;
char temp_file_name[FILENAME_MAX];
struct string_hash* glossa_hash=NULL;
int open_bracket=-1;
int close_bracket=-1;
/* We compute the length of each token */
int* token_length=(int*)malloc(sizeof(int)*tokens->N);
if (token_length==NULL) {
   fatal_alloc_error("create_concordance");
}
compute_token_length(token_length,tokens);
if (option->result_mode==MERGE_) {
	/* If we have to produced a modified version of the original text, we
	 * do it and return. */
	create_modified_text_file(concordance,text,tokens,option->output,n_enter_char,enter_pos);
	return;
}
/* If the expected result is a concordance */
if (option->result_mode==GLOSSANET_) {
	/* The structure glossa_hash will be used to ignore duplicate lines
	 * without sorting */
	glossa_hash=new_string_hash();
	/* Building GlossaNet concordances requires to locate square brackets in the
	 * text. That's why we compute the token numbers associated to '[' and ']' */
	unichar r[2];
	r[0]='[';
	r[1]='\0';
	open_bracket=get_token_number(r,tokens);
	r[0]=']';
	close_bracket=get_token_number(r,tokens);
}
/* We set temporary and final file names */
strcpy(temp_file_name,option->working_directory);
strcat(temp_file_name,"concord_.txt");
strcpy(option->output,option->working_directory);
if (option->result_mode==TEXT_ || option->result_mode==INDEX_
      || option->result_mode==UIMA_ || option->result_mode==AXIS_
      || option->result_mode==XALIGN_)
	strcat(option->output,"concord.txt");
else
	strcat(option->output,"concord.html");
int N_MATCHES;

/* If we are in the 'xalign' mode, we don't need to sort the results.
 * So, we don't need to store the results in a temporary file */
if (option->result_mode==XALIGN_) f=u_fopen(UTF8,option->output,U_WRITE);
else f=u_fopen(UTF16_LE,temp_file_name,U_WRITE);
if (f==NULL) {
	error("Cannot write %s\n",temp_file_name);
	return;
}
/* First, we create a raw text concordance.
 * NOTE: columns may have been reordered according to the sort mode. See the
 * comments of the 'create_raw_text_concordance' function for more details. */
N_MATCHES=create_raw_text_concordance(f,concordance,text,tokens,
                                      option->result_mode,n_enter_char,enter_pos,
                                      token_length,open_bracket,close_bracket,
                                      option);
u_fclose(f);
free(token_length);

if(option->result_mode==XALIGN_) return;

/* If necessary, we sort it by invoking the main function of the SortTxt program */
if (option->sort_mode!=TEXT_ORDER) {
   pseudo_main_SortTxt(0,0,option->sort_alphabet,NULL,option->thai_mode,temp_file_name);
}
/* Now, we will take the sorted raw text concordance and we will:
 * 1) reorder the columns
 * 2) insert HTML info if needed
 */

f=u_fopen(UTF16_LE,temp_file_name,U_READ);
if (f==NULL) {
	error("Cannot read %s\n",temp_file_name);
	return;
}
if (option->result_mode==TEXT_ || option->result_mode==INDEX_
      || option->result_mode==UIMA_ || option->result_mode==AXIS_) {
   /* If we have to produce a unicode text file, we open it
    * as a UTF16LE one */
   out=u_fopen(UTF16_LE,option->output,U_WRITE);
}
else {
   /* Otherwise, we open it as a UTF8 HTML file */
   out=u_fopen(UTF8,option->output,"w");
}
if (out==NULL) {
	error("Cannot write %s\n",option->output);
	u_fclose(f);
	return;
}
/* If we have an HTML or a GlossaNet concordance, we must write an HTML
 * file header. */
if (option->result_mode==HTML_ || option->result_mode==GLOSSANET_) write_HTML_header(out,N_MATCHES,option);
unichar A[3000];
unichar B[3000];
unichar C[3000];
unichar href[3000];
unichar indices[100];
unichar* left=NULL;
unichar* middle=NULL;
unichar* right=NULL;
int j;
int c;
/* Now we process each line of the sorted raw text concordance */
while ((c=u_fgetc(f))!=EOF) {
	j=0;
	/* We save the first column in A... */
	while (c!=0x09) {
		A[j++]=(unichar)c;
		c=u_fgetc(f);
	}
	A[j]='\0';
	c=u_fgetc(f);
	j=0;
	/* ...the second in B... */
	while (c!=0x09) {
		B[j++]=(unichar)c;
		c=u_fgetc(f);
	}
	B[j]='\0';
	c=u_fgetc(f);
	j=0;
	/* ...and the third in C */
	while (c!='\n' && c!='\t') {
		C[j++]=(unichar)c;
		c=u_fgetc(f);
	}
	C[j]='\0';
	indices[0]='\0';
	/* If there are indices to be read like "15 17 1", we read them */
	if (c=='\t') {
		c=u_fgetc(f);
		j=0;
		while (c!='\t' && c!='\n') {
			indices[j++]=(unichar)c;
			c=u_fgetc(f);
		}
		indices[j]='\0';
		/*------------begin GlossaNet-------------------*/
		/* If we are in GlossaNet mode, we extract the url at the end of the line */
		if (option->result_mode==GLOSSANET_) {
			if (c!='\t') {
				error("ERROR in GlossaNet concordance: no URL found\n");
				href[0]='\0';
			} else {
				j=0;
				while ((c=u_fgetc(f))!='\n') {
					href[j++]=(unichar)c;
				}
				href[j]='\0';
			}
		}
		/*------------end GlossaNet-------------------*/
	}
	/* Now we will reorder the columns according to the sort mode */
	switch(option->sort_mode) {
		case TEXT_ORDER: left=A; middle=B; right=C; break;
		case LEFT_CENTER: left=A; middle=B; right=C; break;
		case LEFT_RIGHT: left=A; right=B; middle=C; break;
		case CENTER_LEFT: middle=A; left=B; right=C; break;
		case CENTER_RIGHT: middle=A; right=B; left=C; break;
		case RIGHT_LEFT: right=A; left=B; middle=C; break;
		case RIGHT_CENTER: right=A; middle=B; left=C; break;
	}
	/* We use 'can_print_line' to decide if the concordance line must be
	 * printed, because in GlossaNet mode, duplicates must be removed. */
	int can_print_line=1;
	if (option->result_mode==GLOSSANET_) {
		unichar line[4000];
      u_sprintf(line,"%S\t%S\t%S",left,middle,right);
		/* We test if the line was already seen */
		if (NO_VALUE_INDEX==get_value_index(line,glossa_hash,DONT_INSERT)) {
			can_print_line=1;
			get_value_index(line,glossa_hash);
		} else {
			can_print_line=0;
		}
	}
	/* If we can print the line */
	if (can_print_line) {
		if (option->sort_mode!=TEXT_ORDER) {
			/* If the concordance was sorted, the left sequence was reversed, and
			 * then, we have to reverse it again. However, the Thai sort algorithm
			 * requires to modify some vowels. That's why we must apply a special
			 * procedure if we have a Thai sorted concordance. */
			if (option->thai_mode) reverse_initial_vowels_thai(left);
			/* Now we revert and print the left context */
			if (option->result_mode==HTML_ || option->result_mode==GLOSSANET_) {
            u_fprintf(out,"<tr><td nowrap>%HR",left);
			} else {u_fprintf(out,"%R",left);}
		} else {
			/* If the concordance is not sorted, we do not need to revert the
			 * left context. */
			if (option->result_mode==HTML_ || option->result_mode==GLOSSANET_) {
            u_fprintf(out,"<tr><td nowrap>%HS",left);
			} else {u_fprintf(out,"%S",left);}
		}
		/* If we must produce an HTML concordance, then we surround the
		 * located sequence by HTML tags in order to make it an hyperlink.
		 * This hyperlink will contain a fake URL of the form "X Y Z", where
		 * X and Y are the starting and ending position of the sequence (in
		 * tokens) and Z is the number of the sentence that contains the
		 * sequence. */
		if (option->result_mode==HTML_) {
			u_fprintf(out,"<a href=\"%S\">%HS</a>%HS&nbsp;</td></tr>\n",indices,middle,right);
		}
		/* If we must produce a GlossaNet concordance, we turn the sequence
		 * into an URL, using the given GlossaNet script. */
		else if (option->result_mode==GLOSSANET_) {
			u_fprintf(out,"<A HREF=\"%s?rec=%HS&adr=%HS",option->glossanet_script,middle,href);
         u_fprintf(out,"\" style=\"color: rgb(0,0,128)\">%HS</A>%HS</td></tr>\n",middle,right);
		}
		/* If we must produce a text concordance */
		else if (option->result_mode==TEXT_) {
			u_fprintf(out,"\t%S\t%S\n",middle,right);
		}
      /* If must must produce an index file */
      else if (option->result_mode==INDEX_) {
         unichar idx[128];
         parse_string(indices,idx,P_SPACE);
         u_fprintf(out,"%S\t%S\n",idx,middle);
      }
      else if (option->result_mode==UIMA_) {
         char tmp1[100];
         u_to_char(tmp1,indices);
         int start,end;
         sscanf(tmp1,"%d %d",&start,&end);
         u_fprintf(out,"%d %d\t%S\n",start,end,middle);
      }
      /* If must must produce an axis file...
         VARIABLES :
         -----------
         - f1: position of the first character of a token
         - f2: position of the last character of a token
         - len : length of a token
            -> len = (f2+1) - f1
         - med : position of the median character of a token
            -> med = ((len+1)/2) + f1
      */
      else if (option->result_mode==AXIS_) {
         char tmp1[100];
         u_to_char(tmp1,indices);
         float f1,f2,len,med;
         sscanf(tmp1,"%f %f",&f1,&f2);
         len=(f2+1)-f1;
         med=((len+1)/2)+f1;
         u_fprintf(out,"%.1f\t%S\n",med,middle);
      }
	}
}
/* If we have an HTML or a GlossaNet concordance, we must write some
 * HTML closing tags. */
if (option->result_mode==HTML_ || option->result_mode==GLOSSANET_) write_HTML_end(out);
u_fclose(f);
remove(temp_file_name);
u_fclose(out);
if (option->result_mode==GLOSSANET_) {
	free_string_hash(glossa_hash);
}
}


/**
 * This function computes the length in unicode characters of
 * all the given tokens.
 */
void compute_token_length(int* token_length,struct text_tokens* tokens) {
int i;
for (i=0;i<tokens->N;i++) {
  token_length[i]=u_strlen(tokens->token[i]);
}
}


/**
 * This function writes the HTML header for an HTML or a GlossaNet concordance.
 */
void write_HTML_header(U_FILE* f,int number_of_matches,struct conc_opt* option) {
u_fprintf(f,"<html lang=en>\n");
u_fprintf(f,"<head>\n");
u_fprintf(f,"   <meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\">\n");
u_fprintf(f,"   <title>%d match%s</title>\n",number_of_matches,(number_of_matches>1)?"es":"");
u_fprintf(f,"</head>\n");
u_fprintf(f,"<body>\n<table border=\"0\" cellpadding=\"0\" width=\"100%%\" style=\"font-family: %s; font-size: %d\">\n",option->fontname,option->fontsize);
}


/**
 * This function write the HTML closing tags for an HTML or a GlossaNet concordance.
 */
void write_HTML_end(U_FILE* f) {
u_fprintf(f,"</table></body>\n");
u_fprintf(f,"</html>\n");
}


/**
 * This function moves in the file 'text' until the buffer 'buffer' contains the sequence
 * that begins at the global position 'start_pos', but with a large enough left context:
 *
 * buffer:
 * 0                              X                                buffer->MAXIMUM_BUFFER_SIZE
 * ------------------------------------------------------------------------------
 * |                              |                                             |
 * | <-- MAX_CONTEXT_IN_UNITS --> |                                             |
 * |                              |                                             |
 * ------------------------------------------------------------------------------
 *
 * buffer[X] will contain the token number 'start_pos'.
 *
 *
 * The function updates the '*n_units_already_read' parameter that counts the total
 * number of tokens that have been read since the beginning of the file 'text'. It also
 * updates the parameter '*current_origin_in_chars' that counts the length in chars of the
 * tokens that have been read since the beginning of the file 'text'. This counter is used
 * for computing the offset in characters for a given sequence. This information will
 * be used by the graphical interface to highlight a match in the text. 'token_length'
 * is an array that associates to each token its length in characters.
 * '*current_sentence' is used to count the number of '{S}' in order to know
 * what is the current sentence number.
 */
void move_buffer_to_position(int start_pos,U_FILE* text,struct text_tokens* tokens,int* token_length,
					struct buffer* buffer,int *n_units_already_read,
					int *current_origin_in_chars,int *current_sentence,
					int * position_from_eos,int RES) {
/* Before moving in the file, buffer[0] contains the token number '*n_units_already_read'.
 * After moving, we want it to contain the token number 'start_pos'-'MAX_CONTEXT_IN_UNITS',
 * so we move of ((start_pos-MAX_CONTEXT_IN_UNITS)-*n_units_already_read) int. */
int jump_size=((start_pos-MAX_CONTEXT_IN_UNITS)-(*n_units_already_read));
/* We go to '*n_units_already_read' * sizeof(int) */
fseek(text,(*n_units_already_read)*sizeof(int),SEEK_SET);
/* And we read ((start_pos-MAX_CONTEXT_IN_UNITS)-*n_units_already_read) integers */
while (jump_size!=0) {
	buffer->size=fread(buffer->int_buffer,sizeof(int),(jump_size>=buffer->MAXIMUM_BUFFER_SIZE)?buffer->MAXIMUM_BUFFER_SIZE:jump_size,text);
	jump_size=jump_size-buffer->size;
	for (int i=0;i<buffer->size;i++) {
		/* We update the current position in characters */
      int token_size=0;
      if (RES!=UIMA_ || buffer->int_buffer[i]!=tokens->SENTENCE_MARKER) {
         token_size=token_length[buffer->int_buffer[i]];
      }
      (*current_origin_in_chars)=(*current_origin_in_chars)+token_size;
		(*position_from_eos)=(*position_from_eos)+token_size;
		/* And the current sentence number */
		if (buffer->int_buffer[i]==tokens->SENTENCE_MARKER) {
			(*current_sentence)++;
			(*position_from_eos) = 0;
		}
	}
}
/* Finally, we update '*n_units_already_read' */
(*n_units_already_read)=start_pos-MAX_CONTEXT_IN_UNITS;
/* And we fill the buffer with the integers that we need */
buffer->size=fread(buffer->int_buffer,sizeof(int),buffer->MAXIMUM_BUFFER_SIZE,text);
}


/**
 * This function fills the string 'left' with the string of length
 * 'option.left_context' corresponding to the tokens located before the token number
 * 'pos'. 'token_length' is an array that gives the lengthes of the tokens.
 * 'buffer' contains the token numbers to work on. 'option.thai_mode' indicates by a non
 * zero value that we deal with a Thai sequence; in that case, we must do a special
 * operation in order to count correctly displayable characters.
 *
 * Note that extra spaces will be used to fill 'left' if there no left context enough,
 * in order to preserve alignment at display time.
 */
void extract_left_context(int pos,unichar* left,struct text_tokens* tokens,
                          struct conc_opt* option,int* token_length,
                          struct buffer* buffer) {
int i;
/* If there is no left context at all, we fill 'left' with spaces. */
if (pos==0) {
	for (i=0;i<option->left_context;i++) {
		left[i]=' ';
	}
	left[i]='\0';
	return;
}
i=0;
int count=0;
left[option->left_context]='\0';
/* We must start on the left of the match */
pos--;
int l=token_length[buffer->int_buffer[pos]]-1;
unichar* s=tokens->token[buffer->int_buffer[pos]];
/* We look for every token, until we have the correct number of displayable
 * characters. */
while (pos>=0 && count<option->left_context) {
	left[i]=s[l--];
	if (!option->thai_mode || !is_Thai_skipable(left[i])) {
		/* We increase the character count only we don't have a diacritic mark */
		count++;
	}
	i++;
	if (l<0) {
		/* If we must change of token */
		if (option->left_context_until_eos
                    && !u_strcmp(tokens->token[buffer->int_buffer[pos]],"{S}"))
                  break; /* token was "{S}" */
		pos--;
		if (pos>=0) {
			/* And if we can, i.e. we are not at the beginning of the text */
			l=token_length[buffer->int_buffer[pos]]-1;
			s=tokens->token[buffer->int_buffer[pos]];
		}
	}
}
/* If it was not possible to get to correct number of characters because
 * the sequence was too close to the beginning of the text, we fill
 * 'left' with spaces. */
if (count!=option->left_context) {
	while (count++!=option->left_context) {
		left[i++]=' ';
	}
}
left[i]='\0';
/* Finally, we reverse the string because we want the left context and not its mirror.
 * Note that we cannot fill the buffer from the end because of Thai diacritics that
 * can make the length of left in characters greater than 'LEFT_CONTEXT_LENGTH'. */
mirror(left);
}


/**
 * This function fills 'middle' with the matched sequence represented by the token
 * range [start_pos,end_pos]. 'output' is the output sequence that was computed
 * during the locate operation. If not NULL, we ignore the original text and copy
 * this value to 'middle'; otherwise we concatenate the tokens that compose
 * the matched sequence. Note that we allways take the whole sequence, so that
 * 'middle' must have been carefully allocated. 'buffer' contains the token numbers
 * to work on.
 */
void extract_match(int start_pos,int end_pos,unichar* output,unichar* middle,
					struct text_tokens* tokens,struct buffer* buffer) {
int j,k;
unichar* s;
if (output==NULL) {
	/* If there is no output, we compute the match from the text */
	j=0;
	for (int i=start_pos;i<=end_pos;i++) {
		k=0;
		s=tokens->token[buffer->int_buffer[i]];
		while (s[k]!='\0') {
			middle[j++]=s[k++];
		}
	}
	middle[j]='\0';
} else {
	/* If there is an output, then the match is the output */
	u_strcpy(middle,output);
}
}


/**
 * This function fills the string 'right' with the string of length
 * 'option.right_context'-'match_length' corresponding to the tokens located
 * after the token number 'pos'. Note that the 'right' may be empty if the match
 * was already greater or equal to 'option.right_context'.
 *
 * 'token_length' is an array that gives the lengthes of the tokens.
 * 'buffer' contains the token numbers to work on. 'option.thai_mode' indicates by a non
 * zero value that we deal with a Thai sequence; in that case, we must do a special
 * operation in order to count correctly displayable characters.
 */
void extract_right_context(int pos,unichar* right,struct text_tokens* tokens,
                           int match_length,struct conc_opt* option,
                           struct buffer* buffer) {
right[0]='\0';
if (match_length>=option->right_context || pos+1>=buffer->size) {
   /* We return if we have already overpassed the right context length
    * with the matched sequence, or if there is no right context because
    * we are at the end of the text. */
    return;
}
int right_context_length=option->right_context-match_length;
int i=0;
int count=0;
/* We must start after the last token of the matched sequence */
pos++;
int l=0;
unichar* s=tokens->token[buffer->int_buffer[pos]];
while (pos<buffer->size && count<right_context_length) {
	right[i]=s[l++];
	if (!option->thai_mode || !is_Thai_skipable(right[i])) count++;
	i++;
	if (s[l]=='\0') {
		/* If we must change of token */
		if (option->right_context_until_eos
                    && !u_strcmp(tokens->token[buffer->int_buffer[pos]],"{S}"))
                  break; /* token was "{S}" */
		pos++;
		if (pos<buffer->size) {
			/* And if we can */
			l=0;
			s=tokens->token[buffer->int_buffer[pos]];
		}
	}
}
/* We don't fill 'right' with spaces if we have reached the end of the text, because
 * there is no alignment problem on the right side of concordance. */
right[i]='\0';
}


/**
 * A GlossaNet concordance is supposed to have been computed on a text of the
 * following form:
 *
 * document1
 * [[url1]]
 * document2
 * [[url2]]
 * ...
 *
 * This function tries to find the URL between [[ and ]] that follows the matched sequence
 * in the text (i.e. after the token number 'end_pos'). If there is one, it is copied in
 * 'href'; if not, 'href' is empty. 1 is returned except if the function finds out that
 * the matched sequence is a part of an URL. In that case, 0 is returned in order to
 * indicate that this is not a real matched sequence.
 *
 * The function assumes that 'buffer' is large enough to find the URL. It may
 * not work anymore if the buffer size is too small.
 */
int extract_href(int end_pos,unichar* href,struct text_tokens* tokens,struct buffer* buffer,
				int open_bracket,int close_bracket) {
href[0]='\0';
if (open_bracket==-1 || close_bracket==-1) {
	/* If there are no both open and close square brackets, there
	 * is no chance to find any URL. */
	return 1;
}
int i=end_pos+1;
int op=0;
int cl=0;
/* First, we look for [[ or ]] */
while (i<buffer->size && op!=2 && cl!=2) {
	if (buffer->int_buffer[i]==open_bracket) {op++;cl=0;}
	else if (buffer->int_buffer[i]==close_bracket) {cl++;op=0;}
	else {op=0;cl=0;}
	i++;
}
if (cl==2) {
	/* If we have found ]], it means that the matched sequence is part of
	 * an URL. */
	return 0;
}
if (op!=2) {
	/* If we have reached the end of the buffer without finding [[ */
	return 1;
}
/* We concatenate all the tokens we find before ]] */
while (i+1<buffer->size && (buffer->int_buffer[i]!=close_bracket || buffer->int_buffer[i+1]!=close_bracket)) {
	u_strcat(href,tokens->token[buffer->int_buffer[i]]);
	i++;
}
if (buffer->int_buffer[i]!=close_bracket || buffer->int_buffer[i+1]!=close_bracket) {
	/* If we don't find ]], we empty href */
	href[0]='\0';
}
return 1;
}


/**
 * This function takes a string 's' that is the mirror of a Thai left context.
 * For sorting reasons, we must invert s[i] and s[i+1] when s[i] is
 * an initial vowel, because the Thai sort algorithm would behave strangely
 * when applied on raw reversed text. For more information (written in French),
 * see chapter 3.1 in:
 *
 * Paumier S�bastien, 2003. De la reconnaissance de formes linguistiques �
 * l'analyse syntaxique, Ph.D., Universit� Paris-Est Marne-la-Vall�e. Downloadable
 * at: http://igm.univ-mlv.fr/LabInfo/theses/
 *
 * You can also consult (in French too):
 *
 * Kosawat Krit, 2003. M�thodes de segmentation et d'analyse automatique de
 * textes tha�, Ph.D., Universit� Paris-Est Marne-la-Vall�e. Downloadable
 * at: http://igm.univ-mlv.fr/LabInfo/theses/
 */
void reverse_initial_vowels_thai(unichar* s) {
int i=0;
unichar c;
while (s[i]!='\0') {
	if (is_Thai_initial_vowel(s[i]) && s[i+1]!='\0') {
		c=s[i+1];
		s[i+1]=s[i];
		s[i]=c;
		i++;
	}
	i++;
}
}


/**
 * This function reads a concordance index from the file 'concordance' and produces a
 * text file that is stored in 'output'. This file contains the lines of the concordance,
 * but the columns may have been moved according to the sort mode, and the left
 * context is reversed. For instance, if we have a concordance line like:
 *
 * ABC DEF GHI
 *
 * where ABC is the left context, DEF is the matched sequence and GHI is the right
 * context. If the sort mode is "CENTER_LEFT", the output will contain the following
 * line:
 *
 * DEF^CBA^GHI
 *
 * where ^ stands for the tabulation character. If there are extra information like
 * positions (HTML concordance) or URL (GlossaNet concordance), they are stored at the end
 * of the line:
 *
 * DEF^CBA^GHI^120 124 5
 *
 *
 * 'text' is the "text.cod" file. 'tokens' contains the text tokens.
 * 'option.left_context' and 'option.right_context' specify the lengthes of the
 * contexts to extract. 'expected_result' is used to know if the output is
 * a GlossaNet concordance. 'n_enter_char' is the number of new lines in the text,
 * and 'enter_pos' is an array that contains the positions of these new lines.
 * If 'option.thai_mode' is set to a non zero value, it indicates that the concordance
 * is a Thai one. This information is used to compute correctly the context sizes.
 *
 * The function returns the number of matches actually written to the output file.
 *
 * For the xalign mode we produce a concord file with the following information :
 *
 *    - Column 1: sentence number
 *    - Column 2: shift from the beginning of the sentence to the left side of the match
 *    - Column 3: shift from the beginning of the sentence to the right side of the match
 */
int create_raw_text_concordance(U_FILE* output,U_FILE* concordance,U_FILE* text,struct text_tokens* tokens,
                                int expected_result,
                                int n_enter_char,int* enter_pos,
                                int* token_length,int open_bracket,int close_bracket,
                                struct conc_opt* option) {
struct match_list* matches;
struct match_list* matches_tmp;
unichar left[MAX_CONTEXT_IN_UNITS+1];
unichar middle[MAX_CONTEXT_IN_UNITS+1];
unichar right[MAX_CONTEXT_IN_UNITS+1];
unichar href[MAX_CONTEXT_IN_UNITS+1];
int number_of_matches=0;
int is_a_good_match=1;
int start_pos,end_pos;
int n_units_already_read=0;
int current_origin_in_chars=0;
/* First, we allocate a buffer to read the "text.cod" file */
struct buffer* buffer=new_buffer(1000000,INTEGER_BUFFER);
u_printf("Loading concordance index...\n");
/* Then we load the concordance index. NULL means that the kind of output
 * doesn't matter. */
matches=load_match_list(concordance,NULL);
/* Then we fill the buffer with the beginning of the text */
buffer->size=fread(buffer->int_buffer,sizeof(int),buffer->MAXIMUM_BUFFER_SIZE,text);
int start_pos_char;
int end_pos_char;
int current_sentence=1;
int position_in_chars=0;
int position_in_tokens=0;

/* Information needed by the 'xalign' mode
 * - position_from_eos : current position from the beginning of the sentence
 * - start_from_eos: position of the first character from the beginning of the sentence
 * - end_from_eos: position of the last character from the beginning of the sentence */
int position_from_eos=0;
int start_from_eos=0;
int end_from_eos=0;

/* Now we can proceed all the matches, assuming that they are sorted by starting
 * position */
u_printf("Constructing concordance...\n");
while (matches!=NULL) {
	if (buffer->size==buffer->MAXIMUM_BUFFER_SIZE
		&& ((matches->start-n_units_already_read)+MAX_CONTEXT_IN_UNITS)>buffer->size) {
		/* If we must change of block... */
      move_buffer_to_position(matches->start,text,tokens,token_length,buffer,&n_units_already_read,
						&current_origin_in_chars,&current_sentence,
						&position_from_eos,expected_result);
		/* We update the position in characters (from the beginning of the text
		 * and from the beginning of the sentence) so that we know how
		 * many characters there are before buffer[0]. We update
		 * the sentence number in the same way. */
		position_in_chars=current_origin_in_chars;
		position_in_tokens=0;
	}
	/* Here, we are sure that the buffer contains all the tokens we need.
	 * We adjust 'start_pos' and 'end_pos' so that the tokens that compose
	 * the current match are between buffer[start_pos] and buffer[end_pos]. */
	start_pos=matches->start-n_units_already_read;
	end_pos=matches->end-n_units_already_read;
	start_pos_char=position_in_chars;
	/* We update the position in characters so that we know how
	 * many characters there are before buffer[start_pos]. We update
	 * the sentence number in the same way. */
	if (position_in_tokens>start_pos) {
	   /* If we have to go backward, in the case a Locate made in "All matches mode" */
	   for (int z=position_in_tokens-1; z>=start_pos; z--) {
         int token_size=0;
         if (expected_result!=UIMA_ || buffer->int_buffer[z]!=tokens->SENTENCE_MARKER) {
            token_size=token_length[buffer->int_buffer[z]];
         }
         start_pos_char=start_pos_char-token_size;
         position_from_eos=position_from_eos-token_size;
         start_from_eos=position_from_eos;
         if (buffer->int_buffer[z]==tokens->SENTENCE_MARKER) {
            current_sentence--;
            error("Bug: concordances that contain a sentence marker {S} cannot be used in an unsorted concord.ind file\n");
            position_from_eos = 0;
            start_from_eos = 0;
         }
      }
	   position_in_tokens=start_pos;
	}
	else {
	   /* If we have to go forward */
      for (int z=position_in_tokens; z<start_pos; z++) {
         int token_size=0;
         if (expected_result!=UIMA_ || buffer->int_buffer[z]!=tokens->SENTENCE_MARKER) {
            token_size=token_length[buffer->int_buffer[z]];
         }
         start_pos_char=start_pos_char+token_size;
         position_from_eos=position_from_eos+token_size;
         start_from_eos=position_from_eos;
         if (buffer->int_buffer[z]==tokens->SENTENCE_MARKER) {
            current_sentence++;
            position_from_eos = 0;
            start_from_eos = 0;
         }
      }
	}
	position_in_chars=start_pos_char;
	position_in_tokens=start_pos;
	end_pos_char=start_pos_char;
   end_from_eos=start_from_eos;
	/* We update 'end_pos_char' in the same way */
	for (int z=start_pos;z<=end_pos;z++) {
		int token_size=0;
      if (expected_result!=UIMA_ || buffer->int_buffer[z]!=tokens->SENTENCE_MARKER) {
         token_size=token_length[buffer->int_buffer[z]];
      }
      end_pos_char=end_pos_char+token_size;
      end_from_eos=end_from_eos+token_size;
	}
	/* Now we extract the 3 parts of the concordance */
	extract_left_context(start_pos,left,tokens,option,token_length,buffer);
	extract_match(start_pos,end_pos,matches->output,middle,tokens,buffer);
	/* To compute the 3rd part (right context), we need to know the length of
	 * the matched sequence in displayable characters. */
	int match_length_in_displayable_chars;
	if (option->thai_mode) {match_length_in_displayable_chars=u_strlen_Thai(middle);}
	else {match_length_in_displayable_chars=u_strlen(middle);}
	/* Then we can compute the right context */
	extract_right_context(end_pos,right,tokens,match_length_in_displayable_chars,
                              option,buffer);
	/* If we must produce a GlossaNet concordance, we look for a URL. After the
	 * function call, 'is_a_good_match' can be set to 0 if the match
	 * was a part of a URL instead of a valid match. */
	if (expected_result==GLOSSANET_) {
		is_a_good_match=extract_href(end_pos,href,tokens,buffer,open_bracket,close_bracket);
	}
	/* We compute the shift due to the new lines that count for 2 characters */
	unichar positions[100];
	unichar positions_from_eos[100];
	/* And we use it to compute the bounds of the matched sequence in characters
	 * from the beginning of the text file. */
	int shift=get_shift(n_enter_char,enter_pos,matches->start);
	start_pos_char=start_pos_char+shift;
	/* The shift value can be different at the end of the match since new lines
	 * can occur inside a match. */
	shift=get_shift(n_enter_char,enter_pos,matches->end);
	end_pos_char=end_pos_char+shift;
	/* Finally, we copy the sequence bounds and the sentence number into 'positions'. */
	u_sprintf(positions,"\t%d %d %d",start_pos_char,end_pos_char,current_sentence);
	u_sprintf(positions_from_eos,"%d\t%d\t%d",current_sentence,start_from_eos,end_from_eos);
	//u_printf("MATCH:%S\t%S\n",positions_from_eos,middle);
	/* Now we save the concordance line to the output file, but only if
	 * it's a valid match. */
	if (is_a_good_match) {
		if (option->sort_mode!=TEXT_ORDER) {
			/* If we must reverse the left context in thai mode,
			 * we must reverse initial vowels with their following consonants. */
			if (option->thai_mode) {
				reverse_initial_vowels_thai(left);
			}
		}
		/* We save the 3 parts of the concordance line according to the sort mode */
		switch(option->sort_mode) {
			case TEXT_ORDER:
			if(expected_result==XALIGN_) u_fprintf(output,"%S\t%S",positions_from_eos,middle);
				else u_fprintf(output,"%S\t%S\t%S",left,middle,right);
				break;
			case LEFT_CENTER:  u_fprintf(output,"%R\t%S\t%S",left,middle,right); break;
			case LEFT_RIGHT:   u_fprintf(output,"%R\t%S\t%S",left,right,middle); break;
			case CENTER_LEFT:  u_fprintf(output,"%S\t%R\t%S",middle,left,right); break;
			case CENTER_RIGHT: u_fprintf(output,"%S\t%S\t%R",middle,right,left);	break;
			case RIGHT_LEFT:   u_fprintf(output,"%S\t%R\t%S",right,left,middle); break;
			case RIGHT_CENTER: u_fprintf(output,"%S\t%S\t%R",right,middle,left);	break;
		}
		/* And we add the position information */
		if(expected_result!=XALIGN_) u_fprintf(output,"%S",positions);
		/* And the GlossaNet URL if needed */
		if (expected_result==GLOSSANET_) {
			u_fprintf(output,"\t%S",href);
		}

		if(expected_result==XALIGN_) u_fprintf(output,"\n");
		else u_fprintf(output,"\n");
		/* We increase the number of matches actually written to the output */
		number_of_matches++;
	}
	/* Finally, we go on the next match */
	matches_tmp=matches;
	matches=matches->next;
	free_match_list_element(matches_tmp);
}
free_buffer(buffer);
return number_of_matches;
}


/**
 * This function prints the token 'buffer[offset_in_buffer]' to the output.
 * If the token is a space or a line break, which are the same in 'tokens',
 * the 'enter_pos' array is used to decide, whether a space or a line break
 * has to be printed. 'n_enter_char' is the length of the 'enter_pos' array.
 * 'pos_in_enter_pos' is the current position in this array. The function
 * returns the updated current position in the 'pos_in_enter_pos' array.
 */
int fprint_token(U_FILE* output,struct text_tokens* tokens,long int offset_in_buffer,
				int current_global_position,int n_enter_char,int* enter_pos,
				int pos_in_enter_pos,struct buffer* buffer) {
/* We look for the new line that is closer (but after) to the token to print */
while (pos_in_enter_pos < n_enter_char) {
	if ((current_global_position+offset_in_buffer) < enter_pos[pos_in_enter_pos]) {
		/* We have found the new line that follows the token to print, so
		 * we can stop. */
		break;
	}
	else if ((current_global_position+offset_in_buffer) > enter_pos[pos_in_enter_pos]) {
		/* The current new line is still before the token to print, so we go on */
		pos_in_enter_pos++;
		continue;
	}
	else if ((current_global_position+offset_in_buffer) == enter_pos[pos_in_enter_pos]) {
		/* The token to print is a new line, so we print it and return */
		pos_in_enter_pos++;
		u_fputc((unichar)'\n',output);
		return pos_in_enter_pos;
	}
}
/* The token to print is not a new line, so we print it and return */
u_fprintf(output,"%S",tokens->token[buffer->int_buffer[offset_in_buffer]]);
return pos_in_enter_pos;
}


/**
 * This function saves the text from the token n� 'current_global_position'
 * to the token n� 'match_start'. The text is printed in the file 'output'.
 * The function returns the updated current position in the 'pos_in_enter_pos'
 * array.
 */
int move_in_text_with_writing(int match_start,U_FILE* text,struct text_tokens* tokens,
								int current_global_position,U_FILE* output,
								int n_enter_char,int* enter_pos,int pos_in_enter_pos,
								struct buffer* buffer) {
long int address=current_global_position*sizeof(int);
fseek(text,address,SEEK_SET);
while ((match_start-current_global_position) > buffer->MAXIMUM_BUFFER_SIZE) {
	/* If the distance between current position and final position is larger than
	 * the buffer size, then we read a full buffer. */
	fread(buffer->int_buffer,sizeof(int),buffer->MAXIMUM_BUFFER_SIZE,text);
	for (address=0;address<buffer->MAXIMUM_BUFFER_SIZE;address++) {
		pos_in_enter_pos=fprint_token(output,tokens,address,current_global_position,
										n_enter_char,enter_pos,pos_in_enter_pos,
										buffer);
	}
	current_global_position=current_global_position+buffer->MAXIMUM_BUFFER_SIZE;
}
buffer->size=fread(buffer->int_buffer,sizeof(int),(match_start-current_global_position),text);
for (address=0;address<buffer->size;address++) {
	pos_in_enter_pos=fprint_token(output,tokens,address,current_global_position,
									n_enter_char,enter_pos,pos_in_enter_pos,
									buffer);
}
return pos_in_enter_pos;
}


/**
 * This function saves all the text from the token n� 'current_global_position' to
 * the end.
 */
int move_to_end_of_text_with_writing(U_FILE* text,struct text_tokens* tokens,
									int current_global_position,U_FILE* output,
									int n_enter_char,int* enter_pos,int pos_in_enter_pos,
									struct buffer* buffer) {
long int address=current_global_position*sizeof(int);
fseek(text,address,SEEK_SET);
while (0!=(buffer->size = fread(buffer->int_buffer,sizeof(int),buffer->MAXIMUM_BUFFER_SIZE,text))) {
	for (address=0;address<buffer->size;address++) {
		pos_in_enter_pos=fprint_token(output,tokens,address,current_global_position,
										n_enter_char,enter_pos,pos_in_enter_pos,buffer);
	}
   current_global_position = current_global_position+buffer->MAXIMUM_BUFFER_SIZE;
}
return pos_in_enter_pos;
}


/**
 * This function loads the "concord.ind" file 'concordance' and uses it
 * to produce a modified version of the original text. The output is saved
 * in a file named 'output_name'. The output is obtained by replacing
 * matched sequences by their associated outputs (note that if there is no
 * output, the matched sequence is deleted). In case of overlapping matches,
 * priority is given to left most one. If 2 matches start at the same position,
 * the longest is preferred. If 2 matches start and end at the same positions,
 * then the first one is arbitrarily preferred.
 */
void create_modified_text_file(U_FILE* concordance,U_FILE* text,
                               struct text_tokens* tokens,char* output_name,
                               int n_enter_char,int* enter_pos) {
U_FILE* output=u_fopen(UTF16_LE,output_name,U_WRITE);
if (output==NULL) {
	u_fclose(concordance);
	u_fclose(text);
	fatal_error("Cannot write file %s\n",output_name);
}
struct match_list* matches;
struct match_list* matches_tmp;
int current_global_position=0;
/* We allocate a buffer to read the tokens of the text */
struct buffer* buffer=new_buffer(1000000,INTEGER_BUFFER);
/* We load the match list */
matches=load_match_list(concordance,NULL);
int pos_in_enter_pos=0;
u_printf("Merging outputs with text...\n");
while (matches!=NULL) {
	while (matches!=NULL && matches->start<current_global_position) {
		/* If we must ignore this match because it is overlapping a previous
		 * match */
		matches_tmp=matches;
		matches=matches->next;
		free_match_list_element(matches_tmp);
	}
	if (matches!=NULL) {
		/* There, we are sure that we have a valid match to process */
		pos_in_enter_pos=move_in_text_with_writing(matches->start,text,tokens,
													current_global_position,output,
													n_enter_char,enter_pos,pos_in_enter_pos,
													buffer);
		/* Now, we are sure that the buffer contains all we want */
		if (matches->output!=NULL) {
			u_fprintf(output,"%S",matches->output);
		}
		current_global_position=matches->end+1;
		/* We skip to the next match of the list */
		matches_tmp=matches;
		matches=matches->next;
		free_match_list_element(matches_tmp);
	}
}
/* Finally, we don't forget to dump all the text that may remain after the
 * last match. */
move_to_end_of_text_with_writing(text,tokens,current_global_position,output,
								n_enter_char,enter_pos,pos_in_enter_pos,buffer);
free_buffer(buffer);
u_printf("Done.\n");
}



/**
 * Allocates, initializes and returns a struct conc_opt.
 */
struct conc_opt* new_conc_opt() {
struct conc_opt* opt=(struct conc_opt*)malloc(sizeof(struct conc_opt));
if (opt==NULL) {
   fatal_alloc_error("new_conc_opt");
}
opt->sort_mode=TEXT_ORDER;
opt->left_context=0;
opt->right_context=0;
opt->left_context_until_eos=0;
opt->right_context_until_eos=0;
opt->thai_mode=0;
opt->fontname=NULL;
opt->fontsize=0;
opt->result_mode=HTML_;
opt->output[0]='\0';
opt->glossanet_script=NULL;
opt->sort_alphabet=NULL;
opt->working_directory[0]='\0';
return opt;
}


/**
 * Frees all the memory associated with the given structure.
 */
void free_conc_opt(struct conc_opt* opt) {
if (opt==NULL) return;
if (opt->fontname!=NULL) free(opt->fontname);
if (opt->glossanet_script!=NULL) free(opt->glossanet_script);
if (opt->sort_alphabet!=NULL) free(opt->sort_alphabet);
free(opt);
}

