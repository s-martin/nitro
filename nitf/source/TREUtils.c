/* =========================================================================
 * This file is part of NITRO
 * =========================================================================
 * 
 * (C) Copyright 2004 - 2008, General Dynamics - Advanced Information Systems
 *
 * NITRO is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public 
 * License along with this program; if not, If not, 
 * see <http://www.gnu.org/licenses/>.
 *
 */

#include "nitf/TREUtils.h"

NITFAPI(int) nitf_TREUtils_parse(nitf_TRE * tre,
                            char *bufptr, nitf_Error * error)
{
    int status = 1;
    int iterStatus = NITF_SUCCESS;
    int offset = 0;
    int length;
    nitf_TRECursor cursor;
    nitf_Field *field;

    /* get out if TRE is null */
    if (!tre)
    {
        nitf_Error_init(error, "parse -> invalid tre object",
                        NITF_CTXT, NITF_ERR_INVALID_PARAMETER);
        return NITF_FAILURE;
    }

    /* flush the TRE hash first, to protect from duplicate entries */
    nitf_TRE_flush(tre, error);

    cursor = nitf_TRECursor_begin(tre);
    while (offset < tre->length && status)
    {
        if ((iterStatus = nitf_TRECursor_iterate(&cursor, error)) == NITF_SUCCESS)
        {
            length = cursor.length;
            if (length == NITF_TRE_GOBBLE)
            {
                length = tre->length - offset;
            }

            /* no need to call setValue, because we already know
             * it is OK for this one to be in the hash
             */
            /* construct the field */
			printf("Cursor tag [%s]\n", cursor.tag_str);
            field =
                nitf_Field_construct(length, cursor.desc_ptr->data_type,
                                     error);
            if (!field)
                goto CATCH_ERROR;

            /* first, check to see if we need to swap bytes */
            if (field->type == NITF_BINARY
                    && (length == NITF_INT16_SZ || length == NITF_INT32_SZ))
            {
                if (length == NITF_INT16_SZ)
                {
                    nitf_Int16 int16 =
                        (nitf_Int16)NITF_NTOHS(*((nitf_Int16 *) (bufptr + offset)));
                    status = nitf_Field_setRawData(field,
                                                   (NITF_DATA *) & int16, length, error);
                }
                else if (length == NITF_INT32_SZ)
                {
                    nitf_Int32 int32 =
                        (nitf_Int32)NITF_NTOHL(*((nitf_Int32 *) (bufptr + offset)));
                    status = nitf_Field_setRawData(field,
                                                   (NITF_DATA *) & int32, length, error);
                }
            }
            else
            {
                /* check for the other binary lengths ... */
                if (field->type == NITF_BINARY)
                {
                    /* TODO what to do??? 8 bit is ok, but what about 64? */
                    /* for now, just let it go through... */
                }

                /* now, set the data */
                status = nitf_Field_setRawData(field, (NITF_DATA *) (bufptr + offset),
                                               length, error);
            }

#ifdef NITF_DEBUG
            {
                fprintf(stdout, "Adding Field [%s] to TRE [%s]\n",
                        cursor.tag_str, tre->tag);
            }
#endif

            /* add to the hash */
            nitf_HashTable_insert(tre->hash, cursor.tag_str, field, error);

            offset += length;
        }
        /* otherwise, the iterate function thinks we are done */
        else
        {
            break;
        }
    }
    nitf_TRECursor_cleanup(&cursor);

    /* check if we still have more to parse, and throw an error if so */
    if (offset < tre->length)
    {
        nitf_Error_init(error, "TRE data is longer than it should be",
                        NITF_CTXT, NITF_ERR_INVALID_OBJECT);
        status = NITF_FAILURE;
    }
    return status;

    /* deal with errors here */
CATCH_ERROR:
    return NITF_FAILURE;
}



