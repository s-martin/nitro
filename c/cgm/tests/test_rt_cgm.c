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
 * License along with this program;
 * If not, see <http://www.gnu.org/licenses/>.
 *
 */


#include <import/nitf.h>
#include <import/cgm.h>

/*!
 *  This program reads only a CGM metafile subset that pertains to the
 *  NITF MIL-STD (2301A).  For a NITF file reading example, see
 *  test_read_nitf_cgm.c
 */

int main(int argc, char** argv)
{
    nitf_Error error;
    nitf_IOHandle in;
    nitf_IOHandle out;
    cgm_Metafile* mf = NULL;
    cgm_MetafileReader* reader;
    cgm_MetafileWriter* writer;

    if (argc != 3)
    {
	printf("Usage: %s <Version 1 CGM> <out-file>\n", argv[0]);
	exit(EXIT_FAILURE);
    }
    
    reader = cgm_MetafileReader_construct(&error);
    assert(reader);

    writer = cgm_MetafileWriter_construct(&error);
    assert(writer);
    
    in = nitf_IOHandle_create(argv[1], 
			      NITF_ACCESS_READONLY,
			      NITF_OPEN_EXISTING, &error);
    
    out = nitf_IOHandle_create(argv[2],
                               NITF_ACCESS_WRITEONLY,
                               NITF_CREATE, &error);

    if (NITF_INVALID_HANDLE(in))
    {
	nitf_Error_print(&error, stdout, "Input file.  Exiting...");
	goto END_OF_FILE;
    }

    if (NITF_INVALID_HANDLE(out))
    {
	nitf_Error_print(&error, stdout, "Output file.  Exiting...");
	goto END_OF_FILE;
    }
    
    mf = cgm_MetafileReader_read(reader, in, &error);
    if (!mf)
    {
	nitf_Error_print(&error, stdout, "Read file. Exiting...");
	goto END_OF_FILE;

    }

    assert(cgm_MetafileWriter_write(writer, mf, out, &error));

    cgm_Metafile_destruct(&mf);
    

END_OF_FILE:
    nitf_IOHandle_close(in);
    nitf_IOHandle_close(out);
    cgm_MetafileReader_destruct(&reader);
    assert(!reader);
    return 0;
}