#include "hiredis_helper.h"
#include "hiredis.h"
#include "blog.h"
#include <errno.h>
#include <stdarg.h>

#define REDIS_DEF_ADDR	"127.0.0.1"
#define REDIS_DEF_PORT	6379

static redisContext	*rctx = NULL;
static redisReply *_redisCommand(const char *, ...);

int
hiredis_init(void)
{
	char	*redis_addr;
	int	redis_port;
	char	*envstr;
	int	intval;

	redis_addr = REDIS_DEF_ADDR;
	redis_port = REDIS_DEF_PORT;

	envstr = getenv("REDIS_ADDR");
	if(!xstrempty(envstr))
		redis_addr = envstr;

	envstr = getenv("REDIS_PORT");
	if(!xstrempty(envstr)) {
		intval = atoi(envstr);
		if(intval > 0)
			redis_port = intval;
	}

	rctx = redisConnect(redis_addr, redis_port);
	if(rctx == NULL) {
		blogf("Could not create redis context\n");
		return ENOEXEC;
	}

	if(rctx->err != 0) {
		blogf("Could not connect to redis: %s\n", rctx->errstr);
		return ENOEXEC;
	}

	return 0;
}


int
hiredis_uninit(void)
{

	if(rctx != NULL) {
		redisFree(rctx);
		rctx = NULL;
	}

	return 0;
}

#define MAXTRY	2

static redisReply *
_redisCommand(const char *format, ...)
{
	va_list		arglist;
	redisReply	*res;
	int		trycnt;
	int		ret;

	if(rctx == NULL)
		return NULL;

	if(xstrempty(format))
		return NULL;


	trycnt = 0;
	while(1) {

		++trycnt;
		va_start(arglist, format);
		res = redisvCommand(rctx, format, arglist);
		va_end(arglist);

		if(res != NULL)
			break;

		if(trycnt >= MAXTRY)
			break;

		/* Reset the context (ie. connection) and we'll try again */
#if 0
		blogf("Resetting hiredis context");
#endif
		hiredis_uninit();
		ret = hiredis_init();
		if(ret != 0) {
			blogf("Couln't reset hiredis context");
			break;
		}
	}

	return res;
}


int
hiredis_set(const char *key, bstr_t *val)
{
	redisReply	*r;
	int		err;
	bstr_t		*cmd;

	if(rctx == NULL)
		return ENOEXEC;

	if(xstrempty(key) || bstrempty(val))
		return EINVAL;

	r = NULL;
	err = 0;

	cmd = binit();
	if(cmd == NULL) {
		err = ENOMEM;
		blogf("Couldn't initialize cmd");
		goto end_label;
	}

	bprintf(cmd, "SET %s %%b", key);

	r = _redisCommand(bget(cmd), bget(val), bstrlen(val));

	if(r == NULL) {
		blogf("Error while sending command to redis: NULL reply");
		err = ENOEXEC;
		goto end_label;
	} else
	if(r->type == REDIS_REPLY_ERROR) {
		if(!xstrempty(r->str)) {
			blogf("Error while sending command to redis: %s",
			    r->str);
		} else {
			blogf("Error while sending command to redis,"
			    " and no error string returned by redis!");
		}

		err = ENOEXEC;
		goto end_label;

	} else
	if(r->type == REDIS_REPLY_STATUS) {
		if(!xstrempty(r->str)) {
			if(!xstrbeginswith(r->str, "OK")) {
				blogf("Redis error on SET: %s", r->str);
				err = ENOEXEC;
				goto end_label;
			}
		} else {
			blogf("Error while sending SET to redis, and no"
			    " error string returned by redis!");
		}

	} else {
		blogf("Redis didn't respond with STATUS");
		err = ENOEXEC;
		goto end_label;
	}

end_label:

	if(cmd != NULL) {
		buninit(&cmd);
	}

	if(r != NULL) {
		freeReplyObject(r);
		r = NULL;
	}

	return err;
}


