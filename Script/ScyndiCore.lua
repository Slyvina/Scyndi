-- License:
-- 	Script/ScyndiCore.lua
-- 	Scyndi - Core Script
-- 	version: 25.01.18 VI
-- 
-- 	Copyright (C) 2022, 2023, 2024, 2025 Jeroen P. Broks
-- 
-- 	This software is provided 'as-is', without any express or implied
-- 	warranty.  In no event will the authors be held liable for any damages
-- 	arising from the use of this software.
-- 
-- 	Permission is granted to anyone to use this software for any purpose,
-- 	including commercial applications, and to alter it and redistribute it
-- 	freely, subject to the following restrictions:
-- 
-- 	1. The origin of this software must not be misrepresented; you must not
-- 	   claim that you wrote the original software. If you use this software
-- 	   in a product, an acknowledgment in the product documentation would be
-- 	   appreciated but is not required.
-- 	2. Altered source versions must be plainly marked as such, and must not be
-- 	   misrepresented as being the original software.
-- 	3. This notice may not be removed or altered from any source distribution.
-- End License


 -- Script --


-- ***** Core Init ***** --
Scyndi = {}
local _Scyndi = {}
local SETTINGS={ STRICTNUM=true }
local substr = string.sub

_Scyndi.SETTINGS={}
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

