/*
 * Unitex
 *
 * Copyright (C) 2001-2011 Université Paris-Est Marne-la-Vallée <unitex@univ-mlv.fr>
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

#include "CompressedDic.h"
#include "Error.h"
#include "AbstractAllocator.h"
#include "File.h"
#include "Ustring.h"
#include "StringParsing.h"
#include "List_ustring.h"

static int read_bin_header(Dictionary*);

/**
 * Loads and returns a compressed dictionary.
 */
Dictionary* new_Dictionary(const char* bin,const char* inf,Abstract_allocator prv_alloc) {
Dictionary* d=(Dictionary*)malloc_cb(sizeof(Dictionary),prv_alloc);
if (d==NULL) {
	fatal_alloc_error("new_Dictionary");
}
d->bin=load_BIN_file(bin,&d->bin_size,prv_alloc);
if (d->bin==NULL) {
	free(d);
	return NULL;
}
if (!read_bin_header(d)) {
	free_BIN_file(d->bin,prv_alloc);
	free(d);
	return NULL;
}
if (d->type==BIN_CLASSIC) {
	if (inf==NULL) {
		error("NULL .inf file in new_Dictionary\n");
		free_BIN_file(d->bin,prv_alloc);
		free(d);
		return NULL;
	}
	d->inf=load_INF_file(inf,prv_alloc);
	if (d->inf==NULL) {
		free_BIN_file(d->bin,prv_alloc);
		free(d);
		return NULL;
	}
} else {
	fatal_error("new_Dictionary: unsupported dictionary format\n");
}
return d;
}


/**
 * Frees all resources associated to the given dictionary
 */
void free_Dictionary(Dictionary* d,Abstract_allocator prv_alloc) {
if (d==NULL) return;
if (d->bin!=NULL) free_BIN_file(d->bin,prv_alloc);
if (d->inf!=NULL) {
	free_INF_codes(d->inf,prv_alloc);
}
free(d);
}

#define INF_LINE_SIZE 4096

/**
 * This function takes a line of a .INF file and tokenize it into
 * several single codes.
 * Example: .N,.V  =>  code 0=".N" ; code 1=".V"
 */
struct list_ustring* tokenize_compressed_info(const unichar* line,Abstract_allocator prv_alloc) {
struct list_ustring* result=NULL;
unichar tmp[INF_LINE_SIZE];
int pos=0;
/* Note: all protected characters must stay protected */
while (P_EOS!=parse_string(line,&pos,tmp,P_COMMA,P_EMPTY,NULL)) {
   result=new_list_ustring(tmp,result,prv_alloc);
   if (line[pos]==',') pos++;
}
return result;
}


/**
 * This function loads the content of an .inf file and returns
 * a structure containing the lines of the file tokenized into INF
 * codes.
 */
struct INF_codes* load_INF_file(const char* name,Abstract_allocator prv_alloc) {
struct INF_codes* res;
U_FILE* f=u_fopen_existing_unitex_text_format(name,U_READ);
if (f==NULL) {
   error("Cannot open %s\n",name);
   return NULL;
}
res=(struct INF_codes*)malloc_cb(sizeof(struct INF_codes),prv_alloc);
if (res==NULL) {
   fatal_alloc_error("in load_INF_file");
}
if (1!=u_fscanf(f,"%d\n",&(res->N))) {
   fatal_error("Invalid INF file: %s\n",name);
}
res->codes=(struct list_ustring**)malloc_cb(sizeof(struct list_ustring*)*(res->N),prv_alloc);
if (res->codes==NULL) {
   fatal_alloc_error("in load_INF_file");
}
unichar *s=(unichar*)malloc_cb(sizeof(unichar)*INF_LINE_SIZE*10,prv_alloc);
if (s==NULL) {
   fatal_alloc_error("in load_INF_file");
}
int i=0;
/* For each line of the .inf file, we tokenize it to get the single INF codes
 * it contains. */
while (EOF!=u_fgets_limit2(s,INF_LINE_SIZE*10,f)) {
   res->codes[i++]=tokenize_compressed_info(s,prv_alloc);
}
free(s);
u_fclose(f);
return res;
}


/**
 * Frees all the memory allocated for the given structure.
 */
void free_INF_codes(struct INF_codes* INF,Abstract_allocator prv_alloc) {
if (INF==NULL) {return;}
for (int i=0;i<INF->N;i++) {
   free_list_ustring(INF->codes[i],prv_alloc);
}
free_cb(INF->codes,prv_alloc);
free_cb(INF,prv_alloc);
}