int
hiredis_get(const char *key, bstr_t *val)
{
	redisReply	*r;
	int		err;

	if(rctx == NULL)
		return ENOEXEC;

	if(xstrempty(key) || val == NULL)
		return EINVAL;

	r = NULL;
	err = 0;

	r = _redisCommand("GET %s", key);

	if(r == NULL) {
		blogf("Error while sending command to redis: NULL reply");
		err = ENOEXEC;
		goto end_label;
	} else
	if(r->type == REDIS_REPLY_ERROR) {
		if(!xstrempty(r->str)) {
			blogf("Error while sending command to redis: %s",
			    r->str);
		} else {
			blogf("Error while sending command to redis,"
			    " and no error string returned by redis!");
		}

		err = ENOEXEC;
		goto end_label;

	} else
	if(r->type == REDIS_REPLY_NIL) {
		/* Key not found */
		err = ENOENT;
	} else
	if(r->type == REDIS_REPLY_STRING) {
		if(r->str != NULL) {
			bclear(val);
			bstrcat(val, r->str);
		}

	} else {
		blogf("Redis didn't respond with STRING");
		err = ENOEXEC;
		goto end_label;
	}

end_label:

	if(r != NULL) {
		freeReplyObject(r);
		r = NULL;
	}

	return err;
}




int
hiredis_sadd(const char *key, bstr_t *memb, int *nadded)
{
	int		err;
	redisReply	*r;

	if(rctx == NULL)
		return ENOEXEC;

	if(xstrempty(key) || bstrempty(memb))
		return EINVAL;

	err = 0;
	r = NULL;

	r = _redisCommand("SADD %s %s", key, bget(memb));

	if(r == NULL) {
		blogf("Error while sending command to redis: NULL reply");
		err = ENOEXEC;
		goto end_label;
	} else
	if(r->type == REDIS_REPLY_ERROR) {
		if(!xstrempty(r->str)) {
			blogf("Error while sending command to redis: %s",
			    r->str);
		} else {
			blogf("Error while sending command to redis,"
			    " and no error string returned by redis!");
		}

		err = ENOEXEC;
		goto end_label;

	} else
	if(r->type == REDIS_REPLY_INTEGER) {
		if(nadded != NULL) {
			*nadded = r->integer;
		}
	} else {
		blogf("Redis didn't respond with integer");
		err = ENOEXEC;
		goto end_label;
	}

end_label:

	if(r != NULL) {
		freeReplyObject(r);
		r = NULL;
	}

	return err;
}


int
hiredis_sismember(const char *key, bstr_t *memb, int *ismemb)
{
	int		err;
	redisReply	*r;

	if(rctx == NULL)
		return ENOEXEC;

	if(xstrempty(key) || bstrempty(memb) || ismemb == NULL)
		return EINVAL;

	err = 0;
	r = NULL;

	r = _redisCommand("SISMEMBER %s %s", key, bget(memb));

	if(r == NULL) {
		blogf("Error while sending command to redis: NULL reply");
		err = ENOEXEC;
		goto end_label;
	} else
	if(r->type == REDIS_REPLY_ERROR) {
		if(!xstrempty(r->str)) {
			blogf("Error while sending command to redis: %s",
			    r->str);
		} else {
			blogf("Error while sending command to redis,"
			    " and no error string returned by redis!");
		}

		err = ENOEXEC;
		goto end_label;

	} else
	if(r->type == REDIS_REPLY_INTEGER) {
		*ismemb = r->integer;
	} else {
		blogf("Redis didn't respond with integer");
		err = ENOEXEC;
		goto end_label;
	}

end_label:

	if(r != NULL) {
		freeReplyObject(r);
		r = NULL;
	}

	return err;
}


int
hiredis_zadd(const char *key, int score, bstr_t *memb, int *nadded)
{
	int		err;
	redisReply	*r;

	if(rctx == NULL)
		return ENOEXEC;

	if(xstrempty(key) || bstrempty(memb))
		return EINVAL;

	err = 0;
	r = NULL;

	r = _redisCommand("ZADD %s %d %s", key, score, bget(memb));

	if(r == NULL) {
		blogf("Error while sending command to redis: NULL reply");
		err = ENOEXEC;
		goto end_label;
	} else
	if(r->type == REDIS_REPLY_ERROR) {
		if(!xstrempty(r->str)) {
			blogf("Error while sending command to redis: %s",
			    r->str);
		} else {
			blogf("Error while sending command to redis,"
			    " and no error string returned by redis!");
		}

		err = ENOEXEC;
		goto end_label;

	} else
	if(r->type == REDIS_REPLY_INTEGER) {
		if(nadded != NULL) {
			*nadded = r->integer;
		}
	} else {
		blogf("Redis didn't respond with integer");
		err = ENOEXEC;
		goto end_label;
	}

end_label:

	if(r != NULL) {
		freeReplyObject(r);
		r = NULL;
	}

	return err;
}


