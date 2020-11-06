#ifndef HIREDIS_HELPER_H
#define HIREDIS_HELPER_H

#include "bstr.h"
#include "barr.h"

int hiredis_init(void);
int hiredis_uninit(void);

int hiredis_sendcmd_intresp(bstr_t *, int *);

int hiredis_set(const char *, bstr_t *);
int hiredis_get(const char *, bstr_t *);
int hiredis_sadd(const char *, bstr_t *, int *);
int hiredis_sismember(const char *, bstr_t *, int *);
int hiredis_srandmember(const char *, int, barr_t *);

int hiredis_zadd(const char *, int, bstr_t *, int *);
int hiredis_zcount(const char *, bstr_t *, bstr_t *, int *);
int hiredis_zrange(const char *, int, int, int, barr_t *);
int hiredis_zrem(const char *, bstr_t *, int *);

int hiredis_blpop(const char *, int, bstr_t **);
int hiredis_rpush(const char *, const char *);
int hiredis_lpush(const char *, const char *);
int hiredis_lrange(const char *, int, int, barr_t *);
int hiredis_lrem(const char *, int, const char *, int *);

int hiredis_rename(const char *, const char *);
int hiredis_del(const char *, int *);

#endif
