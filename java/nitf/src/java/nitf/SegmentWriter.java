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

package nitf;

/**
 * The handle that gives access to writing segments
 */
public final class SegmentWriter extends DestructibleObject
{

    /**
     * @see DestructibleObject#DestructibleObject(long)
     */
    SegmentWriter(long address)
    {
        super(address);
    }

    /**
     * Destructs the object
     */
    protected native synchronized void destructMemory();

    /**
     * Returns the IOHandle associated with this SegmentWriter
     * 
     * @return
     */
    public native synchronized IOHandle getOutputHandle();

    /**
     * Returns the ImageSource
     * 
     * @return the ImageSource associated with this ImageWriter
     * @throws NITFException
     */
    public native synchronized SegmentSource getSegmentSource()
            throws NITFException;

    /**
     * Attaches the specified SegmentSource to this SegmentWriter
     * 
     * @param segmentSource
     *            the SegmentSource to attach
     * @return true if it attached successfully, false otherwise
     * @throws NITFException
     *             if a SegmentSource has already been attached, or if an error
     *             occurs
     */
    public native synchronized boolean attachSource(SegmentSource segmentSource)
            throws NITFException;

}