NITFAPI(char *) nitf_TREUtils_getRawData(nitf_TRE * tre, nitf_Uint32* treLength, nitf_Error * error)
{
    int status = 1;
    int offset = 0;
    nitf_Uint32 length;
    int tempLength;
    char *data = NULL;          /* data buffer - Caller must free this */
    char *tempBuf = NULL;       /* temp data buffer */
    nitf_Pair *pair;            /* temp nitf_Pair */
    nitf_Field *field;          /* temp nitf_Field */
    nitf_TRECursor cursor;      /* the cursor */

    /* get acutal length of TRE */
    if (tre->length <= NITF_TRE_DEFAULT_LENGTH)
        length = nitf_TREUtils_computeLength(tre);
    else
        length = tre->length;
    *treLength = length;

    if (length <= 0)
    {
        nitf_Error_init(error, "TRE has invalid length",
                        NITF_CTXT, NITF_ERR_INVALID_OBJECT);
        return NULL;
    }

    /* allocate the memory - this does not get freed in this function */
    data = (char *) NITF_MALLOC(length + 1);
    if (!data)
    {
        nitf_Error_init(error, NITF_STRERROR(NITF_ERRNO),
                        NITF_CTXT, NITF_ERR_MEMORY);
        goto CATCH_ERROR;
    }
    memset(data, 0, length + 1);

    cursor = nitf_TRECursor_begin(tre);
    while (!nitf_TRECursor_isDone(&cursor) && status && offset < length)
    {
        if (nitf_TRECursor_iterate(&cursor, error) == NITF_SUCCESS)
        {
            pair = nitf_HashTable_find(tre->hash, cursor.tag_str);
            if (pair && pair->data)
            {
                tempLength = cursor.length;
                if (tempLength == NITF_TRE_GOBBLE)
                {
                    tempLength = length - offset;
                }
                field = (nitf_Field *) pair->data;

                /* get the raw data */
                tempBuf = NITF_MALLOC(tempLength);
                if (!tempBuf)
                {
                    nitf_Error_init(error, NITF_STRERROR(NITF_ERRNO),
                                    NITF_CTXT, NITF_ERR_MEMORY);
                    goto CATCH_ERROR;
                }
                /* get the data as raw buf */
                nitf_Field_get(field, (NITF_DATA *) tempBuf,
                               NITF_CONV_RAW, tempLength, error);

                /* first, check to see if we need to swap bytes */
                if (field->type == NITF_BINARY)
                {
                    if (tempLength == NITF_INT16_SZ)
                    {
                        nitf_Int16 int16 =
                            (nitf_Int16)NITF_HTONS(*((nitf_Int16 *) tempBuf));
                        memcpy(tempBuf, (char*)&int16, tempLength);
                    }
                    else if (tempLength == NITF_INT32_SZ)
                    {
                        nitf_Int32 int32 =
                            (nitf_Int32)NITF_HTONL(*((nitf_Int32 *) tempBuf));
                        memcpy(tempBuf, (char*)&int32, tempLength);
                    }
                    else
                    {
                        /* TODO what to do??? 8 bit is ok, but what about 64? */
                        /* for now, just let it go through... */
                    }
                }

                /* now, memcpy the data */
                memcpy(data + offset, tempBuf, tempLength);
                offset += tempLength;

                /* free the buf */
                NITF_FREE(tempBuf);
            }
            else
            {
                nitf_Error_init(error,
                                "Failed due to missing TRE field(s)",
                                NITF_CTXT, NITF_ERR_INVALID_OBJECT);
                goto CATCH_ERROR;
            }
        }
    }
    nitf_TRECursor_cleanup(&cursor);
    return data;

    /* deal with errors here */
CATCH_ERROR:
    if (data)
        NITF_FREE(data);
    return NULL;
}


NITFAPI(NITF_BOOL) nitf_TREUtils_readField(nitf_IOHandle handle,
                                      char *fld,
                                      int length, nitf_Error * error)
{
    NITF_BOOL status;

    /* Make sure the field is nulled out  */
    memset(fld, 0, length);

    /* Read from the IO handle */
    status = nitf_IOHandle_read(handle, fld, length, error);
    if (!status)
    {
        nitf_Error_init(error,
                        "Unable to read from IO object",
                        NITF_CTXT, NITF_ERR_READING_FROM_FILE);
    }
    return status;
}


