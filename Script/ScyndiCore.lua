-- <License Block>
-- Script/ScyndiCore.lua
-- Scyndi - Core Script
-- version: 22.12.21
-- Copyright (C) 2022 Jeroen P. Broks
-- This software is provided 'as-is', without any express or implied
-- warranty.  In no event will the authors be held liable for any damages
-- arising from the use of this software.
-- Permission is granted to anyone to use this software for any purpose,
-- including commercial applications, and to alter it and redistribute it
-- freely, subject to the following restrictions:
-- 1. The origin of this software must not be misrepresented; you must not
-- claim that you wrote the original software. If you use this software
-- in a product, an acknowledgment in the product documentation would be
-- appreciated but is not required.
-- 2. Altered source versions must be plainly marked as such, and must not be
-- misrepresented as being the original software.
-- 3. This notice may not be removed or altered from any source distribution.
-- </License Block>
 -- Script --


-- ***** Core Init ***** --
Scyndi = {}
local _Scyndi = {}
_Scyndi.SETTINGS={ STRICTNUM=true }
setmetatable(Scyndi,{
	__newindex = function(self,key,value)
		key=key:upper()		
		error("Cannot assign "..tostring(value).." to key "..tostring(key)..". Either non-existent or read-only");
		return
	end,
	__index = function(self,key)
		key=key:upper()
		if _Scyndi[key] then return _Scyndi[key] end
		error("Scyndi has no core feature named "..key)
		return nil
	end

})
-- ***** Bas Values ***** --
function _Scyndi.BASEVALUE(dtype)
	dtype=dtype:upper()
	if (dtype=="NUMBER" or dtype=="INT" or dtype=="BYTE") then
		return 0
	elseif (dtype=="BOOLEAN" or dtype=="BOOL") then
		return false
	elseif (dtype=="STRING") then
		return ""
	else
		return nil
	end
end

function _Scyndi.WANTVALUE(dtype,value)
	dtype=dtype:upper()
	if (dtype=="BYTE") then
		if _Scyndi.SETTINGS.STRICTNUM then assert(type(value)=="number","Byte expected but got ("..type(value)..")") end
		if type(value)=="string" then value=value:tonumber() or 0 end
		if _Scyndi.SETTINGS.STRICTINT then assert(math.floor(value)~=value,"Byte expected value is not integer typed") end
		if _Scyndi.SETTINGS.STRICTBYTE then assert(value>=0 and value<=255,"Byte expected, but the value exceeded the range") end
		return math.floor(value) % 256
	elseif dtype=="BOOLEAN" or dtype=="BOOL" then
		return value~=nil and value~=false and value~="" and value~=0
	elseif dtype=="STRING" then
		if _Scyndi.SETTINGS.STRICTSTRING then assert(type(value)=="string","String expected but got ("..type(value)..")") end
		return _Scyndi.TOSTRING(value)
	elseif dtype=="VAR" or dtype=="PLUA" then
		return value
	elseif dtype:sub(1,1)=="@" then
		error("ClassType checking not yet impelemted!")
	elseif dtype=="TABLE" then
		assert(value==nil or type(value)=="table","Table expected but got ("..type(value)..")")
		return value
	elseif dtype=="FUNCTION" or dtype=="DELEGATE" then
		assert(value==nil or type(value)=="function","Delegate expected but got ("..type(value)..")")
		return value
	else
		error("Unknown type "..dtype)
	end
end

function _Scyndi.TOSTRING(v)
	if type(v)=="table" and v[".classinstance"] then
		if v[".hasmember"]("TOSTRING") then return v.TOSTRING(v) end
	end
	return tostring(v)
end

-- ***** Identifiers ***** --
local Identifier = {}

-- ***** Class Functions ***** --
local classregister = {}

local function index_static_member(cl,key,allowprivate)
	local cu=cl:upper()
	key=key:upper()
	assert(classregister[cu],"Class "..cl.." unknown (index)")
	assert(classregister[cu].staticmembers[key],"Class "..cl.." has no static member named "..key)
	local member=classregister[cu].staticmembers[key]
	if (not allowprivate) then assert(not member.private,"Class "..cl.." does have a static member named "..key..", however it's private and cannot be called this way.") end
	if member.kind=="PROPERTY" then
		assert(member.propget,"Property "..key.." in class "..cl.." does not have a 'Get' function")
		return member.propget(classregister[cu].priv)
	else
		return member.value
	end
end

local function newindex_static_member(cl,key,value,allowprivate)
	key=key:upper()
	assert(classregister[cu],"Class "..cl.." unknown")
	assert(classregister[cu].staticmembers[key],"Class "..cl.." has no static member named "..key)
	local member=classregister[cu].staticmembers[key]
	if (not allowprivate) then assert(not member.private,"Class "..cl.." does have a static member named "..key..", however it's private and cannot be called this way.") end
	if member.kind=="PROPERTY" then
		assert(member.propset,"Property "..key.." in class "..cl.." does not have a 'Set' function")
		member.propset(classregister[cu].priv,value)
	end
	member.value = _Scyndi.WANTVALUE(member.dtype,value)
end


