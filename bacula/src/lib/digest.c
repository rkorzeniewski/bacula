/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2005-2014 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from many
   others, a complete list can be found in the file AUTHORS.

   You may use this file and others of this release according to the
   license defined in the LICENSE file, which includes the Affero General
   Public License, v3.0 ("AGPLv3") and some additional permissions and
   terms pursuant to its AGPLv3 Section 7.

   Bacula® is a registered trademark of Kern Sibbald.
*/
/*
 * DIGEST is a class which implements SHA Digest handling in area of
 * storage, computation and digest transformation for both binary
 * and text forms.
 *
 * The main author of Bacula Deduplication code is Radosław Korzeniewski
 * 
 *  (c) Radosław Korzeniewski, MMXIV
 *  radoslaw@korzeniewski.net
 * 
 */

#include "digest.h"

/*
 * initialize memory
 */
void digest::init(DigestType t){
   memset (digest_bin, 0, DIGEST_MAX_LEN);
   memset (digest_text, 0, DIGEST_MAX_LEN * 2 + 1);
   type = t;
}

/*
 * constructor clears memory
 */
digest::digest (DigestType t){
   init(t);
}

/*
 * default constructor
 */
digest::digest (){
   init(DIGEST_256);
}

/*
 * constructor building digest
 */
digest::digest (DigestType t, const char * digt){
   init(t);
   operator= (digt);
}

/*
 * default constructor building digest
 */
digest::digest (const char * digt){
   init(DIGEST_256);
   operator= (digt);
}

/*
 * computes a digest of supplied buffer with length of len
 * in:
 *    buf - a memory buffer to compute
 *    len - a length of memory buffer
 * out:
 *    void
 */
void digest::compute (const unsigned char *buf, unsigned long len){

   if (buf){
      switch (type){
      case DIGEST_256:
         SHA256 (buf, len, digest_bin);
         break;
      case DIGEST_384:
         SHA384 (buf, len, digest_bin);
         break;
      case DIGEST_512:
      default:
         SHA512 (buf, len, digest_bin);
         break;
      }
      render_text();
   }
}

/*
 * returns a text digest storage area after rendering a text
 * 
 * in:
 *    void
 * out:
 *    pointer to text digest storage area
 */
char * digest::get_render_text (void){

   render_text();
   return digest_text;
}

/*
 * renders text digest based on binary one
 * 
 * in/out:
 *    void
 */
void digest::render_text (void){

   int i;
   int l = len();
   char c;

   for (i = 0; i < l; i++){
      c = (digest_bin[i] >> 4) & 0xF;
      if (c > 9) {
         c += 'A' - '9' - 1;
      }
      digest_text[i*2] = c + '0';
      c = digest_bin[i] & 0xF;
      if (c > 9) {
         c += 'A' - '9' - 1;
      }
      digest_text[i*2+1] = c + '0';
   }
}

/*
 * Operator: Compare-Equal
 *
 * in:
 *    this, dig - digests to compare
 * out:
 *    false - when not equal
 *    true - when equal
 */
bool digest::operator== (const digest &dig) const {

   /* different digests types are not equal */
   if (type != dig.type){
      return false;
   }
   int l = len();
   return memcmp (digest_bin, dig.digest_bin, l) == 0 ? true : false;
}

/*
 * Operator: Compare-NotEqual
 *
 * in:
 *    this, dig - digests to compare
 * out:
 *    false - when equal
 *    true - when not equal
 */
bool digest::operator!= (const digest &dig) const {

   /* different digests types are not equal */
   if (type != dig.type){
      return true;
   }
   int l = len();
   return memcmp (digest_bin, dig.digest_bin, l) == 0 ? false : true;
}

/*
 * copy a binary digest in class storage area
 *
 * in:
 *    dig - binary digest to copy to the class
 * out:
 *    void
 */
digest& digest::operator= (const digest &dig){

   int l = dig.len();
   type = dig.type;
   memcpy (digest_bin, (void*)dig.digest_bin, l);
   render_text();
   return *this;
}

/*
 * sets and transforms supplied text digest to binary and text ones
 * 
 * in:
 *    digt - SHA digest string in size of DIGEST_LEN
 * out:
 *    void
 *    when digt is NULL nothing happend
 */
digest& digest::operator= (const char *digt){

   int i;
   int l = len();
   char c, d;

   if (digt){
      bstrncpy (digest_text, digt, l*2);
      for (i = 0; i < l; i++){
         digest_text[i*2] = toupper(digt[i*2]);
         c = digest_text[i*2] - '0';
         if (c > 9) {
            c -= 'A' - '9' - 1;
         }
         digest_text[i*2+1] = toupper (digt[i*2+1]);
         d = digest_text[i*2+1] - '0';
         if (d > 9) {
            d -= 'A' - '9' - 1;
         }
         digest_bin[i] = (c << 4) + d;
      }
   }
   return *this;
}

/*
 * prints a text representation of shadigest stored in digest_text
 * require proper digest setting, computuation or rendering
 */
std::ostream & operator<< (std::ostream & out, digest & dig){
   return out << dig.get_digest_text();
}

#ifdef TEST_PROGRAM
using namespace std;
const char * T =  "12345678910";
const char * DT = "63640264849a87c90356129d99ea165e37aa5fabc1fea46906df1a7ca50db492";
const char * D =  "de2f256064a0af797747c2b97505dc0b9f3df0de4f489eac731c23ae9ca9cc31";

int main (void){

   digest dig;
   char * t;

   cout << "data: " << T << endl << "len:  " << strlen (T) << endl << endl;
   cout << "DIGEST exemplar: " << DT << endl;
   dig.compute ((unsigned char*)T, strlen(T));
   cout << "DIGEST computed: " << dig << endl << "digest.len: " << dig.len() << endl;

   digest dignew(DT);
   cout << "DIGEST created : " << dignew << endl;

   if (dig == dignew){
      cout << "DIGEST computed is the same as DIGEST created - good" << endl;
   } else {
      cout << "comparision failed" << endl;
   }
   cout << endl;

   cout << "DIGEST exemplar: " << D << endl;
   dig = D;
   cout << "DIGEST created : " << dig << endl;

   cout << "operator==\t";
   if (dig == dignew){
      cout << "comparision failed" << endl;
   } else {
      cout << "D is not the same as DN - good" << endl;
   }

   cout << "operator!=\t";
   if (dig != dignew){
      cout << "D is not the same as DN - good" << endl;
   } else {
      cout << "comparision failed" << endl;
   }

   t = dig;
   cout << "t = " << t << endl;

   return 0;
}

#endif /* TEST_PROGRAM */