NITFAPI(NITF_BOOL) nitf_TREUtils_setValue(nitf_TRE * tre,
                                     const char *tag,
                                     NITF_DATA * data,
                                     size_t dataLength, nitf_Error * error)
{
    nitf_Pair *pair;
    nitf_Field *field = NULL;
    nitf_TRECursor cursor;
    NITF_BOOL done = 0;
    NITF_BOOL status = 1;
    nitf_FieldType type = NITF_BCS_A;
    int length;                 /* used temporarily for storing the length */

    /* get out if TRE is null */
    if (!tre)
    {
        nitf_Error_init(error, "setValue -> invalid tre object",
                        NITF_CTXT, NITF_ERR_INVALID_PARAMETER);
        return NITF_FAILURE;
    }

    /* If the field already exists, get it and modify it */
    if (nitf_HashTable_exists(tre->hash, tag))
    {
        pair = nitf_HashTable_find(tre->hash, tag);
        field = (nitf_Field *) pair->data;

        if (!field)
        {
            nitf_Error_init(error, "setValue -> invalid field object",
                            NITF_CTXT, NITF_ERR_INVALID_PARAMETER);
            return NITF_FAILURE;
        }

        /* check to see if the data passed in is too large or too small */
        if (dataLength > field->length || dataLength < 1)
        {
            nitf_Error_init(error, "setValue -> invalid dataLength",
                            NITF_CTXT, NITF_ERR_INVALID_PARAMETER);
            return NITF_FAILURE;
        }

        if (!nitf_Field_setRawData
                (field, (NITF_DATA *) data, dataLength, error))
        {
            return NITF_FAILURE;
        }
    }
    /* it doesn't exist in the hash yet, so we need to find it */
    else
    {
        cursor = nitf_TRECursor_begin(tre);
        while (!nitf_TRECursor_isDone(&cursor) && !done && status)
        {
            if (nitf_TRECursor_iterate(&cursor, error) == NITF_SUCCESS)
            {
                /* we found it */
                if (strcmp(tag, cursor.tag_str) == 0)
                {
                    if (cursor.desc_ptr->data_type == NITF_BCS_A)
                    {
                        type = NITF_BCS_A;
                    }
                    else if (cursor.desc_ptr->data_type == NITF_BCS_N)
                    {
                        type = NITF_BCS_N;
                    }
                    else if (cursor.desc_ptr->data_type == NITF_BINARY)
                    {
                        type = NITF_BINARY;
                    }
                    else
                    {
                        /* bad type */
                        nitf_Error_init(error,
                                        "setValue -> invalid data type",
                                        NITF_CTXT,
                                        NITF_ERR_INVALID_PARAMETER);
                        return NITF_FAILURE;
                    }

                    length = cursor.length;
                    /* check to see if we should gobble the rest */
                    if (length == NITF_TRE_GOBBLE)
                    {
                        length = dataLength;
                    }

                    /* construct the field */
                    field = nitf_Field_construct(length, type, error);

                    /* now, set the data */
                    nitf_Field_setRawData(field, (NITF_DATA *) data,
                                          dataLength, error);

#ifdef NITF_DEBUG
                    fprintf(stdout, "Adding Field [%s] to TRE [%s]\n",
                            cursor.tag_str, tre->tag);
#endif

                    /* add to the hash */
                    nitf_HashTable_insert(tre->hash, cursor.tag_str, field,
                                          error);
                    done = 1;   /* set, so we break out of loop */
                }
            }
        }
        /* did we find it? */
        if (!done)
        {
            nitf_Error_initf(error, NITF_CTXT, NITF_ERR_UNK,
                             "Unable to find tag, '%s', in TRE hash for TRE '%s'",
                             tag, tre->tag);
            status = 0;
        }
        nitf_TRECursor_cleanup(&cursor);
    }
    return status;

}