int
hiredis_zcount(const char *key, bstr_t *rmin, bstr_t *rmax, int *count)
{
	/* Instead of taking numgers for the range, we take bstr. This
	 * is so the caller can use "+/-inf", and "(number" notations. */
	int		err;
	redisReply	*r;

	if(rctx == NULL)
		return ENOEXEC;

	if(xstrempty(key) || bstrempty(rmin) || bstrempty(rmax) ||
	    count == NULL)
		return EINVAL;

	err = 0;
	r = NULL;

	r = _redisCommand("ZCOUNT %s %s %s", key, bget(rmin), bget(rmax));

	if(r == NULL) {
		blogf("Error while sending command to redis: NULL reply");
		err = ENOEXEC;
		goto end_label;
	} else
	if(r->type == REDIS_REPLY_ERROR) {
		if(!xstrempty(r->str)) {
			blogf("Error while sending command to redis: %s",
			    r->str);
		} else {
			blogf("Error while sending command to redis,"
			    " and no error string returned by redis!");
		}

		err = ENOEXEC;
		goto end_label;

	} else
	if(r->type == REDIS_REPLY_INTEGER) {
		*count = r->integer;
	} else {
		blogf("Redis didn't respond with integer");
		err = ENOEXEC;
		goto end_label;
	}

end_label:

	if(r != NULL) {
		freeReplyObject(r);
		r = NULL;
	}

	return err;
}

#define ZRANGE_FMT		"ZRANGE %s %d %d"
#define ZRANGE_WITHSCORES_FMT	"ZRANGE %s %d %d WITHSCORES"

int
hiredis_zrange(const char *key, int start, int stop, int withscores,
	barr_t *resp)
{
	/* resp should be a barr of (bstr_t *). Caller responsible for freeing
	 * the returned elements. */
	int		err;
	redisReply	*r;
	redisReply	*elem;
	bstr_t		*str;
	int		i;

	if(rctx == NULL)
		return ENOEXEC;

	if(xstrempty(key) ||  resp == NULL)
		return EINVAL;

	err = 0;
	r = NULL;

	r = _redisCommand(withscores==0?ZRANGE_FMT:ZRANGE_WITHSCORES_FMT,
	    key, start, stop);

	if(r == NULL) {
		blogf("Error while sending command to redis: NULL reply");
		err = ENOEXEC;
		goto end_label;
	} else
	if(r->type == REDIS_REPLY_ERROR) {
		if(!xstrempty(r->str)) {
			blogf("Error while sending command to redis: %s",
			    r->str);
		} else {
			blogf("Error while sending command to redis,"
			    " and no error string returned by redis!");
		}

		err = ENOEXEC;
		goto end_label;

	} else
	if(r->elements == 0) {
		err = ENOENT;
		goto end_label;
	} else
	 if(r->type == REDIS_REPLY_ARRAY && r->element != NULL) {

		for(i = 0; i < r->elements; ++i) {
			elem = r->element[i];
			if(elem->type != REDIS_REPLY_STRING) {
				blogf("Element is not string");
				continue;
			}
			if(xstrempty(elem->str)) {
				blogf("Element is invalid string");
				continue;
			}
			str = binit();
			if(str == NULL) {
				blogf("Couldn't allocate str");
				continue;
			}
			bstrcat(str, elem->str);
			barr_add(resp, (void *) str);
			free(str);
			str = NULL;
		}
	} else {
		blogf("Redis didn't respond with valid array");
		err = ENOEXEC;
		goto end_label;
	}

end_label:

	if(r != NULL) {
		freeReplyObject(r);
		r = NULL;
	}

	return err;
}


