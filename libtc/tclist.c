/*
 *  tclist.c
 *
 *  Copyright (C) Francesco Romani - September 2008
 *
 *  This file is part of transcode, a video stream processing tool
 *
 *  transcode is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  transcode is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include "libtc.h"
#include "tclist.h"

/*************************************************************************/

static int free_item(TCListItem *item, void *unused)
{
    free(item);
    return 0;
}

static TCListItem* next_item(TCListItem *cur, int reversed)
{
    return (!cur) ?NULL :((reversed) ?cur->prev :cur->next);
}

static int foreach_item(TCListItem *start, int reversed, 
                        TCListVisitor vis, void *userdata)
{
    int ret = 0;
    TCListItem *cur = NULL, *inc = NULL;

    for (cur = start; cur; cur = inc) {
        inc = next_item(cur, reversed);
        ret = vis(cur, userdata);
        if (ret != 0) {
            break;
        }
    }

    return ret;
}

static TCListItem *find_item(TCList *L, int pos)
{
    /* common cases first */
    if (L) {
        if (pos == 0) {
            return L->head;
        } else if (pos == -1) {
            return L->tail;
//        } else {
        }
    }
    return NULL;
}

static void del_item(TCList *L, TCListItem *IT)
{
    if (L->use_cache) {
        IT->prev = NULL;
        IT->data = NULL;
        IT->next = L->cache;
        L->cache = IT;
    } else {
        tc_free(IT);
    }
}

static TCListItem *new_item(TCList *L)
{
    TCListItem *IT = NULL;
    if (L->use_cache && L->cache) {
        IT = L->cache;
        L->cache = L->cache->next;
    } else {
        IT = tc_zalloc(sizeof(TCListItem));
    }
    return IT;
}

/*************************************************************************/

int tc_list_init(TCList *L, int elemcache)
{
    if (L) {
        L->head      = NULL;
        L->tail      = NULL;
        L->nelems    = 0;
        L->cache     = NULL;
        L->use_cache = elemcache;

        return 0;
    }
    return -1;
}

int tc_list_fini(TCList *L)
{
    /* if !use_cache, this will not hurt anyone */
    foreach_item(L->cache, 0, free_item, NULL);
    /* now reset to clean status */
    return tc_list_init(L, 0);
}

int tc_list_size(TCList *L)
{
    return (L) ?L->nelems :0;
}

int tc_list_foreach(TCList *L, TCListVisitor vis, void *userdata)
{
    return foreach_item(L->head, 0, vis, userdata);
}

int tc_list_append(TCList *L, void *data)
{
    int ret = TC_ERROR;
    TCListItem *IT = new_item(L);
 
    if (IT) {
        IT->data = data;
        IT->prev = L->tail;
        L->tail = IT;
        if (!L->head) {
            L->head = IT;
        }
        L->nelems++;
 
        ret = TC_OK;
    }
    return ret;
}

int tc_list_prepend(TCList *L, void *data)
{
    int ret = TC_ERROR;
    TCListItem *IT = new_item(L);

    if (IT) {
        IT->data = data;
        IT->next = L->head;
        L->head =IT;
        if (!L->tail) {
            L->tail = IT;
        }
        L->nelems++;
 
        ret = TC_OK;
    }
    return ret;
}

int tc_list_insert(TCList *L, int pos, void *data)
{
    int ret = TC_ERROR;
/*    if (L && data) {
        if (pos == 0) {
            ret = tc_list_prepend(L, data);
        } else if (pos == -1) {
            ret = tc_list_append(L, data);
        } else {
        }
    }*/
    return ret;
}

void *tc_list_get(TCList *L, int pos)
{
    TCListItem *IT = find_item(L, pos);
    return (IT) ?IT->data :NULL;
}

void *tc_list_pop(TCList *L, int pos)
{
    TCListItem *IT = find_item(L, pos);
    void *data = NULL;
    if (IT) {
        data = IT->data;
        del_item(L, IT);
    }
    return data;
}

/*************************************************************************/

int tc_list_insert_dup(TCList *L, int pos, void *data, size_t size)
{
    int ret = TC_ERROR;
    void *mem = tc_malloc(size);
    if (mem) {
        memcpy(mem, data, size);
        ret = tc_list_insert(L, pos, data);
        if (ret == TC_ERROR) {
            tc_free(mem);
        }
    }
    return ret;
}

static int free_item_all(TCListItem *item, void *unused)
{
    free(item->data);
    free(item);
    return 0;
}

int tc_list_fini_all(TCList *L)
{
    /* if !use_cache, this will not hurt anyone */
    foreach_item(L->cache, 0, free_item_all, NULL);
    /* now reset to clean status */
    return tc_list_init(L, 0);
}

/*************************************************************************/

/*
 * Local variables:
 *   c-file-style: "stroustrup"
 *   c-file-offsets: ((case-label . *) (statement-case-intro . *))
 *   indent-tabs-mode: nil
 * End:
 *
 * vim: expandtab shiftwidth=4:
 */
