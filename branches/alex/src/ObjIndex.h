/*
 * $Id$
 *
 * AUTHOR: Alex Rousskov
 *
 * SQUID Internet Object Cache  http://squid.nlanr.net/Squid/
 * --------------------------------------------------------
 *
 *  Squid is the result of efforts by numerous individuals from the
 *  Internet community.  Development is led by Duane Wessels of the
 *  National Laboratory for Applied Network Research and funded by
 *  the National Science Foundation.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *  
 */

#ifndef _OBJ_INDEX_H_
#define _OBJ_INDEX_H_

/* to be defined later @?@ */

struct _ObjIndexItem {
    int id;
    void *obj;
};

struct _ObjIndex {
    /* private, you should not care about them */
    int capacity;
    int count;
    struct _ObjIndexItem *items;
};

typedef int ObjIndexPos;

/* use this and only this to initialize ObjIndexPos */
#define ObjIndexInitPos (-1)

/* create/init/destroy */
extern ObjIndex *objIndexCreate();
extern void objIndexInit(ObjIndex *idx);
extern void objIndexDestroy(ObjIndex *idx);

/* accounting */
#define objIndexIsEmpty(idx) (!objIndexCount(idx))
#define objIndexCount(idx) (idx->count)

/* iterate through objects */
extern void *objIndexGet(const ObjIndex *idx, void **obj, ObjIndexPos *id, ObjIndexPos *pos);

/* add/delete */
extern void objIndexDel(ObjIndex *idx, ObjIndexPos id);
extern void objIndexAdd(ObjIndex *idx, void *obj, ObjIndexPos id);

#endif /* ndef _HTTP_HEADER_H_ */