NITFAPI(void) nitf_TREUtils_setDescription(nitf_TRE* tre, nitf_Error* error)
{

	nitf_TREDescriptionSet *descriptions = NULL;
    nitf_TREDescriptionInfo *infoPtr = NULL;
    int numDescriptions = 0;
    if (!tre)
    {
        nitf_Error_init(error, "setDescription -> invalid tre object",
                        NITF_CTXT, NITF_ERR_INVALID_PARAMETER);
        return;
    }
	descriptions = (nitf_TREDescriptionSet*)tre->handler->data;
	
    if (!descriptions)
    {
        nitf_Error_init(error, "TRE Description Set is NULL",
                        NITF_CTXT, NITF_ERR_INVALID_OBJECT);
        return;
    }
	tre->priv = NULL; 
   
    if (tre->length > NITF_TRE_DEFAULT_LENGTH)
    {
        infoPtr = descriptions->descriptions;
        while (infoPtr && (infoPtr->description != NULL))
        {
            if (infoPtr->lengthMatch == tre->length)
            {
                tre->priv = infoPtr->description;
                break;
            }
            infoPtr++;
        }
    }
    
	if (!tre->priv)
    {
        infoPtr = descriptions->descriptions;
        while (infoPtr && (infoPtr->description != NULL))
        {
            if (numDescriptions == descriptions->defaultIndex)
            {
				tre->priv = infoPtr->description;
                if (tre->length == NITF_TRE_DEFAULT_LENGTH &&
                    infoPtr->lengthMatch != NITF_TRE_DESC_NO_LENGTH)
                    tre->length = infoPtr->lengthMatch;
                break;
            }
            numDescriptions++;
            infoPtr++;
        }
    }
	if (!tre->priv)
    {
        nitf_Error_init(error, "TRE Description is NULL",
                        NITF_CTXT, NITF_ERR_INVALID_OBJECT);
        return;
    }
}


NITFAPI(NITF_BOOL) nitf_TREUtils_fillData(nitf_TRE * tre,
                                   const nitf_TREDescription* descrip,
                                   nitf_Error * error)
{
	nitf_TRECursor cursor;
	
	nitf_TREDescription* privDesc = (nitf_TREDescription *)tre->priv;
	privDesc = descrip;

    /* loop over the description, and add blank fields for the
     * "normal" fields... any special case fields (loops, conditions)
     * won't be added here
     */
    cursor = nitf_TRECursor_begin(tre);
    while (!nitf_TRECursor_isDone(&cursor))
    {
        if (nitf_TRECursor_iterate(&cursor, error))
        {
            nitf_Pair* pair = nitf_HashTable_find(tre->hash, cursor.tag_str);
            if (!pair || !pair->data)
            {
                nitf_Field* field = nitf_Field_construct(cursor.length,
                                             cursor.desc_ptr->data_type,
                                             error);

                /* special case if BINARY... must set Raw Data */
                if (cursor.desc_ptr->data_type == NITF_BINARY)
                {
                    char* tempBuf = (char *) NITF_MALLOC(cursor.length);
                    if (!tempBuf)
                    {
                        nitf_Field_destruct(&field);
                        nitf_Error_init(error, NITF_STRERROR(NITF_ERRNO),
                                        NITF_CTXT, NITF_ERR_MEMORY);
                        goto CATCH_ERROR;
                    }

                    memset(tempBuf, 0, cursor.length);
                    nitf_Field_setRawData(field, (NITF_DATA *) tempBuf,
                                          cursor.length, error);
                }
                else
                {
                    /* this will get zero/blank filled by the function */
                    nitf_Field_setString(field, "", error);
                }

                /* add to hash if there wasn't an entry yet */
                if (!pair)
                {
                    nitf_HashTable_insert(tre->hash,
                                          cursor.tag_str, field, error);
                }
                /* otherwise, just set the data pointer */
                else
                {
                    pair->data = (NITF_DATA *) field;
                }
            }
        }
    }
    nitf_TRECursor_cleanup(&cursor);

    /* no problems */
    /*    return tre->descrip; */
    return NITF_SUCCESS;
CATCH_ERROR:
    return NITF_FAILURE;
}

#if 0
NITFAPI(NITF_BOOL) nitf_TREUtils_fillData(nitf_TRE * tre,
                                   const nitf_TREDescription* descrip,
                                   nitf_Error * error)