setmetatable(_Scyndi.SETTINGS,{
	__newindex = function(self,key,value)
		SETTINGS[key:upper()]=value
	end,
	__index = function(self,key)
		return SETTINGS[key:upper()]
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
	--print("Want value for "..dtype.." -> ",value,type(value)) -- debug only
	if (dtype=="BYTE") then
		if _Scyndi.SETTINGS.STRICTNUM then assert(type(value)=="number","Byte expected but got ("..type(value)..")") end
		if type(value)=="string" then value=value:tonumber() or 0 end
		if _Scyndi.SETTINGS.STRICTINT then assert(math.floor(value)~=value,"Byte expected value is not integer typed") end
		if _Scyndi.SETTINGS.STRICTBYTE then assert(value>=0 and value<=255,"Byte expected, but the value exceeded the range") end
		return math.floor(value) % 256
	elseif dtype=="NUMBER" then
		if _Scyndi.SETTINGS.STRICTNUM then assert(type(value)=="number","Number expected but got ("..type(value)..")") end
		if type(value)=="string" then value=value:tonumber() or 0 end
		return value
	elseif dtype=="INT" or dtype=="INTEGER" then
		--print("1:Integer wanted",value,type(value))
		if _Scyndi.SETTINGS.STRICTNUM then assert(type(value)=="number","Integer expected but got ("..type(value)..")") end
		if type(value)=="string" then value=value:tonumber() or 0 end
		--print("2:Integer wanted",value,type(value))
		if _Scyndi.SETTINGS.STRICTINT then assert(math.floor(value)~=value,"integer expected value is not integer typed") end
		--print( "INTEGER value now ",value) -- debug only
		return math.floor(value)
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
		--for k,v in pairs(classregister[cu].staticprop.pget) do print(cl,k,type(v)) end -- debug only!
		if classregister[cu].staticprop.pget[key] then
		return classregister[cu].staticprop.pget[key](classregister[cu].pub)
	end
	if key==".HASMEMBER" then
		return function (m)
			return classregister[cu].staticmembers[m:upper()]~=nil
		end
	end
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
	-- print("Static define:",cl,key,value) -- StaticNewIndex
	local cu=cl:upper()
	key=key:upper()
	assert(classregister[cu],"Class "..cl.." unknown")
	if classregister[cu].staticprop.pset[key] then
		classregister[cu].staticprop.pset[key](classregister[cu].pub,value)
		return
	end

	assert(classregister[cu].staticmembers[key],"Class "..cl.." has no static member named "..key)
	local member=classregister[cu].staticmembers[key]
	if (not allowprivate) then assert(not member.private,"Class "..cl.." does have a static member named "..key..", however it's private and cannot be called this way.") end
	--if member.kind=="PROPERTY" then
	--	assert(member.propset,"Property "..key.." in class "..cl.." does not have a 'Set' function")
	--	member.propset(classregister[cu].priv,value)
	--end
	member.value = _Scyndi.WANTVALUE(member.dtype,value)
end

local function tabcpy(ori)
	local tar = {}
	for k,v in pairs(ori) do
		if type(v)=="table" then 
			tar[k] = tabcpy(v)
		else
			tar[k] = v
		end
	end
	return tar
end

-- Perform Extend Class
local function PEC(classname)
	local cu = classname:upper()
	local _class = classregister[cu]
	assert(_class,"Cannot perform extend on non-existent class: "..classname)
	if (_class.extended) then return end -- Don't extend again if that's already done
	if _class.extendclass then
		local uex = _class.extendclass:upper()
		assert(classregister[uex],"Extending non-existing class: ".._class.extendclass)
		local base = classregister[uex]
		base.sealed=true -- extending a class must seal the base. 
		_class.extends = { base=base, extname=uex }
		for _,k in pairs{"staticmembers","nonstaticmembers","methods","staticprop","methprop","abstracts","finals"} do
			print(string.format("Extending - copy %s from %s to %s",k,_class.extendclass,classname)) -- debug only
			_class[k] = tabcpy(base[k])
			for mk,v in pairs(_class[k]) do print("\t",type(v),mk) end
			_class.extended = true
		end
	end
end


function _Scyndi.STARTCLASS(classname,staticclass,sealable,extends)
	local _class = { 
		name=classname, 
		staticclass=staticclass, 
		sealed=false, 
		sealable=sealable,
		staticmembers={},
		nonstaticmembers={},
		methods={},
		staticprop={pget={},pset={}},
		methprop={pget={},pset={}},
		pub={},
		priv={},
		abstracts={},
		finals={},
		extends={},
		extendclass=extends,
		extended=false,		
	}		
	--[[
	if extends then
		local uex = extends:upper()
		assert(classregister[uex],"Extending non-existing class: "..extends)
		local base = classregister[uex]
		_class.extends = { base=base, extname=uex }
		for _,k in pairs{"staticmembers","nonstaticmembers","methods","staticprop","methprop","abstracts","finals"} do
			print(string.format("Extending - copy %s from %s to %s",k,extends,classname)) -- debug only
			_class[k] = tabcpy(base[k])
			for mk,v in pairs(_class[k]) do print("\t",type(v),mk) end
		end
	end
	]]
	local _static=_class.staticmembers
	local _nonstatic=_class.nonstaticmembers
	local cu = classname:upper()
	assert(not(Identifier[cu] or classregister[cu]),"Class has dupe name: "..classname)	
	--if (extends) then
	--	extends = extends:upper()
	--	assert(classregister[extends],"Extending non existent class "..extends)
	--	_class.extends=extends
	--	for k,v in pairs(classregister[extends].staticmembers) do _static[k]=v end
	--	for k,v in pairs(classregister[extends].nonstaticmembers) do _snontatic[k]=v end
	--end
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

_Scyndi.CLASSES = setmetatable({},{
	__newindex=function(s,cl,v)
		print(string.format("Error! Trying to write value %s(%s) to class %s!",v,type(v),cl)
		error("Scyndi.Classes is read-only!") end,
	__index=function(s,key) 
		key = key:upper()
		assert(classregister[key],"No class named "..key.." found")
		return classregister[key].pub
	end	})

_Scyndi.CLASS = _Scyndi.CLASSES -- Laziness, but it should fix (read: void) countless issues!

function _Scyndi.ADDPROPERTY(ch,name,static,getset,func)
	local cu=ch:upper()
	name=name:upper()
	assert(classregister[cu],"Class "..cu.." unknown (member addition)")
	--PEC(cu)
	local _class=classregister[cu]
	assert(not _class.sealed,"Class "..cu.." is already sealed. No new members allowed!")
	local _prop
	if static then _prop=_class.staticprop else _prop=_class.methprop end
	local gs = ("p"..getset):lower()
	assert(not _class.staticprop[gs][name],"There already is a '"..name.."' function for static property "..name)
	assert(not _class.methprop[gs][name],"There already is a '"..name.."' function for property "..name)
	assert(not _class.staticmembers[gs],"Property duplicates static member "..name)
	assert(not _class.nonstaticmembers[gs],"Property duplicate member "..name)
	_prop[gs][name]=func
	--print("Property "..name.." added to class "..ch) -- debug
end

function _Scyndi.ADDMBER(ch,dtype,name,static,readonly,constant,value)
	local cu=ch:upper()
	name=name:upper()	
	assert(classregister[cu],"Class "..cu.." unknown (member addition)")
	--PEC(cu)
	local _class=classregister[cu]
	assert(not _class.sealed,"Class "..cu.." is already sealed. No new members allowed!")
	if (_class.staticclass) then status=true end
	assert(not _class.staticmembers[name],"Class "..ch.." already has a static member named "..name)
	assert(not _class.abstracts[name],"Abstract "..name.." can only be overridden with a method!") -- Override abstract not possible with members
	assert(not _class.nonstaticmembers[name],"Class "..ch.." already has a member named "..name)
	local nm={
		dtype=dtype,
		readonly=readonly,
		constant=constant,
		value=_Scyndi.WANTVALUE(dtype,value or _Scyndi.BASEVALUE(dtype))
	}
	--for k,v in pairs(nm) do print(k,v) end
	-- print("Member for ",cu,">",name," Static:",static)
	if (static) then _class.staticmembers[name]=nm else _class.nonstaticmembers[name]=nm end
end

function _Scyndi.ADDABSTRACT(ch,dtype,_name)
	local cu=ch:upper()
	local name=_name:upper()
	PEC(cu)
	assert(classregister[cu],"Class "..cu.." unknown (abstract addition)")
	local _class=classregister[cu]
	assert(not _class.sealed,"Class "..cu.." is already sealed. No new abstracts allowed!")
	assert(not _class.staticmembers[name],"Class "..ch.." already has a static member named "..name)
	assert(not _class.nonstaticmembers[name],"Class "..ch.." already has a member named "..name)
	assert(not _class.abstracts[name],"Class "..ch.." already has an abstract named "..name)
	_class.abstracts[name]=dtype:upper()
end

function _Scyndi.ADDMETHOD(ch,name,IsFinal,func)
local cu=ch:upper()
	PEC(cu)
	name=name:upper()	
	assert(classregister[cu],"Class "..cu.." unknown (member addition)")
	assert(not classregister[cu].sealed,"Class "..cu.." is sealed. No methods can be added anymore")
	local _class=classregister[cu]
	assert(type(func)=="function","That is not a function, so it cannot be turned into a method")
	assert( not ( _class.methods[name] and _class.methods[name].IsFinal ), "Final method cannot be overridden")
	--print("New Method:",name," abstract: "..tostring(_class.abstracts[name]))
	if (_class.abstracts[name]) then
		--print("Abstract "..name.." removed from class: "..ch)
		_class.abstracts[name]=nil 
	end -- abstract overridden after all!
	_class.methods[name] = { IsAbstract=false, IsFinal=IsFinal, Meth = func }
end

function _Scyndi.SEAL(ch)
	local cu=ch:upper()
	assert(classregister[cu],"Class "..cu.." unknown")	
	assert(classregister[cu].sealable,"Class "..cu.." is NOT sealable")
	PEC(cu)
	classregister[cu].sealed=true
	_Scyndi.ADDMBER("..GLOBALS..","TABLE",cu,true,true,true,classregister[cu].pub)
end

local function InstanceIndex(self,key)
	--print("Want: ",key,v)
	if type(key)=="number" then
		return self.NumIndex(key)
	end
	key=key:upper()
	assert(key~="CONSTRUCTOR","Illegal constructor call")
	assert(key~="DESTRUCTOR","Illegal destructor call")
	if key==".CLASSINSTANCE" or key==".ISCLASSINSTANCE" then return true end
	if key==".CLASSNAME" then return self[".TiedToClass"].CL.name end
	if key==".ISCLASS" then return false end
	if key==".HASMEMBER" then
		return function(key)
			if self[".Methods"][key] then return true end
			if self[".TiedToClass"].CR.staticmembers[key] then return true end
			if self[".TiedToClass"].CR.nonstaticmembers[key] then return true end
			if self[".TiedToClass"].CR.methprop.pget[key] then return true end
			if self[".TiedToClass"].CR.staticprop.pget[key] then return true end
			return false
		end
	end
	if self[".Methods"][key] then return function(...) return self[".Methods"][key](self,...) end end
	local TTC = self[".TiedToClass"]
	if self[".TiedToClass"].CR.staticmembers[key] then return index_static_member(self[".TiedToClass"].CH,key) end
	if self[".TiedToClass"].CR.nonstaticmembers[key] then return self[".InstanceValues"][key] end
	if self[".TiedToClass"].CR.methprop.pget[key] then return TTC.CR.methprop.pget[key](self) end	
	if self[".TiedToClass"].CR.staticprop.pget[key] then return TTC.CR.staticprop.pget[key](self) end	
	print(debug.traceback()) -- ?
	error("R:Class "..TTC.CH.." does not have a member named "..key)
end

local function InstanceNewIndex(self,key,value)
	-- print("InstanceNewIndex",self,key,value) -- ???
	assert(key,"Nil received for key")
	key=key:upper()
	local TTC = self[".TiedToClass"]
	-- print(TTC.CH,"Mem:"..key,"Static:",TTC.CR.staticmembers[key],"NonStatic:",TTC.CR.nonstaticmembers[key]) -- InstanceNewIndex Debug
	if self[".Methods"][key] then error("Cannot overwrite methods") end
	if TTC.CR.staticmembers[key] then newindex_static_member(self[".TiedToClass"].CH,key,value); return; end
	if TTC.CR.nonstaticmembers[key] then
		local NSM=TTC.CR.nonstaticmembers[key]
		assert(not NSM.constant,"Constants cannot be overwritten")
		if self[".sealed"] then assert(not NSM.constant,"Read-only members cannot be overwritten") end
		self[".InstanceValues"][key] = _Scyndi.WANTVALUE(NSM.dtype,value)
		return
	end
	if TTC.CR.methprop.pset[key] then TTC.CR.methprop.pset[key](self,value) return end
	if TTC.CR.staticprop.pset[key] then TTC.CR.staticprop.pset[key](self,value) return end
	error("W:Class "..TTC.CH.." does not have a member named "..key)
end

function _Scyndi.NEW(ch,...)
	ch=ch:upper()
	assert(classregister[ch],"Class "..ch.." unknown (new object)")
	local _class = classregister[ch]
	local Ret = {
		--[".TrueInstance"]={}, 
		[".InstanceValues"]={},
		[".Methods"]={},
		[".TiedToClass"]={ CL=_Scyndi.CLASS[ch],CR=classregister[ch],CH=ch },
		[".sealed"] = false
	}
	for ab,_ in pairs(_class.abstracts) do
		error("Abstract "..ab.." found in class "..ch.."! Not possible to create new instance until all abstracts have been overridden!")
	end 
	for MK,MF in pairs(_class.methods) do
		if MF.IsAbstract then error("Class "..ch.." contains abstracts") end
		-- for MFK,MFV in pairs(MF) do io.write(type(MFV)," ",MK,".",MFK," -> ",tostring(MFV),"\n") end -- debug
		Ret[".Methods"][MK] = MF.Meth
	end
	for FK,FV in pairs(_class.nonstaticmembers) do
		-- print("Non-Static",FK,FV) -- debug
		Ret[".InstanceValues"][FK]=FV.value
	end
	setmetatable(Ret,{
		__index=InstanceIndex,
		__newindex=InstanceNewIndex,
		__gc=function(self) if (Ret[".Methods"].DESTRUCTOR) then Ret[".Methods"].DESTRUCTOR(self) end end
	})
	if (Ret[".Methods"].CONSTRUCTOR) then Ret[".Methods"].CONSTRUCTOR(Ret,...) end
	Ret[".sealed"]=true
	return Ret
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
	assert(not tab.truelocals[key],"Dupe local: "..key)
	--print("Local: ",tab," dtype:",dtype," readonly:",readonly," key:",key," value:",value) -- debug
	tab.truelocals[key] = {
		value = _Scyndi.WANTVALUE(dtype,value or _Scyndi.BASEVALUE(dtype)),
		dtype = dtype:upper(),
		readonly = readonly,		
	}
end

-- ***** Base Globals ***** --
local _Glob,_GlobPriv = _Scyndi.STARTCLASS("..GLOBALS..",true,false,nil)
assert(_Glob,"SCYNDI CORE SCRIPT INTERNAL ERROR: Globals not properly initiated!\nPlease report to Jeroen P. Broks!")
_Scyndi.GLOBALS = _Glob

_Scyndi.ADDMBER("..GLOBALS..","Delegate","Range",true,true,true,function(start,einde,stap)
	if einde<start then 
		stap = -math.abs(stap or 1)
	else
		stap = math.abs(stap or 1)
	end
	local i = start
	return function()
		local r = i
		i = i + stap
		if     einde<start and i<=einde then return nil
		elseif i>=einde then return nil
		else   return r end
	end
end)

_Scyndi.ADDMBER("..GLOBALS..","Delegate","INVOKE",true,true,true,function(func,...)
	if func then return true,func(...) else return false end
end)

_Scyndi.ADDMBER("..GLOBALS..","Delegate","PRINT",true,true,true,print)

_Scyndi.ADDMBER("..GLOBALS..","DELEGATE","SOUT",true,true,true,function(...) 
	local ret = ""
	for _,v in ipairs{...} do ret = ret .. _Glob.TOSTRING(v) end
	return ret
end)

_Scyndi.ADDMBER("..GLOBALS..","DELEGATE","ERROR",true,true,true,error)

_Scyndi.ADDMBER("..GLOBALS..","DELEGATE","LEFT",true,true,true, function(s, l) 
			if not assert(type(s)=="string","String exected as first argument for 'left'") then return end
			l = l or 1
			assert(type(l)=="number","Number expected for second argument in 'left'")
			return substr(s,1,l)
		end)
_Scyndi.ADDMBER("..GLOBALS..","DELEGATE","RIGHT",true,true,true,function(s,l)
			local ln
			local st
			ln = l or 1
			st = s or "nostring"
			return substr(st,-ln,-1)
		end)
_Scyndi.ADDMBER("..GLOBALS..","DELEGATE","MID",true,true,true,function(s,o,l)
			local ln
			local of
			local st
			ln=l or 1
			of=o or 1
			st=s or ""
			return substr(st,of,(of+ln)-1)
		end)
_Scyndi.ADDMBER("..GLOBALS..","Delegate","PREFIXED",true,true,true,function(str,pref)
	return _Scyndi.GLOBALS.Left(str,#pref)==pref
end)
_Scyndi.ADDMBER("..GLOBALS..","Delegate","SUFFIXED",true,true,true,function(str,suff)
	return _Scyndi.GLOBALS.Right(str,#suff)==suff
end)

_Scyndi.ADDMBER("..GLOBALS..","DELEGATE","UPPER",true,true,true,string.upper)
_Scyndi.ADDMBER("..GLOBALS..","DELEGATE","LOWER",true,true,true,string.lower)
--[[
_Scyndi.ADDMBER("..GLOBALS..","DELEGATE","RANGE",true,true,function(s,e,st)
	st = math.abs(st or 1)
	local p = s 
	return function()
		if s<e then 
			p = p + st
			if p>e then return nil end
		elseif s>e then
			p = p - st
			if p<e then return nil end
		else
			return nil
		end
		return p
	end
end) ]]

_Scyndi.ADDMBER("..GLOBALS..","DELEGATE","RANGE2",true,true,true,function(s1,e1,s2,e2,st1,st2)
	st1 = math.abs(st1 or 1)
	st2 = math.abs(st2 or 1)
	local p1 = s1
	local p2 = s2
	local c2 = false
	local first = true
	return function()
		if first then
			first = false
		else 
			if s1<e1 then 
				p1 = p1 + st1 
				if p1>e1 then c2=true end
			elseif s1>e1 then
				p1 = p1 - st1
				if p1<e1 then c2=true end
			else
				return nil,nil
			end
			if c2 then
				c2=false
				p1 = s1
				if s2<e2 then 
					p2 = p2 + st2
					if p2>e2 then return nil end
				elseif s2>e2 then
					p2 = p2 - st2
					if p2<e2 then return nil end
				else
					return nil,nil
				end
			end
		end
		return p1,p2
	end
end)

_Scyndi.ADDMBER("..GLOBALS..","DELEGATE","EACH_CHAIN",true,true,true,function (Start)
	local current = Start
	return function()
		if not current then return nil end
		local ret = current
		current = current.Next
		return ret
	end
end)


_Scyndi.ADDMBER("..GLOBALS..","DELEGATE","APPEND",true,true,true,function(tab,...) for _,v in ipairs{...} do tab[Scyndi.Globals.Len(tab)]=v end end)
_Scyndi.ADDMBER("..GLOBALS..","DELEGATE","APPENDARRAY",true,true,true,function(tab,arr) for _,v in Scyndi.Globals.ipairs(arr) do tab[Scyndi.Globals.Len(tab)]=v end end)
_Scyndi.ADDMBER("..GLOBALS..","DELEGATE","SETMETATABLE",true,true,true,setmetatable)
_Scyndi.ADDMBER("..GLOBALS..","NUMBER","PI",true,true,true,math.pi)
_Scyndi.ADDMBER("..GLOBALS..","DELEGATE","TOSTRING",true,true,true,_Scyndi.TOSTRING)
_Scyndi.ADDMBER("..GLOBALS..","DELEGATE","COUT",true,true,true,function(...) io.write(_Glob.SOUT(...)) end)
_Scyndi.ADDMBER("..GLOBALS..","STRING","ENDL",true,true,true,"\n")
_Scyndi.ADDMBER("..GLOBALS..","Delegate","SPRINTF",true,true,true,string.format)
_Scyndi.ADDMBER("..GLOBALS..","DELEGATE","PRINTF",true,true,true,function(fmt,...) io.write(fmt:format(...)) end)
_Scyndi.ADDMBER("..GLOBALS..","DELEGATE","TERNARY",true,true,true,function(cond,wel,niet) if cond then return wel else return niet end end)
_Scyndi.ADDMBER("..GLOBALS..","DELEGATE","SPLIT",true,true,true,function(str,sep)
	-- I *KNOW* that this ain't the fastest method, but this way I can at least make sure the split went the way it SHOULD.
	-- Lua leaves out things that should not be left out otherwise. That's why I chose this method.
	sep = sep or " "
	local w = ""
	local ret = {}
	local i=1
	local wc = 0
	while i<=#str do
		local c=substr(str,i,i+#sep)
		if c==sep then
			ret[wc] = w
			wc = wc + 1
			w = ""
			i = i + #sep
		else
			w = w .. substr(str,i,i)
			i = i + 1
		end
		--print("Scyndi Split>",i,w,#ret)
	end
	if (#w>0) then ret[wc]=w end
	--print("Scyndi Split>",i,w,#ret,"<return")
	return ret
end)
_Scyndi.ADDMBER("..GLOBALS..","Table","LUA",true,true,true,_G)
_Scyndi.ADDMBER("..GLOBALS..","DELEGATE","TRIM",true,true,true,function(s)
		return s:match( "^%s*(.-)%s*$" )
end)
_Scyndi.ADDMBER("..GLOBALS..","DELEGATE","CHR",true,true,true, string.char)
_Scyndi.ADDMBER("..GLOBALS..","DELEGATE","EXPAND",true,true,true,function (t,p)
	assert(type(t)=="table" or type(t)=="string","Table or string expected in Expand request (got "..type(t)..")\n"..debug.traceback())
	p = tonumber(p) or 1                                 
	if p<#t then 
		if type(t)=="string" then
			return t:sub(p,p),_Scyndi.GLOBALS.EXPAND(t,p+1)   
		else
			return t[p],Globals.EXPAND.Value(t,p+1) 
		end
	end
	if p==#t then 
		if type(t)=="string" then
			return t:sub(p,p),_Scyndi.GLOBALS.EXPAND(t,p+1)   
		else
			return t[p] 
		end      
	end
	return nil                                 
end)

_Scyndi.ADDMBER("..GLOBALS..","DELEGATE","EACH",true,true,true,function(tab)
	-- Please note, where Lua starts at index #1, Scyndi will use #0 as the first index, like nearly all other self-respecting programming language.
	assert(type(tab)=="table","Table expected for ipairs, but got "..type(tab))
	local i=0
	local function ret()
		v = tab[i]
		if (v==nil) then return nil end
		i = i + 1
		return v
	end
	return ret
end)

_Scyndi.ADDMBER("..GLOBALS..","DELEGATE","IPAIRS",true,true,true,function(tab)
	-- Please note, where Lua starts at index #1, Scyndi will use #0 as the first index, like nearly all other self-respecting programming language.
	assert(type(tab)=="table","Table expected for ipairs, but got "..type(tab))
	local i=0
	local function ret()
		v = tab[i]
		if (v==nil) then return nil,nil end
		i = i + 1
		return i-1,v
	end
	return ret
end)
_Scyndi.ADDMBER("..GLOBALS..","DELEGATE","PAIRS",true,true,true,pairs)

_Scyndi.ADDMBER("..GLOBALS..","DELEGATE","SPAIRS",true,true,true,function(t, order)
		-- collect the keys
		local keys = {}
		local t2  = {}
		for k,v in pairs(t) do keys[#keys+1] = k  t2[k]=v end
			-- if order function given, sort by it by passing the table and keys a, b,
			-- otherwise just sort the keys 
			if order then
				function bo(a,b) 
					return order(t, a, b) 
				end
				table.sort(keys, bo)
			else
			table.sort(keys)
		end
		-- return the iterator function
		local i = 0
		return function()
			i = i + 1
			if keys[i] then
				return keys[i], t2[keys[i]]
			end
		end
	end)

_Scyndi.ADDMBER("..GLOBALS..","DELEGATE","NEWARRAY",true,true,true,function(...)
	local ret = {}	
	for i,v in ipairs{...} do
		ret[i-1]=v
	end
	return ret
end)

_Scyndi.ADDMBER("..GLOBALS..","DELEGATE","LEN",true,true,true,function(value)
	if type(value)=="string" then
		return #value
	elseif type(value)=="table" then
		if (value[".ISCLASS"]) then
			return value.GetLen()
		elseif (value[".ISCLASSINSTANCE"]) then
			return value.GetLen()
		else
			local i=0
			while value[i] do i=i+1 end
			return i
		end
	else
		error(type(value).." can not be used as an argument for Len()")
	end
end)

_Scyndi.ADDMBER("..GLOBALS..","DELEGATE","ASSERT",true,true,true,assert)
_Scyndi.ADDMBER("..GLOBALS..","DELEGATE","ROUND",true,true,true,function(a) return math.floor(a+.5) end)

_Scyndi.ADDMBER("..GLOBALS..","DELEGATE","FIND",true,true,true,function(HayStack,Needle)
	if type(HayStack)=="string" then
		for i=1,#HayStack do
			if _Scyndi.GLOBALS.MID(HayStack,i,#Needle)==Needle then return i-1 end
		end
		return nil
	end
	if type(HayStack)=="table" then
		for i=0,_Scyndi.Globals.Len(HayStack)-1 do
			if Needle==HeyStack[i] then return i end
		end
		return nil
	end
	error("Cannot search "..type(HayStack))
end)

_Scyndi.ADDMBER("..GLOBALS..","DELEGATE","TYPE",true,true,true,function(id,ignorescyndi)
	if (not ignorescyndi) and type(HayStack)=="table" then
		if id==_Scyndi then return "Scyndi" end
		if id[".Classinstance"] then return "Scyndi Class Instance::"..ud[".ClassName"] end
		if id[".IsClass"] then return "Scyndi Class" end
	end
	return type(id)
end)

_Scyndi.ADDMBER("..GLOBALS..","DELEGATE","ARRAYREMOVE",true,true,true,function(ARRAY,VICTIM,TIMES)
	assert(type(ARRAY)=="table","Table expected for ArrayRemove. Got "..type(ARRAY))
	repeat
		if TIMES then
			if TIMES<=0 then return end 
			TIMES = TIMES - 1
		end
		local pos = _Scyndi.Globals.Find(ARRAY,VICTIM)
		if not pos then return end
		local siz = _Scyndi.Globals.Len(ARRAY)
		for i=pos,siz do
			ARRAY[i] = ARRAY[i+1]
		end
	until false
end)

_Scyndi.ADDMBER("..GLOBALS..","DELEGATE","GLOBALEXISTS",true,true,true,function(gi)
	return _Scyndi.CLASS["..GLOBALS.."][".HasMember"](gi)
end)



-- ***** Incrementor / Decrementor support ***** --
function _Scyndi.INC(v)
	local t=type(v)
	if t=="number" then
		return v+1
	elseif t=="table" and v[".ClassInstance"] then
		return v.__INC()
	else
		error("Incrementor not possible to type "..t)
	end
end

function _Scyndi.DEC(v)
	local t=type(v)
	if t=="number" then
		return v-1
	elseif t=="table" and v[".ClassInstance"] then
		return v.__DEC()
	else
		error("Decrementor not possible to type "..t)
	end
end

function _Scyndi.ADD(v,a)
	local t=type(v)
	if t=="number" then
		return v+a
	elseif t=="string" then
		return v..a
	elseif t=="table" and v[".ClassInstance"] then
		v.__ADD(v,a)
		return v
	elseif t=="table" then
		v[_Scyndi.GLOBALS.LEN(v)] = a
		return v
	else
		print(debug.traceback())
		error("Don't know how to add to a "..t.."\n ADD("..tostring(v)..", "..tostring(a).." )")
	end
end
	
function _Scyndi.SUBSTRACT(v,a)
	local t=type(v)
	if t=="number" then
		return v-a
	elseif t=="string" then
		error("Can't substract from strings")
	elseif t=="table" and v[".ClassInstance"] then
		return v.__SUBSTRACT(v,a)
	elseif t=="table" then
		_Scyndi.Globals.ArrayRemove(v,a,1)
		return v
	else
		error("Don't know how to substract a "..t)
	end
end

_Scyndi.SUB = _Scyndi.SUBSTRACT


-- ***** Lua basic modules/libraries/whatever copied into Scyndi groups ***** --
local function Lua2GlobGroup(original,target)
	local pub,prv = _Scyndi.STARTCLASS(target,true,true,nil)
	for k,v in pairs(original) do
		if type(v)=="function" then
			_Scyndi.ADDMBER(target,"DELEGATE",k:upper(),true,true,true,v)
		elseif type(v)=="userdata" then
			_Scyndi.ADDMBER(target,"VAR",k:upper(),true,true,true,v)
		elseif type(v)=="number" then
			_Scyndi.ADDMBER(target,"NUMBER",k:upper(),true,true,true,v)
		elseif type(v)=="string" then
			_Scyndi.ADDMBER(target,"STRING",k:upper(),true,true,true,v)
		elseif type(v)=="boolean" then
			_Scyndi.ADDMBER(target,"BOOLEAN",k:upper(),true,true,true,v)
		elseif type(v)=="table" then
			_Scyndi.ADDMBER(target,"TABLE",k:upper(),true,true,true,v)
		else
			_Scyndi.ADDMBER(target,"VAR",k:upper(),true,true,true,v)
		end
	end
	_Scyndi.SEAL(target)
end

Lua2GlobGroup(debug,"LDebug") -- Actually just to make sure no confusion with a debug lib I may provide myself would ever be possible.
Lua2GlobGroup(os,"io")
Lua2GlobGroup(os,"os")
Lua2GlobGroup(math,"math")
Lua2GlobGroup(string,"lstring")
Lua2GlobGroup(table,"ltable")

Lua2GlobGroup(_G,"PrLua") -- Difference between this and "Lua" is that "Lua" links directly to _G and this one does not and can therefore also not access non-existent variables, nor be used to assign any data to either new or existent members



-- ***** Scyndi Use ***** --
local function DefaultUse(file)
	local f = assert(io.open(file, "rb"),"File "..file.." could not be opened")
	local src = f:read("*all")
	f:close()
	local func = (loadstring or load)(src) -- In older versions of Lua you need loadstring() and in newer versions you need load.
	func()
end

local FilesUsed = {}
local UseFunction = DefaultUse
local UseCaseSensitive = true -- Taking Unix file systems as standard here.
function _Scyndi.USE(file)
	local ufile = file
	if not UseCaseSensitive then ufile = file:upper() end
	if FilesUsed[ufile] then return end
	UseFunction = UseFuncion or DefaultUse
	UseFunction(file)
	FilesUsed[ufile]=true
end

function _Scyndi.SETUSEFUNCTION(f)
	assert(f==nil or type(f)=="function","Function expected but got "..type(f).." for SetUseFunction")
	UseFuncion = f
end

function _Scyndi.SETUSECASESENSITIVE(b) UseCaseSensitive = b end
	


-- ***** C++ Generator for base globals so the compiler will know them ***** --
function _Scyndi.GLOBALSFORCPLUSPLUS()
	print("// Please note that this code is generated (also the reason why you can't find it in the respository)\n")
	print("// Generated "..os.date())
	print("\n\n#include <map>\n#include <string>\n\n")
	print("namespace Scyndi {")
	print("\tstd::map<std::string,std::string> CoreGlobals {\n")
	local d
	for k,v in _Scyndi.GLOBALS.spairs(classregister["..GLOBALS.."].staticmembers) do
		if d then print(",") end d = true
		io.write(string.format("\t\t{\"%s\", \"Scyndi.Globals[\\\"%s\\\"]\"}",k,k))
	end
	print("\n\t};\n}\n")
end

local function AllStuff_Index(s,key)
	key = key:upper()
	local ret 
	if classregister[key] then return classregister[key].pub end
	if classregister["..GLOBALS.."].staticmembers[key] then return _Scyndi.GLOBALS[key] end
	error("Neither a class nor a global named "..key.." has been found")	
end

local function AllStuff_NewIndex(s,key,value)
	key = key:upper()
	assert(not classregister[key],"Classes are read-only!")
	assert(classregister["..GLOBALS.."].staticmembers[key],"No global named "..key.." found")
	_Scyndi.GLOBALS[key]=value
end

_Scyndi.ALLIDENTIFIERS = setmetatable({},{__index=AllStuff_Index,__newindex=AllStuff_NewIndex})



-- ***** DEBUG linkups ***** --

-- Please note that these require the underlying engine to work properly. 
-- When not properly configured, the functions here will be ignored.
-- The engine should contain its own script linkups to take care of this and load thise prior to the translated scripts.

-- Please note. Scyndi code itself should not be able to link directly 
-- to this (although you can ways do Lua.Globals.Scyndi.Debug if you really desire)
-- The Scyndi compiler will put the calls if you set it to make a debug build

local _ScyndiDebug = {}

function _Scyndi.INITDEBUGFUNCTIONS(

	-- void SetDebug(bool DebugOnOrOff)
	SetDebug,

	-- string StateName()
	StateName,

	-- void Push(string functionname)
	Push,

	-- void Pop()
	Pop,

	-- void Line(string State,string source, int linenumber)
	Line,

	-- bool ReadOnly = true
	ReadOnly
)

	_ScyndiDebug.SETDEBUG=SetDebug
	_ScyndiDebug.STATENAME=StateName or "Nameless State"
	_ScyndiDebug.PUSH=Push
	_ScyndiDebug.POP=Pop
	_ScyndiDebug.LINE=Line
	_ScyndiDebug.READONLY=ReadOnly~=false
end

function _Scyndi.NIKS() end -- "Niks" means "nothing" in Dutch, and that's precisely what I want from this function!

_Scyndi.DEBUG = setmetatable({},{
	__newindex = function(s,k,v)
		if _ScyndiDebug.READONLY then return end
		_ScyndiDebug[k:upper()] = v
	end,
	__index = function(s,k)
		k=k:upper()
		if k=="READONLY" then return _ScyndiDebug.READONLY end
		return _ScyndiDebug[k] or _Scyndi.NIKS
	end
		
})