int
hiredis_zrem(const char *key, bstr_t *memb, int *nremoved)
{
	int		err;
	redisReply	*r;

	if(rctx == NULL)
		return ENOEXEC;

	if(xstrempty(key) || bstrempty(memb))
		return EINVAL;

	err = 0;
	r = NULL;

	r = _redisCommand("ZREM %s %s", key, bget(memb));

	if(r == NULL) {
		blogf("Error while sending command to redis: NULL reply");
		err = ENOEXEC;
		goto end_label;
	} else
	if(r->type == REDIS_REPLY_ERROR) {
		if(!xstrempty(r->str)) {
			blogf("Error while sending command to redis: %s",
			    r->str);
		} else {
			blogf("Error while sending command to redis,"
			    " and no error string returned by redis!");
		}

		err = ENOEXEC;
		goto end_label;

	} else
	if(r->type == REDIS_REPLY_INTEGER) {
		if(nremoved != NULL) {
			*nremoved = r->integer;
		}
	} else {
		blogf("Redis didn't respond with integer");
		err = ENOEXEC;
		goto end_label;
	}

end_label:

	if(r != NULL) {
		freeReplyObject(r);
		r = NULL;
	}

	return err;
}


int
hiredis_blpop(const char *key, int timeout, bstr_t **resp)
{
	int		err;
	redisReply	*r;
	redisReply	*elem;
	bstr_t		*str;

	if(rctx == NULL)
		return ENOEXEC;

	if(xstrempty(key) ||  resp == NULL)
		return EINVAL;

	err = 0;
	r = NULL;
	str = NULL;

	r = _redisCommand("BLPOP %s %d", key, timeout);

	if(r == NULL) {
		blogf("Error while sending command to redis: NULL reply");
		err = ENOEXEC;
		goto end_label;
	} else
	if(r->type == REDIS_REPLY_ERROR) {
		if(!xstrempty(r->str)) {
			blogf("Error while sending command to redis: %s",
			    r->str);
		} else {
			blogf("Error while sending command to redis,"
			    " and no error string returned by redis!");
		}

		err = ENOEXEC;
		goto end_label;

	} else
	if(timeout != 0 && r->type == REDIS_REPLY_NIL) {
		err = ETIMEDOUT;
		goto end_label;
	} else
	if(r->type != REDIS_REPLY_ARRAY || r->elements != 2) {
		blogf("Redis didn't respond with two-element array");
		err = ENOEXEC;
		goto end_label;
	} else {
		elem = r->element[1];
		if(elem->type != REDIS_REPLY_STRING) {
			blogf("Element is not string");
			err = EINVAL;
			goto end_label;
		}
		if(xstrempty(elem->str)) {
			blogf("Element is invalid string");
			err = ENOENT;
			goto end_label;
		}

		str = binit();
		if(str == NULL) {
			blogf("Couldn't allocate str");
			err = ENOMEM;
			goto end_label;
		}
		bstrcat(str, elem->str);
	}

end_label:

	if(r != NULL) {
		freeReplyObject(r);
		r = NULL;
	}

	if(err == 0)
		*resp = str;
	else
		buninit(&str);

	return err;
}



int
hiredis_lpush(const char *key, const char *elem)
{
	int		err;
	redisReply	*r;

	if(rctx == NULL)
		return ENOEXEC;

	if(xstrempty(key) || xstrempty(elem))
		return EINVAL;

	err = 0;
	r = NULL;

	r = _redisCommand("LPUSH %s %s", key, elem);

	if(r == NULL) {
		blogf("Error while sending command to redis: NULL reply");
		err = ENOEXEC;
		goto end_label;
	} else
	if(r->type == REDIS_REPLY_ERROR) {
		if(!xstrempty(r->str)) {
			blogf("Error while sending command to redis: %s",
			    r->str);
		} else {
			blogf("Error while sending command to redis,"
			    " and no error string returned by redis!");
		}

		err = ENOEXEC;
		goto end_label;

	} else
	if(r->type != REDIS_REPLY_INTEGER) {
		blogf("Redis didn't respond with integer");
		err = ENOEXEC;
		goto end_label;
	}

end_label:

	if(r != NULL) {
		freeReplyObject(r);
		r = NULL;
	}

	return err;
}