{
    int bad = 0;
    NITF_PLUGIN_TRE_SET_DESCRIPTION_FUNCTION function =
        (NITF_PLUGIN_TRE_SET_DESCRIPTION_FUNCTION) NULL;

    nitf_PluginRegistry *reg;
    nitf_Pair *pair;            /* temp nitf_Pair */
    nitf_Field *field;          /* temp nitf_Field */
    nitf_TRECursor cursor;      /* the cursor for looping the TRE */
    char *tempBuf;              /* temp buffer */

    /* get out if TRE is null */
    if (!tre)
    {
        nitf_Error_init(error, "nitf_TREUtils_fillData -> invalid tre object",
                        NITF_CTXT, NITF_ERR_INVALID_PARAMETER);
        goto CATCH_ERROR;
    }

    /* see if the description needs to be looked up in the registry */
    if (!descrip)
    {
        reg = nitf_PluginRegistry_getInstance(error);
        if (reg)
        {
            function = nitf_PluginRegistry_retrieveSetTREDescription(reg,
                tre->tag, &bad, error);
            if (bad)
                goto CATCH_ERROR;
        }

        if (function == (NITF_PLUGIN_TRE_SET_DESCRIPTION_FUNCTION) NULL)
        {
            /* try the default TRE handler */
            handler = nitf_DefaultTRE_handler(error);
			if (handler == NULL)
			{
				/* for now, just error out */
				nitf_Error_init(error,
                            "TRE Set Description handle is NULL",
                            NITF_CTXT, NITF_ERR_INVALID_OBJECT);
			}
            goto CATCH_ERROR;
        }
        
        (*function)(tre, error);
        if (!tre->descrip)
        {
            /*nitf_Error_init(error, "TRE Description is NULL",
                            NITF_CTXT, NITF_ERR_INVALID_OBJECT);*/
            goto CATCH_ERROR;
        }
    }
    else
    {
        tre->descrip = (nitf_TREDescription *) descrip;
    }

    /* loop over the description, and add blank fields for the
     * "normal" fields... any special case fields (loops, conditions)
     * won't be added here
     */
    cursor = nitf_TRECursor_begin(tre);
    while (!nitf_TRECursor_isDone(&cursor))
    {
        if (nitf_TRECursor_iterate(&cursor, error))
        {
            pair = nitf_HashTable_find(tre->hash, cursor.tag_str);
            if (!pair || !pair->data)
            {
                field = nitf_Field_construct(cursor.length,
                                             cursor.desc_ptr->data_type,
                                             error);

                /* special case if BINARY... must set Raw Data */
                if (cursor.desc_ptr->data_type == NITF_BINARY)
                {
                    tempBuf = (char *) NITF_MALLOC(cursor.length);
                    if (!tempBuf)
                    {
                        nitf_Field_destruct(&field);
                        nitf_Error_init(error, NITF_STRERROR(NITF_ERRNO),
                                        NITF_CTXT, NITF_ERR_MEMORY);
                        goto CATCH_ERROR;
                    }

                    memset(tempBuf, 0, cursor.length);
                    nitf_Field_setRawData(field, (NITF_DATA *) tempBuf,
                                          cursor.length, error);
                }
                else
                {
                    /* this will get zero/blank filled by the function */
                    nitf_Field_setString(field, "", error);
                }

                /* add to hash if there wasn't an entry yet */
                if (!pair)
                {
                    nitf_HashTable_insert(tre->hash,
                                          cursor.tag_str, field, error);
                }
                /* otherwise, just set the data pointer */
                else
                {
                    pair->data = (NITF_DATA *) field;
                }
            }
        }
    }
    nitf_TRE_cleanup(&cursor);

    /* no problems */
    /*    return tre->descrip; */
    return NITF_SUCCESS;
CATCH_ERROR:
    return NITF_FAILURE;
}

#endif



