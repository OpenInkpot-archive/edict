/*
 * Copyright © 2010 Alexander Kerner <lunohod@openinkpot.org>
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libintl.h>
#include <err.h>

#include <Eina.h>
#include <Ecore.h>
#include <Ecore_File.h>
#include <libpicodict.h>

#define SYS_DICT_DIR "/usr/share/dictd"
#define USER_DICT_DIR "/.dictd"
#define SD_DICT_DIR "/media/sd/.dictd"

#define ARTICLES_MAX 10

#define NO_RES gettext("No results")

#define H2_OPEN "<h2>"
#define H2_CLOSE "</h2>"
#define P_OPEN "<p>"
#define P_CLOSE "</p>"
#define BR "<br>"

#define ADD_TAG(s, t) string_append(s, t, strlen(t));

typedef struct dict_t dict_t;
struct dict_t {
    pd_dictionary *d;
    char *name;
    dict_t *next;
};

static char *
xasprintf(const char *fmt, ...)
{
    char *ret;
    va_list ap;
    va_start(ap, fmt);
    if (-1 == vasprintf(&ret, fmt, ap))
        err(255, "xasprintf");
    va_end(ap);
    return ret;
}

static dict_t *
load_dicts_from_dir(dict_t *dlist, const char *path)
{
    char *f;
    char *dict_file = (char *) malloc(1024);
    char *index_file = (char *) malloc(1024);

    Eina_List *l, *ls;

    ls = ecore_file_ls(path);
    EINA_LIST_FOREACH(ls, l, f) {
        char *s = strrchr(f, '.');
        if (!s || strncmp(s, ".index", 6))
            continue;

        snprintf(index_file, 1024, "%s/%s", path, f);
        *s = 0;
        snprintf(dict_file, 1024, "%s/%s.dict.dz", path, f);

        free(f);

        pd_sort_mode sort = pd_validate(index_file, dict_file);
        if (sort >= 0) {
            dict_t *new = (dict_t *) calloc(sizeof(dict_t), 1);
            if (dlist)
                new->next = dlist;
            dlist = new;

            dlist->d = pd_open(index_file, dict_file, sort);
            dlist->name = pd_name(dlist->d);
        }
    }
    eina_list_free(ls);

    free(dict_file);
    free(index_file);

    return dlist;
}

dict_t *
load_dicts()
{
    dict_t *dlist = NULL;

    char *path[3] = {
        strdup(SYS_DICT_DIR),
        xasprintf("%s" USER_DICT_DIR, getenv("HOME")),
        strdup(SD_DICT_DIR),
    };

    for (int i = 0; i < 3; i++) {
        dlist = load_dicts_from_dir(dlist, path[i]);
        free(path[i]);
    }

    return dlist;
}

void
close_dicts(dict_t *dlist)
{
    if (!dlist)
        return;

    dict_t *next;
    while (dlist) {
        next = dlist->next;
        if (dlist->name)
            free(dlist->name);
        if (dlist->d)
            pd_close(dlist->d);
        free(dlist);
        dlist = next;
    }
}

typedef struct str_t str_t;
struct str_t {
    char *str;
    char *p;
    size_t slen;
    size_t rest;
};

static str_t
string_constructor()
{
    str_t s;
    s.str = NULL;
    s.p = NULL;
    s.slen = 0;
    s.rest = 0;

    return s;
}

static inline void
string_append(str_t *s, const char *t, size_t len)
{
    if (s->rest < len) {
        s->rest += 1024;
        s->slen += 1024;
        s->str = (char *) realloc(s->str, s->slen);
        s->p = s->str + s->slen - s->rest;
        bzero(s->p, s->rest);
    }

    if (len == 1) {
        *s->p++ = *t;
        s->rest--;
    } else {
        strncpy(s->p, t, len);

        s->p += len;
        s->rest -= len;
    }
}

static void
string_destructor(str_t *s)
{
    if (s->str)
        free(s->str);

    s->str = NULL;
    s->p = NULL;
    s->slen = 0;
    s->rest = 0;
}

void
string_add_article(str_t *str, const char *ap, size_t alen)
{
    ADD_TAG(str, P_OPEN);

    while (alen--) {
        if (*ap == '\n') {
            string_append(str, BR, strlen(BR));
            ap++;
        } else {
            string_append(str, ap++, 1);
        }
    }

    ADD_TAG(str, P_CLOSE);
}

void
string_add_dictname(str_t *str, const char *name)
{
    ADD_TAG(str, P_OPEN);
    ADD_TAG(str, H2_OPEN);
    string_append(str, name, strlen(name));
    ADD_TAG(str, H2_CLOSE);
    ADD_TAG(str, P_CLOSE);
}

char *
translate(dict_t *dlist, const char *text)
{
    if(!text || strlen(text) == 0)
        return strdup("");

    str_t str = string_constructor();

    while (dlist) {
        pd_result *r = pd_find(dlist->d, text, PICODICT_FIND_STARTS_WITH);

        if (r && dlist->name)
            string_add_dictname(&str, dlist->name);

        int i = 0;
        while (r && i++ < ARTICLES_MAX) {
            size_t alen;
            const char *a = pd_result_article(r, &alen);

            string_add_article(&str, a, alen);

            pd_result *next = pd_result_next(r);
            pd_result_free(r);
            r = next;
        }

        dlist = dlist->next;
    }

    if (!str.slen) {
        ADD_TAG(&str, H2_OPEN);
        string_append(&str, NO_RES, strlen(NO_RES));
        ADD_TAG(&str, H2_CLOSE);
    }

    return str.str;
}