int
hiredis_rpush(const char *key, const char *elem)
{
	int		err;
	redisReply	*r;

	if(rctx == NULL)
		return ENOEXEC;

	if(xstrempty(key) || xstrempty(elem))
		return EINVAL;

	err = 0;
	r = NULL;

	r = _redisCommand("RPUSH %s %s", key, elem);

	if(r == NULL) {
		blogf("Error while sending command to redis: NULL reply");
		err = ENOEXEC;
		goto end_label;
	} else
	if(r->type == REDIS_REPLY_ERROR) {
		if(!xstrempty(r->str)) {
			blogf("Error while sending command to redis: %s",
			    r->str);
		} else {
			blogf("Error while sending command to redis,"
			    " and no error string returned by redis!");
		}

		err = ENOEXEC;
		goto end_label;

	} else
	if(r->type != REDIS_REPLY_INTEGER) {
		blogf("Redis didn't respond with integer");
		err = ENOEXEC;
		goto end_label;
	}

end_label:

	if(r != NULL) {
		freeReplyObject(r);
		r = NULL;
	}

	return err;
}


int
hiredis_lrange(const char *key, int start, int stop, barr_t *resp)
{
	/* resp should be a barr of (bstr_t). Caller responsible for freeing
	 * the returned elements. */
	int		err;
	redisReply	*r;
	redisReply	*elem;
	bstr_t		*str;
	int		i;

	if(rctx == NULL)
		return ENOEXEC;

	if(xstrempty(key) || resp == NULL)
		return EINVAL;

	err = 0;
	r = NULL;

	r = _redisCommand("LRANGE %s %d %d", key, start, stop);

	if(r == NULL) {
		blogf("Error while sending command to redis: NULL reply");
		err = ENOEXEC;
		goto end_label;
	} else
	if(r->type == REDIS_REPLY_ERROR) {
		if(!xstrempty(r->str)) {
			blogf("Error while sending command to redis: %s",
			    r->str);
		} else {
			blogf("Error while sending command to redis,"
			    " and no error string returned by redis!");
		}

		err = ENOEXEC;
		goto end_label;

	} else
	if(r->elements == 0) {
		goto end_label;
	} else
	 if(r->type == REDIS_REPLY_ARRAY && r->element != NULL) {

		for(i = 0; i < r->elements; ++i) {
			elem = r->element[i];
			if(elem->type != REDIS_REPLY_STRING) {
				blogf("Element is not string");
				continue;
			}
			if(xstrempty(elem->str)) {
				blogf("Element is invalid string");
				continue;
			}
			str = binit();
			if(str == NULL) {
				blogf("Couldn't allocate str");
				continue;
			}
			bstrcat(str, elem->str);
			barr_add(resp, (void *) str);
			free(str);
			str = NULL;
		}

	} else {
		blogf("Redis didn't respond with valid array");
		err = ENOEXEC;
		goto end_label;
	}

end_label:

	if(r != NULL) {
		freeReplyObject(r);
		r = NULL;
	}

	return err;
}


int
hiredis_lrem(const char *key, int count, const char *elem, int *ndel)
{
	int		err;
	redisReply	*r;

	if(xstrempty(key) || xstrempty(elem))
		return EINVAL;

	err = 0;
	r = NULL;

	r = _redisCommand("LREM %s %d %s", key, count, elem);

	if(r == NULL) {
		blogf("Error while sending command to redis: NULL reply");
		err = ENOEXEC;
		goto end_label;
	} else
	if(r->type == REDIS_REPLY_ERROR) {
		if(!xstrempty(r->str)) {
			blogf("Error while sending command to redis: %s",
			    r->str);
		} else {
			blogf("Error while sending command to redis,"
			    " and no error string returned by redis!");
		}

		err = ENOEXEC;
		goto end_label;

	} else
	if(r->type == REDIS_REPLY_INTEGER) {
		if(ndel != NULL) {
			*ndel = r->integer;
		}
	} else {
		blogf("Redis didn't respond with integer");
		err = ENOEXEC;
		goto end_label;
	}

end_label:
	if(r != NULL) {
		freeReplyObject(r);
		r = NULL;
	}

	return err;
	
}