function _Scyndi.STARTCLASS(classname,staticclass,sealable,extends)
	local _class = { 
		name=classname, 
		staticclass=staticclass, 
		sealed=false, 
		sealable=sealable,
		staticmembers={},
		nonstaticmembers={},
		pub={},
		priv={}
	}	
	local _static=_class.staticmembers
	local _nonstatic=_class.nonstaticmembers
	local cu = classname:upper()
	assert(not(Identifier[cu] or classregister[cu]),"Class has dupe name")	
	if (extends) then
		extends:upper()
		assert(classregister[extends],"Extending non existent class "..extends)
		_class.extends=extends
		for k,v in pairs(classregister[extends].staticmembers) do _static[k]=v end
		for k,v in pairs(classregister[extends].nonstaticmembers) do _snontatic[k]=v end
	end
	local meta={}
	local metapriv={}
	function meta.__index(self,key)
		if key:upper()==".ISCLASS" then return true end
		return index_static_member(cu,key)
	end
	function meta.__newindex(self,key,value)
		return newindex_static_member(cu,key,value)
	end
	function metapriv.__index(self,key)
		if key:upper()==".ISCLASS" then return true end
		return index_static_member(cu,key,true)
	end
	function metapriv.__newindex(self,key,value)
		return newindex_static_member(cu,key,value,true)
	end
	local ret = setmetatable(_class.pub,meta)
	local retpriv = setmetatable(_class.priv,metapriv)
	classregister[cu]=_class
	--print(string.format("Class %s has been registered",cu))
	return ret,retpriv
end




function _Scyndi.ADDMBER(ch,dtype,name,static,readonly,constant,value)
	local cu=ch:upper()
	name=name:upper()	
	assert(classregister[cu],"Class "..cu.." unknown (member addition)")
	local _class=classregister[cu]
	assert(not _class.sealed,"Class "..cu.." is already sealed. No new members allowed!")
	if (_class.staticclass) then status=true end
	assert(not _class.staticmembers[name],"Class "..ch.." already has a static member named "..name)
	-- TODO: Override abstract
	assert(not _class.nonstaticmembers[name],"Class "..ch.." already has a member named "..name)
	local nm={
		dtype=dtype,
		readonly=readonly,
		constant=constant,
		value=_Scyndi.WANTVALUE(dtype,value or _Scyndi.BASEVALUE(dtype))
	}
	if (static) then _class.staticmembers[name]=nm else _class.nonstaticmembers[name]=nm end
end

function _Scyndi.SEAL(ch)
	ch=ch:upper()
	assert(classregister[cu],"Class "..cl.." unknown")
	assert(classregister[cu].sealable,"Class "..cl.." is NOT sealable")
	classregister[cu].sealed=true
end

-- ***** Locals Definition Functions ***** --
	
local met = {}
function met.__index(s,key)
	key = key:upper()		
	assert(s.truelocals[key],"G:Local "..key.." not found")
	return s.truelocals[key].value
end
function met.__newindex(s,key,value)
	key = key:upper()		
	assert(s.truelocals[key],"S:Local "..key.." not found")
	local tl=s.truelocals[key]
	assert(not tl.readonly,"Local "..key.." is read-only")
	tl.value = _Scyndi.WANTVALUE(tl.dtype,value)
end

function _Scyndi.CREATELOCALS()
	local ret = { truelocals = {} }
	ret = setmetatable(ret,met)
	return ret
end

function _Scyndi.DECLARELOCAL(tab,dtype,readonly,key,value)
	key=key:upper()
	assert(not s.truelocals[key],"Dupe local: "..key)
	s.truelocals[key] = {
		value = _Scyndi.WANTVALUE(value or _Scyndi.BASEVALUE(dtype)),
		dtype = dtype:upper(),
		readonly = readonly,		
	}
end

-- ***** Base Globals ***** --
local _Glob,_GlobPriv = _Scyndi.STARTCLASS("..GLOBALS..",true,false,nil)
assert(_Glob,"SCYNDI CORE SCRIPT INTERNAL ERROR: Globals not properly initiated!\nPlease report to Jeroen P. Broks!")
_Scyndi.GLOBALS = _Glob

_Scyndi.ADDMBER("..GLOBALS..","Delegate","PRINT",true,true,true,print)

_Scyndi.ADDMBER("..GLOBALS..","DELEGATE","SOUT",true,true,true,function(...) 
	local ret = ""
	for _,v in ipairs{...} do ret = ret .. _Glob.TOSTRING(v) end
	return ret
end)

_Scyndi.ADDMBER("..GLOBALS..","DELEGATE","TOSTRING",true,true,true,_Scyndi.TOSTRING)
_Scyndi.ADDMBER("..GLOBALS..","DELEGATE","COUT",true,true,true,function(...) io.write(_Glob.SOUT(...)) end)
_Scyndi.ADDMBER("..GLOBALS..","STRING","ENDL",true,true,true,"\n")
_Scyndi.ADDMBER("..GLOBALS..","Delegate","SPRINTF",true,true,true,string.format)
_Scyndi.ADDMBER("..GLOBALS..","DELEGATE","PRINTF",true,true,true,function(fmt,...) io.write(fmt:format(...)) end)


-- ***** C++ Generator for base globals so the compiler will know them ***** --
function _Scyndi.GLOBALSFORCPLUSPLUS()
	print("// Please note that this code is generated (also the reason why you can't find it in the respository)\n")
	print("// Generated "..os.date())
	print("\n\n#include <map>\n#include <string>\n\n")
	print("namespace Scyndi {")
	print("\tstd::map<std::string,std::string> CoreGlobals {\n")
	local d
	for k,v in pairs(classregister["..GLOBALS.."].staticmembers) do
		if d then print(",") end d = true
		io.write(string.format("\t\t{\"%s\", \"Scyndi.Globals[\\\"%s\\\"]\"}",k,k))
	end
	print("\n\t};\n}\n")
end