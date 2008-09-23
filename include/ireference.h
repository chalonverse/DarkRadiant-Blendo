/*
Copyright (C) 2001-2006, William Joseph.
All Rights Reserved.

This file is part of GtkRadiant.

GtkRadiant is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

GtkRadiant is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GtkRadiant; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#if !defined(INCLUDED_IREFERENCE_H)
#define INCLUDED_IREFERENCE_H

#include <boost/shared_ptr.hpp>

namespace scene {
	class INode;
	typedef boost::shared_ptr<INode> INodePtr;
} // namespace scene

class Resource
{
public:
	class Observer {
	public:
		virtual void onResourceRealise() = 0;
		virtual void onResourceUnrealise() = 0;
	};
	
	virtual bool load() = 0;
	virtual bool save() = 0;
	virtual void flush() = 0;
	virtual void refresh() = 0;
	virtual scene::INodePtr getNode() = 0;
	virtual void setNode(scene::INodePtr node) = 0;
	
	virtual void addObserver(Observer& observer) = 0;
	virtual void removeObserver(Observer& observer) = 0;
	
	virtual void realise() = 0;
	virtual void unrealise() = 0;
};
typedef boost::shared_ptr<Resource> ResourcePtr;

#endif
