/*
 * Copyright (c) 1997 Kungliga Tekniska H�gskolan
 * (Royal Institute of Technology, Stockholm, Sweden). 
 * All rights reserved. 
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met: 
 *
 * 1. Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright 
 *    notice, this list of conditions and the following disclaimer in the 
 *    documentation and/or other materials provided with the distribution. 
 *
 * 3. All advertising materials mentioning features or use of this software 
 *    must display the following acknowledgement: 
 *      This product includes software developed by Kungliga Tekniska 
 *      H�gskolan and its contributors. 
 *
 * 4. Neither the name of the Institute nor the names of its contributors 
 *    may be used to endorse or promote products derived from this software 
 *    without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND 
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE 
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS 
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY 
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE. 
 */

#include "hdb_locl.h"

RCSID("$Id$");

int
hdb_principal2key(krb5_context context, krb5_principal p, krb5_data *key)
{
    Principal new;
    size_t len;
    unsigned char *buf;
    int ret;

    ret = copy_Principal(p, &new);
    if(ret)
	goto out;
    new.name.name_type = 0;
    len = length_Principal(&new);
    buf  = malloc(len);
    if(buf == NULL){
	ret = ENOMEM;
	goto out;
    }
    ret = encode_Principal(buf + len - 1, len, &new, &len);
    if(ret){
	free(buf);
	goto out;
    }
    key->data = buf;
    key->length = len;
out:
    free_Principal(&new);
    return ret;
}

int
hdb_key2principal(krb5_context context, krb5_data *key, krb5_principal p)
{
    size_t len;
    return decode_Principal(key->data, key->length, p, &len);
}

int
hdb_entry2value(krb5_context context, hdb_entry *ent, krb5_data *value)
{
    unsigned char *buf;
    size_t len;
    int ret;
    len = length_hdb_entry(ent);
    buf = malloc(len);
    if(buf == NULL)
	return ENOMEM;
    ret = encode_hdb_entry(buf + len - 1, len, ent, &len);
    if(ret){
	free(buf);
	return ret;
    }
    value->data = buf;
    value->length = len;
    return 0;
}

int
hdb_value2entry(krb5_context context, krb5_data *value, hdb_entry *ent)
{
    size_t len;
    return decode_hdb_entry(value->data, value->length, ent, &len);
}

krb5_error_code
hdb_next_keytype2key(krb5_context context,
		     hdb_entry *e,
		     krb5_keytype keytype,
		     Key **key)
{
    krb5_error_code ret;
    int i;
    if(*key) 
	i = *key - e->keys.val + 1;
    else
	i = 0;
    for(; i < e->keys.len; i++)
	if(e->keys.val[i].key.keytype == keytype){
	    *key = &e->keys.val[i];
	    return 0;
	}
    return KRB5_PROG_ETYPE_NOSUPP; /* XXX */
}

krb5_error_code
hdb_keytype2key(krb5_context context, 
		hdb_entry *e, 
		krb5_keytype keytype, 
		Key **key)
{
    *key = NULL;
    return hdb_next_keytype2key(context,e, keytype, key);
}

krb5_error_code
hdb_next_etype2key(krb5_context context,
		   hdb_entry *e,
		   krb5_enctype etype,
		   Key **key)
{
    krb5_keytype keytype;
    krb5_error_code ret;
    ret = krb5_etype2keytype(context, etype, &keytype);
    if(ret)
	return ret;
    return hdb_next_keytype2key(context, e, keytype, key);
}

krb5_error_code
hdb_etype2key(krb5_context context, 
	      hdb_entry *e, 
	      krb5_enctype etype, 
	      Key **key)
{
    *key = NULL;
    return hdb_next_etype2key(context,e, etype, key);
}

krb5_error_code
hdb_lock(int fd, int operation)
{
    int i, code;
    for(i = 0; i < 3; i++){
	code = flock(fd, (operation == HDB_RLOCK ? LOCK_SH : LOCK_EX) | LOCK_NB);
	if(code == 0 || errno != EWOULDBLOCK)
	    break;
	sleep(1);
    }
    if(code == 0)
	return 0;
    if(errno == EWOULDBLOCK)
	return HDB_ERR_DB_INUSE;
    return HDB_ERR_CANT_LOCK_DB;
}

krb5_error_code
hdb_unlock(int fd)
{
    int code;
    code = flock(fd, LOCK_UN);
    if(code)
	return 4711 /* XXX */;
    return 0;
}

void
hdb_free_entry(krb5_context context, hdb_entry *ent)
{
    free_hdb_entry(ent);
}



krb5_error_code
hdb_open(krb5_context context, HDB **db, 
	 const char *filename, int flags, mode_t mode)
{
    if(filename == NULL)
	filename = HDB_DEFAULT_DB;
#ifdef HAVE_DB_H
    return hdb_db_open(context, db, filename, flags, mode);
#elif HAVE_NDBM_H
    return hdb_ndbm_open(context, db, filename, flags, mode);
#else
#error No suitable database library
#endif
}
