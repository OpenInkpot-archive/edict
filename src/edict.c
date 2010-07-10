/*
 * Copyright © 2010 Alexander Kerner <lunohod@openinkpot.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define _GNU_SOURCE
#include <string.h>
#include <stdbool.h>
#include <locale.h>
#include <libintl.h>
#include <err.h>

#include <Evas.h>
#include <Ecore.h>
#include <Ecore_Con.h>
#include <Ecore_X.h>
#include <Ecore_Evas.h>
#include <Edje.h>

#include <libchoicebox.h>
#include <libeoi.h>
#include <libkeys.h>
#include <libeoi_help.h>
#include <libeoi_themes.h>
#include <libeoi_clock.h>
#include <libeoi_battery.h>
#include <libeoi_textbox.h>
#include <libeoi_entry.h>

#include "picodict.h"

#define UNUSED __attribute__ ((unused))

#define THEME_EDJ "edict"

typedef struct info_t {
    Ecore_Evas *ee;
    Evas_Object *main_window;
    Evas_Object *textbox;
    keys_t *keys;
    dict_t *dlist;
    char *last_search;
} info_t;

static void
exit_all(void *param UNUSED)
{
    ecore_main_loop_quit();
}

static void
main_win_delete_handler(Ecore_Evas *main_win UNUSED)
{
    ecore_evas_hide(main_win);
}

typedef struct {
    char *msg;
    int size;
} client_data_t;

static int
_client_add(void *param UNUSED, int ev_type UNUSED, void *ev)
{
    Ecore_Con_Event_Client_Add *e = ev;
    client_data_t *msg = malloc(sizeof(client_data_t));
    msg->msg = strdup("");
    msg->size = 0;
    ecore_con_client_data_set(e->client, msg);
    return 0;
}

static int
_client_del(void *param, int ev_type UNUSED, void *ev)
{
    info_t *info = (info_t *) param;

    Ecore_Con_Event_Client_Del *e = ev;
    client_data_t *msg = ecore_con_client_data_get(e->client);

    char *str = strndup(msg->msg, msg->size);
    if(info->last_search)
        free(info->last_search);
    info->last_search = strdup(str);
    char *tr_str = translate(info->dlist, str);
    eoi_textbox_text_set(info->textbox, "");
    eoi_textbox_text_set(info->textbox, tr_str);
    free(tr_str);
    free(str);
    ecore_evas_show(info->ee);
    ecore_evas_raise(info->ee);

    free(msg->msg);
    free(msg);
    return 0;
}

static int
_client_data(void *param UNUSED, int ev_type UNUSED, void *ev)
{
    Ecore_Con_Event_Client_Data *e = ev;
    client_data_t *msg = ecore_con_client_data_get(e->client);
    msg->msg = realloc(msg->msg, msg->size + e->size);
    memcpy(msg->msg + msg->size, e->data, e->size);
    msg->size += e->size;
    return 0;
}

static void
show_help(Evas *evas)
{
    eoi_help_show(evas,
                  "edict", "index", gettext("Dictionary: Help"), NULL,
                  NULL);
}

void entry_handler(Evas_Object *entry,
        const char *text,
        void* param)
{
    info_t *info = (info_t *)param;

    char *tr_str = translate(info->dlist, text);
    eoi_textbox_text_set(info->textbox, "");
    eoi_textbox_text_set(info->textbox, tr_str);
    free(tr_str);

    if(info->last_search)
        free(info->last_search);

    info->last_search = strdup(text);
}

static void
key_handler(void *data, Evas *evas UNUSED, Evas_Object *obj UNUSED,
            void *event_info)
{
    info_t *info = data;

    if (!info->keys)
        info->keys = keys_alloc("edict");

    const char *action =
        keys_lookup_by_event(info->keys, "default", event_info);
    if (!action || !strlen(action))
        return;

    if (!strcmp(action, "Search"))
        entry_new(evas, entry_handler, "entry",
            gettext("Search"), info);
    else if (!strcmp(action, "SearchAgain")) {
        Evas *o = entry_new(evas, entry_handler, "entry",
            gettext("Search"), info);
        if(info->last_search)
            entry_text_set(o, info->last_search);
    } if (!strcmp(action, "PageDown"))
        eoi_textbox_page_next(info->textbox);
    else if (!strcmp(action, "PageUp"))
        eoi_textbox_page_prev(info->textbox);
    else if (!strcmp(action, "Close"))
        ecore_evas_hide(info->ee);
    else if (!strcmp(action, "Quit"))
        ecore_main_loop_quit();
}

static void
page_update_handler(Evas_Object *textbox,
                    int cur_page, int total_pages, void *param UNUSED)
{
    info_t *info = evas_object_data_get(textbox, "myinfo");
    choicebox_aux_edje_footer_handler(info->main_window,
                                      "footer", cur_page, total_pages);
}

int
main(int argc, char *argv[])
{
    if (!evas_init())
        errx(1, "Unable to initialize Evas\n");
    if (!ecore_init())
        errx(1, "Unable to initialize Ecore\n");
    if (!ecore_con_init())
        errx(1, "Unable to initialize Ecore_Con\n");
    if (!ecore_evas_init())
        errx(1, "Unable to initialize Evas\n");
    if (!edje_init())
        errx(1, "Unable to initialize Edje\n");

    setlocale(LC_ALL, "");
    textdomain("edict");

    info_t *info = (info_t *) calloc(sizeof(info_t), 1);

    Ecore_Con_Server *serv =
        ecore_con_server_add(ECORE_CON_LOCAL_USER, "edict", 0, NULL);
    ecore_event_handler_add(ECORE_CON_EVENT_CLIENT_ADD, _client_add, NULL);
    ecore_event_handler_add(ECORE_CON_EVENT_CLIENT_DATA, _client_data,
                            NULL);
    ecore_event_handler_add(ECORE_CON_EVENT_CLIENT_DEL, _client_del, info);

    ecore_x_io_error_handler_set(exit_all, NULL);

    info->ee = ecore_evas_software_x11_new(0, 0, 0, 0, 600, 800);
    ecore_evas_borderless_set(info->ee, 0);
    ecore_evas_shaped_set(info->ee, 0);
    ecore_evas_title_set(info->ee, "edict");
    ecore_evas_show(info->ee);

    ecore_evas_callback_delete_request_set(info->ee,
                                           main_win_delete_handler);

    Evas *evas = ecore_evas_get(info->ee);

    info->main_window = eoi_main_window_create(evas);
    eoi_fullwindow_object_register(info->ee, info->main_window);
    eoi_run_clock(info->main_window);
    eoi_run_battery(info->main_window);


    edje_object_part_text_set(info->main_window, "title",
                              gettext("Dictionary"));
    edje_object_part_text_set(info->main_window, "footer", "0/0");

    evas_object_name_set(info->main_window, "main-window");
    evas_object_move(info->main_window, 0, 0);
    evas_object_resize(info->main_window, 600, 800);
    evas_object_show(info->main_window);

    evas_object_focus_set(info->main_window, true);
    evas_object_event_callback_add(info->main_window, EVAS_CALLBACK_KEY_UP,
                                   &key_handler, info);

    info->textbox = eoi_textbox_new(evas,
                                    THEME_EDJ,
                                    "dict", page_update_handler);

    evas_object_data_set(info->textbox, "myinfo", info);
    edje_object_part_swallow(info->main_window, "contents", info->textbox);

    info->dlist = load_dicts();
    if (!info->dlist)
        eoi_textbox_text_set(info->textbox,
                             gettext("No dictionaries found"));
    else if (argc > 1) {
        char *t = translate(info->dlist, argv[1]);
        eoi_textbox_text_set(info->textbox, t);
        free(t);
    } else
        eoi_textbox_text_set(info->textbox, "");

    ecore_main_loop_begin();

    eoi_textbox_free(info->textbox);
    if (info->keys)
        keys_free(info->keys);
    if (info->last_search)
        free(info->last_search);
    close_dicts(info->dlist);

    ecore_con_server_del(serv);
    ecore_con_shutdown();
    ecore_evas_shutdown();
    ecore_shutdown();
    evas_shutdown();
    edje_shutdown();

    return 0;
}