NITFAPI(int) nitf_TREUtils_print(nitf_TRE * tre, nitf_Error * error)
{
    nitf_Pair *pair;            /* temp pair */
    int status = NITF_SUCCESS;
    nitf_TRECursor cursor;

    /* get out if TRE is null */
    if (!tre)
    {
        nitf_Error_init(error, "print -> invalid tre object",
                        NITF_CTXT, NITF_ERR_INVALID_PARAMETER);
        return NITF_FAILURE;
    }

    cursor = nitf_TRECursor_begin(tre);
    while (!nitf_TRECursor_isDone(&cursor) && (status == NITF_SUCCESS))
    {
        if ((status = nitf_TRECursor_iterate(&cursor, error)) == NITF_SUCCESS)
        {
            pair = nitf_HashTable_find(tre->hash, cursor.tag_str);
            if (!pair || !pair->data)
            {
                nitf_Error_initf(error, NITF_CTXT, NITF_ERR_UNK,
                                 "Unable to find tag, '%s', in TRE hash for TRE '%s'", cursor.tag_str, tre->tag);
                status = NITF_FAILURE;
            }
            else
            {
                printf("%s (%s) = [",
                       cursor.desc_ptr->label == NULL ?
                       "null" : cursor.desc_ptr->label, cursor.tag_str);
                nitf_Field_print((nitf_Field *) pair->data);
                printf("]\n");
            }
        }
    }
    nitf_TRECursor_cleanup(&cursor);
    return status;
}
NITFAPI(int) nitf_TREUtils_computeLength(nitf_TRE * tre)
{
    int length = 0;
    int tempLength;
    nitf_Error error;
    nitf_Pair *pair;            /* temp nitf_Pair */
    nitf_Field *field;          /* temp nitf_Field */
    nitf_TRECursor cursor;

    /* get out if TRE is null */
    if (!tre)
        return -1;

    cursor = nitf_TRECursor_begin(tre);
    while (!nitf_TRECursor_isDone(&cursor))
    {
        if (nitf_TRECursor_iterate(&cursor, &error) == NITF_SUCCESS)
        {
            tempLength = cursor.length;
            if (tempLength == NITF_TRE_GOBBLE)
            {
                /* we don't have any other way to know the length of this
                 * field, other than to see if the field is in the hash
                 * and use the length defined when it was created.
                 * Otherwise, we don't add any length.
                 */
                tempLength = 0;
                pair = nitf_HashTable_find(tre->hash, cursor.tag_str);
                if (pair)
                {
                    field = (nitf_Field *) pair->data;
                    if (field)
                        tempLength = field->length;
                }
            }
            length += tempLength;
        }
    }
    nitf_TRECursor_cleanup(&cursor);
    return length;
}


NITFAPI(NITF_BOOL) nitf_TREUtils_isSane(nitf_TRE * tre)
{
    int status = 1;
    nitf_Error error;
    nitf_TRECursor cursor;

    /* get out if TRE is null */
    if (!tre)
        return NITF_FAILURE;

    cursor = nitf_TRECursor_begin(tre);
    while (!nitf_TRECursor_isDone(&cursor) && status)
    {
        if (nitf_TRECursor_iterate(&cursor, &error) == NITF_SUCCESS)
            if (!nitf_TRE_exists(tre, cursor.tag_str))
                status = !status;
    }
    nitf_TRECursor_cleanup(&cursor);
    return status;
}


NITFPRIV(NITF_BOOL) basicRead(nitf_IOHandle ioHandle, 
							  nitf_TRE* tre, struct _nitf_Record* record, nitf_Error* error)
{
	int ok; 
    char *data = NULL; 
    NITF_BOOL success; 
    if (!tre) {goto CATCH_ERROR;} 
	nitf_TREUtils_setDescription(tre, error); 
    //if (!tre->descrip) goto CATCH_ERROR; 
    data = (char*)NITF_MALLOC( tre->length ); 
    if (!data) 
    { 
        nitf_Error_init(error, NITF_STRERROR( NITF_ERRNO ),NITF_CTXT, NITF_ERR_MEMORY );
        goto CATCH_ERROR;
    }
    memset(data, 0, tre->length);
    success = nitf_TREUtils_readField(ioHandle, data, (int)tre->length, error);
    if (!success) goto CATCH_ERROR;
    ok = nitf_TREUtils_parse(tre, data, error);
    NITF_FREE( data );
    data = NULL;
if (!ok){goto CATCH_ERROR; }
    return NITF_SUCCESS;
CATCH_ERROR:
    if (data) NITF_FREE(data);
    return NITF_FAILURE;

}



