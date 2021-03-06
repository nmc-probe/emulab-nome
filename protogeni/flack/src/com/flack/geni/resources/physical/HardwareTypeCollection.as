/*
 * Copyright (c) 2008-2013 University of Utah and the Flux Group.
 * 
 * {{{GENIPUBLIC-LICENSE
 * 
 * GENI Public License
 * 
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and/or hardware specification (the "Work") to
 * deal in the Work without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Work, and to permit persons to whom the Work
 * is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Work.
 * 
 * THE WORK IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE WORK OR THE USE OR OTHER DEALINGS
 * IN THE WORK.
 * 
 * }}}
 */

package com.flack.geni.resources.physical
{
	/**
	 * Collection of hardware types
	 * @author mstrum
	 * 
	 */
	public class HardwareTypeCollection
	{
		public var collection:Vector.<HardwareType>;
		public function HardwareTypeCollection()
		{
			collection = new Vector.<HardwareType>();
		}
		
		public function add(ht:HardwareType):void
		{
			collection.push(ht);
		}
		
		public function remove(ht:HardwareType):void
		{
			var idx:int = collection.indexOf(ht);
			if(idx > -1)
				collection.splice(idx, 1);
		}
		
		public function contains(ht:HardwareType):Boolean
		{
			return collection.indexOf(ht) > -1;
		}
		
		public function get length():int
		{
			return collection.length;
		}
		
		/**
		 * 
		 * @param name Hardware type name
		 * @return Hardware type matching the name
		 * 
		 */
		public function getByName(name:String):HardwareType
		{
			for each(var ht:HardwareType in collection)
			{
				if(ht.name == name)
					return ht;
			}
			return null;
		}
	}
}