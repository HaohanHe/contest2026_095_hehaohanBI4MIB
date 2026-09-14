/*
* Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
* All rights reserved.
*
* File : M3UParser.h
* Description :
* History :
*   Author  : Kewei Han
*   Date    : 2014/10/08
*/

#ifndef M3U8_PARSER_H
#define M3U8_PARSER_H
#include <pthread.h>
#include <CdxTypes.h>
#include <string.h>

#define maxNumAtom (32)
#define MAX_ATOM_NUM_IN_POOL 32

#define HLS_MALLOC_ATOM_DYNAMIC  0

#define HLS_IOT_USE_ATOM_POOL  0

typedef enum {
    false,
    true
}bool;

typedef enum {
    OK                                 = 0,
    err_unknown                     = -1,
    ERROR_MALFORMED                 = -2,
    ERROR_maxNumAtom_too_little     = -3,
    err_no_memory                     = -4,
    err_URL                         = -5,
    err_no_bandwidth                = -6,
    err_allocateBigBuffer             = -7,
}status_t;

//***********************************************************//
//* AString
//***********************************************************//

typedef struct {
    char    *mData;
    cdx_uint32    mSize;/*实际字符的个数，不含'\0'*/
    cdx_uint32    mAllocSize;/*开辟空间的大小*/
}AString;

//***********************************************************//
//* AMessage
//***********************************************************//
enum Type {
    kTypeInt32,
    kTypeInt64,
    kTypeSize,
    kTypeFloat,
    kTypeDouble,
    kTypeString,
    //kTypePointer
};

typedef struct Atom Atom;
struct Atom {
    union {
        cdx_int32        int32Value;
        cdx_int64        int64Value;
        cdx_uint32        sizeValue;
        float        floatValue;
        double        doubleValue;
        AString        *stringValue;
        //void *ptrValue;
    } u;
    const char *mName;
    enum Type mType;
#if HLS_MALLOC_ATOM_DYNAMIC
    Atom *next;
#endif
};

#if (HLS_IOT_USE_ATOM_POOL && HLS_MALLOC_ATOM_DYNAMIC)
typedef struct {
    Atom *patom[MAX_ATOM_NUM_IN_POOL];
    pthread_mutex_t atomlock;
}AtomPool;
#endif

typedef struct {
#if HLS_MALLOC_ATOM_DYNAMIC
    Atom *mAtom;
#if HLS_IOT_USE_ATOM_POOL
    AtomPool *pool;
#endif
#else
    Atom mAtom[maxNumAtom];
#endif
    cdx_uint8 mNumAtom;
}AMessage;

//***********************************************************//
//* MediaGroup
//***********************************************************//
typedef enum {
    TYPE_UNKNOWN    = -1,
    TYPE_AUDIO        = 0,
    TYPE_VIDEO         = 1,
    TYPE_SUBS        = 2,
}MediaType;

typedef enum {
    CDX_FLAG_AUTOSELECT         = 1,
    CDX_FLAG_DEFAULT            = 2,
    CDX_FLAG_FORCED             = 4,
    CDX_FLAG_HAS_LANGUAGE       = 8,
    CDX_FLAG_HAS_URI            = 16,
}MediaFlagBits;
typedef struct MediaItem MediaItem;
typedef struct MediaGroup MediaGroup;

struct MediaItem {
    AString *mName;
    AString *mURI;
    AString *mLanguage;
    cdx_uint32 mFlags;
    MediaItem *next;
    MediaGroup *father;/*指向所属的MediaGroup*/
    //Playlist *mediaPlaylist;
};

struct MediaGroup {
    AString *groupID;
    MediaType mType;
    MediaItem *mMediaItems;/*media item构成的链表*/
    cdx_uint8 mNumMediaItem;
    cdx_int32 mSelectedIndex;
    MediaGroup *next;
};
typedef struct PlaylistItem PlaylistItem;
struct PlaylistItem {
    AString *mURI;
    union {
        struct {
            cdx_int64 durationUs;
            cdx_int32 seqNum;
            PlaylistItem *cipherReference;
            cdx_int64 baseTimeUs;
        }mediaItemAttribute;
        struct {
            cdx_int64 bandwidth;
        /*带宽编号，从0起计，表征该条目在playlist中的位置，由于PlaylistItem成链结构，也可以没有*/
            cdx_int32 bandwidthNum;
        }masterItemAttribute;
    } u;
    bool isMediaItem;
    int discontinue;
    AMessage itemMeta;
    PlaylistItem *next;
};

typedef struct Playlist Playlist;
struct Playlist {
    AString mBaseURI;
    bool mIsExtM3U;
    bool mIsVariantPlaylist;
    PlaylistItem *mItems;
    cdx_uint32 mNumItems;
    AMessage mMeta;
    Playlist *next;
    union {
        struct {
            bool mIsComplete;
            bool mIsEvent;
            bool hasEncrypte;
            cdx_int64 mDurationUs;
            cdx_int32 lastSeqNum;
            cdx_int8  discontinueTimeshift;//for cmcc timeshift
        } mediaPlaylistPrivate;
        struct {
            cdx_uint8 mNumMediaPlaylist;/*Playlist链表上所挂载的mediaPlaylist的个数*/ //目前没用
            MediaGroup *mMediaGroups;
            cdx_uint8 mNumMediaGroups;
        } masterPlaylistPrivate;

    } u;
};

#if (HLS_IOT_USE_ATOM_POOL && HLS_MALLOC_ATOM_DYNAMIC)
int createAtomPool(AtomPool *p);
void destroyAtomPool(AtomPool *p);
#endif

PlaylistItem *findItemBySeqNum(Playlist *playlist, int seqNum);
PlaylistItem *findItemByIndex(Playlist *playlist, int index);
int findInt64(const AMessage *meta, const char *name, cdx_int64 *value);
int findInt32(const AMessage *meta, const char *name, cdx_int32 *value);
int findString(const AMessage *meta, const char *name, AString **value);
#if (HLS_IOT_USE_ATOM_POOL && HLS_MALLOC_ATOM_DYNAMIC)
status_t M3u8Parse(const void *_data, cdx_uint32 size, Playlist **P, const char *baseURI, AtomPool *pool);
#else
status_t M3u8Parse(const void *_data, cdx_uint32 size, Playlist **P, const char *baseURI);
#endif
bool M3uProbe(const char *data, cdx_uint32 size);
void destroyPlaylist(Playlist *playList);

static inline int startsWith(const char* str, const char* prefix)
{
    return !strncmp(str, prefix, strlen(prefix));
}

#endif