NITFPRIV(NITF_BOOL) basicInit(nitf_TRE * tre, const char* id, nitf_Error * error)
{
	nitf_TREDescriptionSet* set;
	const nitf_TREDescriptionInfo* desc;

	assert(tre);
	
	set = (nitf_TREDescriptionSet*)tre->handler->data;
	
	do 
	{
		desc = set->descriptions;
		assert(desc);
		if (strcmp(desc->name, id) == 0)
		{
			/* we have a match! */
			 
			if (nitf_TREUtils_fillData(tre, desc->description, error) )
				return NITF_SUCCESS;
			else 
			{
				nitf_TRE_destruct(&tre);
				return NITF_FAILURE;
			}
			
		}
		

	}
	while (desc != NULL);

  
	nitf_Error_initf(error, NITF_CTXT, NITF_ERR_INVALID_OBJECT, 
		"No matching id '%s' found!", id);
    return NITF_FAILURE;
}

NITFPRIV(NITF_BOOL) basicWrite(nitf_IOHandle ioHandle, nitf_TRE* tre, 
							   struct _nitf_Record* record, nitf_Error* error)
{
	int length;
	char* data = nitf_TREUtils_getRawData(tre, &length, error);
	if (!data) return NITF_FAILURE;
	return nitf_IOHandle_write(ioHandle, data, length, error);
}

NITFPRIV(int) basicGetCurrentSize(nitf_TRE* tre, nitf_Error* error)
{
	return nitf_TREUtils_computeLength(tre);
}


NITFPRIV(nitf_List*) basicFind(nitf_TRE* tre, const char* tag, nitf_Error* error)
{
    nitf_List* list;
    nitf_Pair* pair = nitf_HashTable_find(tre->hash, tag);
    if (!pair) return NULL;
    list = nitf_List_construct(error);
    if (!list) return NULL;
    nitf_List_pushBack(list, pair, error);
    return list;
}

NITFPRIV(NITF_BOOL) basicSetField(nitf_TRE* tre, const char* tag, NITF_DATA* data, size_t dataLength, nitf_Error* error)
{
	return nitf_TREUtils_setValue(tre, tag,  data, dataLength, error);
}


NITFPRIV(NITF_BOOL) basicIncrement(nitf_TREEnumerator** it, nitf_Error* error)
{
	// Free this baby
	nitf_TRECursor* cursor = (nitf_TRECursor*)(*it)->data;
	
	if (nitf_TRECursor_isDone(cursor))
	{
		nitf_TRECursor_cleanup(cursor);
		NITF_FREE(cursor);
		NITF_FREE(*it);
		*it = NULL;
		return NITF_SUCCESS;	
	}

	
	return nitf_TRECursor_iterate(cursor, error);
	

}

NITFPRIV(nitf_Pair*) basicGet(nitf_TREEnumerator* it, nitf_Error* error)
{
	nitf_TRECursor* cursor = (nitf_TRECursor*)it->data;
	nitf_Pair* data;
	
	if (!nitf_TRE_exists(cursor->tre, cursor->tag_str))
		goto CATCH_ERROR;

	data = nitf_HashTable_find(cursor->tre->hash, cursor->tag_str);
	if (!data)
		goto CATCH_ERROR;

	return data;

CATCH_ERROR:
	nitf_Error_initf(error, NITF_CTXT, NITF_ERR_INVALID_OBJECT, "Couldnt retrieve tag [%s]", cursor->tag_str);
		return NULL;

	
}

NITFPRIV(nitf_TREEnumerator*) basicBegin(nitf_TRE* tre, nitf_Error* error)
{
	
	nitf_TREEnumerator* it = (nitf_TREEnumerator*)NITF_MALLOC(sizeof(nitf_TREEnumerator));
	nitf_TRECursor* cursor = (nitf_TRECursor*)NITF_MALLOC(sizeof(nitf_TRECursor));
	*cursor = nitf_TRECursor_begin(tre);
	assert(nitf_TRECursor_iterate(cursor, error));
	it->data = cursor;
	it->next = basicIncrement;
	it->get = basicGet;
	return it;
	
}


NITFAPI(nitf_TREHandler*) nitf_TREUtils_createBasicHandler(nitf_TREDescriptionSet* set, nitf_Error* error)
{
	

	static nitf_TREHandler handler = {
		basicInit,
		basicRead,
		basicSetField,
		basicFind,
		basicWrite,
		basicBegin,
		basicGetCurrentSize,
		NULL
	};
	

	handler.data = set;
	return &handler;
}