int
hiredis_rename(const char *oldkey, const char *newkey)
{
	redisReply	*r;
	int		err;
	bstr_t		*cmd;

	if(rctx == NULL)
		return ENOEXEC;

	if(xstrempty(oldkey) || xstrempty(newkey))
		return EINVAL;

	r = NULL;
	err = 0;

	cmd = binit();
	if(cmd == NULL) {
		err = ENOMEM;
		blogf("Couldn't initialize cmd");
		goto end_label;
	}

	bprintf(cmd, "RENAME %s %s", oldkey, newkey);

	r = _redisCommand(bget(cmd));

	if(r == NULL) {
		blogf("Error while sending command to redis: NULL reply");
		err = ENOEXEC;
		goto end_label;
	} else
	if(r->type == REDIS_REPLY_ERROR) {
		if(!xstrempty(r->str)) {
			blogf("Error while sending command to redis: %s",
			    r->str);
		} else {
			blogf("Error while sending command to redis,"
			    " and no error string returned by redis!");
		}

		err = ENOEXEC;
		goto end_label;

	} else
	if(r->type == REDIS_REPLY_STATUS) {
		if(!xstrempty(r->str)) {
			if(!xstrbeginswith(r->str, "OK")) {
				blogf("Redis error on SET: %s", r->str);
				err = ENOEXEC;
				goto end_label;
			}
		} else {
			blogf("Error while sending SET to redis, and no"
			    " error string returned by redis!");
		}

	} else {
		blogf("Redis didn't respond with STATUS");
		err = ENOEXEC;
		goto end_label;
	}

end_label:

	if(cmd != NULL) {
		buninit(&cmd);
	}

	if(r != NULL) {
		freeReplyObject(r);
		r = NULL;
	}

	return err;
}


int
hiredis_srandmember(const char *key, int cnt, barr_t *resp)
{
	/* resp should be a barr of (bstr_t). Caller responsible for freeing
	 * the returned elements. */
	int		err;
	redisReply	*r;
	redisReply	*elem;
	bstr_t		*str;
	int		i;

	if(rctx == NULL)
		return ENOEXEC;

	if(xstrempty(key) || resp == NULL)
		return EINVAL;

	err = 0;
	r = NULL;

	r = _redisCommand("SRANDMEMBER %s %d", key, cnt);

	if(r == NULL) {
		blogf("Error while sending command to redis: NULL reply");
		err = ENOEXEC;
		goto end_label;
	} else
	if(r->type == REDIS_REPLY_ERROR) {
		if(!xstrempty(r->str)) {
			blogf("Error while sending command to redis: %s",
			    r->str);
		} else {
			blogf("Error while sending command to redis,"
			    " and no error string returned by redis!");
		}

		err = ENOEXEC;
		goto end_label;

	} else
	if(r->elements == 0) {
		goto end_label;
	} else
	 if(r->type == REDIS_REPLY_ARRAY && r->element != NULL) {

		for(i = 0; i < r->elements; ++i) {
			elem = r->element[i];
			if(elem->type != REDIS_REPLY_STRING) {
				blogf("Element is not string");
				continue;
			}
			if(xstrempty(elem->str)) {
				blogf("Element is invalid string");
				continue;
			}
			str = binit();
			if(str == NULL) {
				blogf("Couldn't allocate str");
				continue;
			}
			bstrcat(str, elem->str);
			barr_add(resp, (void *) str);
			free(str);
			str = NULL;
		}

	} else {
		blogf("Redis didn't respond with valid array");
		err = ENOEXEC;
		goto end_label;
	}

end_label:

	if(r != NULL) {
		freeReplyObject(r);
		r = NULL;
	}

	return err;
}


int
hiredis_del(const char *key, int *ndeleted)
{
	int		err;
	redisReply	*r;

	if(rctx == NULL)
		return ENOEXEC;

	if(xstrempty(key))
		return EINVAL;

	err = 0;
	r = NULL;

	r = _redisCommand("DEL %s", key);

	if(r == NULL) {
		blogf("Error while sending command to redis: NULL reply");
		err = ENOEXEC;
		goto end_label;
	} else
	if(r->type == REDIS_REPLY_ERROR) {
		if(!xstrempty(r->str)) {
			blogf("Error while sending command to redis: %s",
			    r->str);
		} else {
			blogf("Error while sending command to redis,"
			    " and no error string returned by redis!");
		}

		err = ENOEXEC;
		goto end_label;

	} else
	if(r->type == REDIS_REPLY_INTEGER) {
		if(ndeleted != NULL) {
			*ndeleted = r->integer;
		}
	} else {
		blogf("Redis didn't respond with integer");
		err = ENOEXEC;
		goto end_label;
	}

end_label:

	if(r != NULL) {
		freeReplyObject(r);
		r = NULL;
	}

	return err;
}