/**
 * Loads a .bin file into an unsigned char array that is returned.
 * Returns NULL if an error occurs.
 */
unsigned char* load_BIN_file(const char* name,long *file_size,Abstract_allocator prv_alloc) {
U_FILE* f;
/* We open the file as a binary one */
f=u_fopen(BINARY,name,U_READ);
unsigned char* tab;
if (f==NULL) {
   error("Cannot open %s\n",name);
   return NULL;
}
*file_size=get_file_size(name);
if (*file_size==0) {
   error("Error: empty file %s\n",name);
   u_fclose(f);
   return NULL;
}
tab=(unsigned char*)malloc_cb(sizeof(unsigned char)*(*file_size),prv_alloc);
if (tab==NULL) {
   fatal_alloc_error("load_BIN_file");
   return NULL;
}
if (*file_size!=(int)fread(tab,sizeof(char),*file_size,f)) {
   error("Error while reading %s\n",name);
   free_cb(tab,prv_alloc);
   u_fclose(f);
   return NULL;
}
u_fclose(f);
return tab;
}


/**
 * Frees all the memory allocated for the given structure.
 */
void free_BIN_file(unsigned char* BIN,Abstract_allocator prv_alloc) {
free_cb(BIN,prv_alloc);
}


/**
 * This function assumes that the .bin file has already been loaded.
 * It analyzes its header to determine the kind of .bin it is.
 * Returns 0 in case of error; 1 otherwise.
 */
static int read_bin_header(Dictionary* d) {
if (d->bin[0]==0) {
	/* Type 1: old style .bin/.inf dictionary */
	if (d->bin_size<6) {
		/* The minimal empty dictionary is made of the header plus a 2 bytes empty state */
		error("Invalid .bin size\n");
		return 0;
	}
	/* If we have an old style .bin */
	d->type=BIN_CLASSIC;
	d->header_size=4;
	d->char_encoding=BIN_2BYTES;
	d->offset_encoding=BIN_3BYTES;
	return 1;
}
if (d->bin[0]==1) {
	/* Type 2: modern style .bin/.inf dictionary with no limit on .bin size */
	if (d->bin_size<5) {
		/* The minimal empty dictionary is made of the header plus a 2 bytes empty state */
		error("Invalid .bin size\n");
		return 0;
	}
	d->type=BIN_CLASSIC;
	d->header_size=3;
	d->char_encoding=(BinEncoding)d->bin[1];
	d->offset_encoding=(BinEncoding)d->bin[2];
	return 1;
}
error("Unknown dictionary type: %d\n",d->bin[0]);
return 0;
}


/**
 * Reads the information associated to the current state in the dictionary, i.e.
 * finality and number of outgoing transitions. Returns the new position.
 * If the state is final and if it is a classic .bin, we store the inf code
 * in *code.
 */
int read_dictionary_state(Dictionary* d,int pos,int *final,int *n_transitions,int *code) {
if (d->type!=BIN_CLASSIC) {
	fatal_error("read_dictionary_state: unsupported dictionary type\n");
}
*final=!(d->bin[pos] & 128);
*n_transitions=((d->bin[pos] & 127)<<8)+d->bin[pos+1];
pos=pos+2;
if (*final) {
	*code=(d->bin[pos]<<16)+(d->bin[pos+1]<<8)+d->bin[pos+2];
	pos=pos+3;
} else {
	*code=-1;
}
return pos;
}


/**
 * Reads the information associated to the current transition in the dictionary.
 * Returns the new position.
 */
int read_dictionary_transition(Dictionary* d,int pos,unichar *c,int *dest) {
if (d->type!=BIN_CLASSIC) {
	fatal_error("read_dictionary_state: unsupported dictionary type\n");
}
if (d->char_encoding!=BIN_2BYTES) {
	fatal_error("read_dictionary_state: unsupported char encoding\n");
}
*c=(d->bin[pos]<<8)+d->bin[pos+1];
pos=pos+2;
if (d->offset_encoding!=BIN_3BYTES) {
	fatal_error("read_dictionary_state: unsupported offset encoding\n");
}
*dest=(d->bin[pos]<<16)+(d->bin[pos+1]<<8)+d->bin[pos+2];
pos=pos+3;
return pos;
}