 /*
  * Unitex
  *
  * Copyright (C) 2001-2008 Universit� Paris-Est Marne-la-Vall�e <unitex@univ-mlv.fr>
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

#include "StringParsing.h"


/**
 * Here we define some separator sets that will be used
 * many times.
 */
unichar* P_SPACE=u_strdup(" ");
unichar* P_COMMA=u_strdup(",");
unichar* P_DOT=u_strdup(".");
unichar* P_EQUAL=u_strdup("=");
unichar* P_PLUS=u_strdup("+");
unichar* P_COLON=u_strdup(":");
unichar* P_SLASH=u_strdup("/");
unichar* P_EXCLAMATION=u_strdup("!");
unichar* P_DOUBLE_QUOTE=u_strdup("\"");
unichar* P_PLUS_COLON=u_strdup("+:");
unichar* P_PLUS_MINUS_COLON=u_strdup("+-:");
unichar* P_PLUS_COLON_SLASH=u_strdup("+:/");
unichar* P_PLUS_COLON_SLASH_OPENING_BRACKET=u_strdup("+:/[");
unichar* P_PLUS_COLON_SLASH_EXCLAMATION_OPENING_BRACKET=u_strdup("+:/![");
unichar* P_COLON_CLOSING_BRACKET=u_strdup(":]");
unichar* P_COLON_SLASH=u_strdup(":/");
unichar* P_CLOSING_ROUND_BRACKET=u_strdup("}");
unichar* P_COMMA_DOT=u_strdup(",.");
unichar* P_PLUS_COLON_SLASH_BACKSLASH=u_strdup("+:/\\");
unichar* P_COLON_SLASH_BACKSLASH=u_strdup(":/\\");
unichar* P_COMMA_DOT_BACKSLASH_DIGITS=u_strdup(",.\\0123456789");
unichar* P_DOT_PLUS_SLASH_BACKSLASH=u_strdup(".+/\\");


/**
 * Parses the string 's' from the position '*ptr' until it finds '\0' 
 * or a character that is in 'stop_chars'.
 * 'chars_to_keep_protected' is used to define which chars must keep their protection 
 * char. If it has the value NULL, it means that all protected chars must stay protected.
 * Note that forbidden chars and stop_chars will be taken into account if and only
 * if they are not protected. The intersection between 'stop_chars' and 'forbidden_chars'
 * is supposed to be empty. If not, 'stop_chars' is considered first.
 * If an error occurs, a \0 is put at the end of the result and the function 
 * returns an error code; otherwise, the substring obtained
 * is stored in 'result' and P_OK is returned.
 * If 's' is empty, the function returns P_EOS.
 * Note that '*ptr' is updated and points to the stop character (delimiter or '\0').
 * It is the responsability of the caller to increase the position if we are not
 * at the end of the input string.
 * 
 * Example: parse("E\=\mc\2 is a formula",result," ","m","2") will produce the
 *          result = "E=mc\2"
 */
int parse_string(unichar* s,int *ptr,unichar* result,unichar* stop_chars,
                 unichar* forbidden_chars,unichar* chars_to_keep_protected) {
int j=0;
if (s[*ptr]=='\0') return P_EOS;
while (s[*ptr]!='\0') {
   if (s[*ptr]==PROTECTION_CHAR) {
      /* If there is a protection character (backslash) */
      if (s[(*ptr)+1]=='\0') {
         /* It must not appear at the end of the string */
         result[j]='\0';
         return P_BACKSLASH_AT_END;
      }
      if (chars_to_keep_protected==NULL || u_strchr(chars_to_keep_protected,s[(*ptr)+1])) {
         /* If the character must keep its backslash */
         result[j++]=PROTECTION_CHAR;
      }
      result[j++]=s[(*ptr)+1];
      (*ptr)=(*ptr)+2;
   } else {
      /* If we have an unprotected character */
      if (u_strchr(stop_chars,s[*ptr])) {
         /* If it is a stop char, we have finished */
         result[j]='\0';
         return P_OK;
      }
      if (u_strchr(forbidden_chars,s[*ptr])) {
         /* If it is a forbidden char, it's an error */
         result[j]='\0';
         return P_FORBIDDEN_CHAR;
      }
      /* If it's a normal char, we copy it */
      result[j++]=s[(*ptr)++];
   }
}
/* If we arrive here, we have reached the end of the string without error */
result[j]='\0';
return P_OK;
}


/**
 * Parses the string 's' from '*ptr' until it finds '\0' or a character that is in 
 * 'stop_chars'. '*ptr' is updated. All protected characters will be unprotected.
 * If an error occurs, it returns an error code; otherwise, the substring obtained
 * is stored in 'result' and P_OK is returned.
 */
int parse_string(unichar* s,int *ptr,unichar* result,unichar* stop_chars) {
return parse_string(s,ptr,result,stop_chars,P_EMPTY,P_EMPTY);
}


/**
 * Parses the string 's' from '*ptr' until it finds '\0' or a character that is in 
 * 'stop_chars'. '*ptr' is updated. All protected characters will be unprotected.
 * If an error occurs, it returns an error code; otherwise, the substring obtained
 * is stored in 'result' and P_OK is returned.
 */
int parse_string(unichar* s,int *ptr,unichar* result,char* stop_chars) {
unichar* tmp=u_strdup(stop_chars);
int value=parse_string(s,ptr,result,tmp,P_EMPTY,P_EMPTY);
free(tmp);
return value;
}


/**
 * This function do the same as above, except that it starts from the
 * beginning of the string. It can be used for one-shot tokenization.
 */
int parse_string(unichar* s,unichar* result,unichar* stop_chars) {
int ptr=0;
return parse_string(s,&ptr,result,stop_chars,P_EMPTY,P_EMPTY);
}


/**
 * This function do the same as above, except that it starts from the
 * beginning of the string. It can be used for one-shot tokenization.
 */
int parse_string(unichar* s,unichar* result,char* stop_chars) {
unichar* tmp=u_strdup(stop_chars);
int ptr=0;
int value=parse_string(s,&ptr,result,tmp,P_EMPTY,P_EMPTY);
free(tmp);
return value;
}


/**
 * This function copies 's' into 'result', escaping the chars in
 * 'chars_to_escape'. Protected chars are not taken into account.
 * 
 * Example: escape("E\=mc2",result,"2=") => result="E\=mc\2"
 * 
 * Note that the protection char will be escaped if and only if
 * it appears in 'chars_to_escape':
 * 
 * Example: escape("E\=mc2",result,"\") => result="E\\=mc2"
 * 
 * The function returns the length of 'result'.
 * 
 */
int escape(unichar* s,unichar* result,unichar* chars_to_escape) {
int j=0;
for (int i=0;s[i]!='\0';i++) {
   if (u_strchr(chars_to_escape,s[i])) {
      result[j++]=PROTECTION_CHAR;
   } else if (s[i]==PROTECTION_CHAR) {
      i++;
      result[j++]=PROTECTION_CHAR;
   }
   result[j++]=s[i];
}
result[j]='\0';
return j;
}

