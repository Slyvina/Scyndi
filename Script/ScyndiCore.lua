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
		if _Scyndi.SETTINGS.STRICTNUM then assert(type(value)=="number","Byte expected but got ("..type(value)..")")) end
		if type(value)=="string" then value=value:tonumber() or 0 end
		if _Scyndi.SETTINGS.STRICTINT then assert(math.floor(value)~=value,"Byte expected value is not integer typed")) end
		if _Scyndi.SETTINGS.STRICTBYTE then assert(value>=0 and value<=255,"Byte expected, but the value exceeded the range") end
		return math.floor(value) % 256
	elseif dtype=="BOOLEAN" or dtype=="BOOL" then
		return value~=nil and value~=false and value~="" and value~=0
	elseif dtype=="STRING" then
		if _Scyndi.SETTINGS.STRICTSTRING then assert(type(value)=="string","String expected but got ("..type(value)..")")) end
		return _Scyndi.TOSTRING(value)
	elseif dtype=="VAR" or dtype=="PLUA" then
		return value
	elseif dtype:strsub(0)=="@" then
		error("ClassType checking not yet impelemted!")
	elseif dtype=="TABLE" then
		assert(value==nil or type(value)=="table","Table expected but got ("..type(value)..")"))
		return value
	elseif dtype=="FUNCTION" or dtype=="DELEGATE" then
		assert(value==nil or type(value)=="function","Delegate expected but got ("..type(value)..")"))
		return value
	else
		error("Unknown type "..dtype)
	end
end

-- ***** Identifiers ***** --
local Identifier = {}

-- ***** Class Functions ***** --
local classregister = {}

function _Scyndi.STARTCLASS(classname,staticclass,sealable,extends)
	local _class = { 
		name=classname, 
		staticclass=staticclass, 
		sealed=false, 
		sealable=sealable,
		staticmembers={},
		nonstaticmembers={}
		pub={}
		priv={}
	}	
	local _static=_class.staticmembers
	local _nonstatic=_class.nonstaticmembers
	local cu = classname:upper()
	assert(not(Identifier[cu] or classregister[cu]),"Class has dupe name")
	if (extends) then
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
	local ret = setmetatable(class.pub,meta)
	local retpriv = setmetatable(class.priv,metapriv)
	classregister[cu]=_class
	return ret,retpriv
end

function index_static_member(cl,key,allowprivate)
	key=key:upper()
	assert(classregister[cu],"Class "..cl.." unknown")
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

function _Scyndi.ADDMBER(ch,dtype,name,static,readonly,constant,value)
	ch=ch:upper()
	name=name:upper()	
	assert(classregister[cu],"Class "..cl.." unknown")
	local _class=classregister[cu]
	assert(not _class.sealed,"Class "..cl.." is already sealed. No new members allowed!")
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